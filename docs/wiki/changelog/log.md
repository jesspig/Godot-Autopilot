# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 条。

## 2026-08-20

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
