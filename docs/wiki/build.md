---
type: 构建部署指南
title: 构建体系
description: 构建/部署/打包流程、CMake 模块职责、产物清单与环境变量
tags:
  - 构建
  - CMake
  - 部署
timestamp: "2026-10-23T02:39:34+08:00"
resource:
  - CMakeLists.txt
  - CMakePresets.json
  - main.py
  - scripts/
  - cmake/
  - third_party/
---

# 构建体系（build）

> 审计日期：2026-10-23（**10-23 随控制面文档同步批次同步**——本地 CLI 测试口径 193→**257**（`bun test cli/tests` 实测 255 pass / 2 skip / 0 fail，16 文件 257 项；Windows 跳过 2 项）；构建流程本身未变）2026-10-22（10-22 随本地收口批次同步——`ctest -N` 实测注册点 781→**806**（L1 gtest 734→**759** + L2 runner 41 + 守卫与脚本 6），见 [tests.md](./tests.md) 数值核算总表，CLI TS 单测 180→**193**；`validate` 作业的版本示例由 0.2.7 改 0.3.0）2026-10-21（10-21 随开放接口面落地批次同步——`add_library` 源文件 **93→113**（main 1 / core 15→**33** / tools 52→**54** / util 8 / prompts 9 / runtime 4 / resources 2 / ui 2），源清单改由 `cmake/sources/*.cmake` 聚合（`gda_core.cmake` + 五个 `api_*.cmake`，另有 `api_probe.cmake` 定义不注册为 ctest 的传输探针目标 `gda_mcp_transport_probe`）；环境变量表补 `GODOT_AUTOPILOT_HOME` / `GODOT_AUTOPILOT_API_TOKEN` / `GODOT_AUTOPILOT_CLI_HOME` / `GODOT_AUTOPILOT_USERDATA`，`GODOT_AUTOPILOT_PORT` 的口径改为三级解析的第 1 级且「绑定失败不自动换端口」；技能书文件数 34→**35**（新增 REST 接口参考），CLI TS 单测 132→**180**；10-20 随技能书调用阶梯收敛批次同步——技能书 36→**34 个 .md**（9 份 `SKILL.md` + 25 份 references，无脚本文件）；技能书自带的脚本桥 `skills/godot-autopilot-tools/scripts/gda.mjs` 与 `references/script-access.md` 删除，调用阶梯由五级收敛为三级（原生 MCP → GDA CLI → 手动兜底），`bunx`、全局安装与独立二进制合并为 GDA CLI 一档的三种分发方式；`embed_skills.py` 的 `scripts/` 目录通道与 `skill_gen.cmake` 的 `scripts/*.mjs` 依赖项保留（构建接线不变，无技能再使用该目录）；10-20 随知识库事实描述页收敛批次同步——`add_library` 源文件 89→**93**（逐项核对 `CMakeLists.txt:65-158`：main 1 / core 15 / tools 52 / util 8 / prompts 9 / runtime 4 / resources 2 / ui 2）；`src/tools/` 分项 48→52；ctest 注册点 431→**507**；「打包」小节按 `scripts/package.py` 与 `scripts/config.py` 重写——改用**临时暂存目录**收集「三平台库 + 6 个 C# 桥 .cs + .gdextension」，**不含 `.pdb`**，并说明 `--libs-dir` 与无参两条分支的差异；Release 作业新增 `scripts/test_package_integrity.py` 打包完整性自检（失败中止发布）；`deploy` 的 `--demos` 空参语义、`package` 包内含 6 个 C# 桥 `.cs` 与不含 `.pdb` 补入命令速查表；符号表补 `BRIDGE_DIR`/`BRIDGE_SOURCES` 与 `scripts/test_package_integrity.py`；行号漂移修正——`/WHOLEARCHIVE` 由 `CMakeLists.txt:168` 改为实读的 `:171-173`，`add_dependencies` 由 `:156` 改为实读的 `:160`；构建与打包流程本身未变，仅新增打包完整性自检这一步）；2026-10-19（10-19 随 CLI 引擎定位收窄批次同步——`gda start` 不再读 `GODOT_PATH` 与 `<project>/.env`（前者仅 L2 引擎测试使用，生产环境不存在），可执行文件链收敛为 `--godot` > PATH，未命中即报错提示 `--godot`；新增 `cli/tests/process.test.ts` 3 项（TS 108→111、expect 301→310）；10-19 随 mcp-cpp-sdk 0.3.7→0.3.8 与 OpenSSL 显式关闭批次同步——子模块 gitlink `7dfe088`→`d79f9b1`（tag `0.3.8`，commit `d79f9b16ffe512282a62b9bd6c786d0887ee6963`）；`cmake/Dependencies.cmake` 删除 `set(CMAKE_DISABLE_FIND_PACKAGE_OpenSSL ON)`、改为在 `add_subdirectory` 前 `set(MCP_WITH_OPENSSL "OFF" CACHE STRING "OpenSSL integration: AUTO/ON/OFF" FORCE)`（SDK 0.3.8 自带三态开关，GDA 仅监听 127.0.0.1 明文 HTTP、无 TLS 需求，故显式 OFF，交由 SDK 统一裁决）；configure 实测 `[mcp] SDK version: 0.3.8` 与 `[mcp] OpenSSL disabled (MCP_WITH_OPENSSL=OFF)`，`compile_commands.json` 中 `MCP_HAVE_OPENSSL` 0 次，构建 1254/1254 成功、ctest 非 L2 子集 440/440 全过（435 gtest + 2 守卫 + version_sync_guard + cli_contract + cli_unit）；L2 的 38 个 runner 需 Godot 引擎（本机 `GODOT_PATH` 未配置即失败，CI 亦仅跑 L1）；本页「依赖获取」表、「事实来源」两处版本口径同步）；2026-10-18（10-18 随 CLI 载体落地批次同步——新增「本地 CLI 开发」小节；详见 [changelog/2026-10-18-log.md](./changelog/2026-10-18-log.md)）；2026-10-16（10-16 随 mcp-cpp-sdk 0.3.4→0.3.7 迁移同步——子模块 gitlink `e2ed352`→`7dfe088`（tag `0.3.7`，commit `7dfe08876021b3c0aaaf3a375137d7e8ec7a219d`），8 处 `mcp::JsonValue::FromObject` 调用点就地改为 `mcp::JsonValue(mcp::JsonValue::Object{...})`（`json_godot.hpp` 3 处、`environment_ops.cpp` 2 处、`nav_ops.cpp` 3 处；0.3.5 起该工厂函数已删除），`cmake/Dependencies.cmake` 版本注释与本页「依赖获取」表、「事实来源」两处版本口径同步；configure 实测 `[mcp] SDK version: 0.3.7`，L1 428/428 与升级前基线一致，L2 37/38（38 号 C# 桥用例失败系既有 `parse_csharp_classes` 缺陷，非本迁移引入）；10-16 晚随 `parse_csharp_classes` 基类解析缺陷修复批次同步——该缺陷已修复并复验：L2 全量 38/38（38 号转绿）、L1 全量 438、ctest 注册点 476（10-16 实测，见 [changelog/2026-10-16-log.md](./changelog/2026-10-16-log.md)）；10-10 晚随多配置生成器适配批次同步——Visual Studio 等**多配置生成器**下三个只在真跑起来才暴露的坑：① `cmake/CompilerOptions.cmake` 的 MSVC 分支补 `/Zc:lambda`（修 `error C3493`，MSVC 拒绝 lambda 经 `[&]` 隐式引用捕获局部 `const`/`constexpr` 变量，clang 接受同一写法）；② 多配置生成器的产物多一层配置名子目录（`build/debug/Debug/godot-autopilot.dll`、`build/debug/tests/Debug/gda_unit_tests.exe`），`scripts/config.py` 新增 `cached_generator()` 与 `artifact_dir()`，`scripts/deploy.py` 改用 `artifact_dir()`，`scripts/cmake_ops.py` 的私有 `_cached_generator()` 删除、上移复用；③ ctest 在多配置生成器下须 `-C <config>`，否则只注册 40 个测试且 3 个非 gtest 用例报 `Test not available without configuration`，修法为 `CMakePresets.json` 的 portable build/test 预设补 `configuration` 字段（CMake 自动转 `--config` / `-C`，Python 侧零改动）。实测 Windows 无 ninja 无 DevShell 下 L1 全量 394 通过、部署 5 demo + 测试床成功）；10-10 晚随构建环境自适应批次同步——[CMakePresets.json](../../CMakePresets.json) 的 configure 预设 2→6：`base-debug`/`base-release` 为 `hidden` 基座（承载 `binaryDir` 与 `cacheVariables`），派生 `debug`/`release`（显式 `generator: Ninja`）与 `debug-portable`/`release-portable`（省略 `generator` 走 CMake 常规发现），两条路径**共用同一 `binaryDir`**；build/test 预设各 4 个。`scripts/config.py` 新增 `resolve_preset()` 按 `shutil.which("ninja")` 选路；`scripts/cmake_ops.py` 新增 `_cached_generator()`（读 `CMakeCache.txt` 比对生成器并在切换时 AUTO-CLEAN）、`_subprocess_env()`（把当前解释器目录前置到子进程 PATH）；`cmake/skill_gen.cmake` 改 `find_package(Python3)` 优先。命令行接口零变更）；10-10 随依赖 submodule 化同步——`cmake/FetchDependencies.cmake` 更名为 `cmake/Dependencies.cmake` 并改为纯 `add_subdirectory`，FetchContent 全部移除；godot-cpp / mcp-cpp-sdk / googletest 改为 `third_party/` 下的 git submodule，版本由 gitlink commit 锁定（`10.0.0-stable`/`507ed9d`、`0.3.4`/`e2ed352`、`v1.15.2`/`b514bdc`），configure 零网络访问；AUTO-CLEAN、CI 缓存、AGENTS.md 对照段与依赖版本口径同步）；09-26 随 L2 测试床迁移同步——`Example/` 已移除，床改 `tests/testbed/`；2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill 内容外置化同步；09-10 随 7 册重构同步；09-13 晚随 A 组知识库审计修复批次同步——skill_templates 8 册/30 个 .md、server_context/FetchDependencies 行号重核、AGENTS.md/README 对照段更新；09-13 晚随 0.2.3→0.2.4 升版同步示例版本号；09-13 晚随 0.2.4 版知识库全量审计同步——GODOT_PATH 缺失行为修正（退出码 2 报错，非“失败/跳过”），全页其余事实复核一致；09-14 随 CI Windows Python 编码修复同步——ci/release workflow 增 `PYTHONUTF8`、`embed_skills.py` 强制 UTF-8 输出），基于当前工作树文件逐项核对（不依赖 git 历史）；09-15 随 Computer Use grounding 批次同步文档一致性段——`README.md` / `README_zh.md` 工具数口径已同步为 ~379（域工具），与 overview 的 379 域工具 / 可达 386 / catalog 387 一致；09-16 随失败修复批次同步——README 双语工具数口径更新为 ~384，与 overview 的 384 域工具 / 可达 391 / catalog 392 一致；09-16 随视觉辅助与坐标换算批次同步——wiki 实测 385 域工具 / 可达 392 / catalog 393（Capture 1→2），README 双语 ~384 为约数口径（差 1，可接受，待后续版本同步）；09-16 随 0.2.5 升版收尾同步——validate 示例 0.2.4→0.2.5（`VERSION`/AGENTS/changelog 早在 09-14 已同步，`GDA_VERSION` 已为 0.2.5）；09-17 随 T09+T12 文档同步批次更新 compatibility_minimum 4.3→4.7、godot-cpp 10.0.0-rc1→rc2（新增 GODOTCPP_API_VERSION "4.7" FORCE 覆盖旧缓存）、mcp-cpp-sdk 0.3.3→0.3.4；09-18 随知识库一致性审计与 README 拆分批次同步——`CMakeLists.txt` 行数 169→173（实测）、`README_zh.md` 已删除（中文 `README.md` + 英文 `README.en.md` 双版）故事实来源与文档一致性段改引新版 README 口径；09-19 随 ToolSpec 数据化重构同步文档一致性段——wiki 实测 391 域工具 / 可达 398 / catalog 399；09-20 随 Release 触发修复同步——tag 触发 `v*` 改双格式（`v*.*.*` / `[0-9]*.*.*`）+ `workflow_dispatch` 手动指定 tag，validate 去 `v` 后比对 `VERSION`；同日代码-文档一致性审计——add_library 分目录计数修正为 tools 48 / util 8 / runtime 4（总数 83 不变）；同日 0.2.6 升版——根 `VERSION` 0.2.5→0.2.6（单一来源），validate 示例同步至 `v0.2.6` ↔ `0.2.6`；09-20 晚随可重放监控批次同步——add_library 83→86（`src/core/` 9→12：log_persist / trace_recorder / sanitize_policy），块范围 `CMakeLists.txt:65-148`→`:65-151`；09-20 晚随依赖升级同步——godot-cpp 10.0.0-rc2→10.0.0-stable；mcp-cpp-sdk 维持 0.3.4（依赖升级范围经复核收缩，`FetchDependencies.cmake:20/29`，configure 实测 `[mcp] SDK version: 0.3.4`）；09-21 随可重放监控与日志系统批次同步——`add_library` 86→89（`src/core/` 12→15：新增 `monitor.cpp` / `monitor_env.cpp` / `perf_sampler.cpp`），`monitor.cpp` 进 L1 业务源、`monitor_env.cpp`（仅主线程）与 `perf_sampler.cpp`（依赖引擎 tick）不进 L1；09-25 随项目级技能层落地同步——`CMakeLists.txt` 行数 176→179、`add_dependencies` `:153`→`:156`、`WHOLEARCHIVE` `:165`→`:168`（均按当前工作树重核）。09-29 随技能书源标准目录迁移同步。
9（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill 内容外置化同步；09-10 随 7 册重构同步；09-13 晚随 A 组知识库审计修复批次同步——skill_templates 8 册/30 个 .md、server_context/FetchDependencies 行号重核、AGENTS.md/README 对照段更新；09-13 晚随 0.2.3→0.2.4 升版同步示例版本号；09-13 晚随 0.2.4 版知识库全量审计同步——GODOT_PATH 缺失行为修正（退出码 2 报错，非“失败/跳过”），全页其余事实复核一致；09-14 随 CI Windows Python 编码修复同步——ci/release workflow 增 `PYTHONUTF8`、`embed_skills.py` 强制 UTF-8 输出），基于当前工作树文件逐项核对（不依赖 git 历史）；09-15 随 Computer Use grounding 批次同步文档一致性段——`README.md` / `README_zh.md` 工具数口径已同步为 ~379（域工具），与 overview 的 379 域工具 / 可达 386 / catalog 387 一致；09-16 随失败修复批次同步——README 双语工具数口径更新为 ~384，与 overview 的 384 域工具 / 可达 391 / catalog 392 一致；09-16 随视觉辅助与坐标换算批次同步——wiki 实测 385 域工具 / 可达 392 / catalog 393（Capture 1→2），README 双语 ~384 为约数口径（差 1，可接受，待后续版本同步）；09-16 随 0.2.5 升版收尾同步——validate 示例 0.2.4→0.2.5（`VERSION`/AGENTS/changelog 早在 09-14 已同步，`GDA_VERSION` 已为 0.2.5）；09-17 随 T09+T12 文档同步批次更新 compatibility_minimum 4.3→4.7、godot-cpp 10.0.0-rc1→rc2（新增 GODOTCPP_API_VERSION "4.7" FORCE 覆盖旧缓存）、mcp-cpp-sdk 0.3.3→0.3.4；09-18 随知识库一致性审计与 README 拆分批次同步——`CMakeLists.txt` 行数 169→173（实测）、`README_zh.md` 已删除（中文 `README.md` + 英文 `README.en.md` 双版）故事实来源与文档一致性段改引新版 README 口径；09-19 随 ToolSpec 数据化重构同步文档一致性段——wiki 实测 391 域工具 / 可达 398 / catalog 399；09-20 随 Release 触发修复同步——tag 触发 `v*` 改双格式（`v*.*.*` / `[0-9]*.*.*`）+ `workflow_dispatch` 手动指定 tag，validate 去 `v` 后比对 `VERSION`；同日代码-文档一致性审计——add_library 分目录计数修正为 tools 48 / util 8 / runtime 4（总数 83 不变）；同日 0.2.6 升版——根 `VERSION` 0.2.5→0.2.6（单一来源），validate 示例同步至 `v0.2.6` ↔ `0.2.6`；09-20 晚随可重放监控批次同步——add_library 83→86（`src/core/` 9→12：log_persist / trace_recorder / sanitize_policy），块范围 `CMakeLists.txt:65-148`→`:65-151`；09-20 晚随依赖升级同步——godot-cpp 10.0.0-rc2→10.0.0-stable；mcp-cpp-sdk 维持 0.3.4（依赖升级范围经复核收缩，`FetchDependencies.cmake:20/29`，configure 实测 `[mcp] SDK version: 0.3.4`）；09-21 随可重放监控与日志系统批次同步——`add_library` 86→89（`src/core/` 12→15：新增 `monitor.cpp` / `monitor_env.cpp` / `perf_sampler.cpp`），`monitor.cpp` 进 L1 业务源、`monitor_env.cpp`（仅主线程）与 `perf_sampler.cpp`（依赖引擎 tick）不进 L1；09-25 随项目级技能层落地同步——`CMakeLists.txt` 行数 176→179、`add_dependencies` `:153`→`:156`、`WHOLEARCHIVE` `:165`→`:168`（均按当前工作树重核）。；09-26 随构建入口 main.py 化与 scripts/ 拆分同步——build.py 删除（main.py build/deploy/package/clean/test/tui 六子命令 + scripts/ 7 模块：config/cmake_ops/deploy/package/clean_ops/test_ops/tui），部署目标 Example/addons→demo/×5（--demos 可选子集），打包与 CI artifact 取首个 demo，TUI（rich 状态表 + questionary 复选/单选/确认）新增；questionary 2.1.1 入 pyproject 依赖
> 事实来源：`main.py` + `scripts/` 7 模块（config/cmake_ops/test_ops/deploy/package/clean_ops/tui）、`CMakeLists.txt`（183 行）、`CMakePresets.json`、`cmake/` 全部 7 个模块、`tools/embed_skills.py`、`.env.template`、根 `README.md` / `README.zh-CN.md` / `AGENTS.md` 构建段、`.github/workflows/{ci,release}.yml`。
> 09-25 技能脚本通道批次：skill 8→9 册，模板 35 md + 1 mjs + registry，嵌入校验扩展 `.mjs`/`scripts/`（`SKILL_COUNT = 9`）；构建接线不变（内容数据文件不进 add_library）。该脚本通道已于 10-20 随调用阶梯收敛为三档而删除，当前技能书为 35 个 .md（9 份 `SKILL.md` + 26 份 references），`scripts/` 目录通道与 `scripts/*.mjs` 依赖项仍保留在嵌入校验与构建接线中。

