---
type: 模块文档
title: 工具实现模块（B 组）
description: 18 个领域工具模块（调试/显示/OS/运行时/渲染/音频/瓦片/文本/执行）实现细节
tags:
  - 模块
  - 领域工具
  - B组
timestamp: "2026-09-18T19:30:00+08:00"
resource: src/tools/
---

# 工具实现模块 B（调试/显示/OS/运行时/渲染/资源/执行）

> 09-18 E2E 优化批次增补（详见 `changelog/2026-09-18-log.md` 19:30 节）：`property_get` 加批量 `properties[1..32]`（互斥、部分成功+`missing`）；`get_debugger_errors` 加 `group:true` 轻聚类；`execute_game_script` 加 `assert` 沙箱表达式；新增只读 `sample_game_property`/`collect_game_evidence`/`validate_game_ui_layout`（Game）；游戏侧 `send_response` 改 `String::utf8` 构造（CJK 元数据乱码根因，`game_bridge.cpp:1665`）；`debugger_access.cpp:39,132` 同病与 `VariantJson` 内约 8 处 `String(c_str())` 列为遗留待排期。

> 覆盖 `src/tools/` 下 22 个 `.cpp` handler 模块：debug_ops、debugger_ops、debugger_access、display_ops、display_window_ops、os_ops、runtime_ops、runtime_game_ops、audio_ops、render_ops、environment_ops、text_ops、tilemap_ops、tileset_ops、spriteframes_ops、animation_ops、theme_ops、analyze_ops、test_ops、code_exec_ops、log_ops、capture_ops。（09-16 随坐标换算/键名统一/CJK 往返批次同步——text_ops 全链路 `String::utf8` + 诊断 IGNORE 装载、click_game_ui 窗口坐标与键名错误文案，工具数 385 / B 组 192 / 非 SIDE 325 不变；09-17 随 T09+T12 文档同步批次更新输入释放 flush、click 文本选择/annotate 序号与改脚本工作流。）
>
> 工具总数（全系统）：**393**（`ToolRegistry`/catalog/index 口径）= **385 个领域工具**（30 个域 `*_tools.hpp` 以 `GDA_TOOL_CLASS(`/`GDA_TOOL_CLASS_SIDE(` 声明，经 `register_all.cpp` 注册各域 `<域>_tools::make_tools()`）+ `system_status` 1 + **7 个元工具**。本页 B 组占其中 **192 个领域工具**（A 组 193 + B 组 192 = 385；09-16 失败修复批次 B 组 +3：game +2、tilemap +1；视觉辅助批次 capture +1）；`code_execute`/`batch_execute` 为元工具经 `server.RegisterTool()` 直连、`system_status` 单独注册。数量以 `src/tools/*_tools.hpp` 声明与 `ToolRegistry` 运行时注册为准。
>
> 相关页面：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## 结构说明

| 文件（handler） | 命名空间 | 工具类声明（`*_tools.hpp`） | 工具数 |
|---|---|---|---|
| `debug_ops.cpp` | `godot_autopilot::debug_ops` | `debug_tools.hpp` | 15 |
| `debugger_ops.cpp` | `godot_autopilot::debugger_ops` | `debugger_tools.hpp` | 6 |
| `debugger_access.cpp` | `godot_autopilot`（自由函数） | —（无工具） | 0 |
| `display_ops.cpp` + `display_window_ops.cpp` | `godot_autopilot::display_ops` | `display_tools.hpp` | 25 |
| `os_ops.cpp` | `godot_autopilot::os_ops` | `os_tools.hpp` | 18（含 `text_ops` 的 `write_file`/`read_file`/`find_in_files`，归 OS 类） |
| `runtime_ops.cpp` | `godot_autopilot::runtime_ops` | —（基础设施） | 0 |
| `runtime_game_ops.cpp` | `godot_autopilot::runtime_ops` | `game_tools.hpp` | 12 |
| `audio_ops.cpp` | `godot_autopilot::audio_ops` | `audio_tools.hpp` | 20 |
| `render_ops.cpp` + `environment_ops.cpp` | `godot_autopilot::render_ops` | `render_tools.hpp` | 49 |
| `text_ops.cpp` | `godot_autopilot::text_ops` | `text_tools.hpp` | 10（纯 TextServer/字形） |
| `tilemap_ops.cpp` | `godot_autopilot::tilemap_ops` | `tilemap_tools.hpp` | 5 |
| `tileset_ops.cpp` | `godot_autopilot::tileset_ops` | `tileset_tools.hpp` | 3 |
| `spriteframes_ops.cpp` | `godot_autopilot::spriteframes_ops` | `spriteframes_tools.hpp` | 3 |
| `animation_ops.cpp` | `godot_autopilot::animation_ops` | `animation_tools.hpp` | 10 |
| `theme_ops.cpp` | `godot_autopilot::theme_ops` | `theme_tools.hpp` | 8（含 5 个 SIDE：主题资源写入） |
| `analyze_ops.cpp` | `godot_autopilot::analyze_ops` | `analyze_tools.hpp` | 3 |
| `test_ops.cpp` | `godot_autopilot::test_ops` | `test_tools.hpp` | 2 |
| `code_exec_ops.cpp` | `godot_autopilot::code_exec_ops` | —（元工具，不经 `*_tools.hpp`，不计入合计） | 2 |
| `log_ops.cpp` | `godot_autopilot::log_ops` | `system_tools.hpp` | 1 |
| `capture_ops.cpp` | `godot_autopilot::capture_ops` | `capture_tools.hpp` | 2 |
| **合计（领域工具）** | | | **192** |

注：`environment_ops.cpp`、`display_window_ops.cpp`、`runtime_game_ops.cpp` 没有独立 hpp，handler 函数声明复用 `render_ops.hpp`、`display_ops.hpp`、`runtime_ops.hpp`；工具类声明统一落在对应 `<域>_tools.hpp`（分别为 `render_tools.hpp`/`display_tools.hpp`/`game_tools.hpp`），`make_tools()` 产出 `ToolBase` 实例。`animation_ops`/`theme_ops`/`analyze_ops`/`test_ops` 四模块各有独立 `*_ops.hpp`。显示（25）/渲染（49）两模块的 `_ops.cpp` 与 `_tools.hpp` 归并方式见上表。

## debug_ops — 引擎诊断与性能监控（15 工具 + physics_ops 挂靠 1 个）

- **职责**：编辑器侧引擎诊断：日志打印、脚本调用栈回溯、`Performance` 单例监控、FPS/物理帧率调节、编辑器调试可视化开关。
- **代表工具**：`print_debug_log`、`get_debug_stack`、`get_debug_monitor`、`get_debug_monitor_catalog`、`get_debug_monitors`、`set_debug_physics_fps`、`set_debug_collision_visual`、`set_debug_navigation_visual`、`set_debug_performance_visual`、`remove_debug_custom_monitor`、`get_debug_custom_monitor`、`get_debug_custom_monitor_names`、`get_debug_object_count`、`get_debug_memory_usage`、`get_debug_node_count`；Debug 类另含 `get_debug_object_info`（对象 ID → 类名/节点路径，handler 在 physics_ops，自 `resolve_object` 迁移）。
- **关键事实**：
  - 内置 59 个性能监控项静态表（`s_monitors`：time/memory/quantity 三类，id 0-58），`get_debug_monitors` 按表序取 `Performance::get_monitor` 值。
  - `get_debug_stack` 用 `Engine::capture_script_backtraces(include_vars)` 取 `ScriptBacktrace`，可展开帧、全局/局部/成员变量名。
  - 占位工具（`debug_get_object_count_by_class`、`debug_profile_*`、`debug_set_fps_limit`、`debug_add_custom_monitor`、`debug_query_object_count`/`debug_query_memory_usage`，godot-cpp 不可用）与 `resolve_object` 旧名已随全量重命名删除。
  - `set_debug_collision_visual` 等 3 个写 `EditorSettings::set_project_metadata("debug_options", ...)`。
