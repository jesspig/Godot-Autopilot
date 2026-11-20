# 混合 verifier、GOAL 长程规则与反作弊

当判定采纳候选是否通过 verifier、推进长程实施任务、或处理反作弊时读取本文件。

## 混合 verifier（开发者不自评）

每环／每个采纳候选由两类独立判定组成，开发者不对自己的产出做终判：

- **硬指标（程序化）**：以 `bun run gda-dev-tools harness digest --round <轮目录>` 的 `digest.json` 为唯一判定源，只读、不手工改写判定结果。消费点固定为：
  - `q_items` / `s_items`：核对问题与建议编号齐全、无遗漏、与报告一致；
  - `levels` / `playtime_minutes`：判关卡数与总通关时长，口径为「关卡 ≥3 且完整通关流程 ≥5 分钟」；
  - `causal_triples`：核对经验条目、建议与每环增量含「动作→条件→后果」因果结构；
  - `coverage_gaps`：作为离线回放的候选来源；
  - `coverage_used` / `coverage_invalid_touches` / `evidence_rows` / `suspected_gaming`：有效覆盖、无效触碰、证据行数与反作弊标志；
  - `round` / `schema`：溯源与版本核对。
- **软指标（独立子代理）**：玩法手感、主题诠释、画面表现、关卡可玩性，由**独立子代理**判定；开发者只提供证据与复现步骤，不代替子代理下结论。
- 两类都通过才判本环／本候选 verifier 通过；任一不通过按「同一条件下 probe，失败则回退基线并补齐后复验」处理。禁止把「已跑若干环」「报告已写初稿」当作通过依据。

## GOAL 长程规则

分析阶段读取测试轮次的 GOAL 末态作为证据与终态；当分析触发的授权实施本身成为长程多步工作时，沿用同一套 GOAL 规则推进：

1. GOAL.md 七字段：`objective`（本轮目标）、`acceptance`（验收判据）、`limits`（边界与 cap）、`state`（状态）、`completionEvidence`（逐项对应验收的证据）、`blocker`（阻断项，无则「无」）、`checkpoints`（每环一条：环号、增量、基线/改动后证据、覆盖数、verifier 判定、状态快照）。
2. `state` 六取值：`active` / `paused` / `budgetLimited` / `usageLimited` / `complete` / `unmet`。
3. 原子写：先写 `GOAL.md.tmp`，校验完整后重命名为 `GOAL.md`；禁止就地追加导致半截状态。每环结束更新 `checkpoints` 与 `state`。GOAL.md 是运行时产物，不预建、不提交；分析阶段不替测试轮次预建。
4. continuation rule：判为未完成且未触达 `limits` 中的 cap，立即自动进入下一环，不询问用户、不结束会话、禁止提前收尾；只在 `state` 变为 `complete` / `unmet`、出现 `blocker`、或触达 cap（记 `budgetLimited` / `usageLimited`）时通知用户，其余进展只写入报告与 GOAL.md。
5. 无进展检测：连续 2 环「玩法指标无变化且覆盖数无增长」→ 置 `state=paused`，写明停滞指标与已尝试动作并通知用户，不空转续跑。
6. 恢复流程：读 GOAL.md 末态 → 运行 digest 取摘要 → `gda status` 核对实例与工程；再据末态决定续跑或继续暂停。`complete` / `unmet` 为终态不续跑；`paused` / `budgetLimited` / `usageLimited` 仅在用户明确授权或 cap 重设后恢复。
7. 范围口径统一为「关卡 ≥3、完整通关流程 ≥5 分钟」，不得写成「单关 ≥5 分钟」，与 `levels`、`playtime_minutes` 的判定保持一致。

## 反作弊联动

- 以 `digest.suspected_gaming` 为准：`true` 表示该轮证据与所声称服务的玩法功能数不匹配，其覆盖数据**不进经验库**（不写入 HARNESS_LIBRARY），在分析报告中只作观察，或把相关条目标「候选」／「弃用」，等待下一轮真实开发复核；不自评「通过」。
- 证据一致性校验不设数字门槛；不因 `coverage_used` 数值高就认定证据有效。`coverage_invalid_touches` 计入的无效触碰须从有效覆盖中剔除，不进经验库。
- 在 `analysis.md` 写明 `suspected_gaming` 依据与无效触碰剔除情况，供下一轮复核。
