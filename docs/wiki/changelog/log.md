# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 天。

## 2026-09-13

- **mcp-cpp-sdk 0.3.2 → 0.3.3**：`FetchDependencies.cmake` GIT_TAG 升级，重新 configure 拉取 0.3.3 并全量编译通过（无 API 适配），`uv run build.py` 部署通过，ctest L1 114/114 全绿；0.3.3 含 conformance/MRTR、Bearer 鉴权、FileEventStore 会话恢复、传输层死锁修复等（本仓库未启用新特性）。详见 `changelog/2026-09-13-log.md`
- **反馈修复批次全量交付**：property 数组/NodeType 转换与写失败 fail fast、资源 CoW/`reload_resource`/`copy_resource_file`/标签清理、`create_editor_scene` timeout_ms 诊断、调试器无 session 无回退描述、截图经 call_tool 以 image content 交付、技能 8 册（含 C# 专册与开发闭环）、`resolve_resource` path 注册；L1 114/114、L2 8/8，计数 365 域/49 副作用/316 遍历/373 全量。详见 `changelog/2026-09-13-log.md`
- **游戏通道根因修复与收口批次**：`call_tool` 改在 MCP 线程执行（修复主线程等待阻塞消息泵导致的 game_runtime 工具全超时）、新增 `get_plugin_log`（域工具 365→366、可达 373、catalog/index 374）、严格 JSON 形状与值类型 readback、授权门配置持久化 + MCP Config 复选框、运行时通道启动自检与超时诊断、`capture_editor_viewport target=game` 非阻塞化；L1 126、L2 9、ctest 135（L2 需 `GODOT_AUTOPILOT_ALLOW`）；遗留 mcp-cpp-sdk 客户端偶发等待缺陷待跟进。详见 `changelog/2026-09-13-log.md`
- **跨组一致性审计修复（A/B 两组 15 页）**：逐页核对并修复行号/计数/目录结构/过时引用（A 组含 `example.md` 整页重写）；跨组复核以工具 `category` 字段纠正类别分布（Physics 47/Debug 16/Debugger 6，27 类=366），同步 overview 类别表、README 系列（~365→~366、Debugger 5→6）与 McpConfigDock 技能 tooltip 7→8；清理 `Example/gda_tmp_rename/`、`~godot-autopilot_0.pdb` 残留；A/B 模块清单合计 180+186=366 复核通过，L1 126/L2 9/ctest 135。详见 `changelog/2026-09-13-log.md`
- **主 skill（GDA 使用指南）增强**：`godot-autopilot` description 改以"使用 GDA 插件前必读"开头（概括连接前置/搜索优先发现/版本文档查询/状态观测/任务路由/错误水印）；`autopilot.md` 新增 Search techniques、Consult the engine documentation、Choosing the right tool、Watch the engine state 四块内容，既有章节语义与顺序不变；support.md 同步、frontmatter timestamp 更新。详见 `changelog/2026-09-13-log.md`
- **skill 统一纯英文**：`godot-autopilot` description 改为英文必读定位（"Required reading before using the GDA (godot-autopilot) plugin"，其余 7 册不动）、`scene-system.md` 中文错误提示引用改英文转述；全库扫描确认 `src/util/skill_templates/` 其余文件无中文字符；support.md 同步。详见 `changelog/2026-09-13-log.md`

## 2026-09-10

- **skill 体系 19→7 册重构 + Godot 源码研究发现织入**：19 册合并为 `godot-autopilot` 总纲 + 6 册引擎指南（每册带 references/ 渐进披露），84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册（4.7+/4.8 行内简注）；`skill_templates/` 28 个文件、registry 7 条、`SKILL_COUNT` 19→7；McpConfigDock 按钮 Generate/Update 动态化（Update 先清理 `godot-autopilot-` 前缀目录）；skill_gen_test 用例改名 + 白名单 176，ctest L1 103/103 全绿；support/tests/overview/AGENTS.md 知识库同步。详见 `changelog/2026-09-10-log.md`
- **版本号 0.2.2 → 0.2.3**：根 `VERSION` 单一来源变更（CRLF 保留），AGENTS.md/build.md 示例同步，GDA_VERSION 注入验证通过，ctest L1 103/103。详见 `changelog/2026-09-10-log.md`

## 2026-09-08

- **skill 内容外置化重构（迁移零改写 + 构建期嵌入）**：19 册正文从 7 个 `skill_content_*.cpp` 迁出为 `src/util/skill_templates/`（23 个 .md + registry.json）+ `tools/embed_skills.py` 构建期嵌入（5 项校验，生成头入 `build/generated/`，gitignore 覆盖）；`skill_gen.hpp` 收敛为 `make_embedded_skills()`；ctest L1 103/103 零测试改动。详见 `changelog/2026-09-08-log.md`
- **知识库审计与修复（三代理配对审计）**：tests.md 7 处数字修正并新增 skill_gen 测试小节、support.md 2 处精确性修正并补 project_path.hpp 小节、AGENTS.md 同步 103 gtest/110 注册点、index/overview/build 修复过时数字与 util 清单、security_contract 补 UI 写盘面、roadmap 补已交付条目。详见 `changelog/2026-09-08-log.md`
- **一键生成 Agent Skills（19 册 + 生成器 + UI 按钮 + 测试全绿）**：新增 `src/util/skill_gen` 纯函数生成器与 7 个内容单元，聚合 19 册英文 Agent Skills（agentskills.io 规范，4 册带 references/ 全工具表）；McpConfigDock 新增 "Generate Skills" 按钮写入 `.agents/skills/<name>/SKILL.md`（frontmatter version 注入 GDA_VERSION，覆盖写仅限自有 19 册命名空间）；SkillGenTest 7 用例，ctest L1 103/103。详见 `changelog/2026-09-08-log.md`