## 命令速查表

| 命令 | 行为 |
|---|---|
| `uv run main.py build` | Debug：配置 + 构建（不部署） |
| `uv run main.py build --release` | Release 配置 + 构建（不部署、不清理） |
| `uv run main.py deploy` | 部署 Debug 产物到 `demo/*/addons/godot-autopilot/`（6 个 demo）与 L2 测试床 `tests/testbed/addons/godot-autopilot/`（恒被部署，不受子集影响）；`--demos` **不传则部署全部 demo**，**传空**（`--demos` 后不跟任何名字）**则不部署任何 demo**（仅测试床照常刷新），`--demos 名称...` 只部署子集 |
| `uv run main.py deploy --release` | 先清理，再部署 Release 产物 |
| `uv run main.py build --release --package` | 不再支持：构建与部署已拆分，按 `build --release` → `deploy --release` → `package` 分步执行 |
| `uv run main.py package` | 仅打包已部署的 addons（不触发构建，未部署则报错退出）；包内含 6 个 C# 桥 `.cs`、**不含 `.pdb`** |
| `uv run main.py build --package --libs-dir <dir>` / `uv run main.py package --libs-dir <dir>` | 从 `<dir>` 递归收集三平台库合并部署后打包（CI Release 用后者；`build` 下 `--libs-dir` 须与 `--package` 同用，缺失任一平台库即报错退出）；包内含 6 个 C# 桥 `.cs`、**不含 `.pdb`** |
| `uv run main.py build --debug` | 显式 Debug（默认即为 Debug） |
| `uv run main.py clean` | 清理各 demo 的 `.godot` 与 `addons`（原仅 `--release` 顺带做，现独立子命令） |
| `uv run main.py test` | 跑测试（默认 L1 快筛；`--l2 [用例名]`、`--single <用例名>`、`--all` 全量，`--preset` 切预设） |
| `uv run main.py tui` | 交互式构建控制台（rich + questionary，需终端；覆盖构建/打包/清理/状态查看，demo 复选） |
| `cmake --preset debug && cmake --build --preset debug` | 手动构建（不部署）。预设名不带 `-portable` 后缀即**固定 Ninja**，须本机 PATH 有 ninja；无 ninja 时改用 `debug-portable` / `release-portable`，或直接用 `uv run main.py build` 由脚本自动选路（见「预设」小节） |