- **错误模式**：缺失参数 → `{"error": "missing required parameter: ..."}`；单例缺失 → `{"error": "... not available"}`；monitor id 越界 → `{"error": "monitor id out of range: N"}`。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## debugger_ops — 游戏进程调试捕获与断点（6 工具 + 2 个注册类）

- **职责**：捕获编辑器/游戏进程的日志、错误、运行输出、调用栈、场景树，通过编辑器调试会话（`EditorDebuggerPlugin`）与运行中游戏桥接；另提供插件自身 LogSystem 缓冲的读取。
- **代表工具**：`get_debugger_log`、`get_debugger_errors`、`get_debugger_output`、`get_debugger_scene_tree`、`get_debugger_session_info`、`get_plugin_log`（09-13 下午新增，读插件进程内 LogSystem，与编辑器引擎日志是不同来源；原 `get_debugger_stack_dump`/`get_debugger_monitors` 已于 08-24 删除——ScriptDebugger 未暴露给 GDExtension，数据源不可达；断点栈与性能数据改走 `execute_game_script`/`get_game_log_entries`/`get_game_status` 与 `godot://debugger/*` 资源）。
- **关键事实**：
  - 注册两个 Godot 类（`debugger_ops::register_classes()` 经 `ClassDB::register_class`，由 `main.cpp` 调用）：`OutputCaptureLogger`（继承 `godot::Logger`，`_log_error`/`_log_message` 写入捕获缓冲）与 `DebugCapturePlugin`（继承 `EditorDebuggerPlugin`，单例存于静态 `s_instance`；`_setup_session` 登记会话、`_capture` 处理 `gda` 协议消息——`GDA_MSG_READY` 标记就绪并（首次就绪时）触发 `runtime_ops::run_channel_self_check` 通道自检、`GDA_MSG_RESPONSE` 转发给 `runtime_ops::handle_game_response`；09-13 下午起 ready/response 两类消息均 `return true`，修正此前 response 返回 false 导致编辑器逐条打印 `Unknown message: gda:response` 的噪声）。
  - `get_plugin_log`（09-13 下午新增）：读插件进程内 LogSystem 环形缓冲，无需运行游戏；可选 `limit`（默认 100、上限 1000）、`level`（debug/info/warning/error，默认 debug 即不过滤）、`category`（system/transport/tools/resources/prompts）、`filter`（消息子串，大小写不敏感）与 `since_index`（增量读取）；返回 `entries`（serial/UTC timestamp/level/category/message）、`count`、`next_index`。插件侧诊断（授权拒绝、超时账目、迟到响应丢弃、通道自检结果）全部汇聚于此。
  - 内存捕获环形缓冲（`DebuggerCapture` 单例，mutex 保护）：日志 2000 条、错误 500 条、运行输出 2000 条、监视帧 500 条。
  - 会话激活时 `get_debugger_errors`/`get_output`/`get_scene_tree` **切换为经 `runtime_ops::handle_gda_send` 走运行时通道**（get_errors/get_output/get_tree，超时 5000ms）；**无会话时不再回退编辑器捕获**——返回空 `result` + `note`（说明无活动会话、指导 `play_editor_current_scene` 或改用 `get_game_log_entries`）。编辑器进程自身的脚本错误与输出统一读 `get_debugger_log`；栈回溯与监视帧仅存本地捕获缓冲（无对应 MCP 工具），经 `godot://debugger/stack-dump`、`godot://debugger/monitors` 资源读取。
  - `get_debugger_log` 命中 `Invalid access to property or key` 错误时追加 Godot 3→4 重命名提示（`RENAME_HINTS`：frames→sprite_frames、cast_to→target_position、translation→position 等 10 条，`debugger_ops.cpp:331-341`）。
  - 空结果附加 `capture_note_for_empty_result()`：提示启动 `play_editor_current_scene` 或改用 `get_game_log_entries`（辅助函数按会话状态预留两种文案，当前三个 handler 的调用点恒落在无会话分支）；09-13 起工具描述与 schema 同步删除了"编辑器捕获回退/上次场景树"的旧承诺。
- **错误模式**：无会话场景返回 `result` 空串 + `note` 字段；会话内错误经通道原样返回 `{"error": ...}`。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## debugger_access — 运行时通道的调试桥（0 工具）

- **职责**：非工具自由函数接口层，供 `runtime_ops` 调用 `DebugCapturePlugin` 的会话能力，依赖方向无环：`runtime_ops.cpp → debugger_access.hpp`，`debugger_ops.cpp → runtime_ops.hpp`。
- **关键函数**：`debugger_capture_initialized()`（等价实例非空）、`debugger_broadcast_request(payload, &session_id, &session_count)`（向全部 active 且 ready 会话广播 `gda:request`，返回是否至少发出；09-13 下午起可回传会话数供超时诊断）、`debugger_send_cancel`、`debugger_breaked_session_ids()`、`debugger_continue_session()`。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## display_ops + display_window_ops — 显示服务与窗口（25 工具）

- **职责**：`DisplayServer` 直通封装：剪贴板、对话框、鼠标、屏幕信息与截图、TTS 语音、子窗口创建与管理（`display_window_ops.cpp` 内 10 个窗口工具与 `display_ops.cpp` 共享 `display_ops` 命名空间）。
- **代表工具**：`get_display_clipboard`/`set`、`show_display_dialog`、`get_display_mouse_position`/`set_mode`/`warp`、`capture_display_screen`、`get_display_screen_count`/`dpi`/`position`/`refresh_rate`/`size`、`get_display_tts_voices`/`speak`/`stop`、`create_display_window`/`delete`/`get_display_window_rect`/`set_flag`/`set_mode`/`set_position`/`set_size`/`set_title`/`move_to_foreground`/`request_attention`。
- **关键事实**：
  - `capture_display_screen` 经 `Image::save_png_to_buffer` → base64（`capture_ops::base64_encode`），PNG 编码失败时降级返回原始序列化 Image + warning；经 `call_tool` 直调时 PNG 以 MCP image content 交付（09-13 起，见 capture_ops 小节）。
  - `create_display_window` 用 `memnew(godot::Window)` 创建原生子窗口（需 `FEATURE_SUBWINDOWS`），返回 `window->get_window_id()`；`delete_display_window` 经 `instance_from_id` 取 Window 后 `queue_free`。
  - **`get_display_window_rect`（09-15 新增，只读）**：返回窗口客户区矩形 `position`/`size`（虚拟桌面坐标）、装饰后 `decorated_position`/`decorated_size`、所在屏幕 `screen` 与 `screen_scale`/`screen_position`/`screen_size`；`window_id` 默认 0（主窗口），负值报错。用于桌面坐标 ↔ 窗口客户区坐标换算（`warp_display_mouse`/输入工具都按客户区取坐标）。
  - `set_display_mouse_mode` 校验 mode 0-4，越界返回 `invalid mouse mode: N`（并非钳制到边界）；`show_display_dialog` 检查 `DisplayServer::dialog_show` 的返回 Error。
  - 鼠标坐标基准（09-13 下午起描述明确）：`get_display_mouse_position` 返回**桌面屏幕**坐标；`warp_display_mouse` 的 x/y 是**聚焦窗口客户区**像素（引擎转换后移动 OS 光标）——两者不可直接互喂，驱动编辑器 dock/UI 时 warp 与输入工具应使用同一客户区坐标（见 tools_ops_a 的 input_ops 小节）。
  - 15 个副作用工具以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 非空），遍历经 `get_tool_detail` 自动排除（见文末对照）；新增的 `get_display_window_rect` 不标记副作用。
- **错误模式**：`util::error_json`（"missing required parameter" / "DisplayServer not available" / "subwindows not supported on this platform"）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## os_ops — 操作系统接口（18 工具，高副作用密度）

