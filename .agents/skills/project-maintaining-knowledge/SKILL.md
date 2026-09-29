---
name: project-maintaining-knowledge
description: 本项目文档随代码演进需要同步时使用：更新 docs/wiki 页面/索引/小时级 changelog、维护 .agents/skills 下的 project-* 技能、判断一条新知识该写进哪一层、清理过期描述，以及交付前的文档收尾。
---

# 维护三层文档

## 适用范围

- 用于：功能完成后同步知识、提交前收尾、发现文档与代码不一致、需要新增或修订项目级技能、知识库自我迭代。
- 不适用：跨项目的个人偏好沉淀（用户级 `personal-*` 技能）；面向 MCP 客户端的技能书内容（`skills/`，走 `project-adding-domain-tools` 的联动清单）。

## 三层分工（先决定落点，再动笔）

| 层 | 位置 | 承载 | 判据 |
|---|---|---|---|
| 入口 | 根 `AGENTS.md` | 每次会话都必须在场的最小事实：命令、硬约束、分工规则、索引 | 少了它会做错事，且稳定少变 |
| 事实 | `docs/wiki/` | 陈述性事实：架构、模块行为、数值口径、历史结论与坑 | 只是"让人/agent 知道现状" |
| 做法 | `.agents/skills/project-*/SKILL.md` | 可执行指南：步骤、判据、边界、反例、检查清单 | 不写下来 agent 会做错动作 |

- 一个知识只有一个权威落点。发现两处并存详细版本：细节留在权威落点，另一处改成引用；技能里**不复制易漂移的数值**，指向 `docs/wiki/tests.md` 的数值核算总表。
- 改动性质决定改哪层：改变了"怎么做"（步骤、判据、边界）改技能；只改变了"现状"（行为、口径、数值）改 wiki。

## 更新 wiki 的流程

1. `git diff HEAD` 核实本次真实变更范围（`git status` 的 CRLF 伪变更不算依据），只改受影响页。
2. 事实必须一手可核：源码、文档化行为或用户明确讨论结果。核实不了的写 `> [!todo] 待补充`，**禁止推测**，禁止用历史推断当前行为。
3. 删除被取代的描述，不留"已废弃/ formerly"标记；旧事实若要保留价值，移到 `changelog/` 对应日期页。
4. 页头有"审计日期"的页面（overview / build / tests / example / modules/tools_registry / tools_ops_a / support / core）改后要同步日期；其余页不加日期头。
5. 更新 frontmatter `timestamp` 为真实系统时间（ISO 8601）；frontmatter 六键 `type`/`title`/`description`/`tags`/`timestamp`/`resource` 齐全，例外仅 `index.md`、`changelog/log.md`、`changelog/*-log.md`。
6. 每页至少 1 条相对链接，且链接目标文件真实存在。
7. 追加 `docs/wiki/changelog/<YYYY-MM-DD>-log.md`，按 `<YYYY-MM-DD-HH>` 记录动机、事实、验证与限制；时间取本机真实小时，**不许猜**；`changelog/log.md` 只留最近 7 天摘要。
8. 数值有变化时以运行时统计（`ctest -N`、执行器输出、`get_tool_detail` 等）为准更新 `tests.md` 总表，其它位置引用它。

## 更新项目级技能的流程

1. 触发点：同类纠正第二次出现、失败复盘出可复用做法、用户明确说"以后都这样"，以及**本次 wiki 变更使某技能的步骤或判据失效**。
2. 逐条过筛："不写这条，agent 会做错什么？"答不上来就不写；一次性任务细节、临时偏好一律不固化。
3. 格式：目录 `.agents/skills/project-<动词>-<对象>/SKILL.md`；frontmatter 只有 `name`（等于目录名）与 `description`；`description` 写触发条件与不适用边界，不写工作流摘要（它每次会话常驻加载，越短越好）。
4. 正文 60–150 行，按性质裁剪章节，不留空占位；超长内容下沉同目录 `references/`。
5. 与既有技能去重：新增前先 grep `.agents/skills/` 的相近技能，能扩展就不新建。
6. 改完自检：技能里出现的文件路径、命令、枚举值是否仍与源码一致（尤其是行号类引用，尽量改成"grep 定位"的描述而不是死行号）。

## 改 AGENTS.md 的门槛

- 只有"每次会话都必须在场且稳定"的内容才进：命令入口、硬约束（主线程队列、勿删 `_deps`、注释禁令、默认安全姿态）、三层分工、协作约定。
- 架构细节、批次记录、计数、分步流程一律不进；写完回看一遍，超过两三行的条目通常属于 wiki 或技能。

## 常见错误

- 把新工具清单、测试计数写进 `AGENTS.md`（很快过期，且与 wiki 双写）。
- 技能正文变成 wiki 摘录：全是"是什么"，没有"怎么做/何时停"。
- changelog 时间按提交时间或估算填写。
- 改了 wiki 没回看技能：技能里的步骤仍然指向已删除的行为，比 wiki 过期更危险，因为它会被当行动依据。
- 把交付给 MCP 客户端的 `godot-autopilot-*` 技能书（生成到当前打开工程的 `res://.agents/skills/`，本仓库内即 `tests/testbed/.agents/skills/`）与开发用 `project-*` 技能混为一谈，或手改生成产物。

## 检查清单

- [ ] 每条新知识落点唯一，其它层是引用而非副本。
- [ ] 受影响 wiki 页：事实可核、frontmatter 完整、日期头同步、至少 1 条相对链接。
- [ ] 受影响 `project-*` 技能：步骤/判据/路径仍然成立，`description` 仍描述触发条件。
- [ ] `changelog/<今天>-log.md` 已按真实小时追加，`log.md` 在 7 天窗口内。
- [ ] `AGENTS.md` 未新增细节型内容，且其中的命令与硬约束仍与源码一致。

## 协同与参考

- 分工与编写规则的权威页：[docs/wiki/index.md](../../../docs/wiki/index.md)（"三层文档分工""编写规则"）
- 提交与推送纪律：`project-running-tests`（收尾清理）+ 用户级 `personal-using-git`
