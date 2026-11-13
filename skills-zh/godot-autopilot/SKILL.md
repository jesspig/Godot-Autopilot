---
name: godot-autopilot
description: "在 Godot 工程中使用 GDA（godot-autopilot）插件的入口：先读安装册，通过 gda status --json 与 mcp_url 连接，用实时 gda search/describe/tools list 发现工具、用 gda call 调用，观察编辑器与游戏状态，遵守 confirm 协议，并执行变更-观察-验证循环。适用于安装完成后的所有 godot-autopilot 工作。"
---

# Godot Autopilot

本册是每个 Godot 工程会话的入口。机器或工程尚未安装 GDA 时，先读
`godot-autopilot-setup`。本册讲连接、工具发现、任务路由、观察与变更-观察-验证
循环。调用协议（CLI 参考、confirm、错误、限制、批处理与代码执行）在
`godot-autopilot-tools`；引擎级操作在七个领域册。

## 连接模型

- `gda` CLI 是一等接口。它发现插件的随机端口，优先使用 IPC，失败回退 HTTP，
  并输出 JSON。
- 端口由插件拥有并持久化。CLI 从不选择监听端口，也从不假设默认端口。
- `gda status --json` 是连接记录。`mcp_url` 是 MCP 客户端唯一权威地址；
  `transport` 与 `ipc_kind` 说明 CLI 实际使用的本地通道。
- MCP 面是给已经列出原生工具的客户端使用的兼容通道。它提供相同操作、完整
  schema、结构化错误与图像内容。客户端已有工具时直接用；否则用 CLI。
- 服务只监听环回地址。不要转发、代理或发布端口。

看不到实例时，`gda list --all` 显示已知工程，`gda start --project <dir>` 启动
编辑器。编辑器重启后不要复用旧端口，重新读取 `mcp_url`。

## 先发现工具，禁止猜名字

