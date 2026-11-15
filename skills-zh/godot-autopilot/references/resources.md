# 资源与文件

处理 `.tres`/`.tscn`、`.import` 边车、UID 与 `res://` 布局，以及文本文件工具。

## 加载、创建、复制、保存

- `load_resource` 带可选 `type_hint` 加载文件，返回定位器（`class`、`path`、`object_id`、`object_id_str`）。
- `create_resource` 按 `type` 实例化 Resource 子类；`name` 把它注册为 `memory://name`。
- `duplicate_resource` 在内存中复制文件资源（`deep` 复制子资源）；`name` 注册副本。用 `save_resource` 落盘。内存实例必须先保存并重载才能再次复制。
- `save_resource` 需要 `path`；`dest_path` 覆盖目标。定位器为 `object_id`、已注册 `name` 或 `path`。按 `path` 定位会从磁盘读取，未保存的内存改动不会包含；要保存活实例就传 `name` 或 `object_id`。`dest_path` 不同时是写时复制。缺失目录会创建，文件系统刷新，`verified: true` 表示文件可加载。
- `reload_resource` 从磁盘强制重载并替换缓存实例，后续读取与按路径保存可看到外部改动。
- `copy_resource_file` 把字节复制到 `dest_path`，创建父目录并覆盖目标；不重写引用，也不复制 `.import`/`.uid` 边车。需要重新序列化实例时用 load/save。
- `memory://` 资源绝不能赋给节点属性；先落盘，再按路径赋值。tilemap 与动画工具创建的 TileSet、SpriteFrames 同样适用。

## 检查与搜索

- `get_resource_type` 报告文件实际持有的类；`get_resource_types` 列出可实例化的 Resource 子类。`has_resource` 问的是加载器能否加载，不是文件是否存在。
- `get_resource_uid` 读数字 UID（没有时为 `-1`）；`set_resource_uid` 赋值并触发 reimport 使其持久。`get_resource_dependencies` 列出外部依赖；`has_resource_dependency` 检查单条边。`get_resource_references` 做反向查询，`path`、`uid`、`class_name` 只能传一个。
- `read_file` 返回完整文本（`res://`、`user://` 或绝对路径）。`find_in_files` 递归搜索文本；`query` 必填，`dir` 默认 `res://`，`extensions` 默认 gd/tscn/tres/cs/md/json/h/cpp，`max_results` 默认 500；命中上限返回 `truncated: true`。这是证明重命名重写了全部引用的标准方法。

## 事务性重命名与移动

`rename_resource_file`（`path`、`new_path`）只处理单文件；目录会快速失败并提示用 `move_resource_file`（`path`、`new_directory`）。两者都是引擎感知事务：

1. 保存引用该文件的打开场景，重写磁盘文件，再重载这些场景（其 undo 历史重置）。
2. 重写所有依赖 `.tscn`/`.tres` 中的 `path=` 与 `uid=`；移动 `.uid` 边车（以及导入资源的 `.import`），保持 UID 稳定。
3. 重映射 `project.godot` 的 `application/run/main_scene`、`autoload/*` 等条目，保留前导 `*` 单例标记，然后通知编辑器文件系统。

读取结构化报告：`updated_files`、`stale_references`、`uid_preserved`、`saved_scenes`、`reloaded_scenes`、`unsupported_binary_references`、`closed_tabs`、`import_sidecar_warning`。`script_class` token 表示全局类名变了；运行 `scan_editor_file_system` 使其重新注册。

`move_resource_file` 保持文件名，接受 `res://` 内的 `new_directory`（绝对或相对；去掉尾斜杠；拒绝 `user://`）。目录会递归移动，保留子目录布局，留下空源目录，并在之后扫描。

重载受影响的打开场景会清空其 undo 历史。事务先保存以缩小窗口，但无关打开场景中的未保存编辑仍有风险。仍指向已移动或强制删除文件的标签会自动关闭；当前编辑场景保持打开并记入 `closed_tabs_warning`。

## 安全删除

`remove_resource_file` 分两阶段。默认 dry-run 扫描 `res://` 下所有 `.tscn`/`.tres` 的 path 与 uid 引用，返回 `would_delete` 与 `dependents`，不删除任何内容。`force=true` 时优先移入系统回收站（`trashed: true`；回收站不可用时 `permanent: true`），`.uid` 边车随行。要丢弃整个目录，用 OS 域的 `move_os_file_to_trash`。

## 导入与就绪

- `reimport_resource_files` 排队一个或多个文件，异步执行；只对带 `.import` 边车的文件有效。`.tres`/`.gd` 会触发引擎 BUG 错误，应改用 `reload_resource`。轮询 `get_editor_file_system_status` 直到 `scanning` 为 false。
- `set_resource_import_options` 写入白名单 `.import` `[params]` 键（音频 loop/compression、纹理 filter/mipmap）并排队 reimport。手改 `.import` 参数已弃用。
- `scan_editor_file_system` 在带外改动或类名重命名后重新扫描。扫描进行中的请求会被静默丢弃；使用可重试软错误并轮询到 scanning 为 false。

导入或扫描期间，资源写入以 `retryable: true` 与 `retry_after_ms` 软失败；等待后重试同一调用。

## 路径规则

- 资源操作只接受 `res://`。
- `write_file` 与 `read_file` 还接受 `user://`；`read_file` 接受绝对路径。禁止把一个命名空间的路径拼进另一个。
- 路径会规范化并做边界检查：穿越、绝对路径逃逸、未知 scheme 与项目外写入都会被拒绝。
- `res://` 内的 `write_file` 由引擎管理：导入资源排队 reimport，纯文本走 `update_file`，脚本类文件返回诊断。`user://` 与绝对写入报告 `engine_managed: false`。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 保存后节点丢资源 | 赋了 `memory://` 资源 | 先落盘，再按路径赋值 |
| 重命名后仍有旧引用 | 不支持的二进制资源或类名 | 读 `stale_references`；重扫 |
| 写入返回 `retryable` | 编辑器正在导入或扫描 | 等待 `retry_after_ms` 后重试 |
| 重命名后 undo 丢失 | 引用场景被重载 | 事务前保存全部场景 |
