---
type: 模块文档
title: 核心模块
description: 命令队列、配置常量、统一埋点门面 monitor、性能采样 perf_sampler、日志与本地持久化、可重放 trace（schema v2）、脱敏策略、模式检测、资源缓存、脏状态跟踪、错误水印、导入就绪门控、服务器生命周期与插件配置
tags:
  - 模块
  - 核心层
  - 线程模型
timestamp: "2026-09-21T01:24:00+08:00"
resource: src/core/
---

# 核心模块（src/core/）

> 审计日期：2026-09-19（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-13 上午随资源 path 加载注册进 ResourceRegistry 同步；09-13 下午随收口批次同步 PluginConfig allow 键、授权门与 call_tool MCP 线程例外；09-13 17:50 随 B 组知识库审计同步——补正主线程排空点行号、移除已删除的 architecture.md 对照行；09-13 晚随 0.2.4 版知识库全量审计同步——补正 query_recent/query_from 消费方注释、SCENE/EDITOR 两级初始化描述、GDA_FORCE_HEADLESS 语义与 editor_readiness 消费方；09-14 随修复批次同步——PluginConfig 消费方增列 Allow game_runtime 复选框；09-16 随坐标换算批次同步——补 `editor_coords` 职责行与新增纯函数（此前职责表漏列该模块），基于当前工作树代码逐行核对（不依赖 git 历史）；09-18 随知识库一致性审计同步——`query_recent(50)` 消费方行号 470→527（以 `resource_handlers.cpp` 实测为准）、审计日期头补齐 09-16 改动的日期同步；09-19 随注释清理批同步——坐标换算引擎映射与缓存提示取舍入文档（源码整行注释删除）；09-20 代码-文档一致性审计——行号重核（`debugger_ops.cpp:683`→`:763`、`main.cpp:258-325`→`:291-357`、`main.cpp:46-54`→`:47-55`），其余复核一致；09-20 晚随可重放监控与双目录持久化批次同步——新增 `LogPersist`/`TraceRecorder`/`sanitize_policy` 三模块（cpp 9→12、头 12→15）、`LogSystem::log_detailed` 与 `LogEntry.detail`、PluginConfig `desensitize` 键、`_process` 先 flush 后 drain；09-20 深夜代码-文档一致性审计（本页二轮）——修正 `LogPersist` 消费方口径（`tool_invoke`/`dispatch` 为 `TraceRecorder` 事件记录 + `session_id` 回显，`enqueue_trace` 外部调用仅 `main.cpp` 的 `server_ready` 与 `init_session` 的 `session_start`）、脱敏开启时图片字段仍保留 `image_width`/`image_height`、`get_plugin_log` 增量分支过滤与响应字段差异、PluginConfig `save_*` 失败改记 `log_detailed`，并重核行号（`main.cpp:302-369`/`:49-57`、`resource_handlers.cpp:544`、`mcp_log_dock.cpp:241`、`server_context.cpp:95-103`）；09-21 随可重放监控与日志系统批次同步——新增 `monitor`/`monitor_env`/`perf_sampler` 三模块（cpp 12→15、头 15→18）、`TraceRecorder` 的 `TraceKind` 与 `TraceEvent` 新字段（jsonl schema v2）、`LogEntry` 关联字段与 `log_detailed` 6 参重载、`LogPersist` 健康计数与 `trace_dir`、`CommandQueue::stats()`，并登记 `GODOT_AUTOPILOT_PERF_INTERVAL_MS`/`GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS`）。
> 覆盖范围：`src/core/` 下 15 个 cpp + 18 个头 + `version.hpp.in` 模板（`CommandQueue`、`error_watermark`、`config.hpp` 为 header-only，实际 18 业务组 + 版本）。注意：`CommandQueue` 为 header-only（仅 `command_queue.hpp`，无对应 `.cpp`），`error_watermark.hpp` 同为 header-only，`config.hpp` 为纯常量表，`version.hpp.in` 经 `configure_file` 生成 `version.hpp`。

## 模块简介

`src/core/` 是插件的基础设施层：负责把 HTTP 线程（mcp-cpp-sdk 自研网络栈）的请求安全地桥接到 Godot 主线程、提供进程内日志与统一埋点门面（`monitor`/`monitor_env`/`perf_sampler`）、配置常量、运行时模式检测、资源缓存、场景脏状态跟踪，以及 MCP 服务器的生命周期管理。所有模块位于命名空间 `godot_autopilot`（`ResourceRegistry`/`SceneDirtyTracker` 使用 `godot_autopilot::resource_registry` / `godot_autopilot::scene_dirty_tracker` 子命名空间）。

## 职责表

