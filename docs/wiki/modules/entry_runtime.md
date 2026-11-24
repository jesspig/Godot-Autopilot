---
type: 模块文档
title: 入口与运行时桥接
description: GDExtension 入口与插件生命周期、GDA 协议常量、游戏运行时桥接四文件职责
tags:
  - 模块
  - 入口
  - 运行时桥接
timestamp: "2026-11-23T17:28:29+08:00"
resource:
  - src/main.cpp
  - src/runtime/
---

> **0.3.2**：编辑器级只注册 `McpLogDock`；`McpConfigDock` 已删除。启动不再签发 API token、不再启动 REST `ApiServer`、不再写 `endpoints.api`。

# 模块：入口与运行时桥接（entry_runtime）

> 批次要点（09-18 详见 `changelog/2026-09-18-log.md` 19:30 节）：`call_method` 的 native 失败路径附加 `diagnosis` + `hint`（`game_bridge_eval.cpp:534` `eval_bind_suspected_cause` / `:553` `make_native_bind_diagnosis`），成功路径零改动；入口顺序与 lifecycle 事件见 §1.2–§1.4；就绪排队重放、主线程等待预算、游戏侧 eval 协作式截止时间与协议 op 见 §1.6、§2、§3。

覆盖代码：`src/main.cpp`（487 行 / 434 有效代码行；入口点 `:417-487`、收尾 flush `:411`、`gda_cmdline_mode` `:136`、队列 open `:197`、注入 editor queue `:239`）与 `src/runtime/`（`gda_protocol.hpp` 62 行 / 51 有效、`game_bridge.hpp` 99 行 / 59 有效、`game_bridge.cpp` 2547 行 / 2405 有效（09-16 增窗口坐标换算、09-29 增 SceneTree op 分派、11-22/23 增 tween op 与 JSON 容错解析）、`game_bridge_input.cpp` 1086 行 / 1001 有效（09-16 删本地键名表约 60 行、改调共用判定）、`game_bridge_eval.cpp` 1304 行 / 1225 有效（10-20 增 eval 宿主截止时间与 `eval_timeout`/`eval_cancelled`）、`game_bridge_verify.cpp` 855 行 / 809 有效）。

（**总行数** = 物理行数，`(Get-Content <file>).Count`；**有效代码行** = 去掉空行与纯空白行后的行数，`(Get-Content <file> | Where-Object { $_.Trim() -ne '' }).Count` 或 `Get-Content <file> | Measure-Object -Line`。）

职责全景：`main.cpp` 是 GDExtension 的导出入口与编辑器插件本体；`src/runtime/` 在**游戏运行时进程**内与编辑器进程通信，经 EngineDebugger 消息通道承载 GDA 协议。编辑器内的 MCP 服务器（`ServerContext`）与运行时桥接是两条相互独立的消息通路，本页覆盖入口生命周期、GDScript 运行时桥接与 C# 运行时桥接（§3.5，`csharp_bridge/`），MCP 工具侧见相关模块页。

**GDA 协议载体（2026-10-20 复核）**：EngineDebugger 消息通道，三类消息固定为 `gda:request`（编辑器侧 → 游戏进程）/ `gda:response`（游戏进程 → 编辑器侧）/ `gda:ready`（游戏进程就绪广播），正文为 JSON 字符串。**协议无版本字段、无鉴权**——不做版本协商、不做来源认证，安全性依赖「默认仅绑定 127.0.0.1 + 能力门默认拒绝」这层默认安全姿态（见 [安全边界与并发契约](../security_contract.md)）。

## 1. 入口点与插件生命周期（main.cpp）

### 1.1 导出函数 GDExtensionEntryPoint

- 以 `extern "C"` 导出，签名 `GDExtensionEntryPoint(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization*)`。
- 修饰宏 `GDA_EXPORT`：Windows（`_WIN32`）为 `__declspec(dllexport)`，其余平台为空。
- 函数体：构造 `godot::GDExtensionBinding::InitObject`，注册 initializer 与 terminator 回调，最后返回 `init.init()`。
- 两个回调内部均包 try/catch，异常只记录日志不中断；`_enter_tree` 内另有步骤级 try/catch（AutopilotTools 单例注册 / log dock / output logger / debugger plugin），catch 分支统一委托 `log_setup_failure` 助手（`main.cpp:36-48`，`std::exception` 与兜底两个重载）写 System 错误日志。

### 1.2 模块初始化（register_initializer）

按模块级别分派：

- **SCENE 级别**：仅当 `!Engine::is_editor_hint()`（即运行游戏的非编辑器进程）时调用 `runtime::game_bridge::register_listener()`。编辑器进程在此级别不注册桥接监听。并记 `scene_level_initialized` lifecycle 事件。
- **EDITOR 级别**：先 `debugger_ops::register_classes()`，然后依次 `ClassDB::register_class` 注册 **4 个**类：`McpLogDock`、`AutopilotTools`、`ExportGuard`、`GodotAutopilotPlugin`，最后 `EditorPlugins::add_by_type<GodotAutopilotPlugin>()` 把插件类型注册进编辑器。并记 `editor_level_initialized` lifecycle 事件。

### 1.3 模块终止（register_terminator）

- **EDITOR 级别**：仅输出"Editor level terminated"日志（并记 `editor_level_terminated` lifecycle）。
- **SCENE 级别**：**无条件**调用 `runtime::game_bridge::unregister_listener()`（与注册端点的 `!is_editor_hint()` 条件不对称，见第 4 节不一致点）。并记 `scene_level_terminated` lifecycle。

### 1.4 GodotAutopilotPlugin（EditorPlugin 子类）

