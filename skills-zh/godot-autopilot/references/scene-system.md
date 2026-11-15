# 场景系统

编辑器一次只编辑一个场景，其它场景留在标签页。节点路径相对被编辑场景的根节点解析。首次调用前先发现并 describe 工具；调用协议见 `references/tools.md`。

## 场景生命周期

1. `create_editor_scene` 创建带类型根节点的空场景。
2. 构建节点、属性与信号。
3. `save_editor_scene` 或 `save_editor_scene_as` 写入磁盘。
4. `open_editor_scene` / `close_editor_scene` 切换场景。
5. `set_editor_main_scene` 设为项目启动场景。

- `create_editor_scene` 只接受原生 Node 子类，不能创建继承场景；要继承现有场景，复制文件或打开后 `save_editor_scene_as`。
- 当前场景有未保存改动时，替换会被拒绝；先保存，再用 `close_current` 重试。非 Node 子类会明确报错。
- `open_editor_scene` 同样拒绝未保存场景；错误会列出未保存路径，并建议保存或用 `reload_editor_scene` 丢弃。
- `save_editor_scene` 对从未保存的场景写入 `res://<根节点名>.tscn`；需要显式路径时用 `save_editor_scene_as`。`save_editor_scenes` 保存所有打开标签，但没有文件路径的场景无法写入。
- `close_editor_scene` 拒绝未保存改动。`reload_editor_scene` 从磁盘恢复且不确认即丢弃编辑，同时清除该场景 undo 历史；改动重要时先保存。
- `verify_scene_saved` 对比内存树与磁盘 `.tscn`，报告缺失路径；要求每个节点都落盘时在保存后运行。

## 节点

`create_scene_node` 接受 `type`、`name`、`parent_path` 与可选 `properties`。场景没有根时才可省略 `parent_path`；已有根时省略会报 `scene already has a root, use parent_path to add children`。

`properties` 在创建后立即通过与 `property_set` 相同的转换链应用：NodePath 字符串变成节点引用，`{"path": "res://..."}` 解析资源，`{"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}` 创建内联子资源。任一属性失败都会释放新节点并报告属性名，不会留下半配置节点。创建是一步 undo。

`build_nodes_from_spec` 用一份声明式 spec 构建子树；`dry_run` 只校验与转换，不写入；任一步失败会整体回滚。

- `delete_scene_node` 接受 `path`；根节点拒绝删除。节点在帧末释放，立即读树可能仍能看到。undo 会在延迟释放前按原 owner 与兄弟索引重新加入。
- `rename_scene_node` 拒绝 `/`、`:` 与同级重名；是一步 undo。
- `reparent_node` 接受 `path` 与 `new_parent_path`；拒绝根、自身、后代与当前父节点。两个节点同为 2D 或同为 3D 时，`keep_world_position` 保留世界变换。禁止用手写 remove/add 模拟移动：那样不记录 undo，也跳过 owner 与兄弟索引修正。
- `instantiate_scene` 在被编辑场景下实例化 `.tscn`。`owner` 默认 true，实例子节点会随场景保存；源场景的持久连接会被继承，不要重复连接。

组名是普通字符串；`add_group_node` 与 `remove_group_node` 可 undo，成员关系随场景保存。`has_group_node` 检查成员关系。

## 检查

- `get_scene_tree` 返回每个节点的 `name`、`type`、`path` 与 `children`。`max_depth` 默认 8（`-1` 无限）；`include_properties` 为每个节点附加最多 20 个过滤属性。
- `get_editor_edited_scene_root` 用于开始场景工作流；没有打开场景时为 null。
- `property_get_list` 返回全部属性及 type、hint、hint_string、usage flags 与 class_name；`only_script_variables` 与 `property_filter` 可收窄。
- `get_editor_selection` 与 `set_editor_selection` 读取或聚焦选择；`capture_editor_viewport` 返回 PNG 供视觉核对。
- `validate_scene_file` 干跑场景文件：绕过资源缓存加载、检查依赖、实例化并释放；批量编辑提交前使用。

## 属性

