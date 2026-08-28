---
type: 模块文档
title: 入口与运行时桥接
description: GDExtension 入口与插件生命周期、GDA 协议常量、游戏运行时桥接三文件职责
tags:
  - 模块
  - 入口
  - 运行时桥接
timestamp: "2026-08-28"
resource:
  - src/main.cpp
  - src/runtime/
---

# 模块：入口与运行时桥接（entry_runtime）

覆盖代码：`src/main.cpp`（321 行）与 `src/runtime/`（`gda_protocol.hpp` 53 行、`game_bridge.hpp` 47 行、`game_bridge.cpp` 656 行、`game_bridge_input.cpp` 875 行、`game_bridge_eval.cpp` 427 行）。

职责全景：`main.cpp` 是 GDExtension 的导出入口与编辑器插件本体；`src/runtime/` 是在**游戏运行时进程**内与编辑器进程通信的桥接层，通过 EngineDebugger 消息通道承载 GDA 协议。编辑器内的 MCP 服务器（`ServerContext`）与运行时桥接是两条相互独立的消息通路，本页只覆盖入口生命周期与运行时桥接，MCP 工具侧见相关模块页。

## 1. 入口点与插件生命周期（main.cpp）

### 1.1 导出函数 GDExtensionEntryPoint

- 以 `extern "C"` 导出，签名 `GDExtensionEntryPoint(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization*)`。
- 修饰宏 `GDA_EXPORT`：Windows（`_WIN32`）为 `__declspec(dllexport)`，其余平台为空。
- 函数体：构造 `godot::GDExtensionBinding::InitObject`，注册 initializer 与 terminator 回调，最后返回 `init.init()`。
- 两个回调内部均包 try/catch，异常只记录日志不中断；`_enter_tree` 内另有四组步骤级 try/catch（log dock / output logger / debugger plugin / config dock），catch 分支统一委托 `log_setup_failure` 助手（`main.cpp:30-40`）写 System 错误日志。

### 1.2 模块初始化（register_initializer）

按模块级别分派：

- **SCENE 级别**：仅当 `!Engine::is_editor_hint()`（即运行游戏的非编辑器进程）时调用 `runtime::game_bridge::register_listener()`。编辑器进程在此级别不注册桥接监听。
- **EDITOR 级别**：先 `debugger_ops::register_classes()`，然后依次 `ClassDB::register_class` 注册 4 个类：`McpLogDock`、`McpConfigDock`、`ExportGuard`、`GodotAutopilotPlugin`，最后 `EditorPlugins::add_by_type<GodotAutopilotPlugin>()` 把插件类型注册进编辑器。

### 1.3 模块终止（register_terminator）

- **EDITOR 级别**：仅输出"Editor level terminated"日志。
- **SCENE 级别**：**无条件**调用 `runtime::game_bridge::unregister_listener()`（与注册端点的 `!is_editor_hint()` 条件不对称，见第 4 节不一致点）。

### 1.4 GodotAutopilotPlugin（EditorPlugin 子类）

`GDCLASS(GodotAutopilotPlugin, godot::EditorPlugin)`，`_bind_methods()` 为空。成员：

- 静态 `CommandQueue s_queue`，经 `queue()` 静态访问器暴露；`_process` 中 `s_queue.drain()` 即 AGENTS.md 所述"主线程排空"的实现点。
- UI 成员：`McpLogDock *log_dock`、`McpConfigDock *config_dock`；`Ref<OutputCaptureLogger>`、`Ref<DebugCapturePlugin>`、`Ref<ExportGuard>`。
- 覆写方法：`_enter_tree` / `_exit_tree` / `_process` / `_get_unsaved_status`。

`_enter_tree()` 顺序（每个步骤独立 try/catch，失败不中断后续）：

