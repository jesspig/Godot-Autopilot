---
name: thread-security-gate
description: 改动 Godot API 调用、监听、高风险确认、脱敏、导出、路径或截断时守主线程与默认安全姿态。何时用：主线程 / CommandQueue / submit / submit_tracked / execute_sync、非环回、confirm、ExportGuard、desensitize、normalize_project_path、报错 main_thread_timeout。
---

# 主线程与安全门

所有 Godot API 经主线程执行，默认安全姿态不静默放宽。

## 步骤

1. 所有 Godot API 经 `queue.submit()`、`submit_tracked()` 或 `submit_tracked_bounded()` 提交，由插件 `_process()` 的 `drain()` 在主线程排空；`execute_sync()` 是无界等待且已无调用点，禁止新增调用。
2. 保持默认监听 `127.0.0.1:<插件分配的端口>/mcp`（首次启动在 20000–29999 随机并持久化）；非环回在绑定层直接拒绝启动，请求层另校验 Host。
3. 高风险工具（`destructive` / `process` / `code_execute` / `game_runtime`）保持默认拒绝，调用方须显式传 `confirm=true`；不存在全局 allow list。
4. 脱敏保持默认开启（`GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize` > 默认 true）；关闭仅作显式取证选择，截图与原始参数会落盘到本地存储目录。
5. 导出期间拒绝工具调用、MCP 资源与 MCP 提示词三类入口；停止与重载先停服再清队列，不产生悬垂任务。
6. 外部路径先经 `normalize_project_path` 校验；`res://` 与 `user://` 不混拼；截断结果带 `truncated` 类可检测字段。

## 边界

- 输入：触碰引擎或安全的修改；输出：线程路由说明加安全影响声明，无放宽时明确写出。
- 纯 C++ 逻辑可留工作线程；新增例外须代码加文档双说明。

## 非目标

- 不处理工具业务语义与命名；不替代验收清单外的运维决策。

## 验证

- 高风险工具可经 `get_tool_detail.side_effect` 识别；缺 `confirm` 调用返回 `confirmation_required` 结构化错误。
- 主线程停摆返回 `main_thread_timeout` 结构化错误，而非挂死。
- 按 `docs/wiki/security_contract.md` 验收清单逐项自检。

## 权威参考

- [安全与并发契约](../../../docs/wiki/security_contract.md)
- [工程约定](../../../docs/wiki/conventions.md)
