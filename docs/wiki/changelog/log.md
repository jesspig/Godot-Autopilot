# 维护日志摘要

> 详细记录见 `changelog/<YYYY-MM-DD>-log.md`，每条记录 `<YYYY-MM-DD-HH>` 精确到小时；本摘要仅保留最近 7 条。

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