1. 日志"plugin starting"，`runtime_ops::set_editor_queue(&queue())` 注入队列，`ModeDetector` 判定 Editor/Runtime 模式写日志。
2. `gda_cmdline_mode()` 为真则直接 return——不建 UI、不启服务器（队列已在步骤 1 注入）。
3. 创建 `McpLogDock`（标题 "GDA Log"）`add_dock`。
4. `debugger_ops::create_output_logger()` 经 `OS::add_logger` 注册；`debugger_ops::create_debug_plugin()` 经 `add_debugger_plugin` 注册。
5. `new (std::nothrow) ServerContext(queue())` 并 `start()`；成功则记 Transport 日志（含端口），失败记 `last_error()`。
6. 创建 `McpConfigDock` 并 `set_server_context`（面板内显示运行端口/离线状态）`add_dock`。
7. `export_guard_.instantiate()` + `add_export_plugin`，日志 "Plugin ready"。

`_exit_tree()` 逆序清理：停并删 `g_server_ctx` → 移除 ExportGuard → 移除 debugger plugin → `OS::remove_logger` → 移除并 memdelete 停靠面板，整体包 try/catch。

`_process()`：`s_queue.drain()` + `log_dock->poll_new_entries()`。

`_get_unsaved_status()`：`scene_dirty_tracker::is_current_scene_dirty()` 为真时返回场景名，否则返回空串。

### 1.5 cmdline 模式检测（gda_cmdline_mode，main.cpp:46）

实际函数名是 `gda_cmdline_mode()`（**不存在** `gsd_cmdline_mode`）。语义：

- 环境变量 `GDA_FORCE_HEADLESS` 值等于 `"1"` → 直接返回 `false`，即**强制禁用** cmdline 模式（此时即使无头编辑器也照常建 UI/启服务器，变量名与直觉相反）。
- 否则：需要 `Engine::is_editor_hint()` 为真且 `DisplayServer` 存在，且 `!ds->window_can_draw()`（窗口无法绘制）才返回 `true`。

## 2. GDA 协议常量（gda_protocol.hpp）

协议载体：EngineDebugger 消息通道（`register_message_capture("gda", ...)` / `send_message`）。消息正文为 JSON 字符串。消息方向：`gda:request` 由工具侧（`src/tools/debugger_access.cpp`）发向运行时，`gda:response` / `gda:ready` 由运行时（`game_bridge.cpp`）发出；工具侧接收方在 `src/tools/debugger_ops.cpp`。

### 2.1 消息类型（GDA_MSG_*）

| 常量 | 值 |
|---|---|
| `GDA_PREFIX` | `gda` |
| `GDA_MSG_READY` | `gda:ready` |
| `GDA_MSG_RESPONSE` | `gda:response` |
| `GDA_MSG_REQUEST` | `gda:request` |

### 2.2 操作（GDA_OP_*，on_gda_message 分发）

| 常量 | 值 | 处理函数（game_bridge*.cpp） |
|---|---|---|
| `GDA_OP_STATUS` | `status` | `op_status` |
| `GDA_OP_PING` | `ping` | `op_ping` |
| `GDA_OP_CANCEL` | `cancel` | `op_cancel` |
| `GDA_OP_EVAL` | `eval` | `op_eval`（eval 文件） |
| `GDA_OP_INPUT` | `input` | `op_input`（input 文件） |
| `GDA_OP_INPUT_WAIT` | `input_wait` | `op_input_wait`（input 文件） |
| `GDA_OP_INPUT_STATUS` | `input_status` | `op_input_status`（input 文件） |
| `GDA_OP_INPUT_SEQUENCE` | `input_sequence` | `op_input_sequence`（input 文件，08-24 新增） |
| `GDA_OP_UI_ELEMENTS` | `ui_elements` | `op_ui_elements`（bridge 主文件，08-24 新增） |
| `GDA_OP_CAPTURE` | `capture` | `op_capture` |
| `GDA_OP_GET_ERRORS` | `get_errors` | `op_get_errors` |
| `GDA_OP_GET_OUTPUT` | `get_output` | `op_get_output` |
| `GDA_OP_GET_TREE` | `get_tree` | `op_get_tree` |

未知 op 返回 `{"error": "unknown op: <op>"}`。

### 2.3 请求/响应字段（GDA_FIELD_*）

