# GDA (Godot Autopilot) 项目知识库

本知识库与源码同步维护，所有事实以当前代码为准；数值以运行时统计为准。维护记录见 [log.md](log.md)。

## 页面索引

| 页面 | 内容 | 对应代码 |
|---|---|---|
| [overview.md](overview.md) | 项目定位、整体架构、技术栈、目录结构、命名体系 | 全仓库 |
| [build.md](build.md) | 构建/部署/打包流程、CMake 模块、产物清单、环境变量 | `CMakeLists.txt`、`cmake/`、`main.py`、`scripts/` |
| [tests.md](tests.md) | L1/L2 测试体系、遍历排除清单、数值统计 | `tests/` |
| [conventions.md](conventions.md) | 工程约定：命名、日志、错误模式、添加工具流程 | 全仓库 |
| [security_contract.md](security_contract.md) | T0 安全边界与并发契约：可信客户端、监听、风险工具、主线程、生命周期与边界原则 | `src/core/`、`src/tools/`、`src/main.cpp` |
| [example.md](example.md) | 原 Example 示例工程的历史记录（当前部署与打包目标见 build.md，L2 测试床见 tests.md） | 原 `Example/`，现由 `demo/` 承接 |
| [tool_base_design.md](tool_base_design.md) | ToolSpec 工具数据层与执行管线：392 域工具全为 ToolSpec 数据记录、SpecTool 统一执行授权/后处理、dispatch 主线程等待预算、Args 取参器、用户脚本动态工具、迁移规则与守卫 | `tool_spec.hpp`、`tool_args.hpp`、`tool_pipeline.hpp`、`dynamic_spec_store.hpp`、`autopilot_tools.{hpp,cpp}`、30 个域 `*_tools.hpp`、`register_all.cpp` |
| [modules/core.md](modules/core.md) | 核心层：线程模型、端口、统一埋点门面（monitor/monitor_env/perf_sampler）、日志与本地持久化（日志/trace 双目录、脱敏）、配置常量 | `src/core/` |
| [modules/entry_runtime.md](modules/entry_runtime.md) | 插件入口与运行时桥接、gda 协议 | `src/main.cpp`、`src/runtime/` |
| [modules/tools_registry.md](modules/tools_registry.md) | 工具注册管线、ToolSpec 派生、元工具与动态工具、schema 统计、契约缺口与遍历排除 | `src/tools/`（register_all/dispatch/tool_catalog/tool_spec/tool_args/tool_pipeline） |
| [modules/tools_ops_a.md](modules/tools_ops_a.md) | 领域工具 A 组（场景/属性/输入/物理/导航/资源/脚本/配置/文档/编辑器 16 模块，197 工具） | `src/tools/*_ops.cpp` |
| [modules/tools_ops_b.md](modules/tools_ops_b.md) | 领域工具 B 组（调试/显示/OS/运行时/音频/渲染/瓦片等 18 模块，195 工具；A 197 + B 195 = 392） | `src/tools/*_ops.cpp` |
| [modules/support.md](modules/support.md) | 提示词、MCP 资源、UI、工具库 | `src/prompts/`、`src/resources/`、`src/ui/`、`src/util/` |
| [plans/roadmap.md](plans/roadmap.md) | 竞品对齐路线图：竞品定位速览与 P0-P3 批次交付状态 | 全仓库 |
| [.agents/skills/](../../.agents/skills/) | 项目级可执行技能：怎么做、何时停、做错的表现 | 全仓库流程与纪律 |

## 关键数值速查（以运行时统计为准）

