# 长程机制：GOAL、续跑、verifier 与反作弊

当执行数小时、多单元连续操作，判定本轮是否可收尾，或判定采纳候选是否通过 verifier 时读取本文件。全程串行，不引入 worktree 并行（阶段 3 修复在独立 feature 分支上串行推进）。四阶段共用一份 GOAL。

## GOAL.md（开工即写，原子写）

开工（前置检查通过后、第一环前）在 `<project-dir>/GOAL.md` 写目标状态文件；运行时产物，不预建、不提交。字段固定八项：

| 字段 | 内容 |
| --- | --- |
| `objective` | 按固定任务书交付可玩内容充分的小样，并**覆盖全部 252 个 canonical 工具**，完成组合测试与连锁反应验证 |
| `acceptance` | 全部满足才算 complete：功能单元 ≥3 且可玩闭环成立；组合链 C1–C6 全部执行并记录；分域必覆盖达标；**覆盖 252/252**（或逐项客观理由充分）；证据来自同一次连续 run；Q 条目证据齐全；冷启动无错；阶段 2 analysis 完成且独立审查一致；阶段 3 采纳项已授权验证或明确待授权；阶段 4 复测通过 |
| `limits` | 边界：只改授权工程、报告目录与授权后的源码/技能；不改 GDA 版本；不提交 git；cap（最大环数与时长上限）记在此处 |
| `state` | `active` / `paused` / `budgetLimited` / `usageLimited` / `complete` / `unmet` |
| `completionEvidence` | 指向满足各 acceptance 项的具体证据，逐项对应 |
| `blocker` | 当前阻断项；无则写「无」 |
| `checkpoints` | 每环一条：环号、功能增量、基线/改动后证据、本环新增覆盖工具与累计覆盖数、组合链结果、verifier 判定、GOAL 状态快照 |

写入采用**临时文件 + 重命名**原子写：先写 `GOAL.md.tmp`，校验完整后重命名为 `GOAL.md`；禁止就地追加。每环结束更新 `checkpoints` 与 `state`。范围口径统一为「功能单元 ≥3、完整通关流程 ≥5 分钟」，不写「单单元 ≥5 分钟」。

## 续跑规则

每环闭环：功能增量 → 保存 → verifier 判定 → 写 checkpoint 与 GOAL.md。

- 判为未完成且未触达 cap，立即自动进入下一环，不询问用户、不结束会话、禁止提前收尾。
- 阶段 1→2 自动衔接，阶段 2 审查一致后暂停于阶段 3 的授权点；阶段 3→4 在修复分支验证通过后自动衔接，阶段 4 未通过则带新证据回阶段 2。只在 `complete` / `unmet`、`blocker`、触达 cap、审查分歧或等待阶段 3 授权时通知用户；其余环内进展只写报告与 GOAL.md。
- 触达 cap 仍未完成：按触达预算或用量记 `budgetLimited` / `usageLimited` 并通知用户，不擅自突破 cap。
- 严禁把「已跑若干环」「报告已写初稿」当收尾理由。
- 若因覆盖不达标补做增量，补做必须服务真实功能（如把代码生成的贴图改为工具生成），不得是裸调用。

## 混合 verifier（不自评）

- **硬指标**（功能单元数、总时长、组合链执行、覆盖 252/252 与三要素、`suspected_gaming`、跨 run 一致性、崩溃恢复次数、证据文件存在性、无引擎错误日志）：由 `bun run gda-dev-tools harness digest --round <轮目录>` 读取 `digest.json` 判定，以 digest 输出为准，不手工改写。digest 不解析的硬指标（分域覆盖、净可玩内容、跨 run 一致性、组合链证据）在报告中显式写出并由独立子代理复核。
- **硬指标消费点**：`q_items`/`s_items` 核对编号齐全无遗漏；`levels`/`playtime_minutes` 判「功能单元 ≥3 且完整通关 ≥5 分钟」；`causal_triples` 核对动作→条件→后果结构；`coverage_gaps` 作离线回放候选来源；`coverage_used`/`coverage_invalid_touches`/`evidence_rows`/`suspected_gaming` 判覆盖与反作弊；`round`/`schema` 溯源。`digest.json` 只读。
- **软指标**（玩法手感、主题诠释、画面表现、单元可玩性）：由**独立子代理**判定；开发者只提供证据与复现步骤，不代替下结论。
- 两类都通过本环/本候选才算 verifier 通过；任一不通过按「同一条件下 probe，失败则回退基线并补齐后复验」处理。

## 反作弊联动

- 以 `digest.suspected_gaming` 为准：`true` 表示该轮证据与所声称功能数不匹配，其覆盖数据**不进经验库**（不写被 git 忽略的运行时数据 `.handoff/library/HARNESS_LIBRARY.md`），分析报告中只作观察或把相关条目标「候选」/「弃用」，等待下一轮真实操作复核；不自评「通过」。
- 证据一致性校验不设数字门槛；不因 `coverage_used` 数值高就认定证据有效。`coverage_invalid_touches` 计入的无效触碰须从有效覆盖中剔除，不进经验库。
- 在 `analysis.md` 写明 `suspected_gaming` 依据与无效触碰剔除情况，供下一轮复核。

## 无进展检测与恢复

- 连续 2 环「功能指标无变化且累计覆盖数无增长」→ `state` 置 `paused`，写明停滞指标与已尝试动作并通知用户，不空转续跑。
- 恢复流程：读 GOAL 末态 → `harness digest` 取摘要 → `gda status` 核对实例与工程；据末态决定续跑或继续暂停。
- `complete` / `unmet` 为终态不续跑；`paused` / `budgetLimited` / `usageLimited` 仅在用户明确授权或 cap 重设后恢复。