| 常量 | 值 | 用途 |
|---|---|---|
| `GDA_FIELD_REQUEST_ID` | `request_id` | 请求/响应对齐 ID |
| `GDA_FIELD_OP` | `op` | 操作名 |
| `GDA_FIELD_PARAMS` | `params` | 操作参数对象 |
| `GDA_FIELD_OK` | `ok` | 响应成功标志（`!body.Contains("error")`） |
| `GDA_FIELD_ERROR` | `error` | 错误消息 |
| `GDA_FIELD_RESULT` | `result` | 成功结果 |
| `GDA_FIELD_CANCELLED` | `cancelled` | cancel 是否命中 |
| `GDA_FIELD_PARSED_PHYSICS_FRAME` | `parsed_physics_frame` | input 注入时的物理帧 |
| `GDA_FIELD_PARSED_PROCESS_FRAME` | `parsed_process_frame` | input 注入时的处理帧 |
| `GDA_FIELD_MATCHED_AT_PHYSICS_FRAME` | `matched_at_physics_frame` | input_wait 命中时的物理帧 |
| `GDA_FIELD_LAST_ACTIVITY_MS` | `last_activity_ms` | 最近一次 GDA 消息时间（ticks_msec） |
| `GDA_FIELD_READY` | `ready` | ready 消息标志 |
| `GDA_FIELD_HEALTHY` | `healthy` | 距上次活动是否在阈值内 |

### 2.4 数值与环境常量

| 常量 | 值 | 使用方 |
|---|---|---|
| `GDA_NEW_SCENE_SWITCH_WAIT_MS` | `2000` | `src/tools/editor_ops.cpp`（换场景等待上限） |
| `GDA_NEW_SCENE_POLL_MS` | `50` | `src/tools/editor_ops.cpp`（轮询步长） |
| `GDA_AUTO_CONTINUE_ENV` | `GDA_AUTO_CONTINUE` | `src/tools/runtime_ops.cpp`（自动继续次数环境变量） |
| `GDA_AUTO_CONTINUE_MAX` | `3` | `src/tools/runtime_ops.cpp`（默认上限） |

桥接相关常量在 `src/core/config.hpp`：`GDA_HEALTHY_ACTIVITY_THRESHOLD_MS=3000`、`GDA_ERROR_BUFFER_MAX=200`、`GDA_OUTPUT_BUFFER_MAX=500`、`GDA_EVAL_TRUNCATE_BYTES=8192`。

## 3. 运行时桥接三文件职责

### 3.1 game_bridge.cpp — 消息通道与缓冲

- `register_listener()`（幂等，`g_registered` 守卫）：首次调用时注册 7 个类（`GameBridgeListener`、`GameBridgeLogger`，及 eval 组 `GameBridgeEvalAwaiter`、input 组 Watcher/DelayedRelease/Sequence/FrameSequence 共 4 个桥接类；注册顺序 Listener→eval→input→Logger）；实例化监听器与日志器；`EngineDebugger::register_message_capture("gda", on_gda_message)` 捕获前缀 `gda` 的消息；`OS::add_logger` 挂游戏日志器；随后立即发出 `gda:ready`（body 含 `ready:true` 与活动字段）。
- `unregister_listener()`：反注册消息捕获与日志器，unref 两个实例。
- `GameBridgeListener`（RefCounted 子类）：绑定方法 `on_gda_message(p_message: String, p_data: Array) -> bool`。解析 `data[0]` 为 JSON 请求，取 `request_id`/`op`/`params` 分发；每收到一条消息即刷新 `g_last_activity_ms`；响应统一附加 `ok` 字段后经 `send_response` 发出。请求畸形或 op 执行失败会同时记入错误与输出缓冲。
- `send_response(request_id, body)`：补 `request_id` 字段（`GDA_FIELD_REQUEST_ID` 常量），`EngineDebugger::send_message("gda:response", [json])`。
- `GameBridgeLogger`（godot::Logger 子类）：`_log_error` 按错误类型（warning 判定码 3）经 `push_game_error` 入错误缓冲；`_log_message` 错误入错误缓冲、普通消息入输出缓冲。
- 缓冲：`g_error_buffer`（环形，上限 200，每条带递增 `seq`，`g_error_seq` 单调）+ `g_output_buffer`（上限 500），互斥锁保护；`current_error_seq()` / `eval_error_delta(since_seq)` / `append_eval_runtime_errors` 供 eval 附加运行期错误增量（文本增量 + 仅首条的 `structured_error`）；`truncate_error_text` 超 8192 字节截断。
- 状态与工具 op：`status`（版本/fps/physics_frame/paused/node_count/scene、活动字段）、`ping`（physics/process 帧 + 活动字段）、`cancel`（查 `g_cancel_handlers` 并调用 handler）、`capture`（根视口渲染存 PNG 到缓存目录，返回 path/width/height）、`get_errors`（limit 默认 50）、`get_output`（limit 默认 200）、`get_tree`（DFS，深度上限 64、节点上限 2000）、`ui_elements`（08-24 新增，见下）。
- 辅助函数：`gda_string`（string_view → godot::String）、`error_result`/`ok_result`、`get_scene_tree`、`resolve_node`（空路径取当前场景，先场景内再根节点查询）。

