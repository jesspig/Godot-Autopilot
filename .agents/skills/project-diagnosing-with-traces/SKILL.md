---
name: project-diagnosing-with-traces
description: 排查本项目运行期故障时使用：工具无响应/超时、编辑器卡死或崩溃、授权被拒、trace 与人类日志对不上、导入期 retryable、L2 失败回溯。要新增或改动埋点本身请看 project-instrumenting-traces。
---

# 用日志与 trace 排障

## 适用范围

- 用于：MCP 客户端调用不出结果、工具报错原因不明、L2 用例失败要定位步骤、编辑器疑似死锁或掉帧。
- 不适用：编译/链接错误（`project-building-gda`）；测试口径与前置（`project-running-tests`）；修改埋点实现（`project-instrumenting-traces`）。

## 前置条件

- 两份产物在同一会话下生成：人类日志 `user://godot_autopilot/logs/gda-*.log`，结构化事件 `user://godot_autopilot/traces/trace-*.jsonl`（各留最近 20 文件 / 50MB）。
- **不要靠猜拼 OS 绝对路径**：在编辑器插件的 MCP Log Dock 点 Open Logs / Open Traces 拿到真实目录；引擎内扫描用 `execute_script`（需 `code_execute` 授权），可抄 `tests/config/30_observability.json`、`29_trace_persistence.json` 里的 `DirAccess`+`FileAccess` 写法。
- 会话起点先看 `kind=snapshot` 事件：`gda_version`、Godot 版本、OS、编辑器模式、`port`、`desensitize`、`allow`、`session_nonce` 都在里面，能一次排除"版本/配置不对"这类假故障。

## 症状到查法的映射

- **调用完全无响应**：`kind=protocol_request` 有但同 `request_id` 没有 `protocol_response` → 响应侧接线或传输问题；只有 `tool_call` 无响应 → 主线程没 drain（队列积压），看 `system_status` 的 `queue_depth` 与 `kind=concurrency`、`kind=perf`（队列深度/拒绝数/锁等待）。
- **请求超时**：`kind=execution_state, state=timed_out`（看门狗默认 60s）→ 找同一 `trace_id` 里最后一条 `tool_call`，其 `duration_ms`/`queue_wait_ms` 区分"卡在排队"还是"卡在 handler 里"。
- **工具慢**：`tool_call` 的 `duration_ms > 2000`（`kSlowToolMs`）会以 Warning 落人类日志并带 `slow_tool=true`。
- **授权被拒**：trace `auth=` 字段 / 响应里 `authorization_required` + `enable`。`process`、`code_execute`、`game_runtime`、`user_tools` 四类才走授权门；`writes_file`/`writes_config` 只是风险标记，不会拦。
- **导入未就绪**：`editor_readiness` 返回 `retryable` 软错误属正常瞬态，应重试而不是改代码；响应顶层 `new_errors_since_last_call` 是一次性消费的水印，为空不代表没出过错。
- **崩溃/异常**：`kind=error` 带 `error_type`/`stack`；持久化自身的问题看 `kind=persist_health`（`dropped_log_lines`/`dropped_trace_lines`/`flush_failures`/`rotations`/`pruned_files`）。
- **参数看不出内容**：默认脱敏开启，`args_digest` + `args_truncated` 是预期形态；要看原文必须显式关脱敏（`GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize`），此时图片才会写 `traces/images/`。
- **L2 用例失败**：先按 `--file NN_name` 单跑取逐步骤输出，再按 `request_id` 去 trace 里对齐；多份同时失败先判环境污染（见 `project-running-tests`），不要先改代码。

## 规则与边界

- **时间线用 `seq` 与 `monotonic_ns` 排序**，`wall_start_ms` 只用于和其它时钟对齐；`thread`/`thread_id` 用来确认"这条是不是在主线程跑的"。
- **关联链路自下而上找**：`tool_call` → 其 `parent_span` → `call_tool` 的 span → `protocol_request` 的 `request_id`；跨线程断链通常是某处 `execute_sync` 忘了包 `ScopedTraceContext`。
- 日志类别只有 5 个（`System`/`Transport`/`Tools`/`Resources`/`Prompts`），检索时先按类别收窄；`filter_text` 同时匹配 message、detail、`trace_id`、`span_id`。
- **排障时改配置要复原**：关脱敏、`GODOT_AUTOPILOT_ALLOW=all`、改端口都只是临时诊断，收尾恢复默认（脱敏默认开、allow 默认空即拒绝）。
- 无头复现注意：`GDA_FORCE_HEADLESS=1` 与 cmdline 模式判定语义相反（它反向禁用 cmdline 模式），别用变量名推断行为。

## 常见错误

- 拿 `logs/` 里的行数和 `traces/` 里的行数对不齐就断定丢事件——两者是不同写入路径与不同游标，按 `trace_id` 对齐才有意义。
- 在脱敏开启的 trace 里搜索明文参数，搜不到就以为没埋点。
- 把 `retryable`、`new_errors_since_last_call` 这类瞬态/一次性信号当持久故障去"修"。
- 改了埋点想验证，却没重开会话：`session_id`/`session_nonce` 变了才是新会话，旧 jsonl 里没有你的新事件。

## 检查清单

- [ ] 结论能指到具体 `trace_id`/`request_id` 与事件序列，不是"看起来像"。
- [ ] 诊断期临时放开的安全/脱敏设置已复原。
- [ ] 若确认是缺陷，已补一条 L1 或 `tests/config/*.json` 回归用例（防同类问题再靠 trace 手查）。

## 协同与参考

- 事件与字段口径：[docs/wiki/modules/core.md](../../../docs/wiki/modules/core.md)、[docs/wiki/tool_base_design.md](../../../docs/wiki/tool_base_design.md)
