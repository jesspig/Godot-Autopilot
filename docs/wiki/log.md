# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天（按有变更的日期段计，最新一段 2026-11-25；更早的批次留档在 changelog 目录，如 [2026-10-10-log.md](changelog/2026-10-10-log.md)）。
>
> changelog 内的相对链接指向记录当时的源码路径；部分目标已随后续重构删除或迁移，按历史记录阅读，不代表当前文件布局。

## 2026-11-25

- **0.3.5 正式发布收口**：根 `VERSION` / `CLI_VERSION` 去 `-dev` 后缀为 `0.3.5`，`cli/assets/skills.json` 由官方脚本重新生成；新增发布说明 [release-notes/0.3.5.md](../release-notes/0.3.5.md)，汇总脚本能力、确认协议、hello 握手、C# 供给、场景持久化与 CI 跨平台稳定性修复。`build.md` / `modules/cli.md` / `modules/skills.md` / `modules/tools_ops_a.md` / `tool_surface_report.md` 的“当前正式版”表述与 `timestamp` 同步。仅改本地文件，未打 tag / 推送 / 触发发布流程。详见 [changelog/2026-11-25-log.md](changelog/2026-11-25-log.md)。
- **根因修复批次**：脚本能力模型统一编辑器可运行性与运行时可挂载性，`update_autoloads` 接受非 Tool C# 脚本；call_tool 确认协议归一；`gda:hello` 握手替代 fire-and-forget ready 并提供 `ready_diagnosis`；C# autoload 显式供给与缺桥诊断；play 响应显式报告 Godot 运行前 autosave。详见 `.handoff/evidence/ST-01/summary.txt` 与 `.handoff/STATE.md`。

## 2026-11-24

- **游戏运行时监视器**：编辑器启动的游戏运行逐帧记录 59 项引擎监视器与自动发现的标准 API 自定义监控，JSONL 落盘；新增 `game_monitor` MCP 工具与 `gda monitor` CLI，支持按会话调取与离线分析。工具面 252→253、ctest 1101 全绿。详见 [changelog/2026-11-24-log.md](changelog/2026-11-24-log.md) 与 [modules/game_monitor.md](modules/game_monitor.md)。

## 2026-11-23

- **知识库增量维护（代码质量治理与安全加固批次）**：上次同步点 `b349cb5`（2026-11-21），以 `git diff b349cb5` 定界，覆盖 11-21 尾至 11-23 的提交与工作树改动；本轮不升版，`VERSION` / `CLI_VERSION` 保持 `0.3.4-dev`。按真实源码同步受影响页面段落，逐页 `timestamp` 统一为 `2026-11-23T17:28:29+08:00`，页头审计日期同步 `2026-11-23`。要点：ST-01～ST-05 代码质量治理（受保护配置键、trash confirm、反序列化边界、IPC token 契约、公共 util 与按键统一、提示词漂移修正）、多分支安全加固（路径 containment、资源事务、可信回滚、元工具 elicitation/递归守卫、部署/安装/存储可靠性）、CLI 命令面 22→23 与单测口径重算、技能合并 `gda-change-gate`。程序化测试未统一运行，数值以静态源码核算为准，实测口径见 [tests.md](tests.md)「数值核算总表」。详见 [changelog/2026-11-23-log.md](changelog/2026-11-23-log.md)。

- **0.3.4 正式发布收口**：根 `VERSION` / `CLI_VERSION` 去 `-dev` 后缀为 `0.3.4`，`cli/assets/skills.json` 由官方脚本从新 `VERSION` 重新生成；wiki 中五处「当前开发版 0.3.4-dev」陈述改为正式版并同步页头日期与 `timestamp`；新增发布说明 [release-notes/0.3.4.md](../release-notes/0.3.4.md)，汇总工具面压缩（459→252）、技能与 e2e 重组、mcp-cpp-sdk 0.3.12 升级与 ST-01～ST-05 质量治理。仅改本地文件，未打 tag / 推送 / 触发发布流程。详见 [changelog/2026-11-23-log.md](changelog/2026-11-23-log.md)。

## 2026-11-21

- **e2e 技能合并为多子代理四阶段闭环**：删除 `gda-e2e-testing` 与 `gda-e2e-analysis`，新建 [`gda-e2e-loop`](../../.agents/skills/gda-e2e-loop/SKILL.md)，全程由四个独立子代理分工（主代理只做总控）：阶段1 盲测不读源码 → 阶段2 分析定案 + 独立审查复核（真伪三分类）→ 阶段3 修复子代理在独立 feature 分支逐步修复（需授权）→ 阶段4 盲测复测回归。新增固定覆盖任务书 `assets/fixed-task-brief.md`（12 功能单元 + 6 组合场景），不抽题、以覆盖全部 252 canonical 工具为硬验收，新增组合链连锁反应表；共享改进库 HARNESS_LIBRARY 随新技能迁移。「同上下文自审偏差」前提已联网核实（属实、证据强），并据文献把「独立 + 外部 grounding + 盲测」写成硬机制；报告产物统一改到 `.handoff/report/<轮目录>/`（不再放项目根目录）。同步更新 tests.md / gamejam.md / tests/README.md 及历史 changelog 引用。详见 [changelog/2026-11-21-log.md](changelog/2026-11-21-log.md)。