- **职责**：`OS`/`Time` 单例直通：弹窗、进程创建与执行、环境变量、系统信息、文件回收站、shell 打开。
- **代表工具**：`show_os_alert`、`create_os_process`、`execute_os_process`、`get_os_datetime`、`get_os_environment`、`get_os_locale`、`get_os_system_fonts`、`get_os_system_info`、`get_os_unique_id`、`get_os_unix_time`、`get_os_user_data_dir`、`kill_os_process`、`move_os_file_to_trash`、`set_os_environment`、`open_os_path`。
- **关键事实**：
  - **8 个副作用工具**（含 `write_file`，handler 在 text_ops 但归 OS 类）以 `GDA_TOOL_CLASS_SIDE(` 声明，遍历经 `get_tool_detail` 的 `side_effect` 非空自动排除——`show_os_alert` 弹系统模态框；`execute_os_process`/`create_os_process` 拉起进程；`kill_os_process` 杀进程；`open_os_path` 打开 URI；`move_os_file_to_trash` 删文件；`set_os_environment` 改环境；`write_file` 写磁盘。
  - `execute_os_process` 可选 `output: true` 捕获 stdout/stderr（`OS::execute` 第四参 true，返回 exit_code + 拼接输出）。
  - 其余只读查询工具（datetime/环境/系统信息/时间/字体/唯一 ID 等）非副作用（`GDA_TOOL_CLASS(`），遍历会冒烟。
- **错误模式**：必填参数缺失 → error_json；单例缺失 → error_json；`kill_os_process`/`move_os_file_to_trash`/`open_os_path` 返回 Error 枚举数值。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## runtime_ops + runtime_game_ops — 游戏运行时通道（12 工具 + 基础设施）

- **职责**：`game_*` 工具把请求经编辑器调试会话发送到运行中的游戏进程（gda 协议）并等待响应；`runtime_ops.cpp` 提供队列注入、请求生命周期、超时取消、断点自动恢复、捕获文件回读。
- **代表工具**：`get_game_status`、`execute_game_script`、`queue_game_input`、`wait_game_input`、`get_game_input_status`、`capture_game_viewport`、`reload_game_scripts`，及 08-24 新增 `sequence_game_inputs`（逐物理帧时间线注入）、`get_game_ui_elements`（运行中 Control 树枚举）、09-15 新增 `click_game_ui_element`（按节点 path 的游戏侧 UI 点击）、09-16 新增 `start_game_job` / `get_game_job`（长游戏脚本异步提交 + 轮询）。
- **关键事实**：
  - **`set_editor_queue(CommandQueue*)`**：`main.cpp` 注入 Godot 主线程队列（`get_editor_queue()` 全局访问）。`handle_gda_send` 非主线程时经 `queue.submit(...).get()` 投递；游戏响应 `handle_game_response` 由 `DebugCapturePlugin::_capture` 喂入，以 request_id 匹配 `PendingRequest`（mutex + condition_variable）并唤醒等待线程。
  - 请求 ID 原子自增；`extract_timeout` 钳制到 `GDA_MAX_TIMEOUT_MS=30000`（默认 `GDA_DEFAULT_TIMEOUT_MS=5000`，`src/core/config.hpp`）；`handle_gda_send` 返回中间态 `{"__gda_pending": id, "timeout_ms": ...}`，由 `register_all.cpp::meta_call_tool_wait` 转 `wait_pending_response` 轮询合并；`capture_game_viewport` 额外走 `finalize_capture_response`（主线程读文件 → base64），经 `call_tool` 直调时 PNG 以 MCP image content 随响应交付（09-13 起，见 capture_ops 小节）。
  - **超时预算链（09-16 起）**：game 工具 `timeout_ms` 上限 `GDA_MAX_GAME_OP_TIMEOUT_MS=25000`——host 侧等待再加 2000ms 宽限仍低于 30000 传输硬上限，保证"超时错误 + 迟到结果"能在同一次协议往返内完整交付；超时后自动向游戏发 cancel（`breaked` 时再 engine continue），之后到达的游戏响应不再静默丢弃，而是以 `late_results`（≤`GDA_LATE_RESULT_BUFFER_MAX`=5 条、每条摘要截断 `GDA_LATE_RESULT_SUMMARY_CHARS`=200 字符）附在超时错误里。
  - **异步 job（09-16 新增）**：`start_game_job` 把游戏 op（当前仅 `eval`，params 同 `execute_game_script` 字段）异步提交进游戏进程并立即返回 `job_id`——为超过 25s 单次预算的长脚本设计；`get_game_job` 按 `job_id` 轮询（`timeout_ms` 默认 0 = 非阻塞，上限 25000 的内部等待循环），status ∈ {`pending`, `done`（回传游戏响应原文）, `expired`（超过 timeout_ms + 2000ms 后丢弃）, `cancelled`（附带幂等 engine cancel）}；job 表上限 16，收集/过期/取消各自释放 eval 错误中断抑制。`start_game_job` 以 `GDA_TOOL_CLASS_SIDE` + `GameRuntime` 声明，受 `game_runtime` 授权门约束。
  - **`get_game_status` 无参数（09-16 起）**：schema 已对齐实现——不接受任何参数（此前 schema 允许传参但 handler 忽略）。
  - **超时（09-13 下午增强）**：超时后经 `queue.submit` 向会话发 `debugger_send_cancel`（GDA_OP_CANCEL）；错误信息现含实际广播的**会话数**与响应账目——`responses received: N, last response X ms ago, pending: M`（从未收到响应时为 `no gda:response received yet`），据此可区分"请求未送达 / 游戏未回 / 响应未匹配"。
  - **响应出口日志化（09-13 下午起）**：`handle_game_response` 的静默失败出口（payload 非 JSON 对象、`request_id` 缺失或非整数）均写 Warning 日志（含 256 字符载荷预览），配合 `get_plugin_log` 可追踪迟到/丢弃响应。
  - **通道自检（09-13 下午新增）** `run_channel_self_check`：游戏会话首次 ready 时在后台线程发起一次 `status` 往返（1500ms + 2s 宽限），成功/失败均写入插件日志（成功含往返毫秒数）——启动后即可凭 `get_plugin_log` 判断运行时通道是否健康，不必等首个 7s 超时。
  - `get_game_status` 响应注入派生字段：`healthy`（由 `last_activity_ms` 推导）与 `physics_stalled`（physics_frame 连续不变 ≥1000ms 且 fps>0）。
  - **断点自动恢复** `maybe_recover_break`：对全部 breaked 会话按 `GDA_AUTO_CONTINUE` 环境变量决定是否 `debugger_continue_session`，每会话上限 `GDA_AUTO_CONTINUE_MAX` 次。
  - `queue_game_input` 参数白名单（type/keycode/pressed/button_index/position/relative/action/duration_ms/mode/direction/amount/timeout_ms），未知参数由 handler 严格校验并返回 `unknown parameter for queue_game_input: <name>` 错误（`runtime_game_ops.cpp:187-191`；`game_tools.hpp:32` 工具描述与 skill 模板中"忽略并回传 `ignored_params`"的旧表述已一并修正）；游戏侧键名判定自 09-16 起与编辑器侧共用 `input_map_ops::resolve_key_name_code`（裸名/`KEY_` 前缀/大小写不敏感/数字码等价，见[领域工具 A 组](../modules/tools_ops_a.md)），非法键名错误改为 `invalid keycode: <名> — use a bare letter/digit (e.g. P, 0), a KEY_* name (e.g. KEY_P, KEY_SPACE) or a numeric key code (e.g. 4194309)`；`get_game_input_status` 响应不附带 `recent_engine_errors`（handler 仍会读取本地错误缓冲，但该缓冲无写入方，字段实际不会出现）——运行中错误改读 `get_debugger_errors`，编辑器错误读 `get_debugger_log`。
  - `sequence_game_inputs`（gda op `input_sequence`）：`inputs[]` 每项含 `at_frame`（物理帧偏移）与注入字段，由游戏侧 `GameBridgeFrameSequence` 节点逐物理帧执行，完成后响应 `{completed, executed}`；上限 256 步，超时默认按 `max_at_frame×33ms+2000ms` 推导并钳制 30000ms，可被 cancel。协议与帧调度细节见[入口与运行时](../modules/entry_runtime.md)。
  - `get_game_ui_elements`（gda op `ui_elements`）：DFS 遍历运行中 Control 树，每项 `{path, type, visible, text?, global_rect:{position,size}}`，`max_elements` 默认 100、上限 1000，超限标 `truncated:true`。
  - **游戏侧输入扩展（09-15）**：`queue_game_input` 的 `type` 新增 `wheel`（给 `direction` up/down/left/right，或 `button_index` 4/5/6/7 对应上/下/左/右；可选 `amount` 1-10 默认 1 与 `position {x,y}`；wheel 按键按 `factor=amount` 注入按下+释放）与 `mouse_motion`（`position {x,y}` 必填，可选 `relative {x,y}` 设定位移增量）；`sequence_game_inputs` 的 inputs 同增 `kind: wheel`/`mouse_motion` 及同名字段。释放立即 flush（09-17 起）：`DelayedRelease` 到期释放与动作释放恒调 `flush_buffered_events`（此前按 mode/pressed 条件 flush），抬起事件不再滞留缓冲；注入与取消细节见[入口与运行时](../modules/entry_runtime.md)的输入模拟小节。
  - **截图增强（09-15）**：`capture_game_viewport` 新增可选 `region {x,y,width,height}`（裁剪到图像内，空交集报 `region_out_of_bounds`）与 `max_dimension`（64-4096，最长边等比缩小、不放大），裁剪与缩放发生在游戏进程内；`annotate=true` 由游戏侧为每个可见 Control 画编号框，结果附 `annotated:true` 与 `elements`（id/path/type/text/position/size，图像像素，`id` 为单次捕获序号、跨次不稳定，不可传给 `click_game_ui_element`，用同行 `path` 或 `text`/`text_index`）。09-16 起另支持 `annotate_nodes`（1-50 个游戏绝对路径，精确匹配优先、否则 `/<path>` 后缀匹配）与 `annotate_nodes_max`（1-50 自我限流），游戏进程内画蓝色编号框并回传 `node_elements`/`node_truncated`（经 `finalize_capture_response` 白名单透传），单项失败隔离（`ok:false` + `error` + 候选路径，不失败整体）。
  - **`click_game_ui_element`（09-15 新增，09-16 补窗口坐标，09-17 加文本选择）**：按 `path`（优先）或可见文本 `text` + `text_index`（默认 0）在游戏进程内枚举 Control 树（path 精确匹配优先，否则匹配以 `/<path>` 结尾的节点；text 先在可见元素上精确匹配、再子串匹配，多命中无 `text_index` 即报歧义候选清单、`text_index` 越界另报错，`text_index` 须配 `text`），在元素 `global_rect` 中心注入 mouse_button 按下/释放（`button_index` 1=左 默认/2=右/3=中，`double_click` 追加第二对）；注入坐标须为窗口客户区坐标——`UiElement` 矩形来自 `Control::get_global_rect()`（画布空间），经控件所属视口的 `get_screen_transform()`（拉伸 + letterbox 边距）复合其 `get_canvas_transform()`（默认画布 Camera2D / CanvasLayer 层变换）换算，无 transform 按恒等处理；响应保留画布空间 `position`（向后兼容）并新增 `viewport_position`（同值）与 `window_position`（实际注入坐标）；枚举+注入在一次调用内完成（单次协议往返，避免编辑器主线程等待）；`max_elements` 默认/上限 1000、`timeout_ms` 默认 5000/上限 30000；未找到/歧义/越界时错误附元素数与候选路径；`annotate` 的 `id` 为单次捕获序号（1 起，跨次不稳定，不可传给本工具——未知参数错误会连带提示改用同行 `path` 或 `text`/`text_index`）；以 `GDA_TOOL_CLASS_SIDE(` + `GameRuntime` 声明，受 `game_runtime` 授权门约束。坐标换算正确性由 L1 `game_ui_coords_test`（4x 拉伸/letterbox/画布平移/rect 中心）覆盖，端到端换算断言待示例游戏恢复后补测（23_click_ui_coords L2 仅覆盖参数校验与无游戏通道错误）。
  - **改脚本后工作流（09-17 起）**：`reload_game_scripts` 为软重载、游戏下次空闲轮询生效且无确认回执——改完脚本先调它再 `reload_current_scene`/重试，单重载场景不保证重读磁盘版本（`CACHE_MODE_REUSE`），用 `get_game_log_entries` 验证；`create_script` 响应 `note` 同述（见 [A 组](../modules/tools_ops_a.md)的脚本小节）。