约束：`build`/`deploy` 各自的 `--release` 与 `--debug` 互斥，同时指定报错退出（`main.py:22/39`）；`package` 的 `--libs-dir` 缺任一平台库即报错退出；`--demos` 名称不在 6 个 demo 内时报错退出并列出可选名。

## 版本号单一来源（根 `VERSION` 文件）

版本号只在根目录 `VERSION` 文件维护一处，其余全部派生：

| 消费方 | 方式 |
|---|---|
| `CMakeLists.txt` | `project()` 前 `file(READ ...)` 读入并 `string(STRIP)`，作为 `project(... VERSION ...)` 实参 |
| C++（MCP `server_info`、`system_status.version`） | `configure_file(src/core/version.hpp.in → <build>/generated/version.hpp @ONLY)` 生成 `GDA_VERSION` 字符串宏；`server_context.cpp` / `register_all.cpp` 包含 `<version.hpp>` 引用 |
| `scripts/config.py` | `ADDON_VERSION = (PROJECT_ROOT / "VERSION").read_text().strip()`，用于打包产物名 |

升版流程：只改 `VERSION` 文件内容即可（重新 configure 后生效）。

## 构建流水线（main.py + scripts/）

### 1. 清理（`deploy --release` 顺带，或 `clean` 子命令）

`_clean()`（`scripts/clean_ops.py`）按 `DEPLOY_TARGETS` 逐个工程删除两类目录，**不触碰 `build/`**：

- `<工程>/.godot` — Godot 引擎缓存
- `<工程>/addons/godot-autopilot/` — 旧部署产物

`DEPLOY_TARGETS`（`scripts/config.py`）= 6 个 `demo/*` + L2 测试床 `tests/testbed`。清理刻意与部署同口径，否则会留下 demo 已是 Release 库、床仍是 Debug 库的幽灵态；代价是 `deploy --release` 后跑 L2 须先重新 `deploy`。

### 2. 配置 `_configure(preset)`（`scripts/cmake_ops.py`）

先由 [`scripts/config.py:resolve_preset()`](../../scripts/config.py) 把逻辑预设名 `debug|release` 解析为实际预设名（按 ninja 可用性在 `<preset>` 与 `<preset>-portable` 间二选一，两条路径共用 `build/<preset>/`），随后：

1. **生成器切换检测（configure 前主动执行）**：[`scripts/cmake_ops.py`](../../scripts/cmake_ops.py) 的 `_configure()` 调 [`scripts/config.py:cached_generator()`](../../scripts/config.py) 读 `build/<preset>/CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=` 条目——**类型在首个冒号之后**，故先按 `:` 拆出值再按 `=` 取值，否则解析不到；再与「本次期望是否 Ninja」比对，不一致则打印 `[AUTO-CLEAN] Generator changed: <旧> -> <Ninja|platform default>` 并清空该构建目录后重配。读取逻辑上移到 `config.py` 是因为 `artifact_dir()` 需按同一份「实际生成器」判定产物落点（见「scripts/ 模块职责」），`cmake_ops.py` 不再自带私有副本。
2. `cmake --preset <解析后的预设名>`；
3. 失败且 `build/<preset>/` 存在 → **AUTO-CLEAN**：删除该目录内除 `_deps/` 外的全部文件与目录（目录递归删除、文件 unlink），然后重试一次；
4. 依赖源码在 `third_party/` 子模块（不在 `build/` 下），因此清空 `build/<preset>/` 不会丢依赖；子模块二进制目录 `build/<preset>/_sub/` 会被一并删除并在下一次 configure 重建。脚本仍跳过名为 `_deps` 的条目（FetchContent 时代的残留目录名，保留无害）。