| 模块 | 文件 | 职责 | 消费方 |
|---|---|---|---|
| `CommandQueue` | `command_queue.hpp`（header-only） | 跨线程任务队列：`submit()` 入队并返回 `std::future`，主线程 `drain()` 批量执行；09-21 起累计 `Stats`（submitted/executed/rejected_closed/rejected_full/dropped_on_close/lock_wait_ns/pending/capacity）经 `stats()` 暴露 | 所有工具/resource/prompt handler、`main.cpp`、`perf_sampler`（队列深度/拒绝数） |
| 配置常量 | `config.hpp` | 端口、超时、缓冲区上限等编译期常量（GDA_ 前缀） | `server_context.cpp`、`runtime/game_bridge.cpp` 等 |
| `ExportGuard` | `export_guard.cpp/hpp` | 导出期间置位全局原子标志，供工具分发判定"导出中" | `dispatch::export_blocked_result`、`_enter_tree` 注册 |
| `LogSystem` | `log_system.cpp/hpp` | 进程内环形日志（内存缓冲，含可选 `detail` 诊断行与 `trace_id`/`span_id` 关联字段），单例 | 全部模块、`McpLogDock`、log 类资源、`LogPersist`（增量落盘） |
| `LogPersist` | `log_persist.cpp/hpp`（09-20 新增） | 日志/trace 双目录持久化：`user://godot_autopilot/logs/gda-<stamp>.log`（人类可读全量含 detail）与 `traces/trace-<stamp>.jsonl`（结构化事件）+ `traces/images/`（仅脱敏关闭时图片落盘）；两目录各保留最近 20 文件 / 50MB；09-21 起暴露健康计数（`dropped_log_lines`/`dropped_trace_lines`/`bytes_written`/`flush_failures`/`rotations`/`pruned_files`）与 `health_summary()`/`trace_dir()`，溢出/写失败/rotate/prune 均发 `kind=persist_health` 事件 | `main.cpp`（`_enter_tree` 初始化、`_process`/`_exit_tree` flush）、`SpecTool::execute`（`session_id`/`store_trace_image`）、`tool_invoke`/`dispatch`（`TraceRecorder` 事件记录 + `session_id` 回显）、`McpLogDock`（Open Traces 打开 `trace_dir()`） |
| `TraceRecorder` | `trace_recorder.cpp/hpp`（09-20 新增） | 进程内 trace 事件环形缓冲（容量 20000）：`record`/`query_recent`/`query_by_trace`/`query_by_request`/`query_since`/`next_seq`；09-21 起新增 `enum class TraceKind`（15 值）与 `trace_kind_name()`，`TraceEvent` 追加 `kind/name/request_id/correlation_id/phase/state/monotonic_ns/thread_id/attrs/error_type/stack/bytes`（`to_json_line` 在原键序之后追加，jsonl schema v2），敏感键剥离改为大小写不敏感通用集合；纯函数含 id 生成、参数脱敏、`sanitize_text`、`monotonic_now_ns`/`current_thread_id`、FNV-1a 哈希、base64 解码、JSON 行格式化 | `monitor`（统一写入）、`SpecTool::execute`、`dispatch`、`tool_invoke`、`LogPersist` |
| `sanitize_policy` | `sanitize_policy.cpp/hpp`（09-20 新增，header 内含原子缓存） | 脱敏开关全局缓存：`enabled()`/`set_enabled()` 供热路径读取，`initialize()` 按 env > config > 默认 true 解析 | `main.cpp`（入口初始化）、`McpConfigDock`（复选框切换）、`SpecTool::execute`/`tool_invoke`/`LogPersist::store_trace_image` |
| `monitor`（统一埋点门面） | `monitor.hpp/cpp`（09-21 新增，纯 std，可被 MCP SDK worker 线程调用） | 统一埋点入口：`emit/tool_call/lifecycle/data_flow/ui_action/security/state/error_event/perf` 写入便捷函数、`begin_request/end_request/note_protocol_error/note_notification/note_client/note_transport` 协议侧钩子、`RequestRegistry`（request_id→trace/span 关联表，上限 4096、FIFO 淘汰）、`RequestScope`/`push_request`/`pop_request`/`current_request_id`、`Counters`/`counters()`、`sanitize_field`/`json_escape`/`build_attrs`、`request_id_text(RequestId)`、`persist_health`、`LockProbe`（锁等待超过阈值才记录，否则零开销） | `tool_spec`/`dispatch`/`tool_invoke`、`server_context`、`main.cpp`、`autopilot_tools`、`runtime_ops`、`resource_handlers`/`debugger_resources`、`mcp_log_dock`/`mcp_config_dock` |
| `monitor_env` | `monitor_env.cpp`（09-21 新增） | `environment_snapshot()`（**仅主线程**）——会话开始写 `kind=snapshot` 事件，含 gda_version/Godot 版本/OS 名与版本/处理器数/对象数/编辑器模式/场景路径与根节点/port/desensitize/show_time/allow/session_nonce | `main.cpp`（`_enter_tree`） |
| `perf_sampler` | `perf_sampler.hpp/cpp`（09-21 新增） | 主线程周期采样（默认 5s，env `GODOT_AUTOPILOT_PERF_INTERVAL_MS`）写 `kind=perf` 事件（帧时间均值/最大、FPS、近似 `cpu_frame_time_ms`、内存、对象/节点数、队列深度/拒绝数/锁等待、吞吐/错误率/平均延迟）；并做请求超时看门狗（默认 60s，env `GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS`，超时发 `kind=execution_state, state=timed_out`）；`set_queue(const CommandQueue*)` 由 `main.cpp` 注入以读队列统计 | `main.cpp`（`_enter_tree` 注入队列、`_process` 每帧 tick） |
| `ModeDetector` | `mode_detector.cpp/hpp` | 运行时模式检测（编辑器/游戏/未知） | `_enter_tree` 启动日志 |
| 坐标换算（`coords` 命名空间，纯函数） | `editor_coords.cpp/hpp` | 仿射/Rect/图像尺寸纯逻辑 + 画布空间→窗口客户区换算（09-16 新增 `viewport_point_to_window` / `viewport_rect_center_to_window`，`screen_transform×canvas` 复合，恒等原样返回；画布空间即 `Control::get_global_rect()`/`get_global_transform()` 所在空间，注入的 `InputEventMouseButton.position` 为窗口客户区坐标（引擎侧经 `get_final_transform()` 反变换）——`src/core/editor_coords.hpp:12-15`） | 游戏侧 `run_ui_click` 注入坐标换算（见[入口与运行时](entry_runtime.md)） |
| `ResourceRegistry` | `resource_registry.cpp/hpp` | 内存资源缓存（oid 键 + `name:` 前缀键），全局 mutex 保护 | 资源类工具 |
| `SceneDirtyTracker` | `scene_dirty_tracker.cpp/hpp` | 记录"当前编辑场景是否被修改"及根节点实例 ID | `GodotAutopilotPlugin::_get_unsaved_status` |
| `error_watermark` | `error_watermark.hpp`（header-only） | 错误水印计数器：累积待消费错误数，供 MCP 响应附 `new_errors_since_last_call` doorbell | `register_all.cpp`（响应后处理）、`runtime_ops.cpp`（游戏 runtime_error 计数） |
| `editor_readiness` | `editor_readiness.cpp/hpp` | 编辑器导入/扫描进行中检测与 retryable 软错误构造，reimport 类工具的门控 | `resource_ops`（reimport/save 类 handler） |
| `ServerContext` | `server_context.cpp/hpp` | MCP 服务器组装、端口解析、启动/停止/重启、工具/资源/prompt 注册 | `main.cpp` 入口 |
| `PluginConfig` | `plugin_config.cpp/hpp` | 插件自身配置持久化（`user://godot_autopilot/config.json`，当前含 port、show_time（Show timestamps 开关持久化，默认 true）、desensitize（脱敏开关持久化，默认 true）与 allow（能力授权列表，默认空=拒绝）） | `ServerContext` 端口解析（`load_port`）、`McpConfigDock` Apply（`save_port`）与 Allow code_execute 复选框（`load_allow`/`save_allow`）及 Desensitize data 复选框（`load_desensitize`/`save_desensitize`）、`McpLogDock` 时间前缀（`load_show_time`/`save_show_time`）、`authorization::capability_enabled`（经注册的 AllowProvider 每次调用读取）、`sanitize_policy::initialize`（环境变量未设置时） |
| 版本宏 | `version.hpp.in`（configure_file 模板，生成 `<build>/generated/version.hpp`） | 定义 `GDA_VERSION` 字符串宏，取自根 `VERSION` 文件单一来源 | `server_context.cpp`（MCP `server_info`）、`register_all.cpp`（`system_status.version`）、`log_persist.cpp`（trace `session_start` 头的 `gda_version`） |

