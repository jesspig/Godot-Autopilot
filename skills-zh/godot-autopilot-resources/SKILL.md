---
name: godot-autopilot-resources
description: "Godot 资源与文件操作指南：加载、保存、创建、复制资源；带引用重写的事务性重命名与移动；UID 与依赖管理；导入管线与路径规则。适用于处理 .tres/.tscn/.import 文件或工程目录结构。"
---

# 资源与文件操作

调用方式：用 `gda tools search` 与 `gda tools describe` 发现，再用 `gda call`
调用；原生 MCP 工具（`search_tools`、`get_tool_detail`、`call_tool`）是兼容
路径。协议细节见 `godot-autopilot-tools`。

本册覆盖 .tres/.tscn、.import 边车文件、UID 与 res:// 目录布局，以及三个文本
文件工具（`write_file`、`read_file`、`find_in_files`）。所有工具都通过
`call_tool` / `gda call` 调用；调用前用发现协议确认名称与 schema。

## 加载、创建、复制与保存

- `load_resource` 从磁盘加载资源到内存，可选 `type_hint`，返回 locator
  （`class`、`path`、`object_id`、`object_id_str`），属性工具接受该 locator。
- `create_resource` 按类名实例化 Resource 子类（`type`）：ClassDB 引擎类与
  全局脚本类（GDScript class_name / C# [GlobalClass]）都接受；C# 全局类要求
  编辑器已加载程序集；抽象类或无效脚本被拒绝。传 `name` 可注册实例，后续用
  `memory://name` 引用。
- `duplicate_resource` 在内存中复制文件资源：`deep`（默认 false）控制子资源
  复制，副本以 locator 返回，不写盘。传 `name` 会注册副本，跨调用保持存活；
  用 `save_resource` 落盘。
- `save_resource` 把内存资源写到 `path`；父目录缺失会创建。`overwrite`
  默认 false。写后可读回校验。
- `reload_resource` 用 CACHE_MODE_REPLACE 从磁盘强制重载，替换编辑器缓存
  实例，返回 `{result: "reloaded", path, replaced}`。编辑器外改文件后使用，
  让后续读取与按路径保存看到磁盘内容。

```json
{"name": "create_resource", "arguments": {"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}, "name": "player_shape"}}
{"name": "save_resource", "arguments": {"resource": "memory://player_shape", "path": "res://shapes/player.tres"}}
```

## 线程化加载

`load_resource` 的 `threaded` 选项（默认 false）在后台线程加载大型资源；
调用会等待完成并返回相同 locator。加载失败以结构化错误返回，不要把它当成
资源不存在。线程化加载只影响加载阶段，后续读写仍在编辑器主线程。

## 内省与搜索

- `get_resource_type` 返回资源的 `class` 与脚本类名。
- `has_resource` 判断 `res://` 路径是否存在且可加载。
- `get_resource_dependencies` 列出直接依赖；`dependency_count` 是数量。
- `get_resource_references` 反向查找引用某资源的文件。
- `has_resource_dependency` 判断两个资源之间是否存在依赖。
- `get_resource_uid` 返回 `uid://` 标识；`set_resource_uid` 设置并触发重导入。
- `find_in_files` 在目录下按文本或正则搜索，返回文件、行号与匹配行。
- `get_resource_dir_files` 列出目录下的资源文件。

## 事务性重命名与移动

`rename_resource_file` 与 `move_resource_file` 都会先保存受影响的打开场景，
再重写引用，然后重新加载这些场景。两者都是一个事务：失败时尽量回滚，并用
结构化报告说明未完成的部分。

```bash
gda call rename_resource_file --args '{"from": "res://old.tres", "to": "res://new.tres"}'
gda call move_resource_file --args '{"from": "res://a/x.tres", "to": "res://b/x.tres"}'
```

不要假设成功，读结构化报告。`rename_resource_file` 返回 `updated_files`
（逐文件变更数）、`stale_references`（无法重写的 `{file, token}` 残留）、
`uid_preserved`、`remapped_settings`、`saved_scenes`、`reloaded_scenes`、
`unsupported_binary_references`（.scn/.res/.csv/.translation 等二进制资源
不能原地重写）、关闭过期场景标签时的 `closed_tabs`，以及 .import 移动失败时
的 `import_sidecar_warning`。`move_resource_file` 返回 `moved`
（`{from, to, updated_files, uid_preserved}`）、`failed`，以及聚合的
`stale_references`、`unsupported_binary_references`、`saved_scenes`、
`reloaded_scenes`、`closed_tabs`、`closed_tabs_warning` 与
`import_sidecar_warnings`。

已验证的事务行为：重命名后 `find_in_files` 找到新路径、旧路径不再出现，
`get_resource_references` 从旧翻到新，`uid_preserved` 为 true；移动后
`moved` 非空、`failed` 与 `stale_references` 为空、`reloaded_scenes` 列出
保存并重载的引用场景，`has_resource_dependency` 立即报告新位置。