子进程环境：`_subprocess_env()` 把 `Path(sys.executable).parent` 前置到 `PATH`，`uv run main.py build` 下即 `.venv/Scripts`（必然含可用的 `python.exe`），从而让 [`cmake/skill_gen.cmake`](../../cmake/skill_gen.cmake) 的解释器探测不依赖系统 PATH 顺序，也不会命中 Microsoft Store 的 `python` 存根。

### 3. 构建 `_build(preset)`

`cmake --build --preset <解析后的预设名>`（`resolve_preset` 的返回值）。实际生成器由该预设决定：有 ninja 时为 Ninja，否则为平台默认生成器（并行度计算对两者一致生效）。`-portable` 预设带 `configuration` 字段（见「预设」小节），CMake 自动翻译为 `--config Debug|Release`，多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）据此把产物落进配置名子目录。

### 4. 部署 `_deploy(preset, demos)`（`scripts/deploy.py`）

逐个工程循环（`--demos` 未指定即全部 6 个 demo；`--demos` 传空则一个 demo 都不部署），循环体末尾恒追加 L2 测试床 `tests/testbed`，因此 `--demos` 子集部署同样会刷新床的插件。库源目录取 [`scripts/config.py:artifact_dir(preset)`](../../scripts/config.py) 而非 `BUILD_DIR / preset` —— 多配置生成器的产物多一层配置名子目录（见「多配置生成器的产物路径」小节）：

1. 创建部署目录，删除 Godot 热重载遗留的备份库（glob `~*`）；
2. 生成 `godot-autopilot.gdextension`（见下节）；
3. 复制平台库（缺失仅 WARN 不失败）；Windows 额外复制 `.pdb`（存在时）；
4. `_validate_addon_integrity()`（`scripts/deploy.py`）：解析 gdextension 中当前平台（`windows`/`linux`/`macos` 前缀）的 `[libraries]` 条目，逐个校验 `res://` 对应磁盘文件存在；缺失即报错并以退出码 1 结束（防止宿主工程导出失败）。

### 5. 打包 `_package_addon(libs_dir)`（`scripts/package.py`）

产出 `dist/godot-autopilot-<version>.zip`（`ADDON_VERSION` 读自根目录 `VERSION` 文件——版本号单一来源，与 `project()` 版本一致）。实现要点：

- **临时暂存目录收集**：不再就地改写已部署的 addons 目录，而是在 `tempfile.TemporaryDirectory(prefix="gda-package-")` 下建 `addons/godot-autopilot/` 暂存树，把内容收进暂存目录后由 `shutil.make_archive` 打包，zip 内的插件目录前缀固定为 `addons/godot-autopilot`（下游按此路径安装，不可变更），临时目录随上下文退出自动清理。
- **收集内容三件套**（`_collect_package_files`）：① 平台库（`_collect_platform_libs` 递归 rglob 取首个匹配）；② C# 桥源码（`_collect_csharp_bridge`，`BRIDGE_DIR` 下的 6 个 `.cs`，缺失即报错退出）；③ `godot-autopilot.gdextension`（`_generate_gdextension`）。**`.pdb` 不在收集范围，不随包分发。**
- **两条分支的差异**（`_package_addon`）：

| 分支 | 平台库来源 | 必需平台库 | 失败行为 |
|---|---|---|---|
| 传 `--libs-dir <dir>` | `<dir>` 递归收集三平台库 | 三平台齐全（`PLATFORM_LIBS` 全部值） | 缺任一平台库即报错退出（退出码 1） |
| 不传（无参） | 已部署的首个 demo 目录 `demo/2d_dodge_the_creeps/addons/godot-autopilot/` | 仅当前平台的库（按 `platform.system()` 取 `PLATFORM_LIBS`） | 目录不存在 → 报错「Addons not deployed; run a build first」；平台不在 `PLATFORM_LIBS` → 报错「Unsupported platform」 |

  两条路径产出同构内容：平台库 + C# 桥源码 + gdextension 描述文件，均不含 `.pdb`。
- **打包完整性自检**：`scripts/test_package_integrity.py <zip>` 独立校验（解压后逐条核对：三平台库齐全、C# 桥 6 个源文件齐全、`.gdextension` 的 `[libraries]` 条目逐一有对应文件、且包内不含 `.pdb`）；全通过打印 `package-integrity OK` 并退出 0，缺项报错退出 1，参数错误退出 2。Release 作业在打包后、发布前跑这一步，失败即中止发布。

## CI 与 Release

`.github/workflows/ci.yml` — develop push/PR 触发：三平台 matrix（ubuntu-latest / macos-latest / windows-2022）Debug 编译 + L1 测试 `ctest --preset debug -E "^gda_runner_"`（L2 需 Godot 不在 CI 跑）；ninja 由 `seanmiddleditch/gha-setup-ninja@v5` 装入 PATH，因此 `resolve_preset` 恒选 `debug` 而非 `debug-portable`（ctest 步骤直接写死 `ctest --preset debug`、不经过 `resolve_preset`，依赖这一前提成立）；sccache + `third_party/` 子模块缓存加速（`actions/checkout` 带 `submodules: true`；`actions/cache` 的 path 为 `third_party/godot-cpp`、`third_party/mcp-cpp-sdk`、`third_party/googletest`，key `submodules-debug-${{ runner.os }}-${{ steps.submodules.outputs.shas }}`，shas 取 `git ls-files -s third_party` 的 gitlink sha，子模块升级即换 key）。job 级 env 含 `PYTHONUTF8: "1"`（Windows runner 控制台为 cp1252，保证 `main.py` 及其子进程 `embed_skills.py` 的中文日志不触发 `UnicodeEncodeError`；脚本自身亦对 stdout/stderr 强制 UTF-8，本地 Windows 终端同样受益）。`release.yml` 的 job env 同。另有 `cli-bins` 作业：三平台各自 `bun build --compile` 出可执行文件并跑 `--version` / `list` 冒烟，**不发布**（发布走 `release.yml` 的 `cli` 作业）。

`.github/workflows/release.yml` — tag 双格式触发（`v*.*.*` / `[0-9]*.*.*`，另支持 `workflow_dispatch` 手动指定 tag）：

| job | 内容 |
|---|---|
| validate | 校验 tag 与根 `VERSION` 一致（`v` 前缀可选：`v0.3.0` / `0.3.0` ↔ `0.3.0`，版本号以根 `VERSION` 为准，当前 0.3.0），不一致 fail |
| build | 同 CI 环境（Ninja/sccache/msvc-dev-cmd，ninja 由 `seanmiddleditch/gha-setup-ninja@v5` 提供，故 `resolve_preset` 恒选 `release`；checkout 带 `submodules: true`，缓存同 CI 改为 `key: submodules-release-${{ runner.os }}-${{ steps.submodules.outputs.shas }}`），Release 构建后按精确文件名上传各平台库 artifact（天然排除 pdb） |
| cli | 5 个目标的 `bun build --compile` 独立二进制（`windows-x64` / `linux-x64` / `linux-x64-musl` / `macos-arm64` / `macos-x64`），macOS 产物 ad-hoc 签名（`codesign --force --sign -`），各自打 zip/tar.gz 后上传 artifact；二进制名带根 `VERSION` |
| package | 下载全部 artifact → `python main.py package --libs-dir dist` 合并 → **跑 `python scripts/test_package_integrity.py dist/godot-autopilot-*.zip` 自检（失败即中止发布）** → 重命名为 `addons.zip` → 收集 `cli-dist/` 下二进制并生成 `sha256sums.txt` → softprops/action-gh-release 一并发布（`addons.zip` + `gda-<version>-<platform>.*` + 校验和） |

macOS runner 为 ARM64，preset 设 `CMAKE_OSX_ARCHITECTURES=x86_64;arm64` 编译 universal 双架构（仅 Apple 平台生效），gdextension 对应条目为 `macos.{debug,release}.universal`。

## 产物清单