## 关键接口清单

### CommandQueue（header-only）

- `template <typename Fn> auto submit(Fn&&) -> std::future<std::invoke_result_t<Fn>>` — 入队任务，返回 future；任务异常通过 `promise.set_exception` 传播
- `bool drain()` — 主线程调用；关闭或线程不匹配时返回 `false`，否则锁定交换批量任务后逐个执行；首次调用固定当前线程为主线程
- `void open()` / `void close()` / `bool is_closed()` — 管理队列生命周期；默认容量为 1024，关闭时拒绝并完成未执行任务的 future
- `execute_sync(Fn&&)` — 主线程直接执行，其他线程提交后等待 future
- `bool is_main_thread() const` — 与记录的线程 ID 比较
- `Stats stats() const`（09-21 新增）— 返回 `submitted`/`executed`/`rejected_closed`/`rejected_full`/`dropped_on_close`/`lock_wait_ns`/`pending`/`capacity`；`submit`/`drain`/`close` 计数并累计锁等待纳秒（供 `perf_sampler` 与 `system_status.queue_depth` 读取）
- 内部结构：`std::queue<std::unique_ptr<TaskBase>>` + `std::mutex` + 关闭状态 + 容量限制 + 主线程 ID
- 实例：`GodotAutopilotPlugin::s_queue`（静态成员），经静态 `queue()` 访问器暴露；`runtime_ops::set_editor_queue` 也持有同一指针

### config.hpp（全部常量，见文末常量表）

### ExportGuard（`godot::EditorExportPlugin` 子类）

- `static bool is_exporting()` — 查询导出标志
- `_export_begin(...)` 置 `g_exporting = true`；`_export_end()` 置 `false`（`std::atomic<bool>`，relaxed 语义）
- 在 `_enter_tree` 中 `add_export_plugin(export_guard_)` 注册；导出期间领域工具调用返回 `{"error":"editor is exporting; retry after export completes"}`

### LogSystem（单例）

- 枚举：`LogLevel { Debug, Info, Warning, Error }`；`LogCategory { System, Transport, Tools, Resources, Prompts }`
- `void log(LogLevel, LogCategory, const std::string&)` — 环形缓冲，上限 `MAX_ENTRIES = 10000`，超限 `pop_front`；分配递增 `serial`
- `void log_detailed(LogLevel, LogCategory, summary, detail, trace_id = {}, span_id = {})` — 带诊断 `detail` 的日志（`LogEntry.detail`，上限 `MAX_DETAIL_CHARS = 8192`，超出截断并附 `...[truncated]`），与 `log` 共用同一缓冲与 `serial` 序列；09-21 起扩展为 6 参重载（追加 `trace_id`/`span_id` 关联字段，旧 4 参签名委托至它），`LogEntry` 同步追加 `trace_id`/`span_id`
- `query(const Query&)` — 支持 `min_level` / `filter_text`（大小写不敏感；`message` 未命中时继续匹配 `detail`，09-21 起再匹配 `trace_id`/`span_id`） / `category` 过滤
- `query_recent(size_t limit)` — 取最近 N 条（`godot://log/recent` 资源经 `query_recent(50)` 消费，`resource_handlers.cpp:544`）
- `query_from(size_t start_index, size_t* next_index)` + `size_t next_index()` — 增量查询（`McpLogDock::poll_new_entries` 与 `get_plugin_log` 的 `since_index` 消费，`mcp_log_dock.cpp:241`、`debugger_ops.cpp:763`）
- `static LogSystem& instance()` — 局部静态单例
- **缓冲本体仅内存**（无回调、无 Godot 控制台直接输出；`McpLogDock` 经 `poll_new_entries`/`query_from` 轮询消费）；磁盘落盘由 `LogPersist` 增量拉取（游标 `last_log_serial_`，见下节）
- MCP 侧消费方：`get_plugin_log`（debugger_ops，09-13 下午新增）经 `query`/`query_from` 读取同一缓冲（级别/分类/子串/增量过滤，值拷贝快照），与 `McpLogDock` 并行消费互不影响；注意过滤口径差异——非增量分支走 `LogSystem::query`（`filter_text` 匹配 `message`∪`detail`），`since_index` 分支经 `plugin_log_entry_matches`（`debugger_ops.cpp:660`）只匹配 `message`，且响应条目仅含 `serial`/`timestamp`/`level`/`category`/`message` 五个字段（不含 `detail`）