- **降级指引（09-13 下午起）**：`capture_game_viewport` 与 4 个输入工具在运行时通道不可达的错误响应上附加 `hint` 字段，列出不依赖游戏进程的替代路径（`get_game_log_entries`、`capture_display_screen`、`capture_editor_viewport`、`get_plugin_log`）。
  - `runtime_game_ops.cpp` 无独立 hpp，声明在 `runtime_ops.hpp`。
- **错误模式**：`error_json("game not ready: ...")`、超时错误（含 op 名/request_id/会话数、响应账目与 `late_results` 迟到结果摘要）、`unexpected game response (missing result)`；09-16 起超时后的迟到响应不再仅记 Warning 丢弃，进入 `late_results`（见上）。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## audio_ops — 音频总线与播放（20 工具）

- **职责**：`AudioServer` 总线/效果/设备 + `AudioStreamPlayer`(1D/2D/3D) 播放控制。
- **代表工具**：`get_audio_bus_layout`、`set_audio_bus_layout`、`get_audio_bus_count`、`set_audio_bus_volume_db`、`set_audio_bus_mute`、`set_audio_bus_solo`、`add_audio_bus_effect`、`remove_audio_bus_effect`、`play_audio_player`、`stop_audio_player`、`set_audio_player_volume_db`、`set_audio_player_pitch_scale`、`get_audio_player_playback_position`、`seek_audio_player`、`get_audio_device_outputs`、`set_audio_device_output`。
- **关键事实**：
  - 播放类工具经 `resolve_audio_player` 兼容三种节点；`add_audio_bus_effect` 用 `ClassDBSingleton::instantiate` 按 effect_type 字符串建 `AudioEffect`。
  - `set_audio_bus_layout` 用 `VariantJson::deserialize` 回灌 `AudioBusLayout`；`get_audio_bus_layout` 序列化 `generate_bus_layout()`。
  - `play_audio_player` 可选 stream_path 即时加载（`ResourceLoader` + FileAccess 存在性检查），无 stream 时提示先用资源工具设置。
  - 节点路径解析：`find_node` 相对/绝对/带根名三种形态，且限定在编辑场景内。
- **错误模式**：bus_index 越界 → `{"error": "audio bus not found at index: N"}`；加载失败用 `error_detail`（含原因/期望/建议）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## render_ops + environment_ops — RenderingServer 直通（49 工具）

