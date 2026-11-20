# 长程机制：GOAL、续跑与 verifier

当执行数小时、至少 3 关、多机制连续开发，或判定本轮是否可收尾时读取本文件。全程串行，不引入 worktree 并行。

## GOAL.md（开工即写，原子写）

开工（前置检查通过后、六阶段开发第一环前）在 `<project-dir>/GOAL.md` 写目标状态文件；它是运行时产物，不预建、不提交。字段固定八项：

| 字段 | 内容 |
| --- | --- |
| `objective` | 按主题交付可玩内容充分、至少 3 关且关卡间有机制差异的游戏，并达成工具覆盖协议的分域必覆盖与总体 ≥50% |
| `acceptance` | 全部满足才算 complete：≥3 关且机制多样；净可玩内容检验通过（无计时器凑数）；分域必覆盖达标；总体有效覆盖 ≥50%；证据来自同一次连续 run；Q 条目证据齐全；冷启动无错 |
| `limits` | 边界：只改授权工程与报告目录；不改 GDA 源码与版本；不提交 git；cap（最大环数与时长上限）记在此处 |
| `state` | 取值限于 `active` / `paused` / `budgetLimited` / `usageLimited` / `complete` / `unmet` |
| `completionEvidence` | 指向满足各 acceptance 项的具体证据，逐项对应 |
| `blocker` | 当前阻断项；无则写「无」 |
| `checkpoints` | 每环一条：环号、玩法增量、基线/改动后证据、本环新增覆盖工具与累计覆盖数、verifier 判定、GOAL 状态快照 |

写入采用**临时文件 + 重命名**原子写：先写 `GOAL.md.tmp`，校验完整后重命名为 `GOAL.md`；禁止就地追加。每环结束更新 `checkpoints` 与 `state`。

范围口径统一为「关卡 ≥3、**完整通关流程 ≥5 分钟**」，不得写成「单关 ≥5 分钟」。

## 续跑规则

每环闭环：开发增量 → 保存 → verifier 判定 → 写 checkpoint 与 GOAL.md。

- 判为未完成且未触达 cap，立即自动进入下一环，不询问用户、不结束会话、禁止提前收尾。
- 只在 `complete` / `unmet`、`blocker` 或触达 cap 时通知用户；其余环内进展只写报告与 GOAL.md。
- 触达 cap 仍未完成：按触达预算或用量记 `budgetLimited` / `usageLimited` 并通知用户，不擅自突破 cap。
- 严禁把「已跑若干环」「报告已写初稿」当收尾理由；收尾只能由 acceptance 全过、blocker 阻断或 cap 触发。
- 若因覆盖不达标而补做增量，补做必须服务真实玩法功能（如把代码生成的贴图改为工具生成），不得是裸调用。

## 混合 verifier（不自评）

- **硬指标**（关卡数、机制多样性、净可玩内容、分域覆盖、总覆盖率与三要素、`suspected_gaming`、跨 run 一致性、崩溃恢复次数、证据文件存在性、无引擎错误日志）：由 `bun run gda-dev-tools harness digest --round <轮目录>` 读取 `digest.json` 判定，以 digest 输出为准，不手工改写。digest 不解析的硬指标（分域覆盖、净可玩内容、跨 run 一致性）在报告中显式写出并由独立子代理复核。
- **软指标**（玩法手感、主题诠释、画面表现、关卡可玩性）：由**独立子代理**判定；开发者只提供证据与复现步骤，不代替下结论。
- 两类都通过本环才算 verifier 通过；任一不通过按「失败时区分改动无效与回归、回退本环」处理，补齐后复验。

## 无进展检测与恢复

- 连续 2 环「玩法指标无变化且累计覆盖数无增长」→ `state` 置 `paused`，写明停滞指标与已尝试动作并通知用户，不空转续跑。
- 恢复流程：读 GOAL 末态 → `harness digest` 取摘要 → `gda status` 核对实例与工程；据末态决定续跑或继续暂停。
- `complete` / `unmet` 为终态不续跑；`paused` / `budgetLimited` / `usageLimited` 仅在用户明确授权或 cap 重设后恢复。
- `digest.json` 只读派生品：只读它判定与续跑，不回写报告或 `evidence.md`。