### LogPersist（单例，09-20 新增）

可重放的本地日志/事件持久化，写盘统一在主线程 flush 中完成：

- `init_session()` — 生成会话 id（`new_trace_id()`）与 UTC 时间戳 `%Y%m%d_%H%M%S`；递归创建 `user://godot_autopilot/logs` 与 `.../traces`；先 `rotate_on_init()` 修剪两目录（含 `traces/images/`，按前缀/后缀匹配文件，保留最近 `kMaxFiles = 20` 个且总量 ≤ `kMaxTotalBytes = 50 MiB`，`should_prune` 判定，按 mtime 从旧到新删），再入队一条 `{"type":"session_start","session_id","gda_version","stamp"}` trace 头；09-21 起 `init_session` 重置日志/轨迹游标（`last_log_serial_`/`last_trace_seq_`），rotate/prune 经 `note_rotation`/`note_pruned` 累加健康计数并发 `kind=persist_health` 事件
- 增量拉取：`flush_on_main_thread()` 以 `last_log_serial_`（对 `LogSystem::next_index()`/`query_from`）与 `last_trace_seq_`（对 `TraceRecorder::next_seq()`/`query_since`）为游标，把新日志经 `format_human_line` 格式化为 `[wall_ms] [level] [category] summary | detail`（含 detail）追加到 `gda-<stamp>.log`，把 trace 事件经 `to_json_line` 追加到 `trace-<stamp>.jsonl`；两条写入缓冲上限均为 `kBufferCap = 20000`（互斥保护，超限丢最旧）；09-21 起 flush 用 `LockProbe`（阈值 10ms，超阈值才记录）测量互斥锁等待，缓冲溢出丢最旧时累加 `dropped_log_lines`/`dropped_trace_lines`
- 写失败降级：`FileAccess` 以 `READ_WRITE` 打开失败时回退 `WRITE`（前者不创建新文件），仍失败或 `store_string` 失败时 `write_failed_` 置位——仅首次记一条 System 错误日志，本会话后续 flush 丢弃缓冲、不再重试；09-21 起每次失败累加 `flush_failures` 并发 `kind=persist_health` 事件
- `store_trace_image(span_id, kind, base64_png)` — 仅脱敏关闭时落盘 `traces/images/trace-<session>-<span>-<kind>.png`，返回相对路径 `images/...` 作为 `image_ref`；脱敏开启直接返回空串（`image_ref` 为空，jsonl 内仍保留 `image_hash`/`image_bytes`/`image_width`/`image_height`）；写盘成功后累加 `bytes_written`
- 健康计数（09-21 新增）：`dropped_log_lines()`/`dropped_trace_lines()`/`bytes_written()`/`flush_failures()`/`rotations()`/`pruned_files()` 访问器 + `health_summary()`（汇总文本），随 `kind=persist_health` 事件一并落盘；`trace_dir()` 返回 `user://godot_autopilot/traces`（供 McpLogDock 「Open Traces」按钮使用）
- 静态工具函数：`level_name`/`category_name`、`format_human_line`、`session_stamp_from_ticks`、`should_prune`（L1 `log_persist_test` 覆盖 8 项）

### TraceRecorder（单例，09-20 新增）

工具调用事件的结构化记录（内存环形，`deque` + mutex，容量 `kCapacity = 20000`）：

- `record(TraceEvent)` — 分配单调递增 `seq`（从 1 起）后入队，超容 `pop_front` 并返回 seq；`query_recent(limit)` 取尾部、`query_by_trace(id)`/`query_by_request(request_id)` 过滤、`query_since(since_seq, next_seq)` 供 `LogPersist` 增量拉取；`next_seq()`/`size()` 查询，`clear_for_test()` 供 L1 重置
- `enum class TraceKind`（09-21 新增，15 值）：`tool_call`/`protocol_request`/`protocol_response`/`protocol_error`/`protocol_notification`/`lifecycle`/`data_flow`/`execution_state`/`concurrency`/`perf`/`snapshot`/`error`/`persist_health`/`ui_action`/`security`，配 `trace_kind_name()` 取名字符串
- `TraceEvent` 字段：旧字段 `seq/trace_id/span_id/parent_span/session_id/tool/category/flags/side_effect/depth/thread/queue_wait_ms/duration_ms/wall_start_ms/wall_end_ms/auth/ok/error_code/args_digest/args_truncated/result_size/image_ref/image_bytes/image_hash/image_width/image_height`；09-21 追加 `kind/name/request_id/correlation_id/phase/state/monotonic_ns/thread_id/attrs/error_type/stack/bytes`（`to_json_line` 在原键序之后追加这些键，即 jsonl schema v2，旧键与旧序保留）
- 纯函数：`new_trace_id()`/`new_span_id()`（`tr_*`/`sp_*` 前缀，毫秒时间戳 + 原子计数器）、`monotonic_now_ns()`（单调时钟纳秒）、`current_thread_id()`、`sanitize_args(dump, desensitize)`、`sanitize_text(text, desensitize)`（脱敏时把敏感键字符串值替换为 `<stripped len=N>`，上限 `kDesensitizedMaxChars = 4000` 字符；关闭时上限 `kRawMaxChars = 64000`，超限置 `truncated`；敏感键为大小写不敏感的通用集合——data/base64/script_content/token/secret/password/passwd/key/api_key/apikey/authorization/credential/cookie/session_token 及 `*_token`/`_key`/`_secret`/`_password` 后缀）、`fnv1a_hex`（图片哈希）、`decode_base64`、`to_json_line`（固定字段序 + JSON 转义）、`wall_now_ms`
- L1 `trace_recorder_test` 覆盖 18 项（id/seq/查询/脱敏/截断/哈希/解码/JSON 行；09-21 增补 schema v2 新字段与 `TraceKind` 用例）