`GDCLASS(GodotAutopilotPlugin, godot::EditorPlugin)`，`_bind_methods()` 为空。成员：

- 静态 `CommandQueue s_queue`，经 `queue()` 静态访问器暴露；`_process` 中 `s_queue.drain()` 即 AGENTS.md 所述"主线程排空"的实现点。
- UI 成员：`McpLogDock *log_dock`（`McpConfigDock *config_dock` 已于 0.3.2 删除）；`Ref<OutputCaptureLogger>`、`Ref<DebugCapturePlugin>`、`Ref<ExportGuard>`。
- 覆写方法：`_enter_tree` / `_exit_tree` / `_process` / `_get_unsaved_status`。

`_enter_tree()` 顺序（关键 UI/捕获步骤各自独立 try/catch，失败不中断后续；队列/脱敏/持久化初始化、ServerContext 创建与 export_guard 未包裹）：

1. `monitor::lifecycle("plugin_enter_tree")` → `note_editor_startup()` → `set_process_mode(PROCESS_MODE_ALWAYS)` → `s_queue.open()`（`main.cpp:197`）→ `perf_sampler::set_queue(&queue())`（`:198`）→ `sanitize_policy::initialize()`（解析脱敏开关）→ `LogPersist::instance().init_session()`（建 `logs/`/`traces/`、重置日志/轨迹游标、修剪旧文件、写 `session_start` trace 头）→ `monitor::environment_snapshot()`（写 `kind=snapshot`，仅主线程）→ 注册两个路径 provider：`HomeResolver::set_data_dir_provider`（取 `OS::get_data_dir()`，`:202-208`）与 `util::set_scheme_globalize_provider`（取 `ProjectSettings::globalize_path`，`:209-215`；供 `res://` 路径 globalize 后做符号链接 containment 校验，见 [安全契约](../security_contract.md)）→ 注册 `AutopilotTools` 单例 → 日志 "plugin starting"，`runtime_ops::set_editor_queue(&queue())` 注入队列（`main.cpp:239-240`），`ModeDetector` 判定 Editor/Runtime 模式写日志。
2. `gda_cmdline_mode()` 为真则直接 return——不建 UI、不启服务器（队列已在步骤 1 注入）。
3. 创建 `McpLogDock`（标题 "GDA Log"）`add_dock`。
4. `debugger_ops::create_output_logger()` 经 `OS::add_logger` 注册；`debugger_ops::create_debug_plugin()` 经 `add_debugger_plugin` 注册。
5. `new (std::nothrow) ServerContext(queue())` 并 `start()`；成功发 `server_ready` lifecycle（记 Transport 日志含端口，并追加 `{"type":"server_ready","host","port"}` trace），失败发 `error_event` 并记 `last_error()`。
6. `export_guard_.instantiate()` + `add_export_plugin`，日志 "Plugin ready"（并记 `plugin_ready` lifecycle）。

`_exit_tree()` 逆序清理：停并删 `g_server_ctx` → 移除 ExportGuard → 移除 debugger plugin → `OS::remove_logger` → 移除并 memdelete 停靠面板，整体包 try/catch；并记 `plugin_exit_tree` lifecycle；末尾 `LogPersist::instance().flush_on_main_thread()` 收尾写入剩余缓冲（`main.cpp:411`）。

`_process()`：`perf_sampler::tick(delta)`（周期采样 `kind=perf` 并做请求超时看门狗）→ `LogPersist::instance().flush_on_main_thread()`（先落盘上一帧以来累积的日志/trace）→ `s_queue.drain()` → `log_dock->poll_new_entries()`。

`_get_unsaved_status()`：`scene_dirty_tracker::is_current_scene_dirty()` 为真时返回场景名，否则返回空串。

### 1.5 cmdline 模式检测（gda_cmdline_mode，main.cpp:136）

实际函数名是 `gda_cmdline_mode()`（**不存在** `gsd_cmdline_mode`）。语义：

- 环境变量 `GDA_FORCE_HEADLESS` 值等于 `"1"` → 直接返回 `false`，即**强制禁用** cmdline 模式（此时即使无头编辑器也照常建 UI/启服务器，变量名与直觉相反）。
- 否则：需要 `Engine::is_editor_hint()` 为真且 `DisplayServer` 存在，且 `!ds->window_can_draw()`（窗口无法绘制）才返回 `true`。

### 1.6 主线程等待预算（09-29 起，dispatch.cpp）

非主线程的领域工具调用统一经 `dispatch::run_on_main_thread_with_budget` 等待主线程（`call_tool` 代理与直连 meta 工具共用该入口；`call_tool` 编排回调本身仍在 MCP 线程执行）。预算由 `dispatch::main_thread_wait_budget_ms(flags, default_ms, health_ms, long_blocking_ms)` 决策（**2026-10-20 起 4 参**，新增 `long_blocking_ms`）：`kLongBlocking`（`code_execute`/`batch_execute`）用 `GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = 29000ms`（= `GDA_DISPATCH_WAIT_MAX_MS`；env `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` 经 `env_int_clamped` 钳制在 `[1000, 29000]`，未设时生效 29000ms、设了只能下调；**不再是无界等待**；参数非正回退默认预算）；`kHealthProbe`（`ping`/`system_status`）用 5000ms（env `GODOT_AUTOPILOT_HEALTH_TIMEOUT_MS`）；其余默认 27000ms（env `GODOT_AUTOPILOT_DISPATCH_TIMEOUT_MS`，`static_assert` 低于 `GDA_TRANSPORT_TIMEOUT_MS=30000`）。超时时先 `cancel(id)` 取消未执行任务，再返回结构化 `main_thread_timeout` 错误（含 `retryable`/`tool`/`waited_ms`/`queue_depth`/`oldest_pending_ms`/`last_drain_age_ms`/`cancelled`），同时直写一条 trace 与 Transport Warning；队列缺失/关闭/满时返回 `code="queue_unavailable"`（`retryable=true`）。