## 安全删除

`delete_resource_file` 删除 `res://` 文件，默认拒绝删除仍被引用的资源；
`force` 才允许。删除前用 `get_resource_references` 确认影响面。删除打开中的
场景会让编辑器标签失效，先关闭标签。

## 导入与编辑器就绪

- `reimport_resource_files`（仅编辑器）把一个或多个文件（`files` 数组或单个
  `path`）排入重导入。它是异步的：响应只确认排队数量，不确认完成。只有带
  .import 边车的文件有效；.tres/.gd 等非导入文件会触发引擎 BUG 日志，工具
  仍报告 "reimport queued"——这类文件用 `reload_resource` 刷新。轮询
  `get_editor_file_system_status` 直到 `scanning` 为 false。
- `set_resource_import_options`（仅编辑器）是修改导入参数的受支持方式：写入
  白名单 `.import` `[params]` 键（音频 loop/compression 与纹理
  filter/mipmap 族；其他键拒绝），并在一次调用中排队重导入。手写 .import
  改参数已弃用。响应报告已应用键并排队；轮询到 `scanning` 为 false，再用
  `get_resource_type` 或 `read_file` 校验 .import。
- `set_resource_uid` 触发文件系统重导入，使赋值持久化。
- `scan_editor_file_system` 重新扫描工程——class_name 重命名留下
  script_class token，或编辑器外改文件后运行；目录移动会自动触发。

扫描门控：编辑器文件系统是状态机（idle、scanning、processing changes、
importing）。已有扫描或变更过程进行时发出的扫描请求会被静默忽略——不排队、
不报错。会话首次扫描在主线程，之后在低优先级后台线程，其 filesystem_changed
通知延迟到主循环。`get_editor_file_system_status` 是就绪门：响应只有
`scanning`（bool）与 `progress`（float），没有 is_importing 或
doing_first_scan 字段，progress 不是门；轮询到 `scanning` 为 false。

## 编辑器就绪软错误

编辑器导入或扫描时，资源写入会以软错误失败，而不是破坏状态：

```
{"error": "editor is currently importing/scanning resources; retry shortly",
 "retryable": true, "retry_after_ms": 500}
```

等待 `retry_after_ms` 后重试同一调用。

## 路径规则

- 资源操作——加载、保存、创建、复制、重命名、移动、删除、重导入与依赖/引用
  查询——只接受 `res://` 路径。
- `write_file` 与 `read_file` 还接受 `user://`（`read_file` 还接受绝对路径）。
  不要把一种命名空间的路径拼进另一种。
- 所有路径在使用前归一化并做边界检查：路径穿越、绝对路径逃逸、未知 scheme
  与工程外写入都以结构化错误拒绝。
- `write_file` 写 `res://` 时由引擎管理：导入资产类型排队重导入，纯文本走
  update_file，脚本类文件（.gd/.gdshader/.gdshaderinc/.cs）额外返回
  diagnostics，报告脚本是否仍能加载。写 `user://` 或绝对路径是普通 I/O，
  返回 `engine_managed: false`。

## 坑

- **重命名/移动时的打开场景**：每个受影响的打开场景会在依赖重写后立即重载
  （当前编辑场景最先），重载会清空该场景撤销历史。事务前的保存只覆盖事务
  自身编辑；这些场景里其他未保存修改会丢失，撤销无法恢复。
- **复制资源文件会生成新 UID**：通过引擎文件复制管线复制（编辑器复制/粘贴，
  不是内存 `duplicate_resource`）时，副本总是获得新 UID；引擎会先把已编辑的
  缓存资源强制保存回源文件，避免副本与磁盘漂移。后果：原文件旁的副本在
  `uid://` 引用看来是另一个资源——把引用显式指向副本。
- **重复 UID 会被静默重铸**：扫描发现某文件的 UID 已映射到另一个文件时不
  报错——引擎静默为新文件生成新 UID、写回文件数据，只发 WARN。持有旧 UID
  的引用仍解析到旧文件。若复制出的场景“不响应编辑”，比较两文件的
  `get_resource_uid`。
- **扫描不可排队**：编辑器扫描或导入中途的 `scan_editor_file_system` 什么
  都不做。依赖可重试软错误与 `get_editor_file_system_status` 轮询，不要
  fire-and-forget。

## 参见

- `godot-autopilot` - 任务到工具的路由。
- `godot-autopilot-tools` - 发现协议、confirm 与错误协议。
- `godot-autopilot-scene-system` - 场景生命周期（重命名/移动会保存并重载
  打开的场景）与把资源赋给节点属性（memory:// 会被拒绝）。
- `godot-autopilot-content` - 必须先保存才能赋值的 memory:// TileSet 与
  SpriteFrames。