- **职责**：低层 `RenderingServer` RID 句柄封装：CanvasItem 绘制、相机/灯光/网格/材质/视口/粒子、环境后处理、雾、着色器、实例与全局参数；RID 解析。
- **代表工具**：`create_render_canvas_item`、`add_render_canvas_item_rect`、`create_render_camera`、`create_render_light`、`create_render_mesh`、`create_render_material`、`create_render_viewport`、`create_render_particles`、`set_render_environment_bg_color`、`set_render_environment_ambient_light`、`set_render_environment_glow`、`set_render_environment_ssr`、`set_render_environment_tonemap`、`set_render_environment_sdfgi`、`set_render_environment_volumetric_fog`、`create_render_sky`、`set_render_shader_code`、`set_render_shader_parameter_global`、`find_render_node_from_rid`。
- **关键事实**：
  - 全程 RID 数值句柄（`rid_from_json`/`rid_to_json`，经 `UtilityFunctions::rid_from_int64` 互转），不绑定场景节点。
  - **`environment_ops.cpp` 无独立 hpp**：7 个环境后处理工具与 `render_ops.cpp` 共享 `render_ops` 命名空间，声明在 `render_ops.hpp`；RID 辅助已归一至共享头 `util/json_godot.hpp`（`rid_from_json`/`rid_to_json`、`json_to_color` 等），08-22 清理时删除两文件本地副本。
  - `set_render_environment_*` 参数繁多带默认值（含必填 `environment_rid`：glow 12 参、sdfgi 11 参、volumetric_fog 14 参）；渲染工具普遍以 Info 记 `called`、Debug 记 `completed`，环境 7 工具中仅 `set_render_environment_bg_color`/`set_render_environment_ambient_light` 写了 Debug 完成日志，glow/ssr/tonemap/sdfgi/volumetric_fog 只记 Info `called`。
  - `find_render_node_from_rid` 与 `get_render_canvas_item_rid` 桥接场景节点与 RID（`EditorInterface` 参与）。
- **错误模式**：必填 `environment_rid`/RID 参数缺失 → error_json；`RenderingServer not available` → error_json；无 `result` 之外的扩展字段。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## text_ops — 文本服务与字形（10 工具，纯 TextServer/字形）

- **职责**：`TextServer` 直通：字体资源、shaped text、系统字体路径、RTL 检测；另含通用文件读写与项目内文本搜索工具。
- **代表工具**（`text_tools.hpp` 的 10 个，纯 TextServer/字形）：`create_text_font`、`create_shaped_text`、`set_text_font_antialiasing`、`set_text_font_data`、`set_text_font_hinting`、`get_text_font_system_path`、`has_text_feature`、`is_text_locale_right_to_left`、`add_shaped_text_string`、`get_shaped_text_size`；`write_file`/`read_file`/`find_in_files` 的 handler 亦在本文件，但以 `GDA_TOOL_CLASS_SIDE(`/`GDA_TOOL_CLASS(` 声明于 `os_tools.hpp`（归 OS 类，见 os_ops）。
- **关键事实**：
  - **CJK 全链路 UTF-8（09-16 起）**：`get_text_font_system_path` / `is_text_locale_right_to_left` / `add_shaped_text_string`（含 `language`）的入参构造与 `write_file`（`file->store_string`）的内容写入统一改 `String::utf8`——`String(const char*)` 是 latin1 构造，CJK 内容会逐字节变字符、写盘后再读回即乱码；`build_script_diagnostics` 改 `CACHE_MODE_IGNORE` 装载（REUSE 会命中 ResourceCache/GDScriptCache 返回旧实例，诊断看不到刚写入的磁盘内容）。19_cjk_roundtrip L2（`verified`/`readback`/`cache_refreshed` + 全量内容比对 + `find_in_files` 字节级命中）覆盖。
  - `set_text_font_data` 用 `FileAccess::get_file_as_bytes` 读字体文件；`write_file`（原 `file_write`，随全量重命名迁至 OS 类）用 `FileAccess` WRITE/READ_WRITE 直写磁盘，以 `GDA_TOOL_CLASS_SIDE(` 声明于 `os_tools.hpp`（`side_effect` 非空），遍历经 `get_tool_detail` 自动排除（写文件副作用，历史事故源之一）。
  - **`read_file` / `find_in_files`（08-20 新增）**：`read_file` 与 `write_file` 对称，读任意文本文件返回 `{path, content}`；`find_in_files` 递归搜索目录下文本（`query` 必填，可选 `dir` 默认 `res://`、`extensions` 默认 gd/tscn/tres/cs/md/json/h/cpp、`case_sensitive`、`max_results` 默认 500，达到即截断返回 `truncated:true`），按扩展名过滤并统计每文件命中次数，**归 OS 类但 handler 在 text_ops**。路径拼接用 `join_search_path`（`res://` 根免三重斜杠）。
  - 本模块是 `get_os_system_fonts`（os_ops）之外的字体获取路径补充。
- **错误模式**：`TextServer not available` / `missing or invalid parameter: font_rid` / `shaped_rid`；`write_file`/`read_file` 打开失败 → error_json；`find_in_files` 缺 query → missing required parameter。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## tilemap_ops — 瓦片地图（5 工具）

- **职责**：编辑场景中创建 `TileMap`/`TileSet` 并写入格子；兼容 `TileMapLayer` 节点。
- **代表工具**：`create_tilemap`、`set_tilemap_cell`、`set_tilemap_cells`、`create_tilemap_tileset`、`fill_tilemap_rect`（09-16 新增）。
- **关键事实**：
  - `create_tilemap` 经 `ClassDBSingleton::instantiate("TileMap")` 建节点，默认挂编辑场景根并 `set_owner`，返回路径（剥掉根前缀）。
  - `set_tilemap_cell`/`set_cells` 同时支持 `TileMap::set_cell(layer, coords, source_id, atlas_coords)` 与 `TileMapLayer::set_cell(coords, ...)` 两条 API 分支；`source_id=-1` 表示擦除该格（`atlas_coords` 忽略；源自引擎 `set_cell` 的 INVALID 语义，09-13 下午起描述与技能文档已写明）。
  - `set_cells` 无服务端固定条数上限（旧 "~64 条截断" 表述已删除）：批量过大可能在客户端/传输层触发参数上限、表现为工具执行前的 JSON 解析错误，建议分批或改用 `fill_tilemap_rect`/`execute_script`/`code_execute` 循环。
  - **`fill_tilemap_rect`（09-16 新增）**：轴对齐矩形一次铺砖/擦除——`node_path` + `from`/`to` 角点格坐标（均含端点、角点顺序不限），传 `source_id` + `atlas_coords` 逐格放置同一瓦片，或 `erase:true` 清空矩形（忽略 source_id/atlas_coords）；可选 `alternative`（默认 0）与 `layer`（默认 0，TileMapLayer 忽略）；矩形上限 100000 格，超限报错需分块；实现为逐格 set_cell/erase_cell 循环——省一次协议往返并产生**单个 undo 动作**，但不加速填充本身；返回 `set_count`/`from`/`to`/`width`/`height`/`undo`。以 `GDA_TOOL_CLASS_SIDE(` 声明（`side_effect` 返回 None，仅靠宏前缀从遍历中排除）。
  - `create_tilemap_tileset` 产出 **`memory://` 内存资源**（`resource_ops::register_memory_resource`），供 tileset_ops 解析。
  - 写场景后调用 `scene_dirty_tracker::mark_scene_modified()`；`set_cells` 部分无效条目 → error + warnings（"N cells skipped"）。
- **错误模式**：节点类型不符 → error_json（含创建指引）；parent 未找到 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## tileset_ops — 瓦片集深化（3 工具）

- **职责**：对 `memory://` TileSet 添加 atlas 源（自动按纹理尺寸/边距/间距生成格子）、物理层与格子碰撞多边形。
- **代表工具**：`add_tilemap_atlas_source`、`add_tilemap_physics_layer`、`set_tilemap_tile_collision`。
- **关键事实**：
  - `resolve_tileset` 经 `resource_ops::resolve_memory_resource` 找内存 TileSet，缺失时报"先 create_tilemap_tileset"。
  - `add_tilemap_atlas_source`：`add_source` 后按 `compute_grid`（1 + (tex_dim − margin − tile_dim) / (tile_dim + spacing)）逐格 `create_tile`，返回 created_tiles/grid_size。
  - `set_tilemap_tile_collision` 用 `TileSetAtlasSource::get_tile_data(atlas_coords, alternative)` 取 `TileData`（Godot 4.7 迁移事实之一），`set_collision_polygons_count` + `set_collision_polygon_points` 写回；物理层缺失时经 `error_detail` 提示先 `add_tilemap_physics_layer`；空 polygon 数组清空现有碰撞。