`code_execute` 的 `timeout_ms` 参数上界与长阻塞预算同源：`code_exec_ops::max_timeout_ms()` 取 `min(GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000, 本节生效的长阻塞预算)`，默认环境下为 27000ms；该值同时写入 schema 的 `maximum` 与提示词，详见 [工具数据层与执行管线](../tool_base_design.md)。

## 2. GDA 协议常量（gda_protocol.hpp）

协议载体：EngineDebugger 消息通道（`register_message_capture("gda", ...)` / `send_message`），正文为 JSON 字符串。`gda:request` 由工具侧（`src/tools/base/debugger_access.cpp`）发向运行时，`gda:response` / `gda:ready` 由运行时（`game_bridge.cpp`）发出；工具侧接收方在 `src/tools/domains/debugger/debugger_ops.cpp`（09-13 下午起：ready 首次标记会话就绪时触发一次 `runtime_ops::run_channel_self_check` 后台 status 往返自检；ready/response 均返回 true 消费，消除编辑器 `Unknown message` 噪声）。

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
| `GDA_OP_EVAL_ASSERT` | `eval_assert` | `op_eval_with_assert`（game_bridge_verify.cpp，受 `game_runtime` 门禁） |
| `GDA_OP_SAMPLE` | `sample` | `op_sample_property`（game_bridge_verify.cpp，只读） |
| `GDA_OP_COLLECT_EVIDENCE` | `collect_evidence` | `op_collect_evidence`（game_bridge_verify.cpp，只读） |
| `GDA_OP_VALIDATE_UI_LAYOUT` | `validate_ui_layout` | `op_validate_ui_layout`（game_bridge_verify.cpp，只读） |
| `GDA_OP_SCENE_TREE` | `scene_tree` | `op_scene_tree`（game_bridge.cpp，受 `game_runtime` 门禁；8 action 见下） |

`scene_tree` 的 8 个 action：`set_pause`（`paused`）/`is_paused`/`reload_current_scene`/`set_debug_collisions_hint`（`enabled`）/`call_group`（`group_name`/`method`/`args`）/`notify_group`（`group_name`/`notification`）/`get_nodes_in_group`（`group_name`）/`create_timer`（`delay_sec`/`process_always`/`process_in_physics`），未知 action 返回结构化 error；编辑器侧 8 个 SceneTree 工具（`set_scene_tree_pause` 等）均组装为 `GDA_OP_SCENE_TREE` 转发，`SideEffect::GameRuntime`。

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
| `GDA_NEW_SCENE_SWITCH_WAIT_MS` | `2000` | `src/tools/domains/editor/editor_ops.cpp`（`create_editor_scene` 的 `timeout_ms` 默认值，可被参数覆盖，范围 50-30000） |
| `GDA_NEW_SCENE_POLL_MS` | `50` | `src/tools/domains/editor/editor_ops.cpp`（轮询步长） |
| `GDA_AUTO_CONTINUE_ENV` | `GDA_AUTO_CONTINUE` | `src/tools/base/runtime_ops.cpp`（自动继续次数环境变量） |
| `GDA_AUTO_CONTINUE_MAX` | `3` | `src/tools/base/runtime_ops.cpp`（默认上限） |

`create_editor_scene` 的等待循环以 `timeout_ms` 为上限（默认 `GDA_NEW_SCENE_SWITCH_WAIT_MS`；非整数或越界直接报错，超时按 `GDA_NEW_SCENE_POLL_MS` 累计等待），失败返回 `waited_ms`/`timeout_ms`/`node_released`/`editor_state` 诊断并释放未被编辑器接管的临时根节点；多签占用在 add 前秒级拦截（09-17 起：`close_current` 关后编辑根仍非空即报邻签占用，不进入等待循环）。见 [A 组](tools_ops_a.md) 的 editor_ops 小节。

桥接常量在 `src/core/config.hpp`：`GDA_HEALTHY_ACTIVITY_THRESHOLD_MS=3000`、`GDA_ERROR_BUFFER_MAX=200`、`GDA_OUTPUT_BUFFER_MAX=500`、`GDA_EVAL_TRUNCATE_BYTES=8192`；game 工具超时预算：`GDA_MAX_GAME_OP_TIMEOUT_MS=25000`（host 等待 +2000ms 宽限 < 30000 传输硬上限）、`GDA_LATE_RESULT_BUFFER_MAX=5`（超时后迟到结果保留条数）、`GDA_LATE_RESULT_SUMMARY_CHARS=200`（每条摘要截断），语义见 [modules/tools_ops_b.md](tools_ops_b.md) 的 runtime_ops 小节；dispatch 等待预算 `GDA_DISPATCH_WAIT_DEFAULT_MS=27000` / `GDA_DISPATCH_WAIT_HEALTH_MS=5000` 见 §1.6。

### 2.5 就绪窗口与请求重放（工具侧，09-29）