## 2026-11-19

- **mcp-cpp-sdk 0.3.12 依赖升级**：gitlink `93e18ef` → `7a62e63`（tag `0.3.12` dereferenced commit `7a62e636bceeab63c07d820b85a60414a430ea06`），修复 Windows 上 FileTaskStore 压缩句柄冲突导致的 task store 中毒；六处版本 pin 同步为 `0.3.12` / `7a62e63`，页头审计日期与 frontmatter `timestamp` 同步，历史条目未改写；CLI 中毒诊断兜底保留。GDA 业务源码零改动，不升版。详见 [changelog/2026-11-19-log.md](changelog/2026-11-19-log.md)。

## 2026-11-18

- **会话可靠性增强（ST-01 行为修复归并概述）**：任务存储路径回归集中根 `<gda_home>/.godot-autopilot/tasks/`（tasks_enabled lifecycle path 同值）；SDK 日志桥装配时抬到 ≥Error 并路由 LogSystem（task store 前缀归 Tools）；CLI stop/restart 优雅失败识别 `task store: failed to persist` 中毒签名并附 `--force` 风险提示（退出码不变）；cancel 消息 params.request_id 对齐游戏桥契约；编辑器侧 eval 超时回执增 `timeout_cause`（script_error/channel_lost/unknown）与 `game_ready_timeout`；ready 门槛 SendNow 要求 ready>0 且 scene_ready>0，gda:ready 增 scene_ready 字段并在场景入树后补发；编辑器启动窗（无场景根且启动 5000ms 内）create/play 失败改报 `startup_in_progress`（retryable）；capture 退化（≤8px/零尺寸视口）拒绝静默成功——2D 退化触发 3D 回退、均不可用报 `no_scene_open`/`viewport_degenerate`（有意收紧，旧行为 ok+占位图）；传输请求号按 (connection_id, request_id) 去重并以内部 id 改写保障并发路由（跨连接同 id 由拒转受理，60s 宽限不变）。详见 [changelog/2026-11-18-log.md](changelog/2026-11-18-log.md)。

- **T-09 描述诚实化与文档走查（零代码行为改动）**：game_script 描述增补执行上下文（@tool wrap、/root 临时节点、相对路径基准、call 后销毁、persist 容器、启动窗建议）；game_input 增补 hold 到期自动释放/窗口内读回/event duration_ms=0 不释放/_unhandled_input 说明；include_hash 改「排序节点路径集合 FNV-1a 结构哈希（非内容证明）」并删除 byte-level 字样；capture 补退化错误迁移说明并修正 timeout 上限文案 30000→25000。setup 参考离线走查零 not-found（`gda call ping` 改 canonical `gda call system_status`，ping 为注册弃用别名；live 复核待 e2e）；EN/ZH 五篇 references 同批同步，守卫同款正则 parity 自检 10/10 一致；wiki 八页同步（entry_runtime/core/tools_registry/overview/security_contract/conventions/cli/engine-behavior-checklist）。本轮未编译未测试，守卫由主代理统一执行。详见 [changelog/2026-11-18-log.md](changelog/2026-11-18-log.md)。

## 2026-11-17

- **文档批量优化（信噪比收口，零代码改动）**：范围 `docs/wiki/`（非 changelog）、根 `AGENTS.md`、五语 `README*.md`、`.agents/skills/`。[tests.md](tests.md) 数值核算总表重基线到当前实测（单元 589 / ctest 885 / 域工具 246（35 域）/ 类别 28 / 破坏类 7 声明者 + 3 豁免 / add_library 117），CLI 单测统一为 37 文件 348 项（325 pass + 23 skip，本轮实测复跑）；清理 0.3.2 授权移除后的文档残留（tool_base_design 执行链改 confirm 门、conventions 守卫表更新为 12 个 `.ts` 载体、core.md 删除已不存在的 `allow`/`auth_mode` 函数、engine-behavior-checklist 域 2 改确认门口径、roadmap 控制面二期标注 0.3.2 移除、入口注册类 5→4、mermaid 端口 9527→20000–29999）；同步 4 个技能（cpp-registration/thread-security-gate/gda-e2e-loop/domain-tool-authoring）；四语 README 删除与安全说明重复的 confirm 提示行；AGENTS.md 双登记条目按现行 cmake/sources 机制精确化。所有数值均先静态复算或实测核验再写入。详见 [changelog/2026-11-17-log.md](changelog/2026-11-17-log.md)。