### 3.2 game_bridge_input.cpp — 输入模拟

- 取消机制实现在此文件：`g_cancel_handlers`（request_id → handler 映射）、`register_cancel_handler` / `unregister_cancel_handler`。
- 注入方式：构造 `InputEventKey` / `InputEventMouseButton` / `InputEventAction` 经 `Input::parse_input_event` 注入并 `flush_buffered_events()`；`mode="api"` 时改走 `Input::action_press/action_release`；`mode="hold"` 时 `keep_pressed` 每帧重发 press 直到释放。
- `GameBridgeInputWatcher`（Node，PROCESS_MODE_ALWAYS + physics process）：轮询动作的 `just_pressed`/`just_released`/`pressed` 三态，命中即回 `result: "matched"` + `matched_at_physics_frame`，超时（默认 2000ms，上限 30000ms）回错误；可被 cancel。
- `GameBridgeDelayedRelease`（Node）：按键/鼠标/动作按下后按物理帧计数延时自动发送 release 事件（支持 `duration_ms` 与 hold）。
- `GameBridgeInputSequence`（Node）：步骤数组按 `duration_ms` 间隔经 `SceneTreeTimer` 逐条执行 `inject_step`。
- `parse_keycode`：接受整数码、纯数字字符串、单字符（小写转大写）、`KEY_` 前缀名或 57 项名称映射表。
- `op_input`：单步或 `sequence` 数组；返回 `injected_at_physics_frame` / `expected_visible_frame` / `parsed_physics_frame` / `parsed_process_frame`，游戏暂停时附 `warning`。注意：单步成功时返回**不带** `result` 包装的裸对象，而 `error_result` 与 sequence 走 `ok_result` 包装。
- `op_input_wait`：校验 state 三态、可选 `inject` 子对象先注入再等待；挂 watcher 并注册取消 handler，**同步返回空 JSON**（响应由 watcher 在物理帧回调中异步发出）。
- `op_input_status`：动作三态 + `physics_frame` + `paused`。
- **`op_input_sequence`（08-24 新增，逐物理帧时间线）**：`inputs[]` 每项必须为对象且含非负整数 `at_frame`（相对首帧的物理帧偏移）与注入字段（复用 `inject_step` 的 type/keycode/action 等），上限 256 项；超时默认按 `max_at_frame × 33ms + 2000ms` 推导、可被 `timeout_ms` 覆盖并钳制 `GDA_MAX_TIMEOUT_MS=30000`。校验通过后同步返回空 JSON，实际注入与响应由 `GameBridgeFrameSequence` 节点异步完成。
- **`GameBridgeFrameSequence`（Node，PROCESS_MODE_ALWAYS + physics process，08-24 新增）**：`setup()` 记录起始物理帧（`Engine::get_physics_frames()`）与 ticks 后挂到场景根；每物理帧把 `at_frame <= 相对帧数` 的条目经 `inject_step` 注入（失败写入游戏错误缓冲并继续），队列清空即 `finish(true)`；超时未完成则 `finish(false)`。响应体 `{completed, executed}` 经 `ok_result` 包装异步发出，注册 cancel handler 支持中途取消。
- **`op_ui_elements`（08-24 新增，运行中 Control 树枚举）**：从场景根 DFS 遍历（深度上限同 get_tree 的 64），命中 `Control` 节点即收集 `{path, type, visible, text?, global_rect:{position:{x,y}, size:{x,y}}}`；`max_elements` 默认 100、上限 1000，达到上限标 `truncated:true`。响应为 `ok_result({elements, count, truncated})` 包装。MCP 工具侧对应 `get_game_ui_elements`。

