# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-09-10

- **skill 体系 19→7 册重构 + Godot 源码研究发现织入**：19 册合并为 `godot-autopilot` 总纲 + 6 册引擎指南（每册带 references/ 渐进披露），84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册（4.7+/4.8 行内简注）；`skill_templates/` 28 个文件、registry 7 条、`SKILL_COUNT` 19→7；McpConfigDock 按钮 Generate/Update 动态化（Update 先清理 `godot-autopilot-` 前缀目录）；skill_gen_test 用例改名 + 白名单 176，ctest L1 103/103 全绿；support/tests/overview/AGENTS.md 知识库同步。详见 `changelog/2026-09-10-log.md`

## 2026-09-08

- **skill 内容外置化重构（迁移零改写 + 构建期嵌入）**：19 册正文从 7 个 `skill_content_*.cpp` 迁出为 `src/util/skill_templates/`（23 个 .md + registry.json）+ `tools/embed_skills.py` 构建期嵌入（5 项校验，生成头入 `build/generated/`，gitignore 覆盖）；`skill_gen.hpp` 收敛为 `make_embedded_skills()`；ctest L1 103/103 零测试改动。详见 `changelog/2026-09-08-log.md`
- **知识库审计与修复（三代理配对审计）**：tests.md 7 处数字修正并新增 skill_gen 测试小节、support.md 2 处精确性修正并补 project_path.hpp 小节、AGENTS.md 同步 103 gtest/110 注册点、index/overview/build 修复过时数字与 util 清单、security_contract 补 UI 写盘面、roadmap 补已交付条目。详见 `changelog/2026-09-08-log.md`
- **一键生成 Agent Skills（19 册 + 生成器 + UI 按钮 + 测试全绿）**：新增 `src/util/skill_gen` 纯函数生成器与 7 个内容单元，聚合 19 册英文 Agent Skills（agentskills.io 规范，4 册带 references/ 全工具表）；McpConfigDock 新增 "Generate Skills" 按钮写入 `.agents/skills/<name>/SKILL.md`（frontmatter version 注入 GDA_VERSION，覆盖写仅限自有 19 册命名空间）；SkillGenTest 7 用例，ctest L1 103/103。详见 `changelog/2026-09-08-log.md`

## 2026-09-02

- **安全与并行硬化全量落地（T1–T11）**：监听拒绝非环回、队列生命周期与背压、注册快照与 handler 原子替换、统一主线程入口、服务启停清理、pending 先登记、路径与能力分级、扫描/截图/变体限额、日志快照、构建大小写修复；同步 wiki timestamp 与安全契约。详见 `changelog/2026-09-02-log.md`
- **T0 安全边界与并发契约文档化**：新增 `security_contract.md`，明确可信客户端、监听范围、高风险工具、Godot 主线程、队列停止/重载、路径与响应大小边界；同步受影响 wiki 页面 timestamp。详见 `changelog/2026-09-02-log.md`
- **知识库一致性审计**：依据源码与提交实际变更更新安全、并发、运行时、构建、测试和 `AGENTS.md` 口径，清除过时描述；L1/ctest 非 runner 96/96 已验证。详见 `changelog/2026-09-02-log.md`

## 2026-08-29

- **版本 0.2.1 → 0.2.2 与全量知识库审计**：根 `VERSION` 单一来源更新为 `0.2.2`；零代码变更下重核 363 域/42 副作用/371 条目/370 可达/27 类/77 gtest/7 L2/84 ctest/77 .cpp/164 行等全量数值，增量修复 13 个 wiki 页面（overview/build/tests/tools_ops_a/b/tool_base_design/conventions/support/core/entry_runtime/index/example）与 `AGENTS.md`（83→84、6→7 份），frontmatter 与审计日期同步至 2026-08-29；`traversal.cpp` 枚举差异记 `> [!todo]`。详见 `changelog/2026-08-29-log.md`

## 2026-08-28

- **日志系统增强（仅日志，未涉及 MCP SDK / 环回绑定）**：日志 dock 标题 "MCP Log" → "GDA Log"；配置面板新增 "Show timestamps" 开关（默认开、show_time 持久化），每条前缀 `[HH:MM:SS]`，折叠合并行始终显示最新时间；ServerContext 关键流程补诊断日志、启动失败透传真实异常类型；call_tool 分发加 Debug 级日志。详见 `changelog/2026-08-28-log.md`
- **SDK 0.3.1→0.3.2 环回绑定**：支持 `bind_host`/`host`，GDA 默认绑 `127.0.0.1`（`GODOT_AUTOPILOT_HOST` 可覆盖为 `0.0.0.0` 以监听所有接口）。详见 `changelog/2026-08-28-log.md`

## 2026-08-24