| 产物 | 位置 | 说明 |
|---|---|---|
| `godot-autopilot.dll` | `build/{debug,release}/`（多配置生成器再套一层 `Debug`/`Release`）→ 部署目录 | Windows 平台库 |
| `libgodot-autopilot.so` | 同上 | Linux |
| `libgodot-autopilot.dylib` | 同上 | macOS |
| `godot-autopilot.pdb` | 同上（Windows，存在时复制） | 调试符号；**仅部署目录，不进发布包** |
| `godot-autopilot.gdextension` | 部署目录（每次部署重新生成） | 入口 `entry_symbol = "GDExtensionEntryPoint"`、`compatibility_minimum = "4.7"`、`[libraries]` 6 条平台路径（windows/linux 为 `x86_64`，macos 为 `universal`） |
| 6 个 C# 桥 `.cs` | `csharp_bridge/`（`BRIDGE_DIR`）→ 部署目录与发布包内 | `BridgeCall.cs`/`BridgeConvert.cs`/`BridgeEvents.cs`/`BridgeGetSet.cs`/`GdaCSharpBridge.cs`/`GdaCSharpRuntime.cs`（`BRIDGE_SOURCES`）；缺任一即报错退出 |
| `dist/godot-autopilot-<version>.zip` | `dist/` | `--package` 产物（版本号取自根 `VERSION` 文件）；包内为「三平台库 + 6 个 C# 桥 `.cs` + `.gdextension`」，不含 `.pdb` |

**构建目录布局随生成器而变**：单配置生成器（Ninja / Unix Makefiles）直接落 `build/debug/godot-autopilot.dll`；多配置生成器（Visual Studio / Xcode / Ninja Multi-Config）**再套一层以配置命名的子目录**，实测（Windows，`CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026`，`CMAKE_CONFIGURATION_TYPES=Debug;Release;MinSizeRel;RelWithDebInfo`）：

| 产物 | 单配置生成器 | 多配置生成器 |
|---|---|---|
| 平台库 | `build/debug/godot-autopilot.dll` | `build/debug/Debug/godot-autopilot.dll`（同目录另有 `.exp`/`.lib`/`.pdb`） |
| L1 测试可执行 | `build/debug/tests/gda_unit_tests.exe` | `build/debug/tests/Debug/gda_unit_tests.exe` |
| L2 执行器 | `build/debug/tests/gda_test_runner.exe` | `build/debug/tests/Debug/gda_test_runner.exe` |

程序侧一律用 [`scripts/config.py:artifact_dir(preset)`](../../scripts/config.py) 取平台库源目录，不写死 `BUILD_DIR / preset`（见「scripts/ 模块职责」）。

部署目录：`demo/*/addons/godot-autopilot/`（6 个 demo；打包与 CI artifact 取首个）与 `tests/testbed/addons/godot-autopilot/`（L2 测试床，不入库、不参与打包）；`main.py` 与 CI/Release 均使用仓库实际目录大小写，大小写敏感平台不会产生分叉目录。

## CMake 目标

`add_library(godot-autopilot SHARED ...)` 共 **113 个 .cpp**（源清单由 `cmake/sources/*.cmake` 聚合，见下）：

| 目录 | 数量 | 目录 | 数量 |
|---|---:|---|---:|
| `src/main.cpp` | 1 | `src/tools/` | 54 |
| `src/core/` | 33（含 editor_readiness.cpp、editor_coords.cpp、09-20 新增 log_persist / trace_recorder / sanitize_policy、09-21 新增 monitor / monitor_env / perf_sampler、10-21 新增 gda_home / project_store / instance_registry / storage_paths / legacy_store / port_resolver / api_surface / api_auth / api_token / api_envelope / api_server / http_transport / http_route_table / http_stream_registry / godot_transport / mcp_http_shim / api_endpoints / api_server_routes） | `src/util/` | 8（含 scene_verify；skill_gen + skill_content_generated 构建期嵌入薄胶水） |
| `src/resources/` | 2 | `src/ui/` | 2 |
| `src/prompts/` | 9 | `src/runtime/` | 4（含 09-18 新增 game_bridge_verify） |

（分项求和 1 + 33 + 54 + 8 + 9 + 4 + 2 + 2 = 113，与清单逐行核对一致；磁盘 `src/**/*.cpp` 共 113 个，全部已登记。）

- **源清单聚合机制（10-21 引入）**：`add_library` 的源清单不再直接写在根 `CMakeLists.txt` 里，而是拆到 `cmake/sources/` 下的若干清单文件，由根 `CMakeLists.txt` 依次 `include` 后拼成目标源列表——`gda_core.cmake`（基础业务源）、`api_core.cmake` / `api_contract.cmake` / `api_storage.cmake` / `api_transport.cmake` / `api_cli.cmake`（各并行分支独占维护的新增编译单元）；`tests/CMakeLists.txt` 侧对应 `gda_unit_sources.cmake` 与 `api_unit_*.cmake`。目的是让并行开发同一插件的多个分支各自登记新增 `.cpp`，避免在同一文件里产生归并冲突。`cmake/sources/api_probe.cmake` 另定义一个可运行的传输探针目标 `gda_mcp_transport_probe`（**不注册为 ctest 用例**，由验收阶段按需执行）。

- **skill 内容嵌入头**：9 册技能正文存于仓库根 `skills/` 标准目录（35 个 .md，**不进 add_library**），`cmake/skill_gen.cmake` 在构建期经 `tools/embed_skills.py` 生成 `build/<preset>/generated/skill_content_embedded.h`（gitignore 覆盖）；`add_dependencies(godot-autopilot gda_skill_embed_header)` 保证生成先于编译；
- 私有头文件目录：`${CMAKE_SOURCE_DIR}/src`；
- 链接库（PRIVATE）：`godot-cpp`、`mcp-server`、`mcp-http`（后两者来自 mcp-cpp-sdk）；
- MSVC（含 clang-cl）额外 `target_link_options "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>"`（`CMakeLists.txt:172`，处于 `if(MSVC)` 块 `:171-173`）—— 强制导出 godot-cpp 全部符号，防止 GDExtension 入口符号被链接器裁剪；
- **Unity 构建已接线生效**：`set_target_properties(godot-autopilot PROPERTIES UNITY_BUILD ${GDA_UNITY_ENABLED} UNITY_BUILD_BATCH_SIZE ${GDA_UNITY_BATCH})`（`CMakeLists.txt`，add_library 之后）——实测 Debug batch=8（16 核）；此前仅 BuildOptimization.cmake 计算参数、未挂到 target；
- `option(GDA_ENABLE_TESTS ... OFF)`，开启后 `add_subdirectory(tests)`（详见 [tests.md](./tests.md)）。

## cmake/ 模块职责

按根 `CMakeLists.txt:52-58` 的 include 顺序：