- **错误模式**：source_id 冲突 → error_json；纹理文件不存在 → error_detail；引擎拒绝多边形 → error_detail（含回读校验 `get_collision_polygon_points` 判空）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## spriteframes_ops — 精灵帧（3 工具）

- **职责**：`memory://` SpriteFrames 资源：创建（删除默认动画）、加动画、加帧（支持 hframes/vframes 切 AtlasTexture）。
- **代表工具**：`create_spriteframes`、`add_spriteframes_animation`、`add_spriteframes_frame`。
- **关键事实**：
  - `create_spriteframes` 经 `ClassDBSingleton::instantiate("SpriteFrames")` 并 `remove_animation("default")`，注册 `memory://` 路径；09-13 下午起支持可选 `default_animation` 参数——立即创建一个同名空动画（引擎默认 fps/loop），响应回显该字段。用于避免未显式设置 `animation` 的节点在编辑器预览中落到字母序首动画。
  - `add_spriteframes_frame`：单帧直接 `add_frame(anim, tex, duration)`；多帧（hframes*vframes>1）按 `AtlasTexture` 区域切分逐帧添加，返回 frames/added_frames。
  - 纹理路径需磁盘存在且为可加载 Texture2D，否则 `error_detail`（含 reimport 建议）。
- **错误模式**：hframes/vframes 非正整数 → error_json；动画未加 → error_json（提示先 add_animation）。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## animation_ops — 动画与状态机（10 工具）

- **职责**：编辑场景内 `AnimationPlayer`/`Animation` 与 `AnimationTree`（`AnimationNodeStateMachine`）的创建与编辑。
- **代表工具（共 10 个）**：`create_scene_animation_player`、`create_animation`、`remove_animation`、`get_animation_list`、`create_animation_track`、`insert_animation_keyframe`、`remove_animation_track`、`create_scene_animation_tree`、`add_animation_machine_state`、`connect_animation_states`。
- **关键事实**：
  - 所有写操作走 `scene_dirty_tracker::mark_scene_modified()`（`animation_ops.cpp` 9 处）；节点解析限定编辑场景（`util::resolve_scene_node`），player/tree 类型不符时报错并列出实际类名。
  - `create_animation` 在 AnimationPlayer 默认动画库（空 StringName，缺失时自动创建）上 `add_animation`；`length_sec` 默认 1.0 且须 >0，`loop_mode` 0/1/2（none/linear/pingpong），同名动画报错。
  - `create_animation_track` 支持 `value`（必须给 `property`，轨道路径 `<node_path>:<property>`，更新模式 `UPDATE_CONTINUOUS`）与 `method` 两类；同类型同路径重复报错。`insert_animation_keyframe` 的 value 轨道经 `VariantJson::deserialize` 转 Variant，method 轨道要求 `{"method": "...", "args": [...]}` 对象，返回 key_count，`track_index` 越界报错。
  - `create_scene_animation_tree` 建 AnimationTree、默认以 `AnimationNodeStateMachine` 为 tree_root，可选 `anim_player` 关联；`add_animation_machine_state` 每状态建一个 `AnimationNodeAnimation`；`connect_animation_states` 带 `condition` 时同时设 `advance_condition` 并强制 `advance_mode=AUTO`，响应 note 提示条件须是 AnimationTree 的 bool 参数（经 code_execute 设置）。
- **错误模式**：参数缺失、节点不存在、场景未打开、名称或过渡重复、track_index 越界 → `{"error": ...}`。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## theme_ops — 主题资源与 Control 应用（8 工具，5 个 SIDE）

- **职责**：磁盘 `Theme` 资源（.tres）的创建、写入、检视与对编辑场景 Control 的应用。
- **代表工具（共 8 个）**：`create_theme_resource`、`set_theme_color`、`set_theme_constant`、`set_theme_font_size`、`set_theme_stylebox_flat`、`get_theme_info`、`apply_theme_to_control`、`set_control_anchor_preset`。
- **关键事实**：
  - 5 个写资源工具（create/set_color/set_constant/set_font_size/set_stylebox_flat）以 `GDA_TOOL_CLASS_SIDE(` 声明，每次修改经 `persist_theme` 用 `ResourceSaver` 落盘并 `EditorFileSystem::update_file` 刷新。
  - `create_theme_resource` 拒绝 `user://` 路径（错误信息要求 res://）且目标文件不能已存在；可选 `base_type` 写入资源 metadata。
  - 颜色参数支持 `"#rrggbb"`/`"#rrggbbaa"` 或 `{r,g,b,a}`（通道 0-1）；`set_theme_stylebox_flat` 的 `stylebox` 对象支持 bg_color/border_color/border_width/corner_radius/content_margin，其中 width/radius/margin 可传数字（各边/角同值）或对象（left/top/right/bottom、四角键）。
  - `get_theme_info` 按 type 汇总 colors/constants/font_sizes/styleboxes 名单；`apply_theme_to_control` 与 `set_control_anchor_preset`（preset 0-15、可选 `keep_offsets`）只改内存并 `mark_scene_modified`，响应 note 提示用 `save_editor_scene` 持久化。
- **错误模式**：路径为 user://、文件已存在、.tres 加载失败/类型不符、颜色或 preset 非法 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## analyze_ops — 场景与依赖分析（3 工具）

- **职责**：只读分析：场景文件可加载性与依赖完整性、项目资源引用图（找未使用资源）、信号连接图。
- **代表工具**：`validate_scene_file`、`find_unused_resources`、`trace_signal_flow`。
- **关键事实**：
  - `validate_scene_file`：以 `CACHE_MODE_IGNORE` 加载 `PackedScene` 并试实例化；逐个检查 `ResourceLoader::get_dependencies`（uid:// 经 `ResourceUID` 解析）是否落盘。返回 `valid`/`problems{kind,message}`/`missing_dependencies`/`dependency_count`，问题码 `load_failed`/`missing_dependency`/`instantiate_failed`。
  - `find_unused_resources`：经 `EditorFileSystem` 收集文件、用依赖集合求反向引用，豁免 project.godot、主场景、应用图标、autoload 及 `.import`/`icon.svg`；返回 `scanned`/`unused{path,type}`/`unresolved_uids`/`exempted`。
  - `trace_signal_flow`：从编辑场景节点做 BFS，`direction` outgoing/incoming/both（默认 both）、`max_depth` 默认 3 且 ≥1；边含 `from`/`signal`/`to`/`method`/`persisted`，按对象+信号+目标+方法去重，返回 `root`/`edges`/`edge_count`。
- **错误模式**：缺 path、direction 非法、max_depth<1、节点/目录不存在、EditorFileSystem 或 ResourceLoader 不可用 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## test_ops — GDScript 测试套件（2 工具）

- **职责**：在编辑器进程内运行带断言框架的 GDScript 测试并汇总结果。
- **代表工具**：`run_gdscript_tests`（内联 `tests: [{name, source}]` 数组）、`run_gdscript_test_files`（扫描目录，默认 `res://tests`、文件模式 `*.gda_test.gd`，匹配文件名排序后逐个运行）。
- **关键事实**：
  - 每个用例包装为 `@tool extends Node` 脚本，注入 `SceneRoot` 与断言函数：`check(condition, msg)`、`check_equal(actual, expected)`、`check_almost_equal(actual, expected, epsilon=0.001)`（非数值回退相等比较）、`fail(msg)`、`fatal(msg)`（置 `__gda_aborted` 中止本用例）；测试体必须是顺序语句，顶层 `func`/`static`/`class`/`class_name` 被拒绝，缩进风格须统一。
  - `timeout_ms` 默认 10000、上限 30000；GDScript 同步执行不可中断，仅在用例间检查预算，超预算的后续用例跳过并附 note。
  - 返回 `summary{total,passed,failed,skipped}` + `cases`（含每条 check 与运行时错误）；`run_gdscript_test_files` 额外回传 `files`，无匹配文件时附 note。