- 工具注册总入口 `ToolRegistry`（单一来源）：**400 条目** = 392 域工具 + `system_status` + 7 元工具；域工具分 **27 类**（InputMap 并入 Input）；MCP 可达工具总数 **399** = 7 元 + 392 域
- schema：**400 = 338 非空 + 62 空**（L1 `SchemaStatisticsBaseline` 精确断言）；`03_tools_contract` 遍历 warnings 以运行时报告为准（基线 4 条：`start_input_gamepad_vibration`、`stop_input_gamepad_vibration`、`get_resource_extensions`、`reimport_resource_files`）
- 遍历排除 **71 个 `side_effect`/`mutating` 工具**（运行时经 `search_tools` 空 query 枚举 393 条 = 392 域 + `system_status`，跳过 7 元后按 `side_effect`/`mutating`/`dynamic` 字段排除，**322 个**进入空参+冒烟两步骤，上限 644 步；`dynamic` 分支为防回归保留）；L1 单元测试 **391 个 gtest**（36 个 unit 文件 / 37 suites）+ 迁移守卫 + 注释守卫 + `skill_scripts`（`ctest -E "^gda_runner_"` 共 **394**）；L2 引擎用例 **37 个文件**（00-10 共 11 份 + 11-20 共 10 份 + 22-30 共 9 份 + 31_theme_root/32_plane_inline/33_verified_inherit/34_slash_property/35_scene_tree_gate/36_stall_immunity/37_game_ready_gate 共 7 份；21 号段空缺，99 号诊断用例已删）；ctest 注册点 **431**（391 + 2 守卫 + `skill_scripts` + 37 L2；09-29 `ctest -N` 实测，L2 需引擎环境与 `GODOT_AUTOPILOT_ALLOW`，26/27 号另需 `user_tools`，32/34/35/36 号需 `code_execute`，16/23/24/35/37 号需 `game_runtime`）
- 工具命名规范：`<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首，如 create_scene_node、intersect_physics_2d_ray）
- MCP 端口 **9527**（`/mcp`），`GODOT_AUTOPILOT_PORT` 可覆盖；产物名 `godot-autopilot`

## 维护入口

代码改动后：更新受影响页面 → 同步页头"审计日期"（带日期行的页面：overview / build / tests / example / modules/tools_registry / tools_ops_a / support / core；无日期头的页面不新增）→ 更新 frontmatter 的 `timestamp`（真实系统时间，ISO 8601）→ 重核数值 → 追加 `changelog/<YYYY-MM-DD>-log.md`（按小时记录，[log.md](log.md) 仅留最近 7 天）→ 按 [项目级技能](../../.agents/skills/) 的分工核对是否要同步技能（见下"三层文档分工"）→ 需要时才改 [AGENTS.md](../../AGENTS.md)。

## 三层文档分工

同一个知识只允许有一个权威落点，不得两处并存写详细版本；发现并存时把细节留在权威落点、其它位置改成引用。

| 层 | 位置 | 承载 | 不承载 |
|---|---|---|---|
| 入口 | 根 `AGENTS.md` | 每次会话都必须在场的**最小**事实：命令、硬约束、分工规则、索引 | 架构细节、批次历史、计数、流程步骤 |
| 事实 | `docs/wiki/`（本页为入口） | **陈述性事实**：架构、模块行为、数值口径（`tests.md` 数值核算总表为唯一权威）、已知坑与历史结论 | 可执行的分步流程、检查清单 |
| 做法 | 根 `.agents/skills/*/SKILL.md` | **可执行指南**：流程步骤、决策判据、边界与反例、检查清单 | 易漂移的数值（改为指向 wiki 页）、一次性事实 |

- 维护时机与本项目一致：完成功能 / 交付指南 / 提交前。**知识库自我迭代时必须顺带核对受影响的项目级技能**——事实变了而技能里的步骤或判据失效，属于本层未同步，比 wiki 漏更危险（技能会被当作行动依据）。
- 技能与交付给 MCP 客户端的技能书不是一回事：后者以标准 Agent Skills 目录布局存于 `skills/`、由 skill_gen 构建期嵌入并渲染到当前打开工程的 `res://.agents/skills/godot-autopilot*`（仓库内即 `tests/testbed/.agents/skills/`），教的是"怎么用这套工具"；前者存于 `.agents/skills/`、教的是"怎么改这个仓库"，两者按目录隔离，互不删除。
- 归属不确定时按此判据：不写下来 agent 会做错动作 → 技能；写下来只是让人知道现状 → wiki；两者都不需要、但每次会话都要遵守 → `AGENTS.md`。

## 编写规则

- 语言简体中文；事实来源限本仓库代码、文档化行为与用户明确讨论结果，**禁止推测**。
- 更新前以 `git diff HEAD` 核查实际变更范围（`git status` 的 CRLF 伪变更不作依据）；无法核实的论断标 `> [!todo] 待补充`，不要写成结论。
- frontmatter 必含 `type`/`title`/`description`/`tags`/`timestamp`/`resource`，例外页仅 `index.md`、`log.md`、`changelog/*-log.md`。
- 每页至少 1 条相对链接指向仓库内真实文件；`changelog/*` 是历史记录页，只保证"新增条目至少 1 条相对链接"，不回溯补链。
- 只改受影响页；被取代的描述直接删除，不留"已废弃"" formerly"之类标记。
- 数值一律以运行时统计为准，口径以 [tests.md](tests.md) 的"数值核算总表"为准。
- 各页的"与 AGENTS.md 对照"表属审计记录，其中引用的 AGENTS.md 条目是 **2026-09-25 精简前**的版本（当时该文件承载架构与计数）；现行 `AGENTS.md` 只保留最小入口内容，不再逐条对应，遇到此类表格按"历史结论"读、以当前源码与本页正文为准。
