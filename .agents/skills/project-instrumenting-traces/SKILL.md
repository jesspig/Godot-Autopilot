---
name: project-instrumenting-traces
description: 给本项目新增或调整可观测性埋点时使用：monitor 门面事件、trace kind、请求关联与 span、脱敏字段、日志/轨迹双目录持久化与保留策略。只读排障请看 project-diagnosing-with-traces。
---

# 写入埋点与持久化

## 适用范围

- 用于：新模块要出诊断事件、既有流程缺 lifecycle/data_flow/security 埋点、协议请求关联不上、要调整脱敏或持久化保留策略。
- 不适用：读 trace 定位故障（`project-diagnosing-with-traces`）；Godot 引擎自身的 `print`/Profiler。

## 前置条件

- 认清分层：`src/core/monitor.{hpp,cpp}` 是**唯一埋点门面**（纯 std 实现，因此可被 mcp-cpp-sdk 的 worker 线程调用）；`TraceRecorder` 是环缓冲（不落盘即丢），`LogPersist` 在主线程 `_process` 增量刷写。
- 人类日志与结构化事件是两条流：`user://godot_autopilot/logs/gda-*.log`（含 detail）与 `user://godot_autopilot/traces/trace-*.jsonl`，各保留最近 20 文件 / 50MB。

## 流程

1. 选事件类型：`tool_call` / `lifecycle` / `data_flow` / `execution_state` / `concurrency` / `perf` / `snapshot` / `error` / `persist_health` / `ui_action` / `security` / `protocol_request` / `protocol_response` / `protocol_error` / `protocol_notification`（`enum class TraceKind`，`trace_kind_name()` 给 jsonl 里的字符串）。
2. 用对应便捷函数（`monitor::lifecycle(name, attrs)`、`monitor::data_flow(name, bytes, attrs)`、`monitor::ui_action`、`monitor::security`、`monitor::state(name, state, ok)`、`monitor::error_event(code, type, ...)`、`monitor::perf`），只有确属新语义才手工 `monitor::emit(TraceEvent{...})`。
3. 属性串一律经 `monitor::build_attrs({{k,v},...})`，自由文本经 `monitor::sanitize_field(text)`；不要手拼 JSON 字符串。
4. 要跨线程/跨层串同一次请求：入口用 `monitor::begin_request(...)` + `RequestScope`（或 `push_request`/`pop_request`），响应侧配套 `end_request`；内部工具链经 `tools::capture_trace_context()` + `ScopedTraceContext` 传递，`RequestRegistry` 维持 `request_id → trace/span` 关联。
5. 新增 kind 时同步四处：`TraceKind` 枚举 + `trace_kind_name()` + 依赖它的 L1 用例 + wiki 的枚举清单与 L2 断言，否则新事件写出来是空名。

## 规则与边界

- **埋点不得改变行为**：埋点代码里不准调任何 Godot API（`monitor` 故意保持纯 std 才有 worker 线程可用性）；需要引擎侧数值时，由主线程组件（`monitor_env::environment_snapshot()`、`perf_sampler`）取值后传进来。这两个都只能在主线程调用，且**不进 L1 业务源**（`perf_sampler` 依赖引擎 tick，`monitor_env` 仅主线程，`sanitize_policy` 只在 `main.cpp` 调 `initialize()`）。
- **脱敏默认开启且优先级固定**：环境变量 `GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize` > 默认 `true`，改开关即时生效。默认态下参数走 `sanitize_args`（脱敏 4000 字符 / 原始 64000 字符上限），敏感键按大小写不敏感的通用集合剥离（`data`/`base64`/`script_content`/`token`/`secret`/`password`/`key`/`api_key`/`authorization`/`credential`/`cookie`/`session_token` 及 `*_token`/`_key`/`_secret`/`_password` 后缀）。
- **图片**：只有脱敏关闭时才把图片写 `traces/images/`；jsonl 里永远只记 `image_ref`/`image_bytes`/`image_hash`/`image_width`/`image_height`。不要往 jsonl 里塞 base64。
- **jsonl 键序是契约**：`to_json_line` 先输出原 schema v1 键（`seq`/`trace_id`/`span_id`/`parent_span`/`session_id`/`tool`/`category`/`flags`/`side_effect`/`depth`/`thread`/`queue_wait_ms`/`duration_ms`/`wall_start_ms`/`wall_end_ms`/`auth`/`ok`/`error_code`/`args_digest`/`args_truncated`/`result_size`/`image_*`），其后追加 schema v2 键（`kind`/`name`/`request_id`/`correlation_id`/`phase`/`state`/`monotonic_ns`/`thread_id`/`attrs`/`error_type`/`stack`/`bytes`）。旧键保留、位置不动，消费方按前缀解析。
- **追加写文件的老坑**：`FileAccess::READ_WRITE` 打开不存在的文件会失败且不创建，持久化/追加路径必须回退 `WRITE`（`log_persist.cpp` 与 `text_ops.cpp` 都按这个模式）。
- 缓冲溢出、写失败、rotate、prune 都要发 `kind=persist_health` 事件并带计数（`dropped_log_lines`/`dropped_trace_lines`/`bytes_written`/`flush_failures`/`rotations`/`pruned_files`），写失败只降级告警一次，不要每行重试刷屏。
- 锁等待用 `monitor::LockProbe`（超过阈值才记，flush 阈值 10ms）；队列指标经 `CommandQueue::stats()`，别自己加计数器。
- 慢工具阈值 `kSlowToolMs = 2000ms`；`error_code` 取 `structured_error.code`，无该字段的失败一律记 `error`。请求超时看门狗默认 60s（`GODOT_AUTOPILOT_REQUEST_TIMEOUT_MS`）、性能采样默认 5s（`GODOT_AUTOPILOT_PERF_INTERVAL_MS`）。

## 常见错误

- 在 HTTP worker 线程直接调 `LogSystem`/`TraceRecorder` 之外的引擎接口，或反过来把纯 std 的门面当成"必须在主线程"而绕一大圈。
- 手工拼 `attrs` JSON，遇到引号/换行就把整行 jsonl 打废。
- 关掉脱敏做实验，把带原始参数或图片的 trace 目录当成可提交的测试夹具提交上去。
- 只加 `begin_request` 没加 `end_request`（响应侧要经 `opts.outgoing_filters` 捕获出站响应，SDK 的 `on_response` 不覆盖我方回复）。

## 检查清单

- [ ] 新事件在 jsonl 里 `kind` 有名、`request_id`/`trace_id`/`span_id` 能串起来。
- [ ] 默认（脱敏开）不泄漏参数原文、图片二进制、密钥类字段。
- [ ] 埋点异常不影响主流程返回，失败路径写日志/告警但不抛回调用方。
- [ ] 相关 L1 用例与 wiki 的枚举/字段清单已同步。

## 协同与参考

- [docs/wiki/modules/core.md](../../../docs/wiki/modules/core.md) · [docs/wiki/tool_base_design.md](../../../docs/wiki/tool_base_design.md) · [docs/wiki/security_contract.md](../../../docs/wiki/security_contract.md)