- **错误模式**：`tests` 非数组、目录不存在或打开失败 → error_json；用例编译失败记入 cases。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## code_exec_ops — GDScript 动态执行（2 元工具）

- **职责**：`code_execute` 在编辑器进程内动态编译并执行任意 GDScript；`batch_execute` 顺序批调任意工具。
- **代表工具**：`code_execute`、`batch_execute`（均为元工具，`register_all.cpp` 以 `MetaTool` 加入 `ToolRegistry` 并经 `server.RegisterTool()` 直连注册，不经元工具 `call_tool` 代理；`batch_execute` 内部经 `dispatch::call_handler` 批调领域工具）。
- **关键事实（执行机制）**：
  1. **安全检查** `check_source_safety`：禁止 `close_scene(` 调用（会销毁执行节点导致编辑器崩溃）。
  2. **源码包装** `build_wrapped_source`：`extends` 行注释化，包裹为 `@tool extends Node` 脚本；单函数模式（默认 `_run`）把用户代码逐行缩进进 `func _run():`，自动探测 tab/空格缩进风格（混合则报错）；多函数模式（检测到 `func `）保留用户函数，缺失的 `function_name` 补 `pass` 占位。
  3. **编译** `compile_and_map_errors`：`GDScript::set_source_code` + `reload()`；失败时取 `capture_new_error_text` 并把 wrapper 行号回映射为用户源码行（`map_line_numbers`，偏移 5 行），错误文本截断 8192 字节并附 wrapped source。
  4. **执行** `run_with_timeout`：`memnew(Node)` + `set_script` 挂到 SceneTree 根（失败则挂编辑场景根），`TempNodeGuard` 确保析构回收；超时仅在执行前检查一次（同步调用不可中断）。
  5. **结果回收** `reconcile_leaked_children`：执行后比对编辑场景根的子节点数，`auto_owner=true` 递归 `set_owner`（计入 auto_owner_set），false 则删除无主新子节点。
  - **SceneRoot 暴露**：wrapper 内 `var SceneRoot := EditorInterface.get_edited_scene_root()`；错误提示说明执行节点在 `/root` 下、须经 `SceneRoot.get_node("Child")` 访问编辑场景节点。
  - **timeout_ms**：默认 5000；实现钳制上限 30000（`CODE_EXEC_MAX_TIMEOUT_MS`，非正值回落默认；08-24 前仅 `static_cast<int>` 未钳制，已修复，与 runtime_ops 的 `GDA_MAX_TIMEOUT_MS` 钳制对齐）。
  - 返回字段：result（`VariantJson::serialize`）、execution_time_ms、auto_owner_set、wrapped_source、output（≤8192）、runtime_error/error_details/structured_error（首行解析 file/line/message）。
  - 结果为 Resource 时自动 `resource_registry::register_resource`（registered_resource 字段）。
  - **异步 game 工具的 pending 语义（09-14 起，09-16 扩展 await_async）**：`operations` 里异步 game 工具返回的中间态 `{"__gda_pending": id}` 经 `runtime_ops::payload_is_pending` 识别——`await_async=false`（默认）时该条报错并释放 pending（不再假报成功，游戏侧随后到达的响应按迟到响应处理）；`await_async=true` 时等待真实结果合并进该条（**必须直接调用 `batch_execute`**；经 `call_tool` 间接调用会在主线程兜底报错）。响应新增 `pending` 计数，`total` = succeeded + failed + pending，`skipped` 为 `stop_on_error` 截断后未执行数。
- **错误模式**：`util::error_json(error_out)` 统一出口；编译失败含行号映射与 wrapped source；超时报错不含渲染状态。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## log_ops — 游戏日志文件读取（1 工具）

- **职责**：读取游戏进程磁盘日志 `user://logs/godot.log` 尾部。
- **代表工具**：`get_game_log_entries`（limit 默认 50、上限 500；09-14 起可选 `filter`）。
- **关键事实**：
  - Windows 用 `CreateFileW` 共享读（FILE_SHARE_READ|WRITE|DELETE）+ 256KB 尾窗口；打开失败重试一次（100ms 延迟）后回退归档 `godot.log.1`（返回 `from_archive` + warning）。
  - 文件不存在时错误信息附目录诊断（`logs_dir_diagnostic`：目录不存在/为空/内容列表）与运行提示（"start it with play_editor_current_scene"）。
  - **`filter` 过滤（09-14 起）**：可选 `filter`（大小写敏感子串）把扫描窗扩大到最近 2000 行（`FILTER_SCAN_LINES`，`filter_log_lines`），命中行再按 `limit` 取尾部返回；结果附 `filter` 与 `matched_lines`（扫描窗内命中总数，不受 limit 截断影响），`filter=""` 等同未过滤。
- **错误模式**：`error_detail`（open error code + 建议改用 `get_debugger_output`/`get_debugger_errors`）。
- 出站链接：[运行时通道](../modules/entry_runtime.md) · [工具注册表](../modules/tools_registry.md)

## capture_ops — 编辑器视口截图（1 工具 + base64 工具函数）

