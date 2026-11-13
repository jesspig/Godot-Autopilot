---
name: godot-autopilot-scene-system
description: "Godot 场景系统操作指南：场景生命周期、节点创建/删除/重命名/换父、按 JSON 值形状读写属性、信号连接、编辑器撤销历史模型与 .tscn 序列化细节。适用于搭建或重构场景、设置属性或连接信号。"
---

# Godot 场景系统

调用方式：用 `gda search` 与 `gda describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

本册覆盖在 Godot 编辑器中创建、重构、保存与检查场景，以及把节点树变成可运行
场景所需的属性读写与信号连接。节点路径相对当前编辑场景根解析；合法路径形式
与错误文本见“坑”一节。属性值形状、Godot 3 到 4 重命名表与序列化规则见下文
属性一节。

## 场景生命周期

编辑器一次只编辑一个场景，其他场景可留在标签页。典型流程：

1. `create_editor_scene` — 用指定类型根节点新建空场景。
2. 构建内容 — 节点、属性、信号。
3. `save_editor_scene` 或 `save_editor_scene_as` — 落盘。
4. `open_editor_scene` / `close_editor_scene` — 切换场景。
5. 可选 `set_editor_main_scene` — 设为工程启动场景。

### 创建场景

`create_editor_scene` 用给定 `type`（任意 Node 子类，默认 `Node`）与 `name`
（默认 `NewRoot`）创建空场景。`close_current`（默认 false）在旧场景没有未保存
修改时关闭它。`timeout_ms`（默认 2000，范围 50-30000）限制等待编辑器观察到
新根的时间；超时会带 waited_ms、timeout_ms、node_released 与 editor_state
诊断返回。

只支持原生节点根：`type` 必须是可直接实例化的引擎类，因此继承场景（根来自
另一个场景）不能用它创建。要派生已有场景，复制场景文件
（`copy_resource_file`），或打开后另存为新路径（`save_editor_scene_as`）。

当前场景有未保存修改时拒绝替换。先保存再重试。`type` 不是 Node 子类时报
`<type> is not a Node subclass`。

### 打开、保存、关闭与重载

- `open_editor_scene` 打开 `res://game.tscn` 这类场景并替换当前编辑场景；当前
  场景有未保存修改时报错，并列出未保存路径与恢复建议：保存，或对列出的路径
  调用 `reload_editor_scene` 丢弃修改后重试。
- `save_editor_scene` 保存当前场景。从未保存过的场景会自动写到
  `res://<根节点名>.tscn`，响应 note 会说明；需要显式路径时用
  `save_editor_scene_as`（会创建缺失的父目录）。
- `save_editor_scenes` 一次保存所有打开的场景标签；没有文件路径的场景无法
  写盘。
- `close_editor_scene` 拒绝关闭有未保存修改的场景。
- `reload_editor_scene` 从磁盘恢复场景，直接丢弃未保存修改（当前场景，或给定
  `scene_path`）。目标场景未打开时报 `scene is not open`，不伪造成功；成功时
  返回 `{"result": "reloaded", "scene_path": ...}`。重载还会清空该场景的撤销
  历史。
- `set_editor_main_scene` 接收 `res://game.tscn` 这类 `path`，把主场景设置
  持久化到 project.godot。
- `verify_scene_saved` 不写盘，只比较内存场景树与磁盘 .tscn，报告
  match/mismatch 与缺失路径。每个节点都必须落盘时，保存后运行它。

## 创建节点

`create_scene_node` 参数：

- `type` — 任意 Node 子类（默认 `Node`）。
- `name` — 同级唯一（默认 `NewNode`）。
- `parent_path` — 仅在场景还没有根时省略；已有根时必须传父路径。
- `properties` — 创建后立即应用的对象，支持内联资源描述。

场景已有根但未给 `parent_path` 时失败：
`scene already has a root, use parent_path to add children`。

`properties` 通过 `property_set` 同一转换链赋值：`NodePath` 字符串变成节点
引用，`{"path": "res://..."}` 解析为资源，内联资源描述
`{"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}`
一步创建无路径子资源。任一属性失败会释放新节点并报告属性名，不会留下半配置
节点。创建是一个撤销步骤；响应带新节点 `path`、已应用属性、
`inline_resources` 与撤销提示。

```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "Root",
    "name": "Player",
    "type": "CharacterBody2D",
    "properties": {"position": {"x": 0, "y": -10}}
  }
}
```

- `build_nodes_from_spec` 用一份声明式 spec 构建整棵子树（嵌套 type/name/props/
  children 走同一转换链）；`dry_run` 只校验不写，失败步骤整体回滚，无残留。

## 删除、重命名与换父