### 3.3 game_bridge_eval.cpp — 异步求值

- `op_eval` 分派 4 种 action：`script` / `get_property` / `set_property` / `call_method`。
- `op_eval_script`：`source_code` → 实例化 `GDScript` 并 `reload()`（编译失败回错误 + 编译期错误增量文本）；构造临时 Node 挂脚本，`persist` 时挂载到 `/root/__gda_runtime/<persist_name>`（缺省 `eval_<request_id>`，重名报错）；要求脚本含 `_run()` 方法；调用 `_run()` 后若返回 `GDScriptFunctionState`（await）则转 `GameBridgeEvalAwaiter` 异步等待，否则同步返回序列化结果（并附运行期错误增量、persist 时补 `node_path`）。
- `GameBridgeEvalAwaiter`（Node）：连接 state 的 `completed` 信号或超时（默认 5000ms）后响应；完成路径与超时路径都会 `send_response` 并清理（取消 handler、断信号、非 persist 时删除临时节点、queue_free）。
- `get_property` / `call_method`：经 `resolve_node` 定位节点；call_method 校验 `has_method` 并把 args 按 JSON 反序列化后 `callv`。
- `set_property`：查 `get_property_list` 取类型与 hint 构造 `type_hint`，`VariantJson::deserialize(value, type_hint)` 后 set，并用 `util::check_readback` 读回校验（REJECTED 报错、CONVERTED 附 warning）。

## 4. 与现有文档对照

- AGENTS.md "入口点：src/main.cpp → GDExtensionEntryPoint → 注册 GodotAutopilotPlugin 并启动 ServerContext"：方向正确，但**严格说 ServerContext 不是入口点启动的**——`GDExtensionEntryPoint` 只做类注册与 `add_by_type`，插件实例由 Godot 创建后在其 `_enter_tree()` 中才 `new ServerContext` 并 `start()`（main.cpp:161-175）。实际链为：入口点 → 注册插件类型 → Godot 实例化 → `_enter_tree` → ServerContext 启动。
- AGENTS.md "在 MODULE_INITIALIZATION_LEVEL_EDITOR 阶段加载到 Godot 编辑器"：编辑器侧类确在 EDITOR 级别注册；但运行时桥接 `register_listener` 注册在 **SCENE 级别**且仅非编辑器进程，属补充事实而非矛盾。
- AGENTS.md 线程模型 "HTTP 线程 → CommandQueue::submit() → 主线程 _process() 排空"：与 `_process()` 中 `s_queue.drain()` 一致。
- AGENTS.md 错误模式 `{"error": "..."}`：桥接层 `error_result` 同构；差异是桥接响应额外带布尔 `ok` 字段。
- Example/docs/architecture.md 的 HTTP→SDK 线程→CommandQueue→主线程流程图与本文件 `_process` 实现一致；该文档的 `game_*` 工具（get_game_status/queue_game_input/capture_game_viewport 等）正是本页桥接 op 在 MCP 工具侧的封装。

不一致点汇总：

1. `gsd_cmdline_mode` 不存在，实际函数名 `gda_cmdline_mode()`（main.cpp:46）。
2. `GDA_FORCE_HEADLESS=1` 的语义与变量名相反：它是**禁用** cmdline 模式（强制 `false`），而非强制无头。
3. terminator 的 SCENE 级别**无条件** `unregister_listener()`，与 initializer 的 `!is_editor_hint()` 条件注册不对称。
4. `GDA_MSG_REQUEST` 不被桥接侧使用（由工具侧 `debugger_access.cpp` 发出）。
5. 请求消息虽从 `GDA_PREFIX` 取名，但桥接捕获只按 `"gda"` 前缀匹配，`GDA_MSG_*` 常量仅用于发送方向。

## 出站链接

- 详见 [../modules/core.md](../modules/core.md)（命令队列与线程模型）
- 详见 [../modules/tools_ops_b.md](../modules/tools_ops_b.md)（运行时/调试器工具的 MCP 封装）
