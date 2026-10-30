# AGENTS.md

Godot Autopilot（GDA）：进程内 GDExtension，把 Godot 编辑器经 MCP（`127.0.0.1:9527/mcp`）暴露给 AI 客户端。C++17 + godot-cpp + mcp-cpp-sdk，工具经 `ToolSpec` 数据化注册。

本页是最小上下文入口；项目事实与文档规则见 `docs/wiki/index.md`。

## 命令

- 构建部署：`uv run main.py build` + `uv run main.py deploy`（子命令：`build` / `deploy` / `package` / `clean` / `test` / `tui`；`deploy` 下 `--release` / `--demos <名称...>`，`package` 下 `--libs-dir <dir>`）
- 手动构建：`cmake --preset debug && cmake --build --preset debug`
- 快速验证：`ctest --preset debug -E "^gda_runner_|^pool_"`（L1 + 守卫 + CLI，秒级；排除 L2 用例与池 fixture，避免拉起池）
- L2 池化：`uv run main.py test --all`（两阶段：阶段 A 串行跑 L1 + 守卫 + CLI，阶段 B 跑 L2 池化、默认 `-j 5`；`--legacy-l2` 回退 legacy 编排）
- CLI 单测：`bun test cli/tests`（Bun + TS，秒级）
- 版本只改根 `VERSION`，随后重新 configure

## 每次在场的硬约束

- **所有 Godot API 必须经 `CommandQueue` 在主线程执行**（`submit()` / 带预算的等待入口）；直接调用必崩，`main_thread_guard` 在 ctest 阶段静态拦截，编译通过不算过。`execute_sync()` 已无调用点且是无界等待，不得新增调用。
- **新增 `.cpp` 要在两处登记**：根 `CMakeLists.txt` 的 `add_library()` 与 `tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`（`src/tools/*_ops.cpp` 由 glob 自动纳入，只需登记非 `*_ops.cpp` 的源文件）。**漏登根 `CMakeLists.txt` 后果最重**：该编译单元不进插件产物，问题只在运行时暴露（L2 用例整体 ERROR，定位困难）；**漏登 `GDA_UNIT_BUSINESS_SOURCES` 仅在有 L1 用例引用其符号时才在 L1 链接期报错**，无引用时完全静默（等于没有覆盖）。详见 [.agents/skills/cpp-registration/SKILL.md](.agents/skills/cpp-registration/SKILL.md)。
- **依赖是 `third_party/` 下的 git submodule**：构建前必须已检出（`git clone --recurse-submodules`，或 `git submodule update --init --recursive`），缺失则 configure 直接报错，不做网络回退。
- **构建生成器双路径**：CMake 预设按 ninja 可用性自适应——有 ninja 用 `debug`/`release`，无则用 `debug-portable`/`release-portable`（共用 `build/{debug,release}`）；`main.py` 各子命令自动选路，手动 `cmake --preset` / `ctest --preset` 须自己补 `-portable` 后缀。见 [build.md](docs/wiki/build.md)「预设」。
- **源码禁止注释**：除 `// namespace` 结尾标记外不得有 `//` 或 `/*…*/`，解释写进 wiki；`comment_guard` 在 ctest 阶段拦，编译通过不算过。
- **默认安全姿态不可静默放宽**：仅绑环回、脱敏默认开、授权列表默认为空、导出期拒绝工具调用 / MCP 资源 / MCP 提示词三类入口。
- **数值一律以运行时统计为准**，权威口径见 `docs/wiki/tests.md` 的数值核算总表；本页不记录计数。
- **L2 池模式用例共享常驻编辑器会话**：新增用例须自清理场景与文件；累计态敏感用例（如调试器错误计数）加入 `tests/pool/pools.json` 的 `isolated`，不得依赖池复位清除会话级状态；池模式下用例的 `requires` 被忽略，授权由所在池的 `allow` 决定。
- **CLI 为 Bun+TS 源码直跑**：零运行时依赖、无构建步骤；发布流程另产出各平台独立二进制（5 个目标 + `sha256sums.txt`）。技能书钉 `#<VERSION>`（由 `version_sync_guard` 兜底）。

## 三层文档分工

| 层 | 位置 | 承载 |
|---|---|---|
| 入口 | 本文件 | 命令与硬约束 —— 保持最小、少变更 |
| 事实 | `docs/wiki/` | 陈述性事实：架构、模块行为、数值口径、历史结论与坑 |

- 一个知识只有一个权威落点，其它位置改成引用。
- 写入前判据：只是让人知道现状 → wiki；每次会话都必须在场且稳定 → 本页。

## 协作

- `git status` 的大量 `M` 多为 CRLF 伪变更（仓库无 `.gitattributes`），核查一律以 `git diff HEAD` 为准。
- 提交信息 `<type>(<scope>): <中文描述>`，scope 用中文功能域，按功能域拆分提交。
- 多代理并行时子代理只写代码不编译，主代理统一 configure/build 再分批测（并行编译会锁）；L1 禁调已注册 handler。
- 面向 MCP 客户端的技能书（`godot-autopilot-*`）由 `skills/` 模板生成到当前打开工程的 `res://.agents/skills/`；本仓库测试床路径为 `tests/testbed/.agents/skills/`。

## 知识库入口

`docs/wiki/index.md`（页面索引、三层分工细则、编写规则、维护步骤）；安全与并发契约见 `docs/wiki/security_contract.md`。