- `delete_scene_node` 接收 `path`，由编辑器撤销/重做管理：do 步骤移除子节点并
  在帧末释放，undo 按原父、owner 与同级索引恢复。响应返回 `deleted`、
  `undoable: true` 与撤销信息。场景根不可删除；用 `close_editor_scene` 关闭
  场景后再新建。
- `rename_scene_node` 接收 `path` 与 `new_name`。`new_name` 拒绝 `/` 与 `:`，
  同级重名也被拒绝。重命名是一个撤销步骤，undo 恢复旧名；响应返回 `renamed`
  与新相对路径。
- `reparent_node` 移动节点：`path` 与 `new_parent_path`。拒绝场景根、节点
  自身、其后代与当前父节点。它是一个撤销步骤，undo 恢复旧父、原同级索引与
  owner。`keep_world_position`（默认 false）在节点与新父同为 `Node2D` 或同为
  `Node3D` 时保留世界变换；其他类型组合忽略。旧 owner 是场景根时，节点仍归
  当前场景根所有，确保随场景保存。不要用 `remove_child` + `add_child` 模拟
  移动：手工路径不记录撤销，也跳过同级索引与 owner 修正。

## 实例化 PackedScene

`instantiate_scene` 从 `path`（.tscn）实例化到当前编辑场景。`parent_path` 默认
场景根，`name` 可选，`owner` 默认 true，设置场景所有权以便子节点随场景保存。
实例继承源场景中 CONNECT_PERSIST 的连接，不要重复连接同一对信号。

## 分组管理

组名是全局共享的普通字符串，无需预注册；这里添加的成员随场景保存。

- `add_group_node`（`node_path`、`group_name`）：持久添加，注册撤销；已存在时
  先移除再加，幂等。返回 `added`、路径、组名与 `persistent`。
- `remove_group_node`：移除成员，同样可撤销；不在组内是安全 no-op。
- `has_group_node`：返回布尔；节点不存在时报错。

运行时用 `get_scene_tree_nodes_in_group` 列出成员路径，
`call_scene_tree_group` 对组内每个成员调用方法。

## 编辑侧只读检查

修改前先确认存在性与编辑器状态：

- `get_scene_tree` 遍历当前编辑场景，逐节点返回 `name`、`type`、`path` 与
  `children`。`max_depth` 默认 8（-1 不限），`include_properties` 为 true 时
  每节点附带最多 20 个过滤属性。没有打开场景时报错。
- `get_editor_edited_scene_root` 返回当前编辑场景根的 `name`、`type`、`path`；
  无场景时为 null。场景工作从这里开始。
- `property_get_list`（`path`，可选 `only_script_variables`、
  `property_filter`）返回全部属性及元数据；过滤器分别保留脚本变量与名称包含
  给定子串的属性。
- `get_editor_selection` 报告用户选中的节点；`set_editor_selection` 把后续
  操作聚焦到指定节点。
- `get_editor_file_system_tree` 获取工程文件树；超过 12 层截断，响应带
  `max_depth`。在编辑器外创建或修改文件后，调用 `scan_editor_file_system`，
  再轮询 `get_editor_file_system_status` 直到 `scanning` 为 false。
- `capture_editor_viewport` 返回编辑器 2D 视口（3D 回退）的 base64 PNG，用于
  放置节点或改属性后的视觉验证。`save=true` 时另存到
  `user://godot_autopilot/captures/`，只保留最近 20 张。
- `validate_scene_file` 不触碰当前编辑场景，直接干跑磁盘上的 .tscn/.scn：
  绕过资源缓存加载、检查依赖存在性、实例化后释放。返回 `valid`、`problems`
  （`load_failed`、`missing_dependency`、`instantiate_failed`）、
  `missing_dependencies` 与 `dependency_count`。批量修改后或提交前使用。

## 属性读写

- `property_get`（`path`、`property`）返回精确序列化值。未知节点或属性会报错
  并给出候选建议。
- `property_get_list` 返回每个属性的元数据：name、type、hint、hint_string、
  usage flags 与 class_name。先用它发现合法属性名与枚举顺序。
- `get_scene_tree` 的 `include_properties` 是便捷快照（跳过 metadata/、下划线
  前缀与 Object 类型属性）；精确值用 `property_get`。

`property_set` 单条形式接收 `path`、`property`、`value`；批量形式接收 `path`
与 `properties` 对象（互斥，最多 32 条，不能带 `type_hint`）。批量先应用对象
类型再应用其余，返回 `applied_properties`、`property_warnings`、
`inline_resources`、`converted_node_paths` 与逐属性 `undo`。失败项报告
`failed_property` 与已应用属性。属性名支持点路径（`a.b.c`）进入子资源；
中间节点必须是非空对象，叶子做类型检查后 set，禁止表达式求值或方法调用。
省略 `type_hint` 时按属性元数据推断。写入后读回：不一致按错误报告（只读、
不存在或需要显式 type_hint），引擎转换过值时按警告报告。Object/Array 属性
读回 null、空数组或 null 元素属于 `value not applied` 错误，工具会尽量恢复
旧值。