编辑器侧发送游戏请求（`runtime_ops.cpp`）：已 ready **且场景就绪**（scene-ready）的会话直接广播；无活动会话（`debugger_active_session_count` 只计 `is_active()` 会话）返回 `game not running: ...`（`wait_ready=true` 且编辑器正在 play 场景时仍排队等待游戏附加）；有活动会话但未就绪时，`wait_ready`（游戏工具参数，默认 true；`game_tools.hpp`/`capture_tools.hpp` 的 `boolean` 可选参数）为真则标记 `awaiting_ready` 排队并立即返回 `queued_for_ready:true`，`gda:ready`（带 `scene_ready`）到达后由 `on_session_ready` 逐个重放（广播失败重新挂起、等下一次 ready），为假则立即返回 `game started but not ready: ...`；排队调用直到超时仍未就绪返回 `structured_error.code="game_ready_timeout"`（`retryable=true`）。决策纯函数在 `src/tools/base/ready_policy.hpp`（`decide_game_request`：ready>0 **且 scene_ready>0** → SendNow、active<=0 → FailNotRunning、否则 wait_ready ? QueueForReady : FailNotReady；单元测试 `ready_policy_test` 覆盖）。游戏侧 `gda:ready` body 含 `ready:true` 与 `scene_ready`（`send_ready_message`）：注册时 `scene_ready = current_scene != nullptr`；尚未入树则挂 `process_frame` 监听，当前场景首次非空时补发一条 `scene_ready:true` 的 ready（`GameBridgeListener::on_process_frame`，一次性）；SCENE 初始化阶段尚无 SceneTree，旧版在此补发 `scene_ready:true` 属于假阳性，已删除。编辑器侧对每个会话发起 `gda:hello` 握手（250ms 间隔，最多 32 次），游戏收到后在 SceneTree 可用时回发带真实 `scene_ready` 的 ready；`game_ready_timeout` 与 `get_game_state(session)` 返回 `ready_diagnosis`（`extension_not_loaded` / `scene_not_ready` / `ready_lost` / `ready`）。ready body 另含 `csharp_supported` 与 `csharp_bridge`，仅用于 C# 桥诊断，不影响 ready 门。

## 3. 运行时桥接四文件职责

### 3.1 game_bridge.cpp — 消息通道与缓冲

- `register_listener()`（幂等，`g_registered` 守卫）：首次调用时注册 **11 个类**（`game_bridge.cpp:2265-2277`）——本文件 4 个：`GameBridgeListener`、`GameBridgeCaptureAwaiter`、`GameBridgeUiClickAwaiter`、`GameBridgeLogger`；eval 组 1 个：`GameBridgeEvalAwaiter`（`register_eval_bridge_classes`）；input 组 5 个：`GameBridgeInputWatcher`、`GameBridgeDelayedRelease`、`GameBridgeClickPair`、`GameBridgeInputSequence`、`GameBridgeFrameSequence`（`register_input_bridge_classes`）；verify 组 1 个：`GameBridgeSampleAwaiter`（`register_verify_bridge_classes`）。注册顺序 Listener→eval→input→verify→CaptureAwaiter→UiClickAwaiter→Logger。随后实例化监听器与日志器；注册 `gda` 消息捕获前后各做一次 `has_capture` 检查（已存在或注册后验证失败时记 System 错误日志并 return，不挂日志器、不发 `gda:ready`，避免半途状态被当作就绪）；成功后 `OS::add_logger` 挂游戏日志器并立即发出 `gda:ready`（body 含 `ready:true`、`scene_ready` 与活动字段；场景未入树时在当前场景首次非空后补发一条 `scene_ready:true`，见 §2.5）。
- `unregister_listener()`：反注册消息捕获与日志器，unref 两个实例。
- `GameBridgeListener`（RefCounted 子类）：绑定方法 `on_gda_message(p_message: String, p_data: Array) -> bool`。用 `util::try_parse_json` 解析 `data[0]` 为 JSON 请求（11-23 起不再直接 `JV::Parse`，畸形 JSON 走 `malformed gda request` 错误回执而非异常），取 `request_id`/`op`/`params` 分发；每收到一条消息即刷新 `g_last_activity_ms`；响应统一附加 `ok` 字段后经 `send_response` 发出。请求畸形或 op 执行失败会同时记入错误与输出缓冲。
- `send_response(request_id, body)`：补 `request_id` 字段（`GDA_FIELD_REQUEST_ID` 常量），`EngineDebugger::send_message("gda:response", [json])`。
- `GameBridgeLogger`（godot::Logger 子类）：`_log_error` 按错误类型（warning 判定码 3）经 `push_game_error` 入错误缓冲；`_log_message` 错误入错误缓冲、普通消息入输出缓冲。
- 缓冲：`g_error_buffer`（环形，上限 200，每条带递增 `seq`，`g_error_seq` 单调）+ `g_output_buffer`（上限 500），互斥锁保护；`current_error_seq()` / `eval_error_delta(since_seq)` / `append_eval_runtime_errors` 供 eval 附加运行期错误增量（文本增量 + 仅首条的 `structured_error`）；`truncate_error_text` 超 8192 字节截断。
- 状态与工具 op：`status`（版本/fps/physics_frame/paused/node_count（排除 `__gda_runtime` 子树，另报 `instrumentation_node_count`）/scene、活动字段）、`ping`（physics/process 帧 + 活动字段）、`cancel`（查 `g_cancel_handlers` 并调用 handler）、`capture`（根视口渲染存 PNG 到缓存目录，返回 path/width/height；09-14 起渲染后经 `capture_ops::prune_capture_files` 清理缓存目录，仅保留最近 20 张 `gda_capture*`）、`get_errors`（limit 默认 50）、`get_output`（limit 默认 200）、`get_tree`（DFS，深度上限 64、节点上限 2000）、`ui_elements`（08-24 新增，见下）、`ui_click`（09-16 补窗口坐标换算，见下）、`scene_tree`（09-29 新增：8 个 action 见 §2.2，受 `game_runtime` 门禁，编辑器侧 8 个 SceneTree 工具统一经此 op 转发）、`tween_create`（11-22 起：`kind="interval"` 不再要求 `node_path`，改用 `SceneTree` 单例建 tween；`kind="property"` 仍要求节点，并以属性初值的类型名作 `type_hint` 反序列化目标值，避免类型漂移）。
- **`run_ui_click` 窗口坐标换算（09-16 起，`game_bridge.cpp`）**：`UiElement` 矩形来自 `Control::get_global_rect()`（画布空间），而 `InputEventMouseButton.position` 必须是窗口客户区坐标（引擎 `viewport.cpp:_make_input_local` 用 `get_final_transform()` 反变换回画布空间，stretch 与 letterbox 边距都在该链上）。实现取控件所属视口的 `get_screen_transform()` 复合 `get_canvas_transform()`（默认画布 Camera2D / CanvasLayer 层变换），经 `coords::viewport_rect_center_to_window`（`src/core/editor_coords.hpp/cpp` 纯函数，恒等变换原样返回）换算 rect 中心后注入；无 transform 按恒等处理（保持旧行为）。响应保留画布空间 `position`（向后兼容）并新增 `viewport_position`（同值）与 `window_position`（实际注入坐标）。MCP 工具侧见 [modules/tools_ops_b.md](tools_ops_b.md) 的 `click_game_ui_element` 小节。
- 辅助函数：`gda_string`（string_view → godot::String）、`error_result`/`ok_result`、`get_scene_tree`、`resolve_node`（空路径取当前场景，先场景内再根节点查询）。

