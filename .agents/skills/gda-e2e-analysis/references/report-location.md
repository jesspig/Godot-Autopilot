# 报告定位与证据准备

当定位最新报告、准备回放材料、核对源码基线或读取 GOAL 末态时读取本文件。

## 定位最新报告

用户指定轮目录时直接使用该目录；指定 demo 名时只在 `report/<demo>/` 下按旧结构查找。未指定时：

1. 扫描 `report/*/report.md`（新结构，轮目录名 `YYYY-MM-DD_HH-MM`），从路径取轮目录名；把 `_` 归一化为 `-` 后按时间前缀字典序取最大。同分钟多轮按 `-2`、`-3` 后缀数字取最大（无后缀视为 0）。
2. 扫描 `report/*/*/report.md`（旧结构，轮目录名 `YYYY-MM-DD-HH-mm`），按同样规则取最新，作为兼容候选。
3. 目录名缺失或格式不符时回退按文件修改时间取最新。新结构与旧结构都不存在时兼容扫描 `report/**/*-report.md`（旧扁平格式），按文件名中的时间取最新。
4. 同一时间存在多个候选时列出并等待用户选择，不擅自挑一个。
5. 仍无报告时停止，说明需要先完成一轮端到端测试。

按轮目录名的时间前缀判定「最新」：零填充，字典序即时间序；新结构先把 `_` 归一化为 `-`。不首选文件修改时间。轮次数量少，做一次通配扫描即可，不建索引文件。

## 读取报告与历史轮

- 读取最新轮的 `report.md`，重点读取第 4 节「问题记录」与第 5 节「优化建议」；同时读取同目录 `evidence.md` 与 `img/` 清单。保持这些文件只读。
- 读取同一工程历史轮次的 `analysis.md`「可复用经验条目」与「回放索引」（若存在）；跨工程历史经验作为候选来源，但必须标注来源并用当前证据重新验证，不得直接继承。
- 从第 4 节提取问题条目：类型为「失败」或「缺陷」的纳入全链路根因分析；「体验」「文档」类纳入建议评估，不做三段调用链。
- 沿用报告中的 `Q<n>` 编号；报告未编号时按原文位置编号为 `Q<n>`，并在报告中注明「本编号由分析侧分配」。
- 从 `evidence.md` 取工具名、error_code、duration_ms、request_id；缺失时回读原始轨迹，仍缺失时标注证据不足。
- 报告没有失败项时输出「本轮无失败项」，只评估建议部分或直接结束，不虚构失败项。
- 不把良好表现纳入根因分析，将其作为优化依据。
- 历史轮只读；无历史轮或证据不足时明确标注「未验证／无历史回放」，不得编造历史 Q 条目、行号、对照结果或库条目。

## 消费 digest

- 运行 `bun run gda-dev-tools harness digest --round <轮目录>` 取得同目录 `digest.json`；它是只读派生品，只读不回写。digest 缺失或解析失败时标注「digest 未验证」，按 `evidence.md` 手工提取，不编造字段值。
- 消费四类字段与覆盖数据：`q_items`、`s_items`、`levels`、`playtime_minutes`、`causal_triples`、`coverage_gaps`、`coverage_used`、`coverage_invalid_touches`、`evidence_rows`、`suspected_gaming`（字段消费点见分析报告模板的「混合 verifier 判定」一节）。
- 读取 GOAL 末态（`<project-dir>/GOAL.md`，若存在）：`state`、`acceptance`、`completionEvidence`、`blocker`；与报告头部「GOAL 终态」对照，不一致时以 digest 与报告为准并记录差异。
- 读取 `report/HARNESS_LIBRARY.md` 条目表作为冻结快照基线：只记录读到的 id 与状态，供共享库增量合并比对，不据此直接下结论。

## 准备证据源

- 校验绑定层目录：存在 `include/godot_cpp/` 与 `src/`；使用外部仓库时记录 `git -C <dir> describe --tags --always` 与 `rev-parse --short HEAD`。引擎侧行为以 GDExtension API 的 C++ 绑定契约为准，不依赖引擎源码。
- 校验 MCP SDK 目录：`third_party/mcp-cpp-sdk` 存在 `include/` 与 `src/`；记录 `git -C third_party/mcp-cpp-sdk describe --tags --always` 与 `rev-parse --short HEAD`。缺失时停止并请用户初始化子模块。
- 记录本项目基线：`git rev-parse --short HEAD`、`git diff HEAD` 的变更范围。
- 记录报告基线：报告路径、轮目录、报告中的 GDA 版本与引擎版本、`digest.json` 路径与 `schema`。
- 建立回放材料清单：本轮 `report.md`、`evidence.md`、关键 Q 条目与对照证据；并把 `report/*/` 下**全部历史轮**纳入模拟器池（含跨 demo，跨 demo 复用须用当前证据重验）。历史证据不足时准备最小复现步骤代替回放，并注明「历史证据不足，未做回放」。
- 记录降本指标基线：本轮「真实轮次调用数」（本阶段不重跑整轮；确需最小复现时记实际调用数，否则记 0）与「候选探索数」（由离线回放产出），供报告汇总。
- 先列出外部调研的检索主题；检索后记录来源 URL 与检索日期。