### sanitize_policy（命名空间函数，09-20 新增）

- `bool enabled()` / `void set_enabled(bool)` — 进程内原子缓存，热路径（`SpecTool::execute`/`tool_invoke`/`LogPersist::store_trace_image`）直接读取
- `void initialize()` — 解析优先级：`GODOT_AUTOPILOT_DESENSITIZE` 环境变量（`0`/`false`/`off` 判为关闭，其余非空值判为开启）> `PluginConfig::load_desensitize()`（`desensitize` 键，缺省 true）> 默认 `true`；仅在 `_enter_tree` 调用一次，之后由 MCP Config 面板的 "Desensitize data" 复选框实时改写并持久化

### monitor（统一埋点门面，09-21 新增）

纯 std 的埋点门面，可被 MCP SDK worker 线程安全调用（不含 Godot API）：

- 写入便捷函数：`tool_call`/`lifecycle`/`data_flow`/`ui_action`/`security`/`state`/`error_event`/`perf` 与底层 `emit`——统一构造 `TraceEvent` 写入 `TraceRecorder`，同时按需追加 `LogSystem::log_detailed`（人类日志携带 `trace_id`/`span_id`）
- 协议侧钩子：`begin_request`/`end_request`（起止协议请求并回填 `duration_ms`）、`note_protocol_error`/`note_notification`/`note_client`/`note_transport`
- 关联表：`RequestRegistry`（`request_id` → `trace_id`/`span_id` 关联，容量上限 4096、FIFO 淘汰）；`RequestScope`（RAII）与 `push_request`/`pop_request`/`current_request_id` 维护当前线程 request 上下文
- 辅助：`Counters`/`counters()`（进程内埋点计数）、`sanitize_field`/`json_escape`/`build_attrs`（构造 `attrs` JSON 并做字段脱敏）、`request_id_text(RequestId)`（`RequestId` → 文本，供协议侧与工具处理器两侧一致关联）、`persist_health`（写 `kind=persist_health` 事件）、`LockProbe`（RAII 计时，锁等待超过阈值才记录）
- 依赖方向：`monitor` 仅依赖纯 std 与 `trace_recorder`/`log_system`；`tool_spec`/`dispatch`/`tool_invoke`/`server_context`/`main.cpp`/`autopilot_tools`/`runtime_ops`/资源与 UI handler 均经它写事件

### monitor_env（`environment_snapshot()`，09-21 新增）

- `environment_snapshot()`（**仅主线程**）— 会话开始采集并写一条 `kind=snapshot` 事件：`gda_version`、Godot 版本、OS 名与版本、处理器数、对象数、编辑器模式、场景路径与根节点、`port`、`desensitize`、`show_time`、`allow`、`session_nonce`；由 `main.cpp` 的 `_enter_tree` 在 `init_session()` 之后调用

### perf_sampler（`perf_sampler`，09-21 新增）

- `set_queue(const CommandQueue*)` — 由 `main.cpp` 注入队列，用于读 `CommandQueue::stats()`（队列深度/拒绝数/锁等待）
- `tick(delta)` — 主线程每帧调用，按周期（默认 5s，env `GODOT_AUTOPILOT_PERF_INTERVAL_MS`）采合并写一条 `kind=perf` 事件：帧时间均值/最大、FPS、近似 `cpu_frame_time_ms`、内存、对象/节点数、队列深度/拒绝数/锁等待、吞吐/错误率/平均延迟
- 请求超时看门狗：对已 `begin_request` 但超时（默认 60s，env `GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS`）未 `end_request` 的请求发 `kind=execution_state, state=timed_out`

### ModeDetector

- `static RuntimeMode detect()` — `Engine::get_singleton()` 为 null → `Unknown`；否则 `is_editor_hint()` 为真 → `Editor`，反之为 `Game`
- `static bool is_editor()` — `detect() == Editor`

### ResourceRegistry（命名空间函数，非类）

- `register_resource(res, name)` — 以 `instance_id` 为键，另加 `"name:" + name` 键（`name` 为空则只登记 oid 键）
- `lookup_memory(name)` — 先查 `name:` 键；若参数为纯数字再回退查 oid 键；未命中返回空 `Ref`
- `erase_oid(object_id)`
- 线程安全：`std::unordered_map<std::string, godot::Ref<godot::Resource>>` + 全局 `std::mutex`
- **path 加载自动注册（09-13 起）**：`resource_ops::resolve_resource` 经 `path` 加载磁盘文件的实例在返回前调 `register_resource(res, path)`——该实例被注册表持有，跨调用存活，后续可用 `object_id`/`name:`（含原 `res://` 路径字符串）再次定位；这也是"加载后修改、再保存/查询"能在多次工具调用间保持同一实例的原因。

### SceneDirtyTracker（命名空间函数，非类）