### 3.2 game_bridge_input.cpp — 输入模拟

- 取消机制实现在此文件：`g_cancel_handlers`（request_id → handler 映射）、`register_cancel_handler` / `unregister_cancel_handler`。
- 注入方式：构造 `InputEventKey` / `InputEventMouseButton` / `InputEventAction` 经 `Input::parse_input_event` 注入并 `flush_buffered_events()`；`mode="api"` 时改走 `Input::action_press/action_release`；`mode="hold"` 时 `keep_pressed` 每帧重发 press 直到释放。`duration_ms=0`/缺省不调度释放（只 press）；`pressed && duration_ms>0` 才经 `schedule_release` 挂延时释放。注入不经视窗聚焦路径，`_unhandled_input` 收不到注入事件——游戏脚本观测注入输入应在 `_process` 轮询 `Input.is_action_pressed`（事件路由到 `_unhandled_input` 属未实施的增强议题）。释放恒立即 flush（09-17 起）：`DelayedRelease` 到期释放与 `inject_step` 的动作释放统一传 `flush=true`（此前按 mode/pressed 条件 flush），抬起事件不再滞留缓冲；MCP 工具侧见 [B 组](tools_ops_b.md)的游戏运行时小节。
- `GameBridgeInputWatcher`（Node，PROCESS_MODE_ALWAYS + physics process）：轮询动作的 `just_pressed`/`just_released`/`pressed` 三态，命中即回 `result: "matched"` + `matched_at_physics_frame`，超时（默认 2000ms，上限 30000ms）回错误；可被 cancel。
- `GameBridgeDelayedRelease`（Node）：按键/鼠标/动作按下后按物理帧计数延时自动发送 release 事件（支持 `duration_ms` 与 hold）；hold 期间每物理帧重按，到期释放后 `queue_free` 自身。瞬态键态读回必须落在 hold 窗口内——到期自动释放后 `pressed` 恢复 false，持久效果（如累积位移）不受影响。
- `GameBridgeInputSequence`（Node）：步骤数组按 `duration_ms` 间隔经 `SceneTreeTimer` 逐条执行 `inject_step`。
- `parse_keycode`（09-16 起删 57 项本地 `kKeyCodeNameTable`，直接调用编辑器侧 `input_map_ops::resolve_key_name_code`）：接受裸名（`P`/`space`）、`KEY_` 前缀名（大小写不敏感）、数字码字符串（与编辑器侧同一口径，见[领域工具 A 组](tools_ops_a.md)）；非法键名错误经 `invalid_keycode_error` 统一为 `invalid keycode: <名> — use a bare letter/digit (e.g. P, 0), a KEY_* name (e.g. KEY_P, KEY_SPACE) or a numeric key code (e.g. 4194309)`。
- `op_input`：单步或 `sequence` 数组；返回 `injected_at_physics_frame` / `expected_visible_frame` / `parsed_physics_frame` / `parsed_process_frame`，游戏暂停时附 `warning`。**`coord_space`（10-11 起）**：`inject_step` 与 `op_input` 接受 `window`（默认，窗口客户区像素）或 `viewport`（视口坐标，经 `root_screen_transform()` 取根视口 `screen_transform`、由 `coords::viewport_point_to_window` 换算后注入）；非法值在 `inject_step` 报 `input coord_space must be window|viewport`、在 `op_input` 报同错并列入未知参数白名单。注意：单步成功时返回**不带** `result` 包装的裸对象，而 `error_result` 与 sequence 走 `ok_result` 包装。
- `op_input_wait`：校验 state 三态、可选 `inject` 子对象先注入再等待；挂 watcher 并注册取消 handler，**同步返回空 JSON**（响应由 watcher 在物理帧回调中异步发出）。
- `op_input_status`：动作三态 + `physics_frame` + `paused`。
- **`op_input_sequence`（08-24 新增，逐物理帧时间线）**：`inputs[]` 每项必须为对象且含非负整数 `at_frame`（相对首帧的物理帧偏移）与注入字段（复用 `inject_step` 的 type/keycode/action 等），上限 256 项；超时默认按 `max_at_frame × 33ms + 2000ms` 推导、可被 `timeout_ms` 覆盖并钳制 `GDA_MAX_TIMEOUT_MS=30000`。**`coord_space` 默认继承（10-11 起）**：顶层 `coord_space`（默认 `window`，非法报 `input_sequence coord_space must be window|viewport`）在装配 `FrameInputItem` 时填入无自带值的条目（条目非法报 `input_sequence item coord_space must be window|viewport`），`inject_step` 执行期再按视口坐标换算。校验通过后同步返回空 JSON，实际注入与响应由 `GameBridgeFrameSequence` 节点异步完成。
- **`GameBridgeFrameSequence`（Node，PROCESS_MODE_ALWAYS + physics process，08-24 新增）**：`setup()` 记录起始物理帧（`Engine::get_physics_frames()`）与 ticks 后挂到场景根；每物理帧把 `at_frame <= 相对帧数` 的条目经 `inject_step` 注入（失败写入游戏错误缓冲并继续），队列清空即 `finish(true)`；超时未完成则 `finish(false)`。响应体 `{completed, executed}` 经 `ok_result` 包装异步发出，注册 cancel handler 支持中途取消。
- **`op_ui_elements`（08-24 新增，运行中 Control 树枚举）**：从场景根 DFS 遍历（深度上限同 get_tree 的 64），命中 `Control` 节点即收集 `{path, type, visible, text?, global_rect:{position:{x,y}, size:{x,y}}}`；`max_elements` 默认 100、上限 1000，达到上限标 `truncated:true`。**窗口字段（10-11 起）**：每项在 `global_rect`（画布空间）之外另算 `window_rect{position,size}`（rect 四角经 `viewport->get_screen_transform() * ctrl->get_canvas_transform()` 换算后取包围盒）+ `window_center{x,y}`（`coords::viewport_rect_center_to_window`）+ `window_approx`（有视口 true，无视口回退视口值 false）。响应为 `ok_result({elements, count, truncated})` 包装。MCP 工具侧对应 `get_game_ui_elements`。

