# AGENTS.md

Godot Autopilot（GDA）：进程内 GDExtension，把 Godot 编辑器经 MCP（`127.0.0.1:9527/mcp`）暴露给 AI 客户端。C++17 + godot-cpp + mcp-cpp-sdk，工具经 `ToolSpec` 数据化注册。

本页只是最小上下文入口。**怎么做**看 `.agents/skills/project-*`，**是什么/多少**看 `docs/wiki/index.md`，两者都不在此重复。

## 命令

- 构建部署：`uv run main.py build` + `uv run main.py deploy`（子命令：`build` / `deploy` / `package` / `clean` / `test` / `tui`；`deploy` 下 `--release` / `--demos <名称...>`，`package` 下 `--libs-dir <dir>`）
- 手动构建：`cmake --preset debug && cmake --build --preset debug`
- 快速验证：`ctest --preset debug -E "^gda_runner_"`（L1 + 两守卫，秒级）
- 版本只改根 `VERSION`，随后重新 configure

## 每次在场的硬约束

- **所有 Godot API 必须经 `CommandQueue` 在主线程执行**（`submit()` / `execute_sync()`）；直接调用必崩。
- **新增 `.cpp` 要在两处登记**：根 `CMakeLists.txt` 的 `add_library()` 与 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`；漏后者只在 L1 链接期报错。
- **勿删 `build/<preset>/_deps/`**（FetchContent 缓存）。
- **源码禁止注释**：除 `// namespace` 结尾标记外不得有 `//` 或 `/*…*/`，解释写进 wiki；`comment_guard` 在 ctest 阶段拦，编译通过不算过。
- **默认安全姿态不可静默放宽**：仅绑环回、脱敏默认开、授权列表默认为空、导出期拒绝工具调用。
- **数值一律以运行时统计为准**，权威口径见 `docs/wiki/tests.md` 的数值核算总表；本页不记录计数。

## 三层文档分工

| 层 | 位置 | 承载 |
|---|---|---|
| 入口 | 本文件 | 命令、上面这几条硬约束、分工规则 —— 保持最小、少变更 |
| 事实 | `docs/wiki/` | 陈述性事实：架构、模块行为、数值口径、历史结论与坑 |
| 做法 | `.agents/skills/project-*/SKILL.md` | 可执行指南：步骤、判据、边界、反例、检查清单 |

- 一个知识只有一个权威落点，其它位置改成引用；技能里不复制易漂移数值。
- **知识库自我迭代时必须顺带核对受影响的项目级技能**：wiki 改了而技能的步骤/判据仍指向旧行为，比 wiki 过期更危险，因为技能会被当作行动依据。
- 写入前判据：不写下来会做错动作 → 技能；只是让人知道现状 → wiki；每次会话都必须在场且稳定 → 本页。

## 项目级技能索引

| 技能 | 何时用 |
|---|---|
| `project-building-gda` | 构建/部署/打包、源文件登记、升版 |
| `project-adding-domain-tools` | 新增领域工具与注册联动（flags、schema、守卫、契约） |
| `project-writing-tool-handlers` | 写/改 handler：取参、错误、回读、撤销、组合调用 |
| `project-running-tests` | L1/L2 运行与新增用例、授权前置、脏变更清理 |
| `project-instrumenting-traces` | 新增埋点、trace 字段与持久化、脱敏 |
| `project-diagnosing-with-traces` | 用日志/trace 排障：无响应、超时、授权、L2 失败 |
| `project-enforcing-security-gates` | 端口与绑定、能力授权门、脱敏、路径与导出边界 |
| `project-maintaining-knowledge` | 同步 wiki/技能/本页，判断新知识落点 |

## 协作

- `git status` 的大量 `M` 多为 CRLF 伪变更（仓库无 `.gitattributes`），核查一律以 `git diff HEAD` 为准。
- 提交信息 `<type>(<scope>): <中文描述>`，scope 用中文功能域，按功能域拆分提交。
- 多代理并行时子代理只写代码不编译，主代理统一 configure/build 再分批测（并行编译会锁）；L1 禁调已注册 handler。
- 面向 MCP 客户端的技能书（`godot-autopilot-*`，生成到当前打开工程的 `res://.agents/skills/`，本仓库内即 `tests/testbed/.agents/skills/`）与本仓库开发用 `project-*` 技能是两套东西，清理按前缀区分，互不影响。

## 知识库入口

`docs/wiki/index.md`（页面索引、三层分工细则、编写规则、维护步骤）；安全与并发契约见 `docs/wiki/security_contract.md`。