- `property_get`（`path`、`property`）返回精确序列化值；未知节点或属性会带候选建议报错。
- `property_set` 单值形式接受 `path`、`property` 与 `value`；批量形式接受 `path` 加 `properties`（互斥，最多 32 项）。批量先应用对象类型项，再应用其余项。
- 属性名支持点路径（`a.b.c`）进入子资源：逐级取中间值，叶子做类型检查后 set。不做表达式求值或方法调用。
- 省略 `type_hint` 时从属性元数据推断 Variant 类型。每次写入都会读回：不一致要么报错，要么在引擎转换值时警告。Object/Array 属性若引擎未真正应用，会以 `value not applied` 报错。
- 每次成功 set 都可 undo，除非旧值或新值是 Object 引用；此时响应 `undoable: false` 并说明原因。
- 节点类型属性接受路径字符串并转成节点引用；无法解析的路径是错误。
- 数组属性逐元素转换：节点元素接受路径或 `{"__node_ref__": "..."}`；资源元素接受 `res://...`、`memory://...`、`{"path": "res://..."}` 或 null。非数组值会报错，而不是清空属性。
- 字典属性接受 JSON 对象。SpriteFrames 的 `animations` 接受引擎数组或名称到 `{frames, speed, loop}` 的映射。
- 资源属性接受内联描述，嵌套最多四层；保存时写成 `sub_resource`。
- 给 int 属性赋字符串会静默转成 0 并报 ok；任何不寻常值形状写入后都用 `property_get` 读回。

值形状：

| Godot 类型 | JSON |
|---|---|
| Vector2 | `{"x": 1, "y": 2}` |
| Vector3 / Vector3i | `{"x": 1, "y": 2, "z": 3}` |
| Vector4 / Quaternion | `{"x": 0, "y": 0, "z": 0, "w": 1}` |
| Color | `{"r": 1, "g": 0.5, "b": 0.25, "a": 1}` |
| Rect2 / Rect2i | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| AABB | `{"position": {...}, "size": {"w": 64, "h": 32, "d": 16}}` |
| Transform2D | `{"columns": [[1, 0], [0, 1], [x, y]]}` |
| Transform3D | `{"basis": {"rows": [[...]]}, "origin": {...}}` |
| Resource | `{"path": "res://icon.svg"}` 或 `{"resource": "memory://name"}` |
| 内联子资源 | `{"type": "RectangleShape2D", "properties": {...}}` |
| 节点引用 | 路径字符串；数组内用 `{"__node_ref__": "Path/To/Node"}` |

Rect2/AABB 的 size 必须有 `w`/`h`（AABB 还要 `d`）；冲突别名报错。Transform2D 只读 `columns`。Vector3/Vector4/Quaternion 必须给全部分量。等于默认值的属性不会写入保存文件，缺行表示“等于默认”，不是数据丢失。

## 信号

- `signal_connect` 需要 `source_path`、`signal`、`target_path` 与 `method`。`persist` 默认 true，使用 CONNECT_PERSIST，连接随场景保存并被每次实例化继承。已存在的连接返回 `already_connected`。
- `signal_disconnect` 幂等；不存在的连接返回 `not_connected`，不是错误。
- `trace_signal_flow` 检查节点周围的边；每条边报告 `from`、`signal`、`to`、`method` 与 `persisted`。

## Undo 模型

每个编辑工具记录恰好一个 action。action 按目标对象路由：被编辑场景的节点与内建子资源进入该场景历史，其它进入全局历史。每个标签有独立历史，提交 action 会清空其它 redo 栈。没有操作的 action 提交时丢弃。对 Resource 执行操作会标脏，undo 不会恢复干净；每个历史的已保存版本决定未保存状态。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 属性写不进去 | 只读、类型不对或引擎转换 | `property_get`；检查元数据 |
| 保存后丢节点 | owner 不是场景根 | 设置 owner 或正确使用 `instantiate_scene` |
| 删除后立即读树仍有节点 | 帧末延迟释放 | 下一帧再读 |
| 保存后连接丢失 | 连接不是持久连接 | 用 `persist: true` 重连 |
| 节点路径找不到 | 路径形式不对 | 用 `Root/Child`、`/root/Root/Child` 或 `Child` |
| 场景切换报 ok 但没切换 | 编辑器正处于场景切换中 | 等切换结束后重试 |