### 3.3 game_bridge_eval.cpp — 异步求值

- `op_eval` 分派 4 种 action：`script` / `get_property` / `set_property` / `call_method`。
- **协作式截止时间（2026-10-20 新增）**：`eval_timeout_budget_ms(params)`（`game_bridge_eval.cpp:228-239`）取 `timeout_ms`（缺省 `EVAL_DEFAULT_TIMEOUT_MS = 5000`）并**钳制到宿主上限 `GDA_MAX_GAME_OP_TIMEOUT_MS = 25000`**（`src/core/config.hpp:14`）——游戏侧单次 eval 的最长等待不得超过该上限。装配时以 `godot::Time::get_ticks_msec()` 为起点算 `deadline_ms = start_ticks + timeout_ms`，经 `gdscript_wrap::deadline_declaration(deadline_ms)` + `timeout_sentinel_declaration()` 生成**注入脚本头部的截止时间声明**，`gdscript_wrap::inject_deadline_checks(source)` 在源码中插入协作式检查点、`deadline_check_line()` 插到入口函数首行——脚本自己检查时间并提前返回哨兵值，外部不强杀。`Time` 单例不可用时直接报错「Time singleton unavailable; cannot arm the eval deadline」。
- **超时与取消的错误码**（2026-10-20 统一，11-18 增回执区分）：脚本返回超时哨兵（`is_eval_timeout_sentinel`）或 `elapsed_ms >= timeout_ms` → `eval_timeout_result`（`game_bridge_eval.cpp:241-256`），`structured_error.code = "eval_timeout"`（附 `timeout_ms`/`execution_time_ms`/`game_may_be_busy` 与「宿主已停止等待但正在跑的 GDScript 无法被打断、须等脚本返回才释放游戏主线程」的说明）；宿主在脚本完成前取消 → `structured_error.code = "eval_cancelled"`（附 `cancelled=true`/`timeout_ms`/`execution_time_ms`，`game_bridge_eval.cpp:546-560`）。编辑器侧等待超时经 `build_game_timeout_receipt`（`src/tools/base/runtime_ops.cpp:553-619`）：eval 族 op 同样产出 `code="eval_timeout"`，并新增 `timeout_cause` 区分成因——`script_error`（收到本请求的迟到响应或等待期间出现游戏错误）、`channel_lost`（通道无任何响应）、`unknown`（其余）；排队后始终未就绪的请求产出 `code="game_ready_timeout"`（`retryable=true`）；`late_results` 按请求关联。两侧口径一致。
- `op_eval_script`：`source_code` → 实例化 `GDScript` 并 `reload()`（**编译失败 ≤2s 结构化返回（09-16 起）**：不再等到超时，错误含 `gdscript://<id>.gd:<行号>` Parse Error 文本供客户端定位；09-29 起两次尝试——原样 `reload()` 失败且 `has_top_level_func_def` 为假时，经 `util/gdscript_wrap.hpp` 的 `wrap_bare_body` 包装为 `@tool extends Node` + `_run()` 裸语句体重试，重试行号经 `map_error_line_numbers` 映射回原源码，成功响应附 `wrapped:true`）；构造临时 Node 挂脚本并**无条件 `add_child` 到 `/root` 下**（执行节点不在当前场景内，相对路径以该临时节点为基准、其父级为 `/root`）；`persist` 时父节点改为 `/root/__gda_runtime` 容器并命名 `<persist_name>`（缺省 `eval_<request_id>`，重名报错）；要求脚本含 `_run()`；调用后返回 `GDScriptFunctionState`（await）则转 `GameBridgeEvalAwaiter` 异步等待，否则同步返回序列化结果（附运行期错误增量、persist 时补 `node_path`）；**`persist=false`（默认）时调用结束（含超时/取消路径）即 `remove_child` + `memdelete` 临时节点**；**MCP 响应取裸 result 值（09-16 起）**——成功 eval 的响应即 `_run()` 返回值（`void` → `null`），不再包 result 字段结构。
- `GameBridgeEvalAwaiter`（Node）：连接 state 的 `completed` 信号或超时（默认 5000ms）后响应；完成路径与超时路径都会 `send_response` 并清理（取消 handler、断信号、非 persist 时删除临时节点、queue_free）。
- `get_property` / `call_method`：经 `resolve_node` 定位节点；`call_method` 先按方法签名表逐参推导类型提示（无类型参数回退启发式），无提示的对象参数走内置启发式转换（`{r,g,b[,a]}`→Color、`{x,y[,z,w]}`→Vector2/3/4，多余键忽略，对象引用标记原样保留），再直调 GDExtension 接口取同步 `r_error`（脚本方法经 `object_call_script_method`，`INVALID_METHOD` 回退按 `get_method_list` 的 `id` 取 `MethodBind` 经 `object_method_bind_call`，与引擎 `Object::callp` 同序）；方法不存在与参数错误均按 `r_error`（附 `call_error`/`argument`/`expected`）返回明确错误（含节点路径与可用方法查询指引），成功与否只看 `r_error`（`void`/`null` 不再误报），错误水印增量仅作上下文附加（09-17-23 起，见 `src/runtime/game_bridge_eval.cpp`）。
- `set_property`：查 `get_property_list` 取类型与 hint 构造 `type_hint`，`VariantJson::deserialize(value, type_hint)` 后 set，并用 `util::check_readback(..., type_sensitive=true)` 读回校验（09-13 下午起：值类型走分量近似比较，设置未生效（回读仍等于旧值）判 REJECTED 报错、引擎调整值附 warning）。

