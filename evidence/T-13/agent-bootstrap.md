# T-13 新鲜 Agent Bootstrap 冒烟

## 目的
确认一个新 Agent 仅凭重写后的 SKILL.md 与 references 就能完成 CLI-only 启动、发现与安全确认，无需 MCP 知识。

## 只读复核（基于最终技能书）
入口卡 `skills/godot-autopilot/SKILL.md` 提供：
- 健康探测：`gda ping`
- 动态发现：`gda tool list` / `gda tool search` / `gda tool describe` / `gda commands --json`
- 通用调用逃生口：`gda tool call`
- 安全确认：`--confirm`、`confirmation_required`
- 错误恢复：`gda doctor`
- 长任务：`gda job`
- 15 行领域→references 路由表

references 覆盖：setup/cli/safety/offline/scene/scripting/resources/runtime/debugging/testing/content/csharp/servers/workflows/tools，另有 8 个 recipes。

## 命令可执行性校验
入口卡与 recipes 中的所有 `gda` 命令均来自 `cli/src/command-spec.ts` 的 COMMAND_SPECS/COMMAND_GROUPS 与 `cli/src/args.ts` 的 usageText；CLI 契约测试（548 pass/0 fail）与 skills_guard（unknown-command/unknown-flag 规则）确认这些命令与 flag 均在 manifest 中。

## 结论
Bootstrap 流程的每一条命令都可执行、每个路由目标文件都存在（tree.test.ts 与守卫校验通过）；入口卡满足 CLI-only 契约（9 个必需字面串 + 路由表 ≥3 行）。未进行真实 Godot 引擎会话（本阶段 CLI 单测零引擎依赖），引擎级冒烟留待 ST-05 集成门禁。

## 未做
- 未启动真实编辑器/插件 IPC 会话
- 未在有引擎环境下执行端到端 bootstrap
- 本证据为静态复核 + 命令白名单校验，不虚构引擎运行结果