| 文件 | 职责 | 关键变量 |
|---|---|---|
| `Platform.cmake` | 架构与 CI 探测 | `GDA_ARCH`（`x86_64`/`x86`，按指针宽度）、`GDA_IS_CI`（进程环境存在 `CI` 即 ON） |
| `BuildOptimization.cmake` | 硬件感知并行度（核心规则） | `GDA_COMPILE_JOBS`、`GDA_LINK_JOBS`（CACHE 优先、其次 ENV）、`GDA_UNITY_BUILD`（ON 且核数 >1 时启用）、`GDA_UNITY_BATCH_SIZE`（0=auto）、`GDA_MAX_COMPILE_MEM_MB`（默认 1500）、`GDA_MAX_LINK_MEM_MB`（默认 4000）、`GDA_UNITY_MEM_MB`（默认 500）；`CMAKE_BUILD_PARALLEL_LEVEL` 对**所有**生成器生效；`gda_compile_pool`/`gda_link_pool` 仅在 `if(CMAKE_GENERATOR MATCHES "Ninja")` 时注册（`:84-91`），非 Ninja 只打印 `[gda] Ninja jobs: N (non-Ninja generator)`，其余并行度与 Unity 计算照常 |
| `CompilerOptions.cmake` | 按编译器分发 flags | Clang/clang-cl 与 MSVC：`/utf-8 /bigobj /W4 /EHsc` + `_CRT_SECURE_NO_WARNINGS`、`_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`、`_WIN32_WINNT=0x0A00`；**MSVC 分支（`elseif(MSVC)`，即真正的 cl.exe）额外 `/Zc:lambda`**，clang-cl 走的是第一个 `if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")` 分支、不加该开关（见下「MSVC lambda 捕获差异」小节）；MSVC 调试信息 `Embedded`；GCC/Clang 非 Windows 非 CI 追加 `-march=native`；`find_package(Threads REQUIRED)`；`CMAKE_POSITION_INDEPENDENT_CODE ON` |
| `Cache.cmake` | 编译缓存自动探测 | 优先 sccache（支持 MSVC），回退 ccache（仅 GCC/Clang）；命中则设 `CMAKE_C/CXX_COMPILER_LAUNCHER` |
| `Dependencies.cmake` | `third_party/` 子模块依赖（纯 `add_subdirectory`，无 FetchContent） | `GDA_THIRD_PARTY_DIR`（`${CMAKE_SOURCE_DIR}/third_party`）、`GDA_SUBMODULE_BIN_DIR`（`${CMAKE_BINARY_DIR}/_sub`）、`gda_require_submodule(<name>)`（子模块缺失即 `FATAL_ERROR` 并提示 `git submodule update --init --recursive`，故意不做网络回退）、`MCP_WITH_OPENSSL "OFF"`（CACHE STRING ... FORCE，仅 `add_subdirectory(mcp-cpp-sdk)` 前设置；SDK 0.3.8 起三态开关 AUTO/ON/OFF，GDA 仅监听 127.0.0.1 明文 HTTP 无 TLS 需求，故显式 OFF）、`GODOTCPP_API_VERSION "4.7"`（CACHE/FORCE）；`godot-cpp` @ `10.0.0-stable`（commit `507ed9d`）、`mcp-cpp-sdk` @ `0.3.8`（commit `d79f9b1`） |
| `Lto.cmake` | 仅 Release 的链接优化 | 优先级 Clang ThinLTO（`-flto=thin`）> MSVC LTCG（`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`）> GCC IPO（`CheckIPOSupported`）；非 Release 直接跳过 |
| `skill_gen.cmake` | skill 标准目录构建期嵌入（09-08 新增） | `GDA_PYTHON_EXECUTABLE`（**先** `find_package(Python3 QUIET COMPONENTS Interpreter)`，命中取 `Python3_EXECUTABLE`；否则回退 `find_program(NAMES py python python3)`；两者皆无才 `FATAL_ERROR` 并提示改用 `uv run main.py build`）+ configure 期 `--version` 可用性自检（失败 FATAL_ERROR）；`add_custom_command` 生成 `<build>/generated/skill_content_embedded.h` + `add_custom_target(gda_skill_embed_header)`；依赖 `skills/*/[SKILL.md|references/*.md|scripts/*.mjs]`（CONFIGURE_DEPENDS） |

解释器探测为何放宽：`find_package(Python3)` 会**真正执行解释器做验证**，因此不会命中 Microsoft Store 的 `python` 存根，且能识别 `python3.12` 这类带版本号的文件名；而 `find_program(NAMES py python python3)` 只认三个裸名字，uv/conda 等安装的版本化可执行名会漏掉。godot-cpp 已先调用过 `find_package(Python3)`，此处复用其结果。配合 `scripts/cmake_ops.py` 的 `_subprocess_env()`（见「构建流水线 → 配置」），`uv run main.py build` 下必能拿到 `.venv/Scripts` 里的可用解释器。

### 依赖获取（`third_party/` 子模块）

三个依赖由根 `.gitmodules` 管理为 git submodule，版本由 gitlink 锁定的 commit 唯一确定，不随上游 tag 移动（tag 仅作人类可读标识）：

| 子模块 | 上游仓库 | tag | commit |
|---|---|---|---|
| `third_party/godot-cpp` | `godotengine/godot-cpp` | `10.0.0-stable` | `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` |
| `third_party/mcp-cpp-sdk` | `jesspig/modelcontextprotocol-cpp-sdk` | `0.3.8` | `d79f9b16ffe512282a62b9bd6c786d0887ee6963` |
| `third_party/googletest` | `google/googletest` | `v1.15.2` | `b514bdc898e2951020cbdca1304b75f5950d1f59` |

- **检出方式**：`git clone --recurse-submodules <repo>`，或事后 `git submodule update --init --recursive`；子模块源码在 `build/` 之外，随仓库一同流转，不受清理 `build/` 影响。
- **子项目二进制目录**：`${CMAKE_BINARY_DIR}/_sub/<name>`（即 `build/<preset>/_sub/godot-cpp` 等），删除后可由 configure 重建。
- **缺失即失败**：`gda_require_submodule` 直接 `FATAL_ERROR` 并提示 `git submodule update --init --recursive`，**故意不做网络回退**——回退会让构建行为不确定，内网环境下会在 configure 中途抛出难以定位的 git 报错；因此 configure 阶段零网络访问。
- googletest 由 `tests/CMakeLists.txt` 在 `GDA_ENABLE_TESTS=ON` 时经同一 `gda_require_submodule` + `add_subdirectory` 引入（见 [tests.md](./tests.md)）。

### 并行度计算规则（`BuildOptimization.cmake`）

- **编译作业数** = `min(总内存/1500MB, 核数-2)`，下限 1；同时写入 `CMAKE_BUILD_PARALLEL_LEVEL`；
- **链接作业数** = `min(总内存/4000MB, 2)`，下限 1（链接内存密集，刻意收紧）；
- 作业池命名 `gda_compile_pool` / `gda_link_pool`，加 `gda_` 前缀是**刻意规避**与 mcp-cpp-sdk 自带 `compile_pool`/`link_pool` 的全局同名冲突（`BuildOptimization.cmake:83` 注释）；
- **Unity batch** = `min(总内存/500MB, (核数+1)/2)`，下限 2。

### 预设（`CMakePresets.json`）

version 8，**6 个 configure 预设**（2 个 hidden 基座 + 4 个可直接 `--preset` 使用），build / test 预设各 4 个：

| configure 预设 | `inherits` | `generator` | `binaryDir` | 说明 |
|---|---|---|---|---|
| `base-debug` | — | 不声明 | `build/debug` | `hidden: true`，仅供继承，**不可直接 `--preset` 使用** |
| `debug` | `base-debug` | `"Ninja"` | 继承（同一目录） | 检测到 ninja 时使用 |
| `debug-portable` | `base-debug` | 省略 | 继承（同一目录） | 无 ninja 时的回退 |
| `base-release` | — | 不声明 | `build/release` | `hidden: true`，仅供继承 |
| `release` | `base-release` | `"Ninja"` | 继承（同一目录） | 检测到 ninja 时使用 |
| `release-portable` | `base-release` | 省略 | 继承（同一目录） | 无 ninja 时的回退 |

- **两个基座承载全部共性**，经 `inherits` 复用不重复声明：`CMAKE_BUILD_TYPE` = Debug / Release、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`CMAKE_OSX_ARCHITECTURES=x86_64;arm64`（仅 Apple 平台生效，产出 universal 双架构库）、`GDA_ENABLE_TESTS=ON`。
- **两条路径共用同一 `binaryDir`**（`build/debug` / `build/release`），不是分开目录；切换生成器时由 [`scripts/cmake_ops.py`](../../scripts/cmake_ops.py) 的 AUTO-CLEAN 重建缓存（见下）。
- **省略 `generator` 的依据**：CMake 官方文档中 `generator` 字段 "If omitted, CMake will fall back to regular generator discovery procedure" —— 无 ninja 的机器因此仍可 configure（Windows → Visual Studio，Linux/macOS → Unix Makefiles）。
- **选路入口只有一处**：`scripts/config.py:resolve_preset()` 用 `shutil.which("ninja")` 探测，命中返回 `<preset>`，否则返回 `<preset>-portable`。动机：Windows 上 ninja 通常只存在于 VS 开发环境（DevShell），普通终端探测不到。
- **命令行接口零变更**：`uv run main.py build [--release]` / `deploy` / `package` / `clean` / `test [--preset debug|release]` / `tui` 全部照旧，`test --preset` 的可选值仍只有 `debug` / `release` 两个，实际 preset 名由 `scripts/test_ops.py` 内部调 `resolve_preset` 解析；`scripts/tui.py` 同理。手动 `cmake --preset` / `ctest --preset` 绕过脚本，必须自己按 ninja 可用性补 `-portable` 后缀。
- **build / test 预设**：`debug`、`debug-portable`、`release`、`release-portable` 四个，各自 `configurePreset` 关联同名 configure 预设。**`-portable` 的 build 与 test 预设额外声明 `configuration`**（`debug-portable` = `Debug`、`release-portable` = `Release`），Ninja 的 `debug`/`release` 不声明——多配置生成器下 ctest 缺 `-C` 会漏注册测试（见下「ctest 在多配置生成器下需要 `-C`」小节）。
- 根 [`CMakeLists.txt`](../../CMakeLists.txt) 在 `project()` 之前自动探测编译器（Windows 找 `clang-cl` 并加 `--target=x86_64-pc-windows-msvc`，非 Windows 找 `clang++-19/18/17/16` 回退 `clang++`；都找不到就交给系统默认），并在 `find_program(ninja)` 命中但生成器非 Ninja 时给出 `WARNING [gda] Ninja available (...) but generator is '...'. Use -G Ninja for faster builds.`。实测在无 DevShell 的普通 PowerShell 中，CMake 能自行发现 Visual Studio 18 并使用 MSVC，无需 vcvars 环境。