- `mark_scene_modified()` — 读取 `EditorInterface::get_edited_scene_root()`，无根节点则不置脏；记录根实例 ID
- `clear_scene_modified()`、`bool is_current_scene_dirty()` — 脏判定要求"有根节点且实例 ID 与记录一致"
- 消费方：`_get_unsaved_status`（决定编辑器"未保存"标记）

### error_watermark（header-only，命名空间 `godot_autopilot::error_watermark`，08-24 新增）

错误水印 doorbell：AI 客户端无需轮询即可感知"上次调用之后发生了多少个新错误"。

- `record_error(int64_t count = 1)` — 累积待消费错误计数（mutex 保护）
- `int64_t consume_new_errors()` — 取走并清零累积值（一次性消费语义）
- `count_response_errors(const mcp::JsonValue&)` — 统计一个响应对象中的错误数：顶层 `error` 字符串计 1；`results` 数组内逐项 `error` 字符串各计 1（覆盖 batch_execute 的子结果）
- 接线点：编辑器侧在 `register_all.cpp` RegisterTool 回调内对每次工具响应调用 `count_response_errors` 并累入水印，随后把 `consume_new_errors()` 结果作为顶层字段 `new_errors_since_last_call` 附到响应上（含 0 值）；游戏侧 `runtime_ops.cpp` 收到 runtime_error 增量时 `record_error()`
- 同一 `call_tool` 回调（09-13 起）还承担截图响应后处理：经 `util::try_attach_image_content`（`util/mcp_image_content.hpp`）把 `capture_editor_viewport`/`capture_game_viewport`/`capture_display_screen` 的 PNG base64 转为 MCP image content 块，文本 JSON 的 `data` 替换为 `"<attached-as-image-content>"` 并加 `image_attached:true`；`batch_execute`/`code_execute` 内的调用不附加 image 块

### editor_readiness（命名空间函数，非类，08-24 新增）

导入/扫描就绪门控，避免 reimport 类调用撞上进行中的文件系统扫描：

- `bool is_import_in_progress()` — 经 `EditorInterface::get_resource_filesystem()` 查 `is_scanning()` 与 `is_importing()`（后者以 `has_method` 探测，兼容引擎版本差异）；接口不可用时返回 false
- `mcp::JsonValue busy_error()` — 构造软错误 `{"error": "editor is currently importing/scanning resources; retry shortly", "retryable": true, "retry_after_ms": 500}`
- 消费方：`resource_ops`（reimport 双入口与 uid 重建）和 `text_ops`（write_file 命中已导入文件）的 reimport 路径——导入中且有实际工作量时返回 busy_error，scan 场景幂等跳过

### ServerContext

- 构造：持有 `CommandQueue&`，创建 `ToolCatalog` 与 `Bm25Index`，`resolve_port()`/`resolve_host()` 解析端口与主机并写 Transport 日志；默认环回绑定 `127.0.0.1`，`ServerContext::start()` 拒绝非环回地址
- `bool start()` — 依次：`StreamableHttpServerTransport`（`host = resolve_host()` 默认 `127.0.0.1`、`port`、`endpoint = "/mcp"`、`stateless = true`、`enable_legacy_sse = false`）→ `mcp::McpServer::Create` → `register_tools()` → `transport_->Start()`；成功后回写 `port_ = http_opts.port`；SDK 自身日志默认关闭（`MCP_LOG_LEVEL` 未设置时为 Off）
- `void stop()` — 幂等关闭 `server_`/`transport_`，清空 dispatch handler 与 active registry；析构函数对非空资源兜底调用
- `bool restart(uint16_t port)` — `stop()` → 更新 `port_` → `start()`；供配置面板运行时改端口（配置面板 Apply 后立即生效，无需重启编辑器）
- `int get_port()` / `bool is_running()` / `const std::string& last_error()`
- MCP 服务器标识：`mcp::Implementation{"godot-autopilot", GDA_VERSION}`（宏经 `configure_file` 由根 `VERSION` 文件生成，见 `build.md` "版本号单一来源"）
- 缓存提示（`server_context.cpp:95-103`）：MCP 2026-07-28 规范要求六类 cacheable 结果携带缓存提示，协商到 2026 era 的客户端会按必填字段校验；`ttlMs=0` 表示"立即过期"，仅满足字段声明，客户端仍每次重新拉取，行为与未声明时一致
- 生命周期回调（全部写 Transport 类别日志）：`on_method_called`（Debug）、`on_client_connected` / `on_initialized` / `on_transport_close`（Info）、`on_protocol_error` / `on_transport_error`（Error）
- 诊断日志增强：初始化 / 注册工具 / 启动传输 / 停止 / 重启等关键流程均补充诊断日志；`start()` 启动失败不再硬编码 "unknown exception"，改为输出捕获到的真实异常类型，便于排查；09-20 起非环回拒绝、协议/传输错误、启动失败与 `restart` 路径另发 `log_detailed`（`endpoint=host:port` + `msg=...`），把失败与端点关联
- `register_tools()` 注册四类：工具、资源、prompt、调试器专用资源/prompt

### PluginConfig（命名空间静态方法，非类实例）

