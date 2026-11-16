# 调用协议与文档 CLI

本篇是跨域调用知识：通道、CLI 面、引擎文档、confirm、错误、限制与工具面。领域行为见其它参考。

## 通道

- **GDA CLI。** 有 shell 时优先用 `gda`。它自动发现插件端口，优先 IPC，回退 HTTP，输出 JSON。
- **原生 MCP 工具。** harness 已列出 GDA 工具时直接用。语义与 CLI 相同，只是传输不同。
- **手动编辑器步骤。** 两者都不可用时，给用户一段简短、可核对的步骤，并要求一次只回报一个观测。禁止编造工具输出。

## CLI 主流程

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"node_path": "Level", "cells": [{"x": 0, "y": 0, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}]}'
gda tools list
```

- `gda search <query>` 按名称、描述、分类与标签搜索 live 服务；`--category` 与 `--tags` 收窄范围，默认最多 20 条。
- `gda describe <name>` 打印 schema 与 `side_effect` 标记；首次写入前必读，禁止推断参数名。
- `gda call <name> --args '<json>'` 或 `--args-file <path>` 执行一次调用。
- `gda tools list` 列出全部可直接调用的工具。发现只针对 live 实例；没有实例时用 `gda start --project <dir>` 启动编辑器。

目标解析顺序：显式 `--socket` 或 `--port`，再 `--id`，再匹配 `--project` 或当前目录的 live 实例。端口由插件拥有；`--port` 只是连接覆盖。`gda status --json` 报告解析后的 `mcp_url` 与通道。解析失败时，`gda list --all` 列出已知项目。

退出码：

| 码 | 含义 |
|---|---|
| 0 | 成功 |
| 1 | 工具运行并返回失败 envelope |
| 2 | 传输或环境失败：无实例、端点不可达、超时 |
| 3 | 用法错误：未知命令、flag 或参数格式错误 |

stdout 是 JSON，诊断进 stderr。退出 2 表示通道不可用；禁止在此后编造结果。

## 工具面与弃用别名

- live catalog 有 252 个 canonical 工具；`gda tools list` 与 `search_tools` 只暴露这些。`get_tool_detail` 也能解析别名，但返回 canonical 工具详情并附替换字段。
- 另有 325 个弃用别名，经 `call_tool`、`batch_execute` 与 CLI 的 `gda call` 解析到 canonical 工具。别名不进搜索索引，也不进 catalog，因此按名称搜索每个能力只返回一个条目。
- 每个别名的响应顶层带替换信息：deprecated_alias、canonical_name、deprecated_since、alias_removal_version。这些字段只作提示；分支必须基于 canonical 工具，而不是别名。
- 别名自 0.4.x 起标记弃用，将在 0.6.x 移除。迁移就是改用下面的 canonical 工具加 action，不要等到移除。`gda tools list` 找不到某工具时，按对象搜索并调用 canonical 名称。
- 别名仍走确认门：高风险别名同样返回 `confirmation_required`，不能用来绕过确认。

常见替换：

```bash
gda describe resource_query
gda call resource_query --args '{"action": "references", "path": "res://assets/icon.png"}'
gda call get_game_state --args '{"action": "status"}'
```

| 弃用别名 | Canonical 调用 |
|---|---|
| `get_game_status` | `get_game_state` action=status |
| `capture_game_viewport` | `capture_editor_viewport` target=game |
| `get_debugger_log` / `get_debugger_errors` / `get_debugger_output` | `get_debugger_stream` action=log / errors / output |
| `validate_scene_file` | `analyze_project` action=validate_scene |
| `verify_scene_saved` | `get_scene_tree` verify=true |
| `load_resource` / `reload_resource` | `resource_load` action=load / reload |
| `save_resource` | `resource_save` action=save |
| `create_resource` / `duplicate_resource` | `resource_create` action=create / duplicate |
| `get_resource_type` / `get_resource_dependencies` / `get_resource_references` / `get_resource_uid` | `resource_query` action=type / deps / references / uid |
| `get_resource_property` / `set_resource_property` | `resource_property` action=get / set |
| `rename_resource_file` / `move_resource_file` / `remove_resource_file` / `copy_resource_file` | `resource_file` action=rename / move / remove / copy |
| `write_file` / `read_file` / `find_in_files` | `os_file_write` action=write / `os_file_text` action=read / find |
| `create_editor_scene` / `open_editor_scene` / `close_editor_scene` / `reload_editor_scene` / `play_editor_current_scene` / `stop_editor_playing` | `editor_scene_control` action=create / open / close / reload / play / stop |
| `save_editor_scene_as` / `save_editor_scenes` | `save_editor_scene` action=save_as / save_all |
| `get_editor_selection` / `get_editor_edited_scene_root` | `get_editor_state` action=selection / root |
| `set_editor_selection` | `editor_selection_set` action=set |
| `scan_editor_file_system` / `get_editor_file_system_status` | `get_editor_files` action=scan / status |
| `instantiate_scene` / `build_nodes_from_spec` | `create_scene_node` mode=scene / spec |
| `rename_scene_node` / `reparent_node` | `edit_scene_node` action=rename / reparent |
| `add_group_node` / `remove_group_node` / `has_group_node` | `node_group` action=add / remove，或 `get_scene_tree` include_groups=true |
| `set_tilemap_cell` | `set_tilemap_cells` 的 `cells` 条目 |
| `add_tilemap_atlas_source` / `add_tilemap_physics_layer` / `set_tilemap_tile_collision` | `update_tileset` action=add_atlas_source / add_physics_layer / set_tile_collision |
| `create_spriteframes` / `add_spriteframes_animation` / `add_spriteframes_frame` | `update_spriteframes` action=create / add_animation / add_frame |
| `create_scene_animation_tree` / `add_animation_machine_state` / `connect_animation_states` | `animation_tree` action=create / add_node / connect |
| `get_audio_bus_layout` / `set_audio_bus_volume_db` / `add_audio_bus_effect` | `audio_bus` action=get_layout / set_volume / add_effect |
| `play_audio_player` / `set_audio_player_volume_db` / `seek_audio_player` | `audio_player` action=play / set_volume / seek |
| `set_audio_device_output` / `set_audio_device_input` | `audio_device` action=set_output / set_input |
| `create_theme_resource` / `set_theme_item_bundle` / `set_theme_color` / `set_theme_stylebox_flat` | `update_theme` action=create / bundle |
| `set_control_anchor_preset` / `apply_theme_to_control` | `update_control_layout` action=layout / apply |
| `queue_game_input` / `wait_game_input` / `sequence_game_inputs` | `game_input` action=queue / wait / sequence |
| `execute_game_script` | `game_script` action=execute，并传 `script_action` |
| `set_scene_tree_pause` / `is_scene_tree_paused` / `reload_scene_tree_current_scene` | `game_scene_tree` action=set_pause / is_paused / reload_current_scene |
| `get_nav_2d_map_path` / `get_nav_3d_map_path` | `nav_query` mode=path_query，dimension=2 / 3 |
| `get_nav_3d_map_closest_point_to_segment` | `nav_query` mode=closest_point_to_segment，dimension=3 |
| `intersect_physics_2d_ray` / `intersect_physics_3d_ray` | `physics_query` mode=intersect_ray，dimension=2 / 3 |
| `create_render_camera` / `create_render_light` / `create_render_mesh` / `create_render_canvas_item` | `render_camera` / `render_light` / `render_mesh` / `render_canvas_item` action=create |
| `csharp_list_members` / `csharp_get_property` | `csharp_member` action=list / get |

完整规则对每个别名都相同：调用 canonical 工具，并按上表传 action、mode 或参数。

## 引擎文档 CLI

`gda docs` 是离线引擎参考：读取本机引擎 dump，按引擎 minor 缓存，并检索类、成员与全局符号。

```bash
gda docs search "tween"
gda docs class Node2D --member position
gda docs member Node2D look_at
gda docs sync
```

- `status` 只读：项目 pin、解析到的引擎、缓存路径、新鲜度。
- `search` 默认 20 条（`--limit` 上限 200），支持 `--kind`（`class`、`method`、`property`、`signal`、`enum`、`constant`、`utility`）与 `--class`。`class` 默认返回简要、描述与计数；用 `--section`、`--member` 或 `--full` 展开。`member` 自动判定类型，歧义名返回全部匹配。
- `sync` 总是导出。缓存缺失或解析版本不一致时，下一次查询自动导出；导出失败是错误，绝不回退到其它版本。
- `--docs-file <path>` 读取外部 `extension_api.json` 并绕过缓存。项目 pin 与可用二进制不一致时，传入匹配的 `--godot <path>`；命令拒绝混用 minor。
- `clear --engine <x.y>` 删除一个桶；`--all` 需要 `--force`。

## 确认

高风险副作用在执行前返回结构化 `confirmation_required`。审查描述与目标后，用 `--confirm`（CLI）或 `confirm: true`（MCP）重试。类别为 `destructive`（删除已存在对象）、`process`（控制 OS 进程）、`code_execute`（运行任意 GDScript）与 `game_runtime`（进入运行中的游戏）。

文件、配置与场景写入不需要确认，但仍改变内容：每次写入后读回目标。禁止换一个效果相同的工具绕过确认，也禁止把高风险调用藏进 batch。弃用别名与 canonical 工具走同一确认门。

## batch 与 code 执行

- `batch_execute` 顺序执行 `operations` 列表（最多 256 条）。每项有 `tool` 与可选 `args`；`stop_on_error` 默认 true，`rollback_on_error` 可选。用于确定性序列；每项可用 canonical 工具或弃用别名。
- `code_execute` 在编辑器里运行 GDScript，用于循环、数学与计算值。没有行首 `func ` 时把源码包进一个入口函数；有 `func ` 时保留源码并用 `function_name`。`timeout_ms` 默认 5000，上限由 schema 决定。
- 判据：少量已有调用 → `batch_execute`；循环、数学、条件或超大 payload → `code_execute`。

## 错误与水位线

每次工具失败都是带 `error` 键的 JSON 对象。部分带稳定 `code`；按 code 分支，不按文字。每个 meta 工具响应还带 `new_errors_since_last_call`，统计自上次调用以来的新错误，包括本次调用产生的错误。它是一次性的：读取即重置。

| Code | 触发条件 | 重试？ |
|---|---|---|
| `exec_timeout` | `code_execute` 脚本超过 `timeout_ms` | 拆分工作或在 schema 上限内提高超时 |
| `eval_timeout` | 游戏侧 eval 超过宿主等待 | 不要立即重试；游戏可能仍在执行 |
| `main_thread_timeout` | 编辑器主线程超出预算 | 先读状态；副作用可能已落地 |
| `queue_unavailable` | 主线程队列满或正在关闭 | 稍后重试 |
| `confirmation_required` | 高风险工具未确认 | 审查目标后用 `--confirm` 重试 |

编辑器导入或扫描资源期间，软错误带 `retryable: true` 与 `retry_after_ms`。至少等待建议间隔再重试同一调用。项目导出期间所有工具调用都会被拒绝；等待导出结束，不要紧密循环重试。

用错误水位线确认修复：复现、诊断、修复、重跑失败调用，再做 meta 调用直到水位线归零。成功调用后为零，是修复未产生新错误的收口证据。

## 资源、提示词与任务

`gda resources read <uri>` 读取编辑器与调试器状态；`gda resources list` 是 URI 权威。`gda prompts list|get` 渲染已注册提示词。`gda call --async` 配合 `gda tasks list|get|wait|cancel|clean` 管理本地记录的任务。

## 限制

- 默认操作超时 5000 ms；schema 报告当前上限。游戏操作 `timeout_ms` 上限 25000 ms。
- JSON 响应上限 4 MiB，并暴露截断字段；信任完整性前先检查。`batch_execute` 最多 256 条操作，最多 8 个请求并发在途。

## 失败签名

| 症状 | 原因 | 首步动作 |
|---|---|---|
| 退出 2 且无 JSON | 无 live 实例或端点不可达 | `gda list --all`，再 `gda start --project .` |
| 退出 3 | 未知命令、flag 或 JSON 格式错误 | `gda --help`；校验 `--args` JSON |
| `confirmation_required` | 高风险调用未确认 | 审查目标后加 `--confirm` |
| `main_thread_timeout` | 编辑器主线程超出预算 | 先读状态再重试 |
| 熟悉的工具名找不到 | 该名是弃用别名，已移除或未列入清单 | 按对象搜索；调用 canonical 工具并传 action |