### 3.4 game_bridge_verify.cpp — 断言 / 采样 / 证据 / 布局校验

只读为主的校验类 op（全部经 `GDA_OP_*` 分派，详见 §2.2 的 op 表）：

- `op_eval_with_assert`（`GDA_OP_EVAL_ASSERT`，受 `game_runtime` 门禁）：以 Expression 语法子集断言 `value` 与内建运算，`base_instance=null` 故只能访问 `value`，接触不到场景节点。
- `op_sample_property`（`GDA_OP_SAMPLE`）：只读采样，`get_property` 走 raw 取值，断言直接作用于 Variant，无 JSON 往返精度损失。
- `op_collect_evidence`（`GDA_OP_COLLECT_EVIDENCE`）：只读证据包采集，各节独立失败（不整体判定）。
- `op_validate_ui_layout`（`GDA_OP_VALIDATE_UI_LAYOUT`）：只读布局校验，含可见性快照先行过滤（过滤不可见与白名单节点）与遮挡启发式（按文档序后者覆盖前者，忽略 `canvas_layer`/`z_index`，故 kind 为 `possibly_occluded`、仅 warning）。
- 采样类 op 依赖 `GameBridgeSampleAwaiter`（`game_bridge_verify.cpp:120`）异步等待，由 `register_verify_bridge_classes()`（`:792-793`）注册为 10 个桥接类之一。

### 3.5 csharp_bridge — C# 运行时桥接（11-23 增量）

- `csharp_bridge/GdaCSharpRuntime.cs` 在节点入树时把 `GdaCSharpBridge` 实例挂到桥节点的 `bridge` meta 上；11-23 起覆写 `_ExitTree()`：先调 `_bridge.unsubscribeAll()` 解除全部事件订阅，再对桥节点 `RemoveMeta("bridge")`，避免退出后残留订阅与 meta。
- `csharp_bridge/BridgeEvents.cs`：订阅 token 由实例内递增改为**静态全局 token**（`Interlocked.Increment(ref _nextGlobalToken) - 1`），跨桥实例不重号；新增 `unsubscribeAll()` 逐条 `RemoveEventHandler` 并清空订阅表，返回解除条数（单条失败吞异常继续）。
- `GdaCSharpBridge.cs` 删除从未置真的 `_allowNonPublic` 字段，`ResolveFlags()` 恒返回公开成员标志——无可见行为变化。
- 工具侧就绪判定要求桥节点的 `bridge` meta 存在**且非 null**（`src/tools/domains/csharp_bridge/bridge_ready_expression()`），见 [B 组](tools_ops_b.md) 的 `csharp_member` 小节。

## 4. 与现有文档对照

对照对象是 2026-09-25 精简前的 `AGENTS.md`，逐条对应已失效，以本页正文与当前源码为准（见 [index.md](../index.md)「编写规则」）。

- `ServerContext` 不是入口点启动的：`GDExtensionEntryPoint` 只做类注册与 `add_by_type`，插件实例由 Godot 创建后在其 `_enter_tree()` 中 `new ServerContext` 并 `start()`（main.cpp:196-216）；链条为入口点 → 注册插件类型 → Godot 实例化 → `_enter_tree` → ServerContext 启动。编辑器侧类在 EDITOR 级别注册；运行时桥接 `register_listener` 注册在 **SCENE 级别**且仅非编辑器进程。
- `_process()` 中 `s_queue.drain()` 之前依次执行 `perf_sampler::tick(delta)`（周期采样与请求超时看门狗）与 `LogPersist::flush_on_main_thread()`（本地持久化写盘同走主线程、不排队列）；跨线程调用另设主线程等待预算（见 §1.6）。
- 桥接层 `error_result` 与工具侧 `{"error": "..."}` 同构；差异是桥接响应额外带布尔 `ok` 字段。
- `game_*` 工具（get_game_status/queue_game_input/capture_game_viewport 等）是本页桥接 op 的 MCP 侧封装（原 Example/docs/architecture.md 已随 2026-09-13 Example 文档重构删除，不再作为对照基线）。

