# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 条。

## 2026-08-21

- **遍历副作用排除 `side_effects()` 驱动**：`SideEffect` 扩 6 值（+Process）；`GDA_TOOL_CLASS_SIDE` 变体宏标记 35 个副作用工具；`get_tool_detail` 补 `side_effect` 字段；runner 读该字段排除、删除硬编码 `kExcludedSideEffectTools`；L1 78/78 + L2 6/6。详见 `changelog/2026-08-21-log.md`
- **元工具接口+组合化**：新增 `IMetaTool` 标记接口 + `MetaTool`（ToolBase+IMetaTool，依赖组合注入）；`ToolRegistry::add()` 按 `dynamic_cast<IMetaTool>` 自动归类（实现接口即元工具）；7 个元工具改以 `MetaTool` 注册；新增 L1 路由断言；L1 78/78 + L2 6/6。详见 `changelog/2026-08-21-log.md`
- **ToolBase 工具统一标准化全量落地**：新增 `tool_decl.hpp`（真类宏）+ 26 个 `<域>_tools.hpp`，**336 域工具悉数迁移为独立 `ToolBase` 子类**；`tool_defs.def` 删除，`register_all` 改为注册各域 `make_tools()` + system_status + 7 元工具，catalog(344)/index/分发/RegisterTool 全从 registry 派生；遍历 runner 改从 `*_tools.hpp` 枚举（修正 compare 偏移 11→10 bug）；L1 77/77 + L2 6/6（03 全遍历 546 步）；side_effects 遍历驱动未落地。详见 `changelog/2026-08-21-log.md`
- **重构收尾文档同步**：清除 `AGENTS.md`/overview/conventions/tools_ops_a/b/tests-README/tools_registry 对已删 `tool_defs.def` 的遗留引用与旧计数，统一为 336 域/344 catalog/35 副作用驱动排除；`get_debug_object_info` 归 Debug（Physics 47/Debug 16）；改动仅文档、未动代码。详见 `changelog/2026-08-21-log.md`

## 2026-08-20

- **ToolBase 工具统一标准化落地（ToolRegistry 单一来源）**：新增 `tool_base.hpp`/`tool_registry.hpp`/`fn_tool.hpp`；`register_all` 重构为 registry 单一来源，336 域 + system_status + 7 元全入静态 `ToolRegistry`，派生 catalog(344)/index/g_handlers/RegisterTool；修复函数局部对象悬垂（`g_active_registry` 升为文件级静态）；L1 77/77 + L2 6/6（含 03_tools_contract 336 工具全遍历）通过；逐域真类迁移 M1–M8 与 `side_effects()` 遍历驱动未落地。详见 `changelog/2026-08-20-log.md`
- **ToolBase 工具统一标准化设计定稿**（未实现，仅立项）：接口 + 组合取向，`ToolBase` 纯虚根 + 角色接口切片 + 域助手注入；明确"自动注册推导而非自动发现"（C++ 无反射）；`ToolRegistry` 取代三容器、`GD_TOOL_LIST` 类名清单、元工具最后注册、`side_effects()` 替排除清单。新增设计提案页 `tool_base_design.md` 并补录 index；未动任何代码。详见 `changelog/2026-08-20-log.md`
- **rename 事务化 + 覆盖度补齐**：`rename_resource_file` 改事务化——搬运 `.uid` 保 uid 不重生成（P0-2）、rename 前扫描 res:// 依赖并回写 `path` 引用（P0-1）、返回 `updated_files`/`stale_references`/`uid_preserved` 影响报告（P2-1）、`script_class` 不再经 `ResourceLoader.load` 确认（防重入崩溃）；`property_set` 对 Node 类型属性 + NodePath 自动转节点引用（P1-1，落 `node_paths`）；新增 4 个域工具（总数 339→343、领域 332→336）：`get_resource_references`/`read_file`/`find_in_files`/`build_csharp_assembly`（进程副作用入剔除 34→35）；全量 ctest 78/78 通过；详见 `changelog/2026-08-20-log.md`

## 2026-08-17

- **新增右侧栏 MCP 配置面板**（`McpConfigDock`）：端口 SpinBox + Apply 运行时重启（`ServerContext::restart`）+ 持久化到 `user://godot_autopilot/config.json`（`PluginConfig`，解析优先级 环境变量 > 持久化 > 默认 9527）；一键生成 8 个客户端（opencode / Claude Code / Codex / Cursor / Copilot / Trae / Qoder / WorkBuddy）项目级配置文件，JSON 智能合并、Codex TOML 已配置跳过；L1 新增 11 用例（77/77 全过）；详见 `changelog/2026-08-17-log.md`
- **知识库全量复核**：11 个概念页面新增 YAML frontmatter（type/title/description/tags/timestamp/resource）；数值修正——build.md 源 .cpp 68→70、index.md gtest 61→72 与 A 组 180→172、tools_registry.md 步数统一 410、core.md 16 文件、support.md util 6 组与行数修正、entry_runtime.md 行数修正、conventions.md 332 工具、example.md project.godot 32 行

## 2026-08-16

- **mcp-cpp-sdk 升级 0.2.2 → 0.3.1**（破坏性重构）：去 libhv/simdjson 换 SDK 自研网络栈与 JSON 解析器；项目仅改 3 处（FetchDependencies GIT_TAG、server_context.cpp 删 hlog_disable、tests/CMakeLists.txt 删 hv_static），API 面兼容已逐一验证；构建 + ctest 66/66 通过（含真实 MCP HTTP L2 闭环）；文档 18 处 libhv/simdjson 引用全量同步
- 知识库更新规则完善（AGENTS.md 新增审计日期同步/日志联动，index.md 维护入口同步）

## 2026-08-13

- 测试开关持久化：`GDA_ENABLE_TESTS` 固化进 `CMakePresets.json` debug/release 预设（清理 build/ 后自动恢复，不再丢测试注册）；debug 与 release 各 66/66 全量测试复验通过
- 新增 `reload_game_scripts` 运行中脚本热重载工具（引擎原生 reload_scripts 消息，领域 331→332、总数 339、ToolCatalog 343）；create_script 写盘读回校验（verified/readback）；eval 脚本错误经响应回传不再静默超时；call_method 支持 await 等待完成；输入瞬态物理帧限定文档补充；文档全量同步 + tests.md 补 L2 环境漂移说明

## 2026-08-12

- 新建知识库 `docs/wiki/`（9 个分析单元并发配对审计 + 数值核算）
- AGENTS.md 全面复核与修订（补 `--package`、docs/plan→docs/wiki、新增工程原则、知识库维护规则）
- 维护日志迁移至 `changelog/` 按天分文件，`log.md` 摘要仅留最近 7 条
- **领域工具全量重命名**（348→331：18 删、1 拆、3 类别迁移），规范改动词置首；总数 338、ToolCatalog 342、23 类；文档全量同步