工具名是 snake_case，通常动词在前，但段数与命名会随工具集变化。发现是强制
步骤：

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"path": "Level", "cells": []}'
```

1. `gda search <query>` 通过实时服务器按名称、描述、分类与标签搜索，默认最多
   返回 20 条。
2. `gda describe <name>` 打印 schema 与 `side_effect` 标记。首次调用前必读，
   不要推断参数名。
3. `gda call <name> --args '<json>'`（或 `--args-file <path>`）对实时编辑器
   执行一个工具。
4. 需要全量清单而非关键词搜索时，用 `gda tools list` 列出全部直接工具。

发现只有实时通道：没有离线目录。没有实例时用
`gda start --project <dir>` 启动编辑器。

先按对象搜索（"tilemap cell"、"animation track"、"import sidecar"），结果太
宽时再加动词。下结论说工具不存在前，用同义词重试：tile/cell、sprite/texture、
log/output、delete/remove/erase、create/add/new。精确工具集以发现结果为准；
不要凭记忆断言数量或名字。

原生 MCP 工具可用时协议相同：`search_tools`、`get_tool_detail`、`call_tool`，
外加元工具 `batch_execute` 与 `code_execute`。先发现，再调用。

## 插件暴露的能力

MCP 层有七个元工具：`ping`、`search_tools`、`list_categories`、
`get_tool_detail`、`call_tool`、`batch_execute`、`code_execute`。所有领域工具
都通过 `call_tool` 传名称与参数对象执行；CLI 把这条路径包装为 `gda call`。

- `batch_execute` 顺序执行现有工具调用列表（最多 256 条），支持
  `stop_on_error` 与可选回滚。适合“设置五个属性再保存”这类确定性序列。
- `code_execute` 在编辑器里运行 GDScript，适合循环、数学与计算值。超时在
  脚本运行期间强制执行，上限以 schema 报告为准；长任务应拆分。
- `side_effect` 标记风险：`none`、`writes_file`、`writes_config`、
  `shows_alert`、`modifies_window`、`writes_scene`、`process`、
  `code_execute`、`game_runtime`、`destructive`。高风险类需要显式确认。

## 查引擎文档

不要凭记忆调用引擎 API。属性名、方法签名、枚举值与默认值会随 Godot 版本变化，
回忆出的签名是静默失败的常见来源。写或改调用陌生 API 的脚本、不确定属性名或
可选值、或追查 “unknown property” / 解析错误时，先用文档族
（`find_docs_class`、`get_docs_class`、`get_docs_method`、`get_docs_property`）。
文档缓存随运行中的编辑器提供，与当前引擎构建一致。

## 任务路由

先把任务归约到对象与意图，再发现对应工具族。下表给出代表性工具；当前名称与
schema 一律用 `gda search` 与 `gda describe` 确认。

### 读取与检查

| 任务 | 代表性工具 |
|---|---|
| 当前编辑场景树 | `get_scene_tree`、`get_editor_edited_scene_root` |
| 节点属性 | `property_get`、`property_get_list` |
| 编辑器选择 | `get_editor_selection` |
| 运行中游戏的场景树与 UI | `get_debugger_scene_tree`、`get_game_ui_elements` |
| 编辑器 UI 元素 | `get_editor_ui_elements`、`hit_test_editor_point` |
| 磁盘资源 | `get_resource_dir_files`、`get_resource_type`、`has_resource`、`get_resource_dependencies` |
| 工程文件系统 | `get_editor_file_system_tree`、`get_editor_file_system_status` |
| 工程与编辑器设置 | `get_project_settings`、`has_project_settings`、`get_editor_settings` |
| API 反射 | `get_docs_class`、`find_docs_class`、`get_docs_method`、`get_docs_property` |
| 视觉状态 | `capture_editor_viewport`、`capture_game_viewport` |

### 构建与修改

| 任务 | 代表性工具 |
|---|---|
| 节点 | `create_scene_node`、`rename_scene_node`、`reparent_node`、`delete_scene_node`、`instantiate_scene`、`add_group_node` |
| 属性 | `property_set`（单条与批量形式） |
| 信号 | `signal_connect`、`signal_disconnect`、`trace_signal_flow` |
| 资源 | `create_resource`、`load_resource`、`save_resource`、`duplicate_resource`、`set_resource_property` |
| 文件 | `rename_resource_file`、`move_resource_file`、`write_file`、`read_file`、`find_in_files` |
| 脚本 | `create_script`、`attach_script_to_node`、`call_script_node`、`reload_script` |
| 主题与布局 | `create_theme_resource`、`set_theme_color`、`apply_theme_to_control`、`set_control_anchor_preset` |
| 瓦片 | `create_tilemap`、`create_tilemap_tileset`、`set_tilemap_cell`、`set_tilemap_cells` |
| 动画 | `create_animation`、`create_animation_track`、`insert_animation_keyframe`、`create_scene_animation_tree` |
| 编辑器 UI 自动化 | `click_editor_element`、`type_editor_element_text`、`run_editor_shortcut`；坐标回退 `click_input_mouse`、`type_input_text` |

### 运行与调试

| 任务 | 代表性工具 |
|---|---|
| 启动与停止 | `play_editor_current_scene`、`stop_editor_playing` |
| 游戏通道状态 | `get_game_status` |
| 在游戏中执行代码 | `execute_game_script`、`reload_game_scripts` |
| 向游戏注入输入 | `queue_game_input`、`wait_game_input`、`sequence_game_inputs`、`get_game_input_status` |
| 游戏截图与 UI | `capture_game_viewport`、`get_game_ui_elements` |
| 日志 | `get_debugger_log`、`get_debugger_errors`、`get_debugger_output`、`get_game_log_entries`、`get_plugin_log` |
| 性能 | `get_debug_monitor_catalog`、`get_debug_monitors`、`get_debug_memory_usage`、`get_debug_object_count` |
| 暂停与重载 | `set_scene_tree_pause`、`reload_scene_tree_current_scene` |
| 内联测试 | `run_gdscript_tests`、`run_gdscript_test_files` |
| 工程分析 | `validate_scene_file`、`find_unused_resources` |

### 工程管理

| 任务 | 代表性工具 |
|---|---|
| 工程设置 | `get_project_settings`、`set_project_settings`、`save_project_settings`；set 只在内存中，直到 save |
| 编辑器设置 | `get_editor_settings`、`set_editor_settings` |
| InputMap | `get_input_map_actions`、`add_input_map_action`、`add_input_map_action_event`、`save_input_map` |
| 主场景 | `set_editor_main_scene` |
| 场景文件 | `create_editor_scene`、`open_editor_scene`、`save_editor_scene`、`save_editor_scene_as`、`close_editor_scene` |
| 文件系统刷新 | `scan_editor_file_system` |

## 确认与安全

高风险副作用不会直接执行，而是返回结构化的 `confirmation_required` 错误。
需要确认的类是 `destructive`、`process`、`code_execute`、`game_runtime`。
核对工具说明与精确目标后重试：

```bash
gda call code_execute --args '{"source_code": "return 1"}' --confirm
```

使用原生 MCP 工具时，在参数对象里传 `confirm: true`。文件与场景写入
（`writes_file`、`writes_config`、`writes_scene`）不需要确认，但仍会修改工程
内容：每次写入后读回目标。不要为了绕过确认错误而换一个效果相同的工具。

MCP 客户端能看到工具 annotations 与 `side_effect` 标记。把 `destructive` 与
`game_runtime` 注解当作“停下来核对”的信号，而不是形式。

## 观察引擎状态

不要连续盲写。每次变更都落在带导入队列、调试器与引擎日志的实时编辑器上；
没有观察到的状态就是不知道的状态。

- 每次调用后读 `new_errors_since_last_call` 水位。非 0 表示记录了新错误；
  取出真实日志行，修复或解释后再继续。计数器只读一次，读后归零。
- 游戏运行时，`get_debugger_errors` 与 `get_debugger_output` 携带其错误与
  打印；没有运行中的游戏时，`get_debugger_log` 读编辑器输出与引擎日志，
  `get_game_log_entries` 在会话结束后读磁盘日志。
- 问题出在插件本身（确认被拒、超时、响应丢失）时，读 `get_plugin_log`。
- 优先相信观察到的输出——日志行、截图、读回值——而不是假设调用名等于效果。
  `ok` 只表示调用送达。

## 变更-观察-验证循环

路由表覆盖单次调用；真实工作是循环：

1. 通过工具修改工程，或用 `batch_execute` / `code_execute` 编排确定性序列。
2. 用截图（`capture_editor_viewport`、`capture_game_viewport`）、读回
   （`property_get`、`get_scene_tree`、`get_resource_property`）或输出
   （`get_debugger_log`，游戏运行时用 `get_debugger_errors` /
   `get_debugger_output`）观察。
3. 根据观察诊断、修复，再次观察，直到结果符合意图。
4. 用水位为 0 确认修复，再运行相关验证工具（`validate_scene_file`、内联
   GDScript 测试或游戏探测）后才宣布成功。

## 协议细节位置

- CLI 命令与参数参考、退出码、输出契约：`godot-autopilot-tools`。
- 发现、`batch_execute` 与 `code_execute` 语义：`godot-autopilot-tools`。
- confirm 协议、MCP annotations 与错误码：`godot-autopilot-tools`。
- 超时、响应大小、并发与批处理限制：`godot-autopilot-tools`。
- 引擎特定行为：下面七个领域册。

## 参见

- `godot-autopilot-setup` - 安装、启动、连接、生成 MCP 配置。
- `godot-autopilot-tools` - CLI-first 调用协议、confirm、错误、限制。
- `godot-autopilot-scene-system` - 场景、节点、属性、信号、撤销。
- `godot-autopilot-resources` - 资源、导入、重命名与移动。
- `godot-autopilot-scripting` - GDScript 生命周期与执行通道。
- `godot-autopilot-runtime` - 运行游戏、注入输入、调试。
- `godot-autopilot-servers` - RenderingServer、PhysicsServer、NavigationServer。
- `godot-autopilot-content` - 瓦片、动画、音频、UI 主题。
- `godot-autopilot-csharp` - C# 构建循环、导出、反射桥。