不一致点：`gsd_cmdline_mode` 不存在，实际函数名 `gda_cmdline_mode()`（main.cpp:136，见 §1.5）；`GDA_FORCE_HEADLESS=1` 与变量名相反，是**禁用** cmdline 模式（强制 `false`）而非强制无头（见 §1.5）；terminator 的 SCENE 级别**无条件** `unregister_listener()`，与 initializer 的 `!is_editor_hint()` 条件注册不对称（见 §1.3）；`GDA_MSG_REQUEST` 不被桥接侧使用（由工具侧 `debugger_access.cpp` 发出）；请求消息虽从 `GDA_PREFIX` 取名，但桥接捕获只按 `"gda"` 前缀匹配，`GDA_MSG_*` 常量仅用于发送方向。

## 出站链接

- 详见 [../modules/core.md](../modules/core.md)（命令队列与线程模型）
- 详见 [../modules/tools_ops_b.md](../modules/tools_ops_b.md)（运行时/调试器工具的 MCP 封装）

## 源码注释归档（2026-09-19）

本批按"代码即文档"约定删除 `src/runtime/` 全部整行 `//` 注释与行内 `/*名字=*/` 标注（`game_bridge.cpp` 18 行、`game_bridge_eval.cpp` 23 行加 7 处行内标注、`game_bridge_input.cpp` 5 行、`game_bridge_verify.cpp` 24 行）。纯复述代码的注释直接删除，有信息量的"为什么"归档如下（行号均为清理前原文行号）：

### game_bridge.cpp

- `1443-1458` 分发门禁：I7 `eval_assert` 与 eval 共用 `game_runtime` 门禁；I1 采样、I2 证据包、I3 布局校验为只读，无门禁（与 status/capture 同）。
- `1573-1574` I4 轻量分组与编辑器侧 `DebuggerCapture::get_grouped_errors` 同形。
- `1668-1669`（P2-1 定界）：`Dump` 输出为 UTF-8 字节，`String(const char*)` 是 latin1 构造会把 CJK 逐字节拆开，必须用 `String::utf8`（与 text_ops/script_ops 同模式）。

### game_bridge_eval.cpp

- `407-414` 启发式内建参数转换：JSON 对象无类型标签，裸 `deserialize()` 得 Dictionary，引擎侧 callv 报 `CALL_ERROR_INVALID_ARGUMENT`；匹配内建值类型的形状做转换，浮点变体（`x`+`y` 即 Vector2），整数变体经方法签名 hint 解析，对象引用标记（`__node_ref__`/`object_id`/`object_id_str`/`class`）原样保留；优先级 Color > Vector4 > Vector3 > Vector2，多余键忽略。
- `455-458` 逐参类型 hint 取自节点方法表，复用 `set_property` 同一类型化路径（`util::infer_type_hint` 加 `deserialize(arg, hint)`），无类型（NIL）参数回退启发式路径。
- `546-549` native-bind 失败诊断只做加法：KIND/message 形状保持兼容，调用方以 `diagnosis` + `hint` 附加；零参内建（如 `is_on_wall`/`is_on_floor`）的 MethodBind 哈希可能与实时方法表不一致（与调用路径有关）。
- `597-598` `classdb_get_method_bind` 所需哈希取自实时方法表的 `id` 字段（与生成绑定烘焙值一致）。
- 行内标注：删 7 处（`bind_found`/`tried_script`/`native_bind_found`/`target`/`parent`/`persist`/`persist_path`），函数签名已具名，不损失信息。

### game_bridge_input.cpp

- `31-32` 键名到键码的唯一判定实现在 `src/tools/domains/input_map/input_map_ops.cpp`，此处仅跨 TU 前置声明（`input_map_ops.hpp` 只声明工具 handler）；`366-368` 该判定与编辑器侧 `add_input_map_action_event` 共用实现，接受裸名、`KEY_` 前缀（大小写不敏感）、数字码字符串（正文 §3.2 已记录）。

### game_bridge_verify.cpp

- `43-49` 设计约束：单次协议往返（编辑器侧 handler 只做参数校验与转发，等待发生在传输线程，主线程永不阻塞，与 `click_game_ui_element` 的 enumerate+inject 单往返同构，见 [tools_ops_b.md](tools_ops_b.md)）；不碰 `game_bridge_eval.cpp`（eval 复用公开 `op_eval`，协程结果走已有 cancel 句柄释放，避免同一 request_id 双响应）；文本链路全 `String::utf8`（P2-1 定界）。
- `275-276` `evidence_status` 的 healthy/last_activity 在请求时刻恒为真/零：该请求刚激活通道（入口已刷新），与分步 `get_game_status` 编辑器侧 healthy 回退语义一致；`physics_stalled` 需跨调用状态，略去。
- `401-402` `get_property` 走 raw 取值、断言直接作用于 Variant（Vector2/Color 等值类型保原类型，`value.x` 可用），无 JSON 往返精度损失。
- `439-441` 其余动作复用 `op_eval`，对序列化结果推断反序列化后断言（标量/字符串/数组/字典精确，值类型以 `{"x":..}` 字典形态到达，用 `value["x"]` 写法）。
- `452` `op_eval` 为协程挂起其内部 awaiter 时直接释放，避免同一 request_id 双响应；`eval_assert` 只接受同步结果，协程改用 `start_game_job`/`get_game_job` 观察。
- `561` `collect_evidence` 只读组合不做自动判定，各节独立失败（与 `review_scene_visually` 同构）。
- `693`/`780-781` 可见性快照先行过滤不可见与白名单节点防误报；遮挡启发式按文档序（后者覆盖前者），忽略 canvas_layer/z_index，故 kind 为 `possibly_occluded`、仅 warning。