- **职责**：编辑器 2D/3D 视口截图并 base64 返回，可选落盘保存。
- **代表工具**：`capture_editor_viewport`（注册名按动词置首规范，hpp 函数名 `handle_capture_viewport`；旧名 `editor_capture_viewport` 的 `editor_` 前缀已随全量重命名消除）。
- **关键事实**：
  - `target="game"`（08-24 接通；09-13 下午改为非阻塞 pending 协议）：返回中间态 `{"__gda_pending": id, "timeout_ms": ...}`，由 `call_tool` 的 `meta_call_tool_wait` 等待后经 `finalize_capture_response`（主线程读盘 → base64）定型；旧的阻塞式 `runtime_ops::game_capture_blocking` 已删除（它曾在主线程等待，阻塞编辑器消息泵）。`timeout_ms` 可选，默认 5000、钳制 30000。
  - `target="editor"`（默认）优先 2D 视口（`EditorInterface::get_editor_viewport_2d`）回退 3D；`ViewportTexture::get_image` → `save_png_to_buffer` → `base64_encode`，返回 data/format/width/height；`save:true`（09-14 起，仅 editor 目标）额外把 PNG 写入 `user://godot_autopilot/captures/gda_capture_editor_<ticks>.png` 并在结果附 `path`。
  - **截图参数扩展（09-15）**：`space` 选编辑器目标取图范围——`viewport`（默认，2D 视口回退 3D）或 `window`（编辑器主窗口根视口，含 dock/工具栏与 2D 网格/选择框）；`region {x,y,width,height}` 裁剪（width/height 必须为正，矩形钳制到图像内，空交集报 `region_out_of_bounds`）与 `max_dimension`（64-4096，最长边缩小、不放大）对两个目标都生效（`target=game` 时在游戏进程内执行）；`annotate=true` 画编号红框并附 `elements` 数组（最终图像素）；`diff_against_last=true` 与上一次编辑器截图比较，返回 `diff`（`comparable` + `changed_ratio`，有变化附 `changed_bbox`），尺寸不同时按公共左上区域比较并附 `size_mismatch=true`/`current_size` 与 `baseline_size`，不可比较时 `comparable=false` 且 `reason` ∈ {`no_baseline`, `unsupported_format`, `baseline_too_large`}。输出图发生裁剪/缩放时附 `source_width`/`source_height`，结果 `width`/`height` 为最终图像尺寸。
  - **帧定时与条件抓拍（09-16，game 目标）**：`capture_game_viewport`（及 `capture_editor_viewport` 的 `target=game`）新增 `after_frames`（延迟 N 帧后抓拍）与 `when`（逐帧求值 GDScript 表达式、相对 `current_scene` 求条件，成立帧抓拍；解析失败报 `when_parse_error`、等待超时报 `when_timeout`）；另新增 `scale` 1-8 最近邻放大。处理顺序固定：`region` → `scale` → `max_dimension`。实测注意：条件成立帧与抓拍渲染帧相差约 1 帧，**取证场景条件宜留滞后余量**（如要求状态持续 2 帧再触发）。
  - **场景节点标注（09-16 新增）**：`capture_editor_viewport` 与 `capture_game_viewport` 新增 `annotate_nodes`（1-50 个节点路径字符串，非空；空数组或超限直接报错）与 `annotate_nodes_max`（1-50 可选自我限流，缺席=50，超限报错）。节点框为蓝色、编号自 1 独立计数（与 `annotate` 红色 UI 框不混排），结果附 `node_elements[{id,path,type,ok,position,size,visible,behind?,error?}]`（最终图像素，与输入一一对应；单项失败置 `ok:false` + `error` 而整体不失败：`node not found` / `unsupported node type` / `viewport incompatible`；3D 相机背后点仅标 `behind:true`；不可见/零面积跳过绘制但保留 `visible:false` 条目）与 200 总框预算超限时的 `node_truncated`。编辑器侧解析编辑场景路径（复用 `resolve_scene_node`），游戏侧解析绝对路径。VLM 标准链路：截图（`annotate_nodes` 选框）→ `hit_test_editor_point` 复核坐标（见[领域工具 A 组](../modules/tools_ops_a.md)）→ 点击/输入动作 → `diff_against_last` 复验。
  - **可视差分图与场景评审包（09-16 新增）**：`diff_image:true`（editor 目标专用，且须配 `diff_against_last:true`，否则报 `invalid parameter: diff_image requires diff_against_last`）在数值 `diff` 之外多返 `diff_image_data`（base64 PNG，在最终图拷贝上高亮 `changed_bbox`；无 `changed_bbox` 或不可比较时不返图；计入 PNG/JSON 上限，超限走既有 `capture_bytes_exceeded`/`response_too_large`）；`review_scene_visually`（Capture 类只读组合，一次返回 `editor_capture`/`game_capture`/`nodes`/`mapping` 四要素——编辑器图、游戏图、`get_scene_node_screen_rect` 节点表（`paths=annotate_nodes`，缺席时 `nodes={skipped:true}`）、`get_editor_viewport_geometry` 映射；各节失败隔离为按节 `error`，`include_editor/include_game=false` 跳过对应节、双 false 直接报错，未知参数拒绝；`node_viewport` 取 `auto`/`2d`/`3d`；`region/max_dimension/scale/annotate/annotate_nodes` 语义与截图工具一致，`timeout_ms` 仅约束游戏节；评审包内的编辑器截图同样刷新 `diff_against_last` 基线）。协议细节见[运行时通道](../modules/entry_runtime.md)。
  - **截图保留上限（09-14 起）**：`prune_capture_files` 按修改时间倒序只保留最近 20 个 `gda_capture*.png`——editor 侧在保存后清理 `user://godot_autopilot/captures/`，游戏侧 `op_capture` 同样清理 OS 缓存目录（`game_bridge.cpp`）。
  - **MCP image content 交付（09-13 起）**：经元工具 `call_tool` 直调三个截图工具（`capture_editor_viewport`/`capture_game_viewport`/`capture_display_screen`）时，`register_all.cpp` 经 `util::mcp_image_content.hpp` 的 `try_attach_image_content()` 把 PNG base64 转为 MCP image content 块随响应返回（多模态模型可直接看图）；文本 JSON 中该 `data` 替换为 `"<attached-as-image-content>"` 并加 `image_attached:true`，`format`/`width`/`height` 等其余字段保留。`batch_execute`/`code_execute` 内调用不附加 image 块，JSON 中仍是完整 base64；`format` 非 `"png"` 或无 data 时不转换。
  - `base64_encode` 为跨模块工具函数（display_ops 截图、runtime_ops 文件回读共用）。
- **错误模式**：视口不可用 → error_detail（"open a scene with a visible viewport first"）；PNG 编码空缓冲 → error_json。
- 出站链接：[工具注册表](../modules/tools_registry.md) · [运行时通道](../modules/entry_runtime.md)

## 与 AGENTS.md 对照结果

- **副作用工具（60 个 SIDE 宏声明）排除机制**：工具以 `GDA_TOOL_CLASS_SIDE(` 声明并经 `ISideEffect` 暴露 `side_effect`；L2 遍历解析器（`tests/runner/traversal.cpp`）仅匹配 `GDA_TOOL_CLASS(`，这类工具天然不入枚举（385 域中 325 个入枚举），入枚举者再经 `get_tool_detail` 的 `tool.side_effect` 非空兜底排除，不再硬编码清单。B 组占 **36 个**——display_tools 15（`show_display_dialog`、`speak_display_tts`、`stop_display_tts`、`set_display_clipboard`、`set_display_mouse_mode`、`warp_display_mouse` 与 `display_window_*` 9 个）、os_tools 8（`show_os_alert`、`create_os_process`、`execute_os_process`、`kill_os_process`、`open_os_path`、`move_os_file_to_trash`、`set_os_environment`、`write_file`）、theme_tools 5、game_tools 7（09-15 增 `click_game_ui_element`、09-16 增 `start_game_job`）、tilemap_tools 1（09-16 增 `fill_tilemap_rect`，side_effect=None 仅宏前缀排除）；其余 24 个在 A 组：editor 10（原 6 含 `build_csharp_assembly` + `editor_ui_actions` 3 + 09-16 增 `select_scene_tree_node`）/input_map 2/config 2/resource 4（`save_resource`/`copy_resource_file`/`move_resource_file`/`create_directory`）/script 2（`execute_script`/`create_script`）/input_click_ops 4。按 `side_effects()` 返回值分布：writes_file 15、writes_config 6、shows_alert 4、modifies_window 20、process 6、game_runtime 7、code_execute 1、None 1（`tests/README.md` 为准）。**注意 `display_` 前缀并非全排除**：只读的 `get_display_clipboard`、`get_display_mouse_position`、`get_display_screen_*`（5 个）、`get_display_tts_voices`、`get_display_window_rect`、`capture_display_screen` 等非副作用工具遍历会冒烟。
- **不一致点**：
  1. AGENTS.md「添加工具需在 `<category>_ops.hpp` 声明」与实现不符：`environment_ops.cpp`（7 工具）、`display_window_ops.cpp`（10 工具）无独立 hpp，handler 声明分别复用 `render_ops.hpp`、`display_ops.hpp`；工具类声明则统一落在对应 `<域>_tools.hpp`（`render_tools.hpp`/`display_tools.hpp`）。另 `runtime_game_ops.cpp`（12 工具）声明在 `runtime_ops.hpp`。
  2. `debugger_ops` 的 `get_debugger_*` 5 工具、`get_plugin_log` 与 `log_ops` 的 `get_game_log_entries` 覆盖四条不同日志来源（编辑器引擎日志 / 调试会话捕获 / 插件进程内 LogSystem / 游戏磁盘日志），AGENTS.md 未区分。
  3. AGENTS.md 称元工具"不经过 call_tool"：`batch_execute` 元工具本身直连注册 ✓，但其实现内部经 `dispatch::call_handler` 批调领域工具，路径上会与 `call_tool` 一致地命中 `ExportGuard` 等出口。