- 每次成功 set 都进入编辑器撤销栈（响应 `undoable: true`）。旧值或新值是
  Object 引用时不记录，响应 `undoable: false` 并给出原因。
- 节点类型属性（引擎 hint `NodeType`，含 C# `[Export]` 节点字段）接受节点路径
  字符串，自动转成节点引用并报告 `converted_node_path`。不可解析路径是错误。
- 数组属性逐元素转换：节点元素用路径字符串或 `{"__node_ref__": "..."}`，
  资源元素用 `res://...` / `memory://...` 字符串、`{"path": "res://..."}` 或
  `{"resource": "memory://..."}`，`null` 留空槽。给数组属性传非数组是错误，
  不安全元素也是错误。
- 字典属性接受 JSON 对象；非对象报错，不写空字典。SpriteFrames
  `animations` 接受引擎数组或名称到 `{frames, speed, loop}` 的字典，逐项
  校验；子资源错误给出真实资源路径。
- 资源属性还接受内联描述：`{"type": "RectangleShape2D", "properties":
  {"size": {"x": 20, "y": 28}}}` 一步创建无路径子资源，保存时写成
  `sub_resource`，响应回显 `inline_resources`，嵌套最多 4 层。
- 给 int 属性赋字符串会静默变成 0 并报告 ok。异常值形状的 set 之后，用
  `property_get` 读回确认实际落值。

```json
{"name": "property_set", "arguments": {"path": "Player", "property": "position", "value": {"x": 12.5, "y": -3.25}}}
{"name": "property_set", "arguments": {"path": "Player", "properties": {"position": {"x": 0, "y": -10}, "visible": false}}}
{"name": "property_set", "arguments": {"path": "Hero", "property": "sprite_frames.animations", "value": {"idle": {"frames": [{"texture": "res://icon.svg"}], "speed": 5.0, "loop": true}}}}
```

常用 Godot 类型的 JSON 形状：

| 类型 | JSON 形状 |
|---|---|
| Vector2 | `{"x": 1, "y": 2}` |
| Vector3 / Vector3i | `{"x": 1, "y": 2, "z": 3}`；三分量必填 |
| Vector4 / Quaternion | `{"x": 0, "y": 0, "z": 0, "w": 1}`；四分量必填 |
| Color | `{"r": 1, "g": 0.5, "b": 0.25, "a": 1}`；`a` 默认 1 |
| Rect2 / Rect2i | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| AABB | `{"position": {"x": 0, "y": 0, "z": 0}, "size": {"w": 64, "h": 32, "d": 16}}` |
| Transform2D | `{"columns": [[1, 0], [0, 1], [x, y]]}` |
| Transform3D | `{"basis": {"rows": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]}, "origin": {"x": 0, "y": 0, "z": 0}}` |
| Basis / Projection | `{"rows": [[...]]}` / `{"columns": [[...]]}` |
| Resource | `{"path": "res://icon.svg"}` 或 `{"resource": "memory://name"}` |
| 内联子资源 | `{"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}` |
| 节点引用 | 节点路径字符串；数组内也可用 `{"__node_ref__": "Path/To/Node"}` |
| PackedByteArray | 普通 JSON 数字数组 |

尺寸与变换规则：Rect2/AABB 的 `size` 必须有 `w`/`h`（AABB 还要 `d`）；
`x`/`y`/`z` 是别名，别名冲突报错。Transform2D 只读 `columns`；Transform3D
必须有 `basis.rows`，`origin` 默认零。Vector3/Vector4/Quaternion 每个分量
必填。RID 读回为 `{"id": 42}`，但 JSON 不会还原成 RID；传工具返回的整数
句柄。等于默认值的属性不会写进保存文件，缺失行表示“等于默认值”，不是数据
丢失。Godot 3 到 4 重命名表（发明名字前先查）：`frames`→`sprite_frames`、
`cast_to`→`target_position`、`rect_position`→`position`、
`rect_global_position`→`global_position`、`rect_size`→`size`、
`rect_min_size`→`custom_minimum_size`、`rect_rotation`→`rotation`、
`rect_scale`→`scale`、`rect_pivot_offset`→`pivot_offset`、
`translation`→`position`。

## 信号

- `signal_connect`：把 `source_path` 上的信号连到 `target_path` 的方法，需要
  `source_path`、`signal`、`target_path`、`method`。`persist` 默认 true，使用
  CONNECT_PERSIST：连接随场景保存，`instantiate_scene` 的实例继承它。设为
  false 是仅运行时不保存的连接。返回 `connected` 或 `already_connected`。
