---
name: project-enforcing-security-gates
description: 触及绑定与端口、能力授权、脱敏策略、文件路径校验、导出期行为，或新增有副作用工具与用户脚本工具时使用；纯只读工具实现与埋点写入不在范围内。
---

# 守住安全与授权边界

## 适用范围

- 用于：新增/修改 `side_effect` 非 `None` 的工具、改 `ServerContext`/端口/host、改配置面板授权复选框、改脱敏策略、写文件/读文件路径逻辑、动态（用户脚本）工具相关改动。
- 不适用：只读引擎查询类工具的实现细节（`project-writing-tool-handlers`）；埋点事件本身（`project-instrumenting-traces`）。

## 前置条件

- 默认威胁模型：同一用户账户、同一台机器上的受信 MCP 客户端。能连本机端口的一律视为可调用方；本项目不提供鉴权与加密，**不得**被改造成远程可达服务。
- 契约正文以 [docs/wiki/security_contract.md](../../../docs/wiki/security_contract.md) 为准，本技能只写"怎么不越界"。

## 规则与边界

- **监听两层都要环回**：绑定层 `ServerContext::start()` 用 `is_loopback_host()` 拒绝非环回地址（`refusing non-loopback listen address`）；请求层 mcp-cpp-sdk 校验 `Host` 头，默认白名单只有 `localhost`/`127.0.0.1`/`::1`（本项目未设 `allowed_hosts`）。放宽任一层都不等于打开远程访问，**要放宽必须用户明确要求并附鉴权方案**。
- **端口/主机解析顺序固定**：`GODOT_AUTOPILOT_PORT`/`GODOT_AUTOPILOT_HOST` 环境变量 > `user://godot_autopilot/config.json` > 默认 `9527`/`127.0.0.1`；端点固定 `/mcp`。新增配置项不要改变这个优先级。
- **授权门按能力名判定**，现只有四类能力：`process`、`code_execute`、`game_runtime`、`user_tools`（外加 `all` 通配）。映射来自 `capability_for_tool`：`SideEffect::Process`→`process`；`CodeExecute` 或工具名 `code_execute`/`execute_script`→`code_execute`；`GameRuntime` 或 `execute_game_script`→`game_runtime`。
  - 关键推论：`WritesFile`/`WritesConfig`/`ShowsAlert`/`ModifiesWindow` **不进授权门**（返回 `nullptr`），它们只用于风险可见性与测试遍历排除。给工具加这几个枚举不会让它变"需要授权"，别据此得出"已经安全"的结论。
  - 判定顺序：`GODOT_AUTOPILOT_ALLOW` 环境变量 > 配置 `allow` 字段 > 默认空=拒绝。`code_execute`/`game_runtime`/`user_tools` 在 MCP Config Dock 有复选框、勾选即时生效；`process` 没有 dock 开关，只能靠 env/配置。
  - 授权拒绝走统一出口 `authorization::deny_if_unauthorized(name, side_effect)`（响应含 `error`/`authorization_required`/`enable`），不要在 handler 里各写一份判权逻辑；`SpecTool::execute` 已保证"先授权门、后 handler"。
- **动态工具另有一道门**：用户脚本注册的工具执行前受 `user_tools` 约束（`AutopilotTools` 的 `is_enabled`/`set_enabled`），注册即 `refresh_dynamic_tools()`。新增注册 API 时保持"未授权即拒"的默认，不要提供绕过开关。
- **脱敏默认开**：`GODOT_AUTOPILOT_DESENSITIZE` > 配置 `desensitize` > `true`。任何把参数/图片/脚本内容写盘的通路，默认路径必须经 `sanitize_field`/`sanitize_args`；只有在脱敏关闭时才允许写原文与 `traces/images/`。日志与 trace 里禁止出现密钥与 PII。
- **路径边界**：接受外部输入的路径必须校验在 `res://` / `user://` 范围内；目录扫描类能力保持限额（递归 256 文件、深度 8、逐文件错误隔离）。不要为了"方便"加绝对路径或盘符支持。
- **导出期**：`ExportGuard` 置位时 `dispatch` 拒绝工具调用，`tools::invoke_tool` 也先走导出拦截。新增的任何旁路入口必须保留这道拦截。
- **Godot API 只能经 `CommandQueue` 在主线程执行**：这条同时是稳定性约束（详见 `project-writing-tool-handlers`）。

## 常见错误

- 给写文件的工具加了 `SideEffect::WritesFile` 就以为"受授权保护"，实际它只进遍历排除清单。
- 在 handler 内自行判 `GODOT_AUTOPILOT_ALLOW`，绕开 `deny_if_unauthorized`，导致 `authorization_required`/`enable` 字段缺失、客户端无法自助恢复。
- 把 dock 复选框的即时生效复制到 `process` 能力上（它没有 dock 开关）。
- 为了跑通测试把默认绑定改成 `0.0.0.0`，或给 SDK 传 `allowed_hosts` 放宽 Host。正确做法是给测试进程设置 `GODOT_AUTOPILOT_ALLOW`。
- 关掉脱敏做实验，然后把带原始参数或截图的 `user://godot_autopilot/` 产物提交进仓库、贴进 issue。

## 检查清单

- [ ] 新增/变更的风险工具在 `ToolSpec` 里如实声明 `side_effect`（不声明等于让调用方猜）。
- [ ] 需要授权的能力走 `deny_if_unauthorized`，拒绝响应含 `authorization_required` 与可执行的 `enable` 提示。
- [ ] 默认配置未变：`127.0.0.1:9527/mcp`、脱敏开、`allow` 空。
- [ ] 敏感数据在默认态不出现在日志、trace、错误消息与测试夹具里。
- [ ] `security_parallel_hardening_test` 等相关 L1 用例通过。

## 协同与参考

- [docs/wiki/security_contract.md](../../../docs/wiki/security_contract.md) · [docs/wiki/modules/core.md](../../../docs/wiki/modules/core.md) · [docs/wiki/modules/tools_registry.md](../../../docs/wiki/modules/tools_registry.md)