#### 生成器切换（已知坑）

`-portable` 预设省略 `generator` 时，CMake **会沿用 cache 里已有的生成器而非重新发现**。因此在装过 ninja 的 DevShell 里构建过一次、再切到探测不到 ninja 的普通终端时，`build/<preset>/CMakeCache.txt` 会留下不可用的 ninja 路径，后续构建必然失败。故 `_configure()` 在 configure 前主动比对并清空目录，而不是等 configure 自己报错——机制与实现见上文「构建流水线 → 配置」。`BuildOptimization.cmake` 的并行度与 Unity 计算对所有生成器一致生效。

#### MSVC lambda 捕获差异（已知坑）

上一批让「无 ninja 机器能 configure」之后，MSVC 路径的**首次全量编译**才暴露出第二个问题：

- **现象**：`src/tools/runtime_ops.cpp(891,33): error C3493` 与 `tests/unit/tool_catalog_test.cpp(93,27): error C3493`。
- **根因**：MSVC **拒绝 lambda 经 `[&]` 隐式引用捕获局部 `const`/`constexpr` 变量**，clang（clang-cl）接受同一写法。典型形态是函数内 `constexpr size_t N = 256;` 配合 `std::thread([&s]{ ... s.size() > N ... })`；经最小复现逐条定位，属 MSVC 实现开关而非源码违规 —— 源码本身合法，只在两个编译器间行为不一致。
- **修法**：[`cmake/CompilerOptions.cmake:48`](../../cmake/CompilerOptions.cmake) 的 MSVC 分支（`elseif(MSVC)`，即真正的 cl.exe；clang-cl 命中的是前面的 `if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")` 分支，不受影响）追加 `/Zc:lambda`。这是 MSVC 的 lambda 标准符合性开关，**CMake 默认不启用**。加上后**零源码改动**即可编译，并顺带拉齐了 clang 与 MSVC 的行为差异。

#### 多配置生成器的产物路径（已知坑）

- **现象**：在 Visual Studio 生成器下构建成功，但 `deploy` 报 `[WARN] godot-autopilot.dll not found in <dir>`，库根本没复制出去。
- **根因**：Visual Studio / Xcode / Ninja Multi-Config 等**多配置生成器**会把产物再放进一层**以配置命名的子目录**。实测落点：`build/debug/Debug/godot-autopilot.dll`、`build/debug/tests/Debug/gda_unit_tests.exe`、`build/debug/tests/Debug/gda_test_runner.exe`；单配置生成器（Ninja / Unix Makefiles）则直接落 `build/debug/godot-autopilot.dll`。对照表见「产物清单」。
- **修法**：[`scripts/config.py`](../../scripts/config.py) 新增 `cached_generator(target)`（读 `CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=`，**类型在首个冒号之后**，须先按 `:` 拆再按 `=` 取值）与 `artifact_dir(preset)`；后者按前缀表 `("Visual Studio", "Xcode", "Ninja Multi-Config")` 判定，命中则返回 `build/<preset>/Debug|Release`，否则返回 `build/<preset>`。**判定依据是构建目录里实际记录的生成器，而非预设名** —— 手动用 `--preset debug-portable` 而 PATH 恰有 ninja 时，CMake 发现的仍是单配置生成器，产物就在 `build/debug` 根下。消费方 `scripts/deploy.py:_deploy()`（`binary_dir = artifact_dir(preset)`）不再写死 `BUILD_DIR / preset`。
- **去重**：读取生成器的逻辑原先是 `scripts/cmake_ops.py` 的私有 `_cached_generator()`，现已删除、改为从 `config` 导入 `cached_generator` —— 全仓库只有一份实现，`artifact_dir()` 与 `_configure()` 共用。

#### ctest 在多配置生成器下需要 `-C`（已知坑）

- **现象**：只跑 `ctest --preset debug-portable` 时**只注册了 40 个测试**，且三个非 gtest 用例直接报 `Test not available without configuration. (Missing "-C <config>"?)`。
- **根因**：多配置生成器下 `gtest_discover_tests`（[`tests/CMakeLists.txt:91`](../../tests/CMakeLists.txt)）生成的**发现文件名带配置名**。实测 `build/debug/tests/` 下为 `gda_unit_tests_e3b0c442_Debug_tests.cmake`（另有 `_include.cmake` / `_discovery.cmake` 以及 Release / RelWithDebInfo / MinSizeRel 对应文件）；ctest 不带 `-C` 时不会 include 对应配置的发现文件，于是 gtest 用例整体缺席。
- **对照实测**：`ctest --test-dir build/debug -N` → **40** 个（多配置生成器下缺 `-C` 的缺省表现）；`ctest --test-dir build/debug -C Debug -N` → **806** 个（759 个 L1 gtest + 41 个 L2 runner + 6 个守卫/脚本，与 [tests.md](./tests.md) 口径一致）。
- **修法**：在 [`CMakePresets.json`](../../CMakePresets.json) 的 build/test 预设上加 `configuration` 字段（`debug-portable` = `Debug`、`release-portable` = `Release`；Ninja 的两个预设不加）。CMake 会自动把它翻译成 `--config` / `-C`，因此 **`scripts/test_ops.py` 与 `main.py` 零改动**，`uv run main.py test` 在两类生成器下行为一致。
- **注意**：这是「配置名」维度，与预设名正交 —— 手动执行 `ctest --preset debug-portable` 已自动带上 `-C Debug`；若绕过 preset 直接 `ctest --test-dir build/debug`，则须自己补 `-C Debug`。

## scripts/ 模块职责（7 个模块 + 1 个独立自检脚本）

| 模块 | 职责 |
|---|---|
| [`config.py`](../../scripts/config.py) | 路径与集合常量（`PROJECT_ROOT`/`BUILD_DIR`/`TESTBED_DIR`/`DEMO_DIRS`/`DEPLOY_TARGETS`/`PLATFORM_LIBS`）、**`BRIDGE_DIR`**（`PROJECT_ROOT / "csharp_bridge"`）、**`BRIDGE_SOURCES`**（6 个 C# 桥源文件名的必需清单，缺失即打包失败）、`ADDON_VERSION`（读根 `VERSION`，版本号单一来源）、**`resolve_preset()`**（按 `shutil.which("ninja")` 在 `<preset>` 与 `<preset>-portable` 间选路，生成器双路径的**唯一决策点**）、**`cached_generator(target)`**（读 `CMakeCache.txt` 的 `CMAKE_GENERATOR:INTERNAL=`，无缓存返回 `None`，生成器读取的唯一实现）、**`artifact_dir(preset)`**（按实际生成器判定多配置生成器的配置名子目录，返回 `build/<preset>/Debug|Release` 或 `build/<preset>`）、`resolve_demos()`（`None` → 全部 demo；空列表 → 不部署任何 demo；未知名字报错） |
| [`cmake_ops.py`](../../scripts/cmake_ops.py) | `_configure()` / `_build()`：preset 解析 → 从 `config` 导入的 `cached_generator()` 比对 cache 生成器、切换时 `_purge()` 后重配 → `cmake --preset` → 失败重试清目录重配一次；`_subprocess_env()` 把当前解释器目录前置到子进程 PATH；`_run()` 统一打印命令行并按返回码判成败 |
| [`test_ops.py`](../../scripts/test_ops.py) | ctest 薄封装（`run_l1`/`run_l2`/`run_single`/`run_all`）；preset 名经 `resolve_preset` 解析，故 `main.py test --preset` 的可选值仍只有 `debug`/`release` 两个 |
| [`deploy.py`](../../scripts/deploy.py) | 把 `artifact_dir(preset)` 指向的产物（**不是** `BUILD_DIR / preset`，多配置生成器下多一层配置名子目录）复制到 `DEPLOY_TARGETS`，生成 `.gdextension`，`_validate_addon_integrity()` 逐条校验 res:// 路径存在（库缺失时报错并以退出码 1 结束） |
| [`package.py`](../../scripts/package.py) | 打包首个 demo 的 addons；`_collect_platform_libs()` 递归收集三平台库、`_collect_csharp_bridge()` 收集 6 个 C# 桥 `.cs`、`_collect_package_files()` 汇总「平台库 + C# 桥 + gdextension」到**临时暂存目录**（不含 `.pdb`），再 `shutil.make_archive` 出 `dist/godot-autopilot-<version>.zip` |
| [`test_package_integrity.py`](../../scripts/test_package_integrity.py) | 打包完整性自检（纯标准库脚本）：解压 zip 后核对三平台库齐全、`BRIDGE_SOURCES` 6 个 C# 桥源文件齐全、`.gdextension` 的 `[libraries]` 条目逐一有对应文件、包内不含 `.pdb`；通过打印 `package-integrity OK` 退出 0，缺项退出 1，参数错误退出 2 |
| [`clean_ops.py`](../../scripts/clean_ops.py) | 按 `DEPLOY_TARGETS` 清 `.godot/` 与 `addons/godot-autopilot/`，**不碰 `build/`** |
| [`tui.py`](../../scripts/tui.py) | 交互式构建控制台；构建类型与测试预设的可选值仍只有 Debug/Release，走同一 `resolve_preset` |

