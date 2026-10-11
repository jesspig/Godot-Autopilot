---
name: thread-security-gate
description: 触碰 Godot API 或改监听、授权、脱敏、导出、路径、截断时守主线程与默认安全姿态；触发词：主线程、CommandQueue、submit、非环回、allow、desensitize、exporting、normalize_project_path、main_thread_timeout。
---

# 主线程与安全门

所有 Godot API 经主线程执行，默认安全姿态不静默放宽。

## 步骤

1. 所有 Godot API 经 `queue.submit()` 或 `execute_sync()` 提交，由插件 `_process()` 的 `drain()` 在主线程排空；带预算调用走 `submit_tracked()` 以支持取消。
2. 保持默认监听 `127.0.0.1:9527/mcp`；非环回需绑定层加请求层双验证，当前实现拒绝启动。
3. 高风险能力保持默认拒绝（环境变量大于配置文件大于拒绝）；`process` 无面板开关，须环境变量或手改配置。
4. 脱敏保持默认开启；关闭仅作显式取证选择，截图与原始参数会落盘 `user://`。
5. 导出期间 `dispatch` 拒绝工具调用；停止与重载先停服再清队列，不产生悬垂任务。
6. 外部路径先经 `normalize_project_path` 校验；`res://` 与 `user://` 不混拼；截断结果带 `truncated` 类可检测字段。

## 边界

- 输入：触碰引擎或安全的修改；输出：线程路由说明加安全影响声明，无放宽时明确写出。
- 纯 C++ 逻辑可留工作线程；新增例外须代码加文档双说明。

## 非目标

- 不处理工具业务语义与命名；不替代验收清单外的运维决策。

## 验证

- 高风险工具可经 `get_tool_detail.side_effect` 识别；授权拒绝返回含 `authorization_required` 与 `enable` 字段。
- 主线程停摆返回 `main_thread_timeout` 结构化错误，而非挂死。
- 按 `docs/wiki/security_contract.md` 验收清单逐项自检。

## 权威参考

- [安全与并发契约](../../../docs/wiki/security_contract.md)
- [工程约定](../../../docs/wiki/conventions.md)
