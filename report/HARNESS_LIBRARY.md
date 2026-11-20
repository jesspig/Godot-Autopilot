# GDA Harness 改进库

> 跨轮、跨 demo 共享的条目化改进记忆。由 `gda-e2e-analysis` 以增量条目更新，`gda-e2e-testing` 读取「候选 / 待验证」条目作为本轮附加观察点。
> 只登记条目与状态，不写结论性叙述；判定规则见对应技能文件。
> 条目来自真实轮次的 deep 深挖与直接证据；无对照证据不得写入条目。当前条目 L1–L3 来自 `2026-11-20_03-28` 轮次（已实施并通过构建/静态测试，真实 e2e probe 待下一轮）。

## 条目字段

| 字段 | 取值或格式 | 说明 |
| --- | --- | --- |
| id | `L<n>`，库内唯一且连续 | 条目标识；修订沿用原 id，不新建副本 |
| 状态 | 候选 / 待验证 / 采纳 / 冻结 / 弃用 | 见「状态流转」 |
| 可编辑面 | `文件:章节` 或 `模板:字段` | 条目指向的可编辑位置 |
| 适用条件 | 触发场景、工程类型或工具类别 | 写明何时适用 |
| 教训 | 动作 → 条件 → 后果 | 可复用的因果结构，不是结论 |
| 证据 | `Q<n>` / `S<n>` + 对照来源（轮目录、轨迹、测试） | 无对照证据不得进「采纳」 |
| 置信度 | 高 / 中 / 低 | 证据充分程度 |
| 可复用范围 | 跨 demo / 同 demo / 单次 | 跨 demo 需用当前证据重验 |
| 来源轮次 | `YYYY-MM-DD_HH-MM` | 产出该条目的轮目录名，对齐 `digest.json.round` |
| 更新历史 | `日期: 动作与依据` 列表 | 记录字段修订与状态流转 |

## 与 digest 的字段对齐

条目由 `bun run gda-dev-tools harness digest --round <轮目录>` 的 `digest.json` 提供可溯源字段，写入时按下列对齐，不得编造：

- 教训（动作→条件→后果）对齐 `digest.json.causal_triples` 的 `action` / `condition` / `consequence`。
- 证据对齐 `digest.json.q_items` / `digest.json.s_items` 与对照来源（轮目录、轨迹、测试）；无对照证据不得进「采纳」。
- 来源轮次对齐 `digest.json.round`；状态判定参考 `digest.json.coverage_used` / `coverage_invalid_touches` / `evidence_rows` / `suspected_gaming`（见「合并规则」第 5 条与状态流转）。
- `digest.json` 为只读派生品，只读取、不回写。

## 状态流转

- 候选：离线回放产出，尚未经对照验证。
- 待验证：回放对照通过，等待下一轮真实开发复核。
- 采纳：对照通过并冻结，当前执行 harness 的一部分。
- 冻结：历史采纳项，保留可追溯，不再主动应用。
- 弃用：对照失败、被新条目取代或 `suspected_gaming=true`，不进采纳。

## 合并规则

1. 合并前先读取当前条目表作为冻结快照；按 id 定位，同 id 只修订字段并追加「更新历史」，不新建副本。
2. 按 id 增量合并去重：只写入本次涉及条目，其余条目原样保留，不重写整库；串行写入，避免并发冲突。
3. 教训语义相同的条目合并到既有 id；跨 demo 复用前必须用当前证据重验。
4. 失效条目改状态为「冻结」或「弃用」，不删除历史条目。
5. `digest.json.suspected_gaming=true` 轮次的覆盖数据不进经验库，相关条目只能标「候选」或「弃用」；证据不足的观察不写条目。

## 条目

| id | 状态 | 可编辑面 | 适用条件 | 教训（动作→条件→后果） | 证据 | 置信度 | 可复用范围 | 来源轮次 | 更新历史 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| L1 | 采纳 | `src/runtime/game_bridge.cpp:2275` + `docs/wiki/modules/entry_runtime.md:165` | 新增 `GDCLASS` 派生运行时类 | 动作=定义并 `memnew`/`add_child` 新 awaiter；条件=`register_listener()` 注册块漏 `register_class`；后果=ClassDB 查类失败、异步回包不发、game op 超时 | Q2/S1 + `game_bridge.cpp:1286/1582/2268-2277` + 日志类名 + 构建/844 ctest | 高 | 跨 demo | 2026-11-20_03-28 | 2026-11-20: 新增（根因证实并实施） |
| L2 | 采纳 | `devtools/commands/harness.ts:114` + `cli/tests/harness.test.ts:120` | `harness setup --project` 且 cwd≠目标工程 | 动作=setup 序列末步无参 `gda ping`；条件=cwd 回退仓库根而实例按 `--project` 注册；后果=定位失败 exit 2、编辑器被作业对象连带终止 | Q1/S2 + `harness.ts:114`/`target.ts:139`/`process.ts:82-96` + 21 CLI 单测 | 高 | 跨 demo | 2026-11-20_03-28 | 2026-11-20: 新增（根因证实并实施） |
| L3 | 采纳 | `devtools/commands/harness.ts:168-181/198-253/287` + `docs/wiki/build.md:40` | `harness coverage --round` 统计 | 动作=全量 trace 导出取最小 `wall_end_ms` 当 `round_start_ms`；条件=trace 跨多会话；后果=起始时间落到最早历史轮、coverage/invalid 跨轮膨胀 | Q3/S3 + `harness.ts:192-197/226/255` + 74,215 条实测 + 2 新增单测 | 高 | 跨 demo | 2026-11-20_03-28 | 2026-11-20: 新增（根因证实并实施） |