- `int load_port()` — 读 `user://godot_autopilot/config.json` 的 `port` 键；文件不存在/解析失败/非整数时返回 `-1`（表示未配置）
- `bool save_port(int port)` — 写回 `{"port": N}`（先 `DirAccess::make_dir_recursive_absolute` 建目录，复用 `save_config_value` 合并写回保留其他键）；失败经 `save_config_value` 记 `log_detailed`（System 类别，detail 含 `path=`/`reason=`）并返回 false
- `bool load_show_time()` — 读 `show_time` 键（`user://godot_autopilot/config.json` 的 `show_time`）；文件不存在/解析失败/非布尔时返回 `true`（默认开启，与配置面板 Show timestamps 开关一致）
- `bool save_show_time(bool show)` — 写回 `{"show_time": bool}`（同经 `save_config_value` 合并写回）；失败经 `save_config_value` 记 `log_detailed`（System 类别，detail 含 `path=`/`reason=`）并返回 false
- `bool load_desensitize()` — 读 `desensitize` 键（09-20 新增）；文件不存在/解析失败/非布尔时返回 `true`（脱敏默认开启）
- `bool save_desensitize(bool value)` — 写回 `{"desensitize": bool}`（同经 `save_config_value` 合并写回）；失败经 `save_config_value` 记 `log_detailed`（System 类别，detail 含 `path=`/`reason=`）并返回 false
- `std::string load_allow()` — 读 `allow` 键（逗号分隔能力列表）；文件不存在/解析失败/非字符串时返回空串
- `bool save_allow(const std::string &allow)` — 写回 `{"allow": string}`（同经 `save_config_value` 合并写回）
- **AllowProvider 桥接（09-13 下午起）**：`plugin_config.cpp` 的静态对象在构造时调用 `authorization::set_allow_provider(&PluginConfig::load_allow)`，使 `capability_enabled` 在无 `GODOT_AUTOPILOT_ALLOW` 环境变量时改读配置——每次调用实时读取，故配置改动下一次工具调用即生效
- 消费方：`ServerContext::resolve_port()`（启动时 `load_port`）、`McpConfigDock::_on_apply_port()`（Apply 成功后 `save_port`）与 "Allow code_execute" / "Allow game_runtime" 复选框（`load_allow`/`save_allow`）及 "Desensitize data" 复选框（`load_desensitize`/`save_desensitize`，勾选同时经 `sanitize_policy::set_enabled` 立即生效）、`McpLogDock` 时间前缀开关（`load_show_time`/`save_show_time`，配置面板持久化）、`authorization`（能力门的 AllowProvider）、`sanitize_policy::initialize`（无 `GODOT_AUTOPILOT_DESENSITIZE` 时读取 `desensitize`）

## 线程模型

```mermaid
flowchart LR
    Client[MCP Client] -->|POST /mcp 端口 9527| HTTP[SDK HTTP 线程]
    HTTP -->|queue.submit() 返回 future| Q[CommandQueue 互斥队列]
    Q -->|_process 每帧 drain| Main[Godot 主线程]
    Main --> API[Godot API / 场景 / 引擎]
    HTTP -.等待 future.get().|H 调用方阻塞等待结果
```

- HTTP 线程绝不直接触碰 Godot API：领域工具经 `dispatch::call_handler` 判断——非主线程时 `submit(...).get()` 同步等待（`dispatch.cpp`）；资源/prompt 注册同样走 `submit`
- 唯一编排层例外（09-13 下午起）：`call_tool` 元工具回调在 MCP 线程执行（等待运行时响应/截图定型不再经 `execute_sync` 占用主线程），编排逻辑自身不触碰 Godot API、领域工具 handler 仍由 dispatch 路由回主线程；其余 6 个元工具仍走 `execute_sync`
- 排空点唯一：`GodotAutopilotPlugin::_process(double)` 先 `perf_sampler::tick(delta)`（09-21 起按周期采样 `kind=perf` 并做请求超时看门狗）再 `LogPersist::instance().flush_on_main_thread()`（把上一帧以来累积的日志/trace 增量写盘）再 `s_queue.drain()`，随后轮询 `McpLogDock`；`LogPersist` 的缓冲与游标有互斥保护（入队可发生在任意线程），写盘固定主线程
- `drain()` 首次执行时把当前线程记为"主线程"，此后 `is_main_thread()` 据此判定

## 生命周期（插件 ↔ ServerContext）

1. `GDExtensionEntryPoint`（`main.cpp:302-369`）：`MODULE_INITIALIZATION_LEVEL_SCENE` 在非编辑器进程调用 `game_bridge::register_listener()`（桥接类注册与消息捕获）；`MODULE_INITIALIZATION_LEVEL_EDITOR` 注册 debugger 类与 4 个类后 `EditorPlugins::add_by_type<GodotAutopilotPlugin>()`
2. `_enter_tree`：`monitor::lifecycle("plugin_enter_tree")` → `perf_sampler::set_queue(&queue())` → `s_queue.open()` → `sanitize_policy::initialize()` + `LogPersist::init_session()`（09-21 起重置日志/轨迹游标）→ `monitor::environment_snapshot()`（写 `kind=snapshot`）→ 设置 editor queue → 记 `tools_singleton_registered` lifecycle + Log Dock/输出捕获/调试器插件 → `new ServerContext(queue)` 并 `start()`（成功发 `server_ready` lifecycle）→ Config Dock → `add_export_plugin(ExportGuard)` → 记 `plugin_ready`；`gda_cmdline_mode()` 为真时跳过 UI/服务器（`GDA_FORCE_HEADLESS=1` 反向禁用 cmdline 模式，语义与变量名相反）
3. `_process`：每帧 `perf_sampler::tick(delta)` + `LogPersist::flush_on_main_thread()` + `s_queue.drain()` + Dock 轮询
4. `_exit_tree`：`ServerContext::stop()` + delete → 注销各组件，末尾再 `flush_on_main_thread()` 收尾（`main.cpp:298`）

安全与停止/重载的可执行约束见 [T0 安全边界与并发契约](../security_contract.md)。当前队列关闭会拒绝新任务并为未执行任务完成异常 future；运行时 pending 请求在队列清空时统一取消。

## 环境变量与端口