- **竞品对齐批次 wiki 沉淀**：新建 plans/roadmap.md（竞品定位速记 + P0/P1/P2 交付清单 + P3 遗留）；tools_registry 类别表重算 27 类/363 域、371 条目口径、副作用 42 重分组；core.md 补 error_watermark/editor_readiness 小节；entry_runtime 补 input_sequence/ui_elements op 与 GameBridgeFrameSequence 帧调度；清理被删 debugger 工具死引用；AGENTS.md 数字同步（363/370/371/42/77/83）；reimport 字段名缺口确认已修。详见 `changelog/2026-08-24-log.md`

## 2026-08-23

- **CI/Release 工作流落地**：新增 `.github/workflows/ci.yml`（develop 触发，三平台 Debug + L1）与 `release.yml`（tag `v*` 触发：tag↔VERSION 校验 → 三平台 Release → 合并 `addons.zip` 发布）；build.py 增 `--package --libs-dir` 跨平台合并打包、gdextension macOS 条目改 universal；preset 设 `CMAKE_OSX_ARCHITECTURES` 双架构；版本升至 **0.2.1**；L1 71/71 验证通过。详见 `changelog/2026-08-23-log.md`

## 2026-08-22

- **全量代码-文档一致性审计**：零代码变更前提下逐页核对 24 个 wiki 页与 AGENTS/README/测试资产；修正 README ~339→~343、L1 分文件计数 77→71、L2 用例 5→6、监控项表 83→59、RENAME_HINTS 9→10、editor 计数 22→23、register_listener 注册类 4→6、排除清单分组 12+22+1、步数对齐实测 546；support.md 补四个 util 共享头小节；core/entry_runtime/build/overview 行号与语义残留清除。详见 `changelog/2026-08-22-log.md`
- **大规模死代码清理与跨域去重**：删 ArgReader/IExportGuard/IAsync/make_ok·error/add_meta/to_tool_info_all/is_registered/get_queue/start_time_/GDA_*_READY_WAIT_MS 等死代码；新建 util/json_godot、rid_registry、type_hint、gdscript_wrap 四个 header-only 头；7 份 find_node 归一 resolve_scene_node（tilemap 获 root/ 剥离修复）、RidStore 归一、json_number 约 80 处收敛；register_all 删 25 冗余 include 与 build_schema_for_none_by_name，dispatch 元名单派生化；修复 restart 后 BM25 索引翻倍（index.clear()）；Unity 构建接线生效 batch=8；L1 71/71 + L2 77/77，净删约 1400 行/56 文件。详见 `changelog/2026-08-22-log.md`
- **版本号单一来源化**：新增根 `VERSION` 文件为唯一真源；CMake `file(READ)` 喂 `project()`、`configure_file` 生成 `GDA_VERSION` 宏供 `server_info`/`system_status` 引用、`build.py` 运行时读取；版本升至 **0.2.0**；文档产物名泛化 `<version>` 占位。详见 `changelog/2026-08-22-log.md`

## 2026-08-21

- **遍历副作用排除 `side_effects()` 驱动**：`SideEffect` 扩 6 值（+Process）；`GDA_TOOL_CLASS_SIDE` 变体宏标记 35 个副作用工具；`get_tool_detail` 补 `side_effect` 字段；runner 读该字段排除、删除硬编码 `kExcludedSideEffectTools`。详见 `changelog/2026-08-21-log.md`
- **元工具接口+组合化**：新增 `IMetaTool` 标记接口 + `MetaTool`（ToolBase+IMetaTool，依赖组合注入）；`ToolRegistry::add()` 按 `dynamic_cast<IMetaTool>` 自动归类（实现接口即元工具）；7 个元工具改以 `MetaTool` 注册；新增 L1 路由断言。详见 `changelog/2026-08-21-log.md`
- **ToolBase 工具统一标准化全量落地**：新增 `tool_decl.hpp`（真类宏）+ 26 个 `<域>_tools.hpp`，**336 域工具悉数迁移为独立 `ToolBase` 子类**；`tool_defs.def` 删除，catalog(344)/index/分发/RegisterTool 全从 registry 派生。详见 `changelog/2026-08-21-log.md`

## 2026-08-20

- **ToolBase 标准化与 rename 事务化（4 新工具）**：`ToolBase`/`ToolRegistry` 单一来源 + 26 域真类化雏形；`rename_resource_file` 事务化（.uid 伴生、依赖回写、结构化影响报告）、`property_set` NodePath 自动转引用、新增 `get_resource_references`/`read_file`/`find_in_files`/`build_csharp_assembly`（339→343）；`collect_text_file_paths`/`join_path` 规避 `res:///` 三重斜杠；L2 新增 `05_rename_references`，ctest 78/78。详见 `changelog/2026-08-20-log.md`