- `signal_disconnect`：同样四个参数，幂等；不存在时返回 `not_connected`，
  不是错误。断开持久连接后不再随场景保存。
- `trace_signal_flow`：查看节点周围接线，`path` 加可选 `direction`
  （`outgoing`/`incoming`/`both`）与 `max_depth`（默认 3，循环会截断）。每条边
  为 `{from, signal, to, method, persisted}`。

```json
{
  "name": "signal_connect",
  "arguments": {
    "source_path": "UI/StartButton",
    "signal": "pressed",
    "target_path": "Game",
    "method": "_on_start_button_pressed"
  }
}
```

## 撤销历史模型

上面每个编辑工具都只记录一个撤销/重做动作，一次 Ctrl-Z 即可回退。引擎模型：

- 动作按目标对象路由：当前编辑场景的节点与内置子资源进入该场景历史，其他
  进入全局历史。每个打开的场景标签有独立历史。
- 提交动作会清空其他历史的重做栈；普通 Ctrl-Z 按时间戳选择最新动作所在的
  历史。
- 没有记录操作的动作在提交时丢弃。撤销/重做时，目标对象已释放的操作会被
  静默跳过。
- 对 Resource 执行操作会标记 dirty；撤销不会让它重新干净。每个历史的已保存
  版本决定未保存状态；带 `mark_unsaved` 的动作即使撤销后也保持未保存。

## 工作流清单

1. 用 `gda search` 与 `gda describe` 确认工具名与 schema，禁止猜。
2. 确认已打开场景：新建用 `create_editor_scene`，已有文件用
   `open_editor_scene`；未保存场景先保存。
3. 创建根节点——只在没有根时省略 `parent_path`。
4. 用 `create_scene_node` 添加子节点，创建时值放 `properties`。大量同形节点用
   `batch_execute` 顺序执行（`stop_on_error` 默认 true）。
5. 设置剩余属性并连接信号。
6. 播放或关闭前用 `save_editor_scene` 落盘。
7. 读回验证：`get_scene_tree`（可带 `include_properties`）与
   `property_get`。
8. 外观重要时截图：`capture_editor_viewport`；批量场景修改提交前运行
   `validate_scene_file`。

## 坑

- 三种合法节点路径：`Root/Child`（带根名）、`/root/Root/Child`（绝对）、
  `Child`（相对根）。不可解析时错误提示会给出当前场景根与合法写法；读提示、
  改路径、重试。
- 当前场景有未保存修改时，`create_editor_scene` 与 `open_editor_scene` 拒绝
  执行；后者建议保存或对列出路径调用 `reload_editor_scene` 丢弃修改，前者
  要求先保存。
- 刚创建或打开场景后，`get_scene_tree` 可能仍反映旧场景——再调一次或稍等
  编辑器刷新。
- 根节点不能删除；用 `close_editor_scene` 关闭场景。
- `delete_scene_node` 在帧末释放节点，紧接着读树可能仍能看到它。
- 编辑器正在切换场景时，打开或重载会被静默跳过（无错误、无效果）；调用报告
  ok 但场景没变时，等切换结束再试。
- 编辑器启动早期打开场景会推迟到初始化完成，随后出现，不会失败。
- 打开已打开的场景只切换标签，不从磁盘重读；要拾取外部修改用
  `reload_editor_scene`，它会清空该场景撤销历史。
- 场景依赖的脚本更新后，编辑器可能静默重新打包并重建打开的场景，清空撤销
  历史；只靠 undo 的修改会无错误地消失。
- 保存会重新打包场景并静默丢弃：owner 不是保存场景根的节点、没有
  CONNECT_PERSIST 的连接、等于计算默认值且未钉住的属性、没有本地属性或组
  变化的实例子场景节点。
- 保存场景会级联保存同时编辑的 dirty 资源；写盘前清 dirty 标记，PackedScene
  资源跳过。
- 每个场景写入工具（`create_scene_node`、`delete_scene_node`、
  `rename_scene_node`、`property_set`、`attach_script_to_node`）都报告
  `scene_path` 与 `scene_unsaved`：`scene_path` 是磁盘路径，保存前为空
  （此时 `scene_unsaved` 为 true）。场景切换静默失败时，后续写入会落到仍
  打开的场景；读 `scene_path` 才能发现放错位置。
- 等于默认值的属性行不会出现在保存文件里——缺失行表示“等于默认值”，不是
  数据丢失。
- 4.7+：进入树时父先于子，`_ready` 子先于父，退出子先于父——节点不能在
  `_enter_tree` 里依赖同级节点已 ready。

## 参见

- `godot-autopilot-resources` - 场景背后的 .tscn/.tres 文件、带引用重写的
  重命名与移动。
- `godot-autopilot-scripting` - 用 `code_execute` 程序化编辑场景。
- `godot-autopilot-runtime` - 运行游戏并实时检查。