## 环境变量

| 变量 | 作用 | 生效方式 |
|---|---|---|
| `GODOT_PATH` | 定位 Godot 可执行文件（L2 引擎内测试） | 进程环境变量优先，为空才回退仓库根 `.env`（复制 `.env.template`，不入库）；二者皆缺 → 执行器以退出码 2 报错（`tests/runner/main.cpp`，不自动跳过） |
| `GODOT_AUTOPILOT_PORT` | 覆盖端口（10-21 起为**第一优先级**：环境变量 ＞ 工程级配置偏好 ＞ 自动分配） | 运行时 `std::getenv`（`src/core/server_context.cpp`）；显式指定时绑定失败**不自动换端口**，直接启动失败 |
| `GODOT_AUTOPILOT_HOST` | 读取监听地址；默认 `127.0.0.1`，非环回地址由 `ServerContext::start()` 拒绝 | 运行时 `std::getenv`（`src/core/server_context.cpp`） |
| `GODOT_AUTOPILOT_HOME` | 覆写插件数据的家目录集中根（10-21 新增） | 插件侧：`HomeResolver::gda_home()` 的第 1 级来源（形状校验，不做存在性校验）；CLI 侧：`gdaHome()` 的第 1 级来源 |
| `GODOT_AUTOPILOT_API_TOKEN` | 覆写 REST 面的 Bearer 令牌（10-21 新增） | 插件侧：`ApiAuth::load_token()` / `issue_token()` 的第 1 级来源；设置后不再签发也不写令牌文件 |
| `GODOT_AUTOPILOT_CLI_HOME` | 覆写 CLI 的 state dir（10-21 新增） | CLI 侧：`registry.ts` 的 `stateDir()` 第 1 级来源 |
| `GODOT_AUTOPILOT_USERDATA` | 覆写 CLI 解析 Godot userdata 目录的位置 | CLI 侧：`platform.ts` 的 `userDataBase()` 第 1 级来源（仅影响 CLI 寻址链的旧路径回退） |
| `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` | 强制编译/链接并行度 | CACHE（`-D`）优先，其次进程环境变量 |
| `CI` | 存在即 `GDA_IS_CI=ON` | 隐式；关闭 `-march=native` 以保证可复现 |
| `GDA_UNITY_BUILD` / `GDA_UNITY_BATCH_SIZE` / `GDA_MAX_COMPILE_MEM_MB` / `GDA_MAX_LINK_MEM_MB` / `GDA_UNITY_MEM_MB` | Unity 与内存估算 | 仅 CMake 缓存参数（`-D`），**不支持环境变量** |

## 本地 CLI 开发（Bun）

CLI 载体（`cli/`）为 Bun + TypeScript 源码直跑，**不参与 `main.py build`/`deploy`/`package` 构建流水线**。本地开发只需 Bun：

- 跑测试：`bun test cli/tests`（255 通过 / 2 跳过，秒级）
- 类型检查：`bun x tsc -p cli/tsconfig.json --noEmit`
- 手动调用：`bun cli/bin/gda.ts --help`

CI 经 ctest `cli_unit`（封装 `bun test`）与 `cli_contract`（CLI 载体契约）接入，Bun 由 CI 环境提供；bun 缺失时两测试打印 SKIP 并退出 0。详见 [modules/cli.md](./modules/cli.md)。

## 与 AGENTS.md / README 对照

逐条核对结论（一致，另有一处精度补充）：

- `uv run main.py build` / `--release` 语义、手动 `cmake --preset` 命令 ✓（**精度补充**：手动命令绕过 `resolve_preset`，无 ninja 时须改写 `debug-portable` / `release-portable`，见「预设」小节）；
- "依赖以 `third_party/` 子模块随仓库检出" ✓（`cmake/Dependencies.cmake` 的 `gda_require_submodule` 缺依赖即 FATAL_ERROR，`.gitmodules` 锁定 commit）；
- "添加新 .cpp 时必须在 `add_library()` 中加入" ✓（Unity 构建只编译列出的文件）；**精度补充**：`skills/` 下 .md 为内容数据文件，经生成头机制（`skill_gen.cmake` + `tools/embed_skills.py`）进入编译，不进 add_library；
- 依赖版本 `godot-cpp 10.0.0-stable`（`507ed9d`）/ `mcp-cpp-sdk 0.3.8`（`d79f9b1`）、git submodule 检出 ✓（`cmake/Dependencies.cmake` + `.gitmodules`，含 `GODOTCPP_API_VERSION "4.7"` FORCE 与 `MCP_WITH_OPENSSL "OFF"` FORCE）；
- 编译器优先 Clang/clang-cl、MSVC/GCC 回退 ✓（根 CMakeLists 自动探测 + `CompilerOptions.cmake` 分发）；**精度补充**：MSVC 分支的 `/Zc:lambda` 是 clang-cl 路径不需要的符合性开关，仅为让 MSVC 接受 clang 已接受的写法而加（见「MSVC lambda 捕获差异」小节）；
- 优化自适应（sccache/ccache、LTO、Unity、生成器自适应 + Ninja 作业池）✓；**精度补充**：「生成器自适应」覆盖的不只是能否 configure，还包括多配置生成器下的产物路径（`artifact_dir`）与 ctest 配置维度（preset `configuration`）；AGENTS.md 与实现一致：仅 `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道，`GDA_UNITY_BUILD`/`GDA_UNITY_BATCH_SIZE` 等内存参数只接受 `-D` CACHE；
- **文档一致性**：`README.md` / `README.zh-CN.md` 工具数口径为 badge `tools-451+`（`README.md:8` / `README.zh-CN.md:8`）与“Over 451 tools”（`README.md:26`，域工具口径，数量随版本增长以 `search_tools` 查到的为准），与 wiki 实测的 [overview.md](./overview.md) **451 域工具**一致——wiki 另计的 458/459（MCP 可达 / catalog-index）含 7 元工具与 `system_status`，README 未列这两个数字属“域工具 vs 全量口径”差异；新版 README 已无独立构建章节（`README.md:70` 指向 wiki [build.md](./build.md) 与 `tests/README.md`），`--package` / `--debug` 参数以本页命令速查表为准。

## 关联页面

- [modules/core.md](./modules/core.md) — 核心模块（ServerContext、命令队列、模式检测）
- [tests.md](./tests.md) — L1/L2 测试体系与 `GDA_ENABLE_TESTS` 用法
- [overview.md](./overview.md) — 项目总览与工具统计
- [modules/support.md](./modules/support.md) — 支撑模块（技能书、客户端配置生成、资源与提示词）
