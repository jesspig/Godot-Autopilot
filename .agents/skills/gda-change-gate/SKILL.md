---
name: gda-change-gate
description: "源码变更的三道守卫：编译单元登记、工具接口契约、并发与安全姿态。何时用：改源码或工具、增删移动源文件、构建或链接失败、新增或改动工具、引擎 API 与高风险确认/脱敏/导出；不适用于仅改头文件、只调用现有工具、纯逻辑改动。"
---

# 变更三道门

按本次变更触及的链路加载对应 reference；复合任务按 1→2→3 顺序全部加载。每道门的命令、判据与细则在各 reference 内。

## 路由

1. 新增/删/移编译单元、构建清单登记、编译或守卫报错 → 加载 [references/cpp-registration.md](references/cpp-registration.md)。仅改头文件时跳过。
2. 新增/改名/改参/改副作用工具、工具接口契约 → 加载 [references/domain-tool-authoring.md](references/domain-tool-authoring.md)。只调用现有工具时跳过。
3. 涉及引擎 API、主线程调度、运行时、高风险确认、脱敏、导出、路径或截断 → 加载 [references/thread-security-gate.md](references/thread-security-gate.md)。纯逻辑默认跳过；新增例外时加载。

## 共用验证

- 命中门 1 或 2 时运行项目守卫测试，预期全过（命令见对应 reference）。
- 命中门 3 时按该 reference 的判据逐项核对，高风险工具从工具描述识别副作用。
- 统计以运行时为准，不复制旧数值；用 `git diff HEAD` 确认真实变更，忽略 CRLF 伪变更。
