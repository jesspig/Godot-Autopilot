# T-13 新鲜 Agent Playtest-Debug 冒烟

## 目的
确认 `playtest-debug` recipe 在新技能书中具备完整八段结构，且流程命令可执行、安全确认点齐全。

## recipe 结构校验
`references/recipes/playtest-debug.md`（EN/ZH 同构）：
1. 触发条件与输入
2. 前置检查
3. 可复制的 CLI 命令
4. 每步观察字段
5. 验证与退出条件
6. 失败分支
7. 安全与确认点
8. 明确不做什么

首行 `# playtest-debug`，八段 `## 1.`–`## 8.` 按序（守卫 checkRecipes 校验通过）。

## 流程命令（来自 COMMAND_SPECS）
- 运行状态：`gda game status`
- 进入游戏：`gda game play`（高风险，需 `--confirm`）
- 注入输入：`gda game input`
- 截图：`gda game capture`
- 停止：`gda game stop`（高风险，需 `--confirm`）
- 调试：`gda debug status` / `gda debug errors` / `gda debug repro`（repro 编排，需 `--confirm`）
- 日志：`gda logs game`
- 报告：`gda debug report`

## 安全确认点
第 7 段覆盖：`confirmation_required` → `--confirm`；非交互缺确认停止；结果未知不重发有副作用调用；game_runtime 是停下审查信号；覆盖写显式确认；批量逐项授权。

## 结论
playtest-debug recipe 结构完整（8/8）、命令在 manifest 中（CLI 契约 548 pass 覆盖）、零 MCP/零漂移（守卫通过）。EN/ZH 命令集合与顺序 parity 通过。未进行真实引擎 playtest（零引擎依赖阶段），引擎级冒烟留待 ST-05。

## 未做
- 未启动真实 Godot 游戏会话
- 未捕获真实截图/错误流
- 本证据为静态复核 + 命令白名单校验