- `GODOT_AUTOPILOT_PORT`：`resolve_port()` 用 `std::getenv` 读取、`std::atoi` 转换（**无格式校验**）
- 端口解析优先级：**环境变量 > `PluginConfig::load_port()`（user:// 持久化值，需 > 0）> `GDA_DEFAULT_PORT`（9527）**——环境变量优先保证测试/CI 场景不受面板配置影响
- `GODOT_AUTOPILOT_HOST`：`resolve_host()` 用 `std::getenv` 读取，非空即生效；默认环回绑定 `127.0.0.1`，`ServerContext::start()` 拒绝非环回地址
- 运行时改端口：`ServerContext::restart(uint16_t)`（配置面板 Apply 触发，成功后经 `PluginConfig::save_port` 持久化）
- `GDA_FORCE_HEADLESS`：值为 `1` 时**禁用** cmdline 模式（强制建 UI/启服务器，语义与变量名相反；`main.cpp:49-57` 读取）
- `GODOT_AUTOPILOT_DESENSITIZE`（09-20 新增）：脱敏开关的最高优先级来源，`0`/`false`/`off` 关闭、其余非空值开启；未设置时读 `user://godot_autopilot/config.json` 的 `desensitize` 键，再默认开启（`sanitize_policy::initialize`，仅在入口读取一次）
- `GODOT_AUTOPILOT_PERF_INTERVAL_MS`（09-21 新增）：`perf_sampler` 采样周期毫秒数，默认 `5000`
- `GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS`（09-21 新增）：`perf_sampler` 请求超时看门狗阈值毫秒数，默认 `60000`

监听、可信客户端模型、路径和响应大小边界见 [T0 安全边界与并发契约](../security_contract.md)。

## config.hpp 常量全表

| 常量 | 值 | 类型 | 主要用途 |
|---|---|---|---|
| `GDA_DEFAULT_PORT` | `9527` | `int` | MCP HTTP 默认端口 |
| `GDA_HEALTHY_ACTIVITY_THRESHOLD_MS` | `3000` | `int64_t` | `get_game_status` 健康判定阈值（`game_bridge.cpp`） |
| `GDA_DEFAULT_TIMEOUT_MS` | `5000` | `int64_t` | 默认操作超时 |
| `GDA_MAX_TIMEOUT_MS` | `30000` | `int64_t` | 超时上限（与 `code_execute` 工具 30s 上限呼应） |
| `GDA_ERROR_BUFFER_MAX` | `200` | `size_t` | 错误缓冲上限（`game_bridge.cpp`） |
| `GDA_OUTPUT_BUFFER_MAX` | `500` | `size_t` | 输出缓冲上限（`game_bridge.cpp`） |
| `GDA_EVAL_TRUNCATE_BYTES` | `8192` | `size_t` | eval 输出截断字节数（`game_bridge.cpp`） |
| `GDA_CAPTURE_MAX_DIMENSION` | `4096` | `int64_t` | 截图单边像素上限 |
| `GDA_CAPTURE_MAX_PNG_BYTES` | `8 MiB` | `size_t` | 截图 PNG 大小上限 |
| `GDA_VARIANT_MAX_STRING_BYTES` | `64 KiB` | `size_t` | Variant 字符串单项上限 |
| `GDA_VARIANT_MAX_ARRAY_ELEMENTS` | `10000` | `size_t` | Variant 数组/PackedArray 元素上限 |
| `GDA_MAX_JSON_RESPONSE_BYTES` | `4 MiB` | `size_t` | JSON 响应大小上限 |
| `GDA_SCAN_MAX_FILES` | `10000` | `size_t` | 扫描文件数上限 |
| `GDA_SCAN_MAX_FILE_BYTES` | `2 MiB` | `size_t` | 扫描单文件大小上限 |
| `GDA_SCAN_MAX_TOTAL_BYTES` | `32 MiB` | `size_t` | 扫描累计字节上限 |
| `GDA_SCAN_MAX_DEPTH` | `64` | `size_t` | 扫描目录深度上限 |

## 与现有文档的不一致点

| 文档 | 声称 | 代码事实 | 判定 |
|---|---|---|---|
| AGENTS.md（构建段） | 暗示每个模块有成对的 `.cpp/.hpp` 参与 `add_library()` | `command_queue.hpp` 无对应 `.cpp`，header-only，不进 CMake 源列表 | 文档未明说，审计时需注意 |
| README.md / README.en.md（前提） | "Godot 4.7+"（`README.md:5,53` / `README.en.md:5,53`） | Example 项目为 4.7（`Example/project.godot`），AGENTS.md 亦写 4.7 | 一致 ✓ |
| README.md / README.en.md（安装） | "服务端自动启动……无需启用开关"（`README.md:65` / `README.en.md:65`） | cmdline/`GDA_FORCE_HEADLESS` 模式下 UI 与服务器被禁用（`main.cpp`） | 存在例外，描述不完整 |
| AGENTS.md（架构/端口段） | 端口 9527、`/mcp`、`GODOT_AUTOPILOT_PORT` 覆盖、日志类别五枚举 | 全部与代码一致 | 一致 ✓ |
| README.md / README.en.md（安全段） | 回环监听 + 端口 9527 + `/mcp`（`README.md:65,74` / `README.en.md:65,74`） | 与 `server_context.cpp` 一致 | 一致 ✓ |
| AGENTS.md | 日志类别 "仅此几个：System、Transport、Tools、Resources、Prompts" | `LogCategory` 枚举完全相同 | 一致 ✓ |

## 相关页面

- 入口：由 [../index.md](../index.md) 链接
- 模块总览：[../overview.md](../overview.md)
- 测试覆盖：[../tests.md](../tests.md)
