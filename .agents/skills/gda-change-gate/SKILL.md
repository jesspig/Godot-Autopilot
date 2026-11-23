---
name: gda-change-gate
description: "GDA 源码变更的三道守卫：C++ 编译单元登记、领域工具 ToolSpec、主线程与安全姿态。何时用：改 GDA 源码或工具、新增/删/移 .cpp、CMake 双登记、守卫报错、编译链接失败、新增或改领域工具、Godot API/主线程/CommandQueue/submit/execute_sync、非环回/confirm/脱敏/导出/路径/截断；不适用于仅 .hpp 变更、仅调用已有工具、纯 C++ 逻辑。"
---

# GDA 变更三道门

按本次变更实际触及的链路加载对应 reference；复合任务按 1→2→3 顺序全部加载。

## 路由

1. 新增/删/移 `.cpp`、CMake 双登记、`migration_guard` / `comment_guard` 报错 → 加载 [references/cpp-registration.md](references/cpp-registration.md)。仅 `.hpp` 变更时跳过。
2. 新增/改名/改参/改副作用领域工具、ToolSpec、`make_spec_tool` / `ParamSpec` / `args.Find` → 加载 [references/domain-tool-authoring.md](references/domain-tool-authoring.md)。仅调用已有工具时跳过。
3. 涉及 Godot API、主线程、`submit*`、运行时、高风险确认、脱敏、导出、路径或截断 → 加载 [references/thread-security-gate.md](references/thread-security-gate.md)。纯 C++ 逻辑默认跳过；新增例外时加载。

## 共用验证

- 命中门 1 或 2 时执行 `ctest --preset debug -R "migration_guard|comment_guard"`，预期全过。
- 命中门 3 时按该 reference 的验证项逐项核对；高风险工具可经 `get_tool_detail.side_effect` 识别。
- 统计以运行时为准，不复制旧数值；用 `git diff HEAD` 确认真实变更，忽略 CRLF 伪变更。