- **GDA Game Jam 技能与 e2e 沙盒分流**：新增 `gda-game-develop` 创作向技能（原名 `gda-gamejam`），默认走比赛流程 `jam` → `jam create` → `examples/<theme_id>/<slug>-<hash8>`，创建后立即 `build` + `deploy`，收尾 `jam index`。`gda-e2e-loop` 恢复 Game Jam 之前的 RSI 基线（保留 `Q`/`S`、递归闭环与证据体系），只补固定沙盒 `examples/e2e`、每轮 `jam e2e --reset`、创建/重置后装插件、报告目录 `report/<YYYY-MM-DD_HH-MM>/` 与“只记录不修复”边界。新增 `jam e2e [--reset]`；`jam.json.kind=e2e` 的沙盒不进入自动发现、`used` 与 `index`；比赛流程不变。同步更新 `gda-e2e-loop` 的报告路径扫描。验证：`bun run typecheck` 通过，`bun test cli/tests` 37 文件 / 348 项（325 pass + 23 skip + 0 fail）。详见 [changelog/2026-11-17-log.md](changelog/2026-11-17-log.md)。

- **Game Jam 生成器 v2 与 e2e 抽题**：题库升为 `schema_version: 2`、50 条，新增 `organizer` / `event` / `edition` / `year` / `source_title` / `source_kind` / `rules_url` / `verified_at` / `tags` / `design_constraints`，`source_url` 必填，官方来源优先、历史主题允许归档；每条只记录真实的设计/内容层创意约束（时间、引擎、团队、提交平台、素材授权、AI 限制等不写入）；赛事模板由用户固定维护，当前模板不渲染创意约束，约束仅保留在题库与 `jam.json`。抽题时关键词与概念灵感各自随机抽 3–5 条并打乱；`jam` 只生成 `examples/<theme_id>/project/` intake，`jam create` 再复制为 `examples/<theme_id>/<slug>-<hash8>/`，写入 `jam.json`；作品说明文档完全按 `GAMEJAM.md` 的提交要求处理，技能不额外约束。新增 `jam list [--projects]`、`jam index`、`devtools/lib/jam_projects.ts` 递归发现与 intake 跳过；`gda-e2e-loop` 改为「抽题 → 读题 → 命名 → 创建 → 按模板准备作品说明文档 → 开发 → 索引」。验证：`bun run typecheck` 通过，`bun test cli/tests` 37 文件 / 345 项（322 pass + 23 skip + 0 fail），`jam --list` 50 条与 CLI 抽题/创建/索引冒烟通过。完整 e2e 需用户在 Godot 中打开最终项目，未声称跑通。详见 [changelog/2026-11-17-log.md](changelog/2026-11-17-log.md)。

## 2026-11-16

- **版本标记改为开发版 `0.3.4-dev`**：根 `VERSION` 与 `CLI_VERSION` 同步为 `0.3.4-dev`；CMake 取数字前缀 `0.3.4` 建 project，完整字符串写入 `GDA_VERSION`，`gda status` / `--version` / MCP `server_info` 均显示后缀；`version_sync_guard` 放行预发布后缀，技能资产重新生成；五语 README 安装钉改为 `develop`。递增规则写入 `AGENTS.md`：发布格式固定 `0.y.x`，默认只增 `x`；只有用户明确要求才增 `y`；未经许可不得去掉 `-dev` 或发布。release workflow 的 tag 校验会阻止带后缀发布。

- **工具面压缩收口（T-25 文档）**：catalog 459→**252**（246 域 + 5 元 + `system_status`）、生产别名 **325**、schema **235/17**、能力矩阵 561 行 mapped 315 / missing 0；35 个域文件（新增 Image、Mesh）。重写 [overview.md](overview.md) 工具面与 [modules/tools_ops_a.md](modules/tools_ops_a.md) / [modules/tools_ops_b.md](modules/tools_ops_b.md)（按 canonical action 工具列表，旧名只留别名列与迁移说明），[tool_surface_baseline.md](tool_surface_baseline.md) 改为最终 252 基线，新增 [changelog/2026-11-16-log.md](changelog/2026-11-16-log.md) 压缩段；五语 README 的 badge/描述改为 252 并换用 canonical 示例名，非 EN/ZH 版标注以 EN 为准。旧名残留与相对链接已自查；本轮未编译、未测试，真实引擎 e2e 留待 T-26。详见 [changelog/2026-11-16-log.md](changelog/2026-11-16-log.md)。

- **示例工程退场（devtools 目标解耦）**：删除五语 README 的 `examples/` 素材声明；`deploy`/`clean` 改为可重复 `--project <dir>`（省略时自动发现 `examples/*`），`package` 默认从本机构建产物打包并新增 `--preset debug|release`；release workflow 直接从 `build/release/` 上传平台库；[gda-e2e-loop](../../.agents/skills/gda-e2e-loop/SKILL.md) 扩展为任意 Godot 工程路径。验证：typecheck、临时工程 deploy/clean 冒烟、`package --preset debug`、4 个守卫 6/6 通过。详见 [changelog/2026-11-16-log.md](changelog/2026-11-16-log.md)。
