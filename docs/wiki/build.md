---
type: 构建部署指南
title: 构建体系
description: 构建/部署/打包流程、CMake 模块职责、产物清单与环境变量
tags:
  - 构建
  - CMake
  - 部署
timestamp: "2026-09-20T23:05:34+08:00"
resource:
  - CMakeLists.txt
  - CMakePresets.json
  - build.py
  - cmake/
---

# 构建体系（build）

> 审计日期：2026-09-19（2026-08-29 随 0.2.2 版本与全量审计同步；09-02 随安全与并行硬化同步；09-08 随 skill 内容外置化同步；09-10 随 7 册重构同步；09-13 晚随 A 组知识库审计修复批次同步——skill_templates 8 册/30 个 .md、server_context/FetchDependencies 行号重核、AGENTS.md/README 对照段更新；09-13 晚随 0.2.3→0.2.4 升版同步示例版本号；09-13 晚随 0.2.4 版知识库全量审计同步——GODOT_PATH 缺失行为修正（退出码 2 报错，非“失败/跳过”），全页其余事实复核一致；09-14 随 CI Windows Python 编码修复同步——ci/release workflow 增 `PYTHONUTF8`、`embed_skills.py` 强制 UTF-8 输出），基于当前工作树文件逐项核对（不依赖 git 历史）；09-15 随 Computer Use grounding 批次同步文档一致性段——`README.md` / `README_zh.md` 工具数口径已同步为 ~379（域工具），与 overview 的 379 域工具 / 可达 386 / catalog 387 一致；09-16 随失败修复批次同步——README 双语工具数口径更新为 ~384，与 overview 的 384 域工具 / 可达 391 / catalog 392 一致；09-16 随视觉辅助与坐标换算批次同步——wiki 实测 385 域工具 / 可达 392 / catalog 393（Capture 1→2），README 双语 ~384 为约数口径（差 1，可接受，待后续版本同步）；09-16 随 0.2.5 升版收尾同步——validate 示例 0.2.4→0.2.5（`VERSION`/AGENTS/changelog 早在 09-14 已同步，`GDA_VERSION` 已为 0.2.5）；09-17 随 T09+T12 文档同步批次更新 compatibility_minimum 4.3→4.7、godot-cpp 10.0.0-rc1→rc2（新增 GODOTCPP_API_VERSION "4.7" FORCE 覆盖旧缓存）、mcp-cpp-sdk 0.3.3→0.3.4；09-18 随知识库一致性审计与 README 拆分批次同步——`CMakeLists.txt` 行数 169→173（实测）、`README_zh.md` 已删除（中文 `README.md` + 英文 `README.en.md` 双版）故事实来源与文档一致性段改引新版 README 口径；09-19 随 ToolSpec 数据化重构同步文档一致性段——wiki 实测 391 域工具 / 可达 398 / catalog 399；09-20 随 Release 触发修复同步——tag 触发 `v*` 改双格式（`v*.*.*` / `[0-9]*.*.*`）+ `workflow_dispatch` 手动指定 tag，validate 去 `v` 后比对 `VERSION`；同日代码-文档一致性审计——add_library 分目录计数修正为 tools 48 / util 8 / runtime 4（总数 83 不变）；同日 0.2.6 升版——根 `VERSION` 0.2.5→0.2.6（单一来源），validate 示例同步至 `v0.2.6` ↔ `0.2.6`；09-20 晚随可重放监控批次同步——add_library 83→86（`src/core/` 9→12：log_persist / trace_recorder / sanitize_policy），块范围 `CMakeLists.txt:65-148`→`:65-151`；09-20 晚随依赖升级同步——godot-cpp 10.0.0-rc2→10.0.0-stable；mcp-cpp-sdk 维持 0.3.4（依赖升级范围经复核收缩，`FetchDependencies.cmake:20/29`，configure 实测 `[mcp] SDK version: 0.3.4`）。
> 事实来源：`build.py`（239 行）、`CMakeLists.txt`（176 行）、`CMakePresets.json`、`cmake/` 全部 7 个模块、`tools/embed_skills.py`、`.env.template`、根 `README.md` / `README.en.md` / `AGENTS.md` 构建段、`.github/workflows/{ci,release}.yml`。

## 命令速查表

| 命令 | 行为 |
|---|---|
| `uv run build.py` | Debug：配置 + 构建 + 部署到 `Example/addons/godot-autopilot/` |
| `uv run build.py --release` | 先清理，再 Release 配置 + 构建 + 部署 |
| `uv run build.py --release --package` | Release 构建部署后，再打包 `dist/godot-autopilot-<version>.zip` |
| `uv run build.py --package` | 仅打包已部署的 addons（不触发构建，未部署则报错退出） |
| `uv run build.py --package --libs-dir <dir>` | 从 `<dir>` 递归收集三平台库合并部署后打包（CI Release 用，须与 `--package` 同用，缺失任一平台库即报错退出） |
| `uv run build.py --debug` | 显式 Debug（默认即为 Debug） |
| `cmake --preset debug && cmake --build --preset debug` | 手动构建（不部署） |

约束：`--release` 与 `--debug` 互斥，同时指定报错退出（`build.py:204`）；`--libs-dir` 必须与 `--package` 同用（`build.py:208`）。

## 版本号单一来源（根 `VERSION` 文件）

版本号只在根目录 `VERSION` 文件维护一处，其余全部派生：

| 消费方 | 方式 |
|---|---|
| `CMakeLists.txt` | `project()` 前 `file(READ ...)` 读入并 `string(STRIP)`，作为 `project(... VERSION ...)` 实参 |
| C++（MCP `server_info`、`system_status.version`） | `configure_file(src/core/version.hpp.in → <build>/generated/version.hpp @ONLY)` 生成 `GDA_VERSION` 字符串宏；`server_context.cpp` / `register_all.cpp` 包含 `<version.hpp>` 引用 |
| `build.py` | `ADDON_VERSION = (PROJECT_ROOT / "VERSION").read_text().strip()`，用于打包产物名 |

升版流程：只改 `VERSION` 文件内容即可（重新 configure 后生效）。

## 构建流水线（build.py）

### 1. 清理（仅 `--release`）

`_clean()`（`build.py:181-188`）删除两类目录，**不触碰 `build/`**：

- `Example/.godot` — Godot 引擎缓存
- `Example/addons/godot-autopilot/` — 旧部署产物

### 2. 配置 `_configure(preset)`（`build.py:42-56`）

1. 先尝试 `cmake --preset <debug|release>`；
2. 失败且 `build/<preset>/` 存在 → **AUTO-CLEAN**：删除该目录内除 `_deps/` 外的全部文件与目录（目录递归删除、文件 unlink），然后重试一次；
3. `_deps/` 是 FetchContent 依赖缓存（godot-cpp、mcp-cpp-sdk、googletest），刻意保留 —— 删除会导致全部重新下载。

### 3. 构建 `_build(preset)`

`cmake --build --preset <preset>`（Ninja 生成器）。

### 4. 部署 `_deploy(preset)`（`build.py:82-108`）

1. 创建部署目录，删除 Godot 热重载遗留的备份库（glob `~*`）；
2. 生成 `godot-autopilot.gdextension`（见下节）；
3. 复制平台库（缺失仅 WARN 不失败）；Windows 额外复制 `.pdb`（存在时）；
4. `_validate_addon_integrity()`（`build.py:111-138`）：解析 gdextension 中当前平台（`windows`/`linux`/`macos` 前缀）的 `[libraries]` 条目，逐个校验 `res://` 对应磁盘文件存在；缺失即报错并以退出码 1 结束（防止宿主工程导出失败）。

### 5. 打包 `_package_addon(libs_dir)`（`build.py:153-178`）

将 `Example/addons/godot-autopilot/` 打包为 `dist/godot-autopilot-<version>.zip`（`ADDON_VERSION` 读自根目录 `VERSION` 文件——版本号单一来源，与 `project()` 版本一致）。传入 `--libs-dir <dir>` 时先经 `_collect_platform_libs()`（`build.py:141-150`）递归收集三平台库（dll/so/dylib，rglob 取首个匹配）复制进部署目录并重新生成 gdextension，任一平台库缺失即报错退出。

## CI 与 Release

`.github/workflows/ci.yml` — develop push/PR 触发：三平台 matrix（ubuntu-latest / macos-latest / windows-2022）Debug 编译 + L1 测试 `ctest --preset debug -E "^gda_runner_"`（L2 需 Godot 不在 CI 跑）；sccache + `_deps` 缓存加速。job 级 env 含 `PYTHONUTF8: "1"`（Windows runner 控制台为 cp1252，保证 `build.py` 及其子进程 `embed_skills.py` 的中文日志不触发 `UnicodeEncodeError`；脚本自身亦对 stdout/stderr 强制 UTF-8，本地 Windows 终端同样受益）。`release.yml` 的 job env 同。

`.github/workflows/release.yml` — tag 双格式触发（`v*.*.*` / `[0-9]*.*.*`，另支持 `workflow_dispatch` 手动指定 tag）：

| job | 内容 |
|---|---|
| validate | 校验 tag 与根 `VERSION` 一致（`v` 前缀可选：`v0.2.6` / `0.2.6` ↔ `0.2.6`，版本号以根 `VERSION` 为准，当前 0.2.6），不一致 fail |
| build | 同 CI 环境（Ninja/sccache/msvc-dev-cmd），Release 构建后按精确文件名上传各平台库 artifact（天然排除 pdb） |
| package | 下载全部 artifact → `python build.py --package --libs-dir dist` 合并 → 重命名为 `addons.zip` → softprops/action-gh-release 发布 |

macOS runner 为 ARM64，preset 设 `CMAKE_OSX_ARCHITECTURES=x86_64;arm64` 编译 universal 双架构（仅 Apple 平台生效），gdextension 对应条目为 `macos.{debug,release}.universal`。

## 产物清单

| 产物 | 位置 | 说明 |
|---|---|---|
| `godot-autopilot.dll` | `build/{debug,release}/` → 部署目录 | Windows 平台库 |
| `libgodot-autopilot.so` | 同上 | Linux |
| `libgodot-autopilot.dylib` | 同上 | macOS |
| `godot-autopilot.pdb` | 同上（Windows，存在时复制） | 调试符号 |
| `godot-autopilot.gdextension` | 部署目录（每次部署重新生成） | 入口 `entry_symbol = "GDExtensionEntryPoint"`、`compatibility_minimum = "4.7"`、`[libraries]` 6 条平台路径（windows/linux 为 `x86_64`，macos 为 `universal`） |
| `dist/godot-autopilot-<version>.zip` | `dist/` | `--package` 产物（版本号取自根 `VERSION` 文件） |

部署目录：`Example/addons/godot-autopilot/`；`build.py` 与 CI/Release 均使用仓库实际目录大小写，Linux/macOS 不会产生 `example/` 分叉目录。

## CMake 目标

`add_library(godot-autopilot SHARED ...)`（`CMakeLists.txt:65-151`）共 **86 个 .cpp**：

| 目录 | 数量 | 目录 | 数量 |
|---|---:|---|---:|
| `src/main.cpp` | 1 | `src/tools/` | 48 |
| `src/core/` | 12（含 editor_readiness.cpp、editor_coords.cpp、09-20 新增 log_persist / trace_recorder / sanitize_policy） | `src/util/` | 8（含 scene_verify；skill_gen + skill_content_generated 构建期嵌入薄胶水） |
| `src/resources/` | 2 | `src/ui/` | 2 |
| `src/prompts/` | 9 | `src/runtime/` | 4（含 09-18 新增 game_bridge_verify） |

- **skill 内容嵌入头**：8 册技能正文外置为 `src/util/skill_templates/`（30 个 .md + registry.json，**不进 add_library**），`cmake/skill_gen.cmake` 在构建期经 `tools/embed_skills.py` 生成 `build/<preset>/generated/skill_content_embedded.h`（gitignore 覆盖）；`add_dependencies(godot-autopilot gda_skill_embed_header)`（`CMakeLists.txt:153`）保证生成先于编译；
- 私有头文件目录：`${CMAKE_SOURCE_DIR}/src`；
- 链接库（PRIVATE）：`godot-cpp`、`mcp-server`、`mcp-http`（后两者来自 mcp-cpp-sdk）；
- MSVC（含 clang-cl）额外 `target_link_options "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>"`（`CMakeLists.txt:165`）—— 强制导出 godot-cpp 全部符号，防止 GDExtension 入口符号被链接器裁剪；
- **Unity 构建已接线生效**：`set_target_properties(godot-autopilot PROPERTIES UNITY_BUILD ${GDA_UNITY_ENABLED} UNITY_BUILD_BATCH_SIZE ${GDA_UNITY_BATCH})`（`CMakeLists.txt`，add_library 之后）——实测 Debug batch=8（16 核）；此前仅 BuildOptimization.cmake 计算参数、未挂到 target；
- `option(GDA_ENABLE_TESTS ... OFF)`，开启后 `add_subdirectory(tests)`（详见 [tests.md](./tests.md)）。

## cmake/ 模块职责

按根 `CMakeLists.txt:52-58` 的 include 顺序：

| 文件 | 职责 | 关键变量 |
|---|---|---|
| `Platform.cmake` | 架构与 CI 探测 | `GDA_ARCH`（`x86_64`/`x86`，按指针宽度）、`GDA_IS_CI`（进程环境存在 `CI` 即 ON） |
| `BuildOptimization.cmake` | 硬件感知并行度（核心规则） | `GDA_COMPILE_JOBS`、`GDA_LINK_JOBS`（CACHE 优先、其次 ENV）、`GDA_UNITY_BUILD`（ON 且核数 >1 时启用）、`GDA_UNITY_BATCH_SIZE`（0=auto）、`GDA_MAX_COMPILE_MEM_MB`（默认 1500）、`GDA_MAX_LINK_MEM_MB`（默认 4000）、`GDA_UNITY_MEM_MB`（默认 500） |
| `CompilerOptions.cmake` | 按编译器分发 flags | Clang/clang-cl 与 MSVC：`/utf-8 /bigobj /W4 /EHsc` + `_CRT_SECURE_NO_WARNINGS`、`_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`、`_WIN32_WINNT=0x0A00`；MSVC 调试信息 `Embedded`；GCC/Clang 非 Windows 非 CI 追加 `-march=native`；`find_package(Threads REQUIRED)`；`CMAKE_POSITION_INDEPENDENT_CODE ON` |
| `Cache.cmake` | 编译缓存自动探测 | 优先 sccache（支持 MSVC），回退 ccache（仅 GCC/Clang）；命中则设 `CMAKE_C/CXX_COMPILER_LAUNCHER` |
| `FetchDependencies.cmake` | FetchContent 依赖 | `godot-cpp` @ `10.0.0-stable`（godotengine/godot-cpp，GIT_SHALLOW）+ `GODOTCPP_API_VERSION "4.7"`（CACHE/FORCE，覆盖旧缓存）、`mcp-cpp-sdk` @ `0.3.4`（jesspig/modelcontextprotocol-cpp-sdk，GIT_SHALLOW）；`FETCHCONTENT_QUIET OFF`；文件头部注释明确"禁止删除 _deps/" |
| `Lto.cmake` | 仅 Release 的链接优化 | 优先级 Clang ThinLTO（`-flto=thin`）> MSVC LTCG（`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`）> GCC IPO（`CheckIPOSupported`）；非 Release 直接跳过 |
| `skill_gen.cmake` | skill 模板构建期嵌入（09-08 新增） | `GDA_PYTHON_EXECUTABLE`（`find_program(NAMES py python python3 REQUIRED)`，py launcher 优先）+ configure 期 `--version` 自检（失败 FATAL_ERROR）；`add_custom_command` 生成 `<build>/generated/skill_content_embedded.h` + `add_custom_target(gda_skill_embed_header)`；依赖 `src/util/skill_templates/` 全部 .md 与 registry.json（CONFIGURE_DEPENDS） |

### 并行度计算规则（`BuildOptimization.cmake`）

- **编译作业数** = `min(总内存/1500MB, 核数-2)`，下限 1；同时写入 `CMAKE_BUILD_PARALLEL_LEVEL`；
- **链接作业数** = `min(总内存/4000MB, 2)`，下限 1（链接内存密集，刻意收紧）；
- 作业池命名 `gda_compile_pool` / `gda_link_pool`，加 `gda_` 前缀是**刻意规避**与 mcp-cpp-sdk 自带 `compile_pool`/`link_pool` 的全局同名冲突（`BuildOptimization.cmake:83` 注释）；
- **Unity batch** = `min(总内存/500MB, (核数+1)/2)`，下限 2。

### 预设（`CMakePresets.json`）

version 8；`debug`/`release` 两个 configure 预设：Ninja 生成器、`build/{debug,release}`、`CMAKE_BUILD_TYPE` 对应、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`CMAKE_OSX_ARCHITECTURES=x86_64;arm64`（仅 Apple 平台生效，产出 universal 双架构库）；build/test 预设各两个。根 `CMakeLists.txt` 在 `project()` 之前自动探测编译器（Windows 找 `clang-cl`，非 Windows 找 `clang++-19/18/17/16` 回退 `clang++`），并用 `find_program(ninja)` 提示非 Ninja 生成器。

## 环境变量

| 变量 | 作用 | 生效方式 |
|---|---|---|
| `GODOT_PATH` | 定位 Godot 可执行文件（L2 引擎内测试） | 进程环境变量优先，为空才回退仓库根 `.env`（复制 `.env.template`，不入库）；二者皆缺 → 执行器以退出码 2 报错（`tests/runner/main.cpp`，不自动跳过） |
| `GODOT_AUTOPILOT_PORT` | 覆盖默认 MCP 端口 9527 | 运行时 `std::getenv`（`src/core/server_context.cpp:30`） |
| `GODOT_AUTOPILOT_HOST` | 读取监听地址；默认 `127.0.0.1`，非环回地址由 `ServerContext::start()` 拒绝 | 运行时 `std::getenv`（`src/core/server_context.cpp:41`） |
| `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` | 强制编译/链接并行度 | CACHE（`-D`）优先，其次进程环境变量 |
| `CI` | 存在即 `GDA_IS_CI=ON` | 隐式；关闭 `-march=native` 以保证可复现 |
| `GDA_UNITY_BUILD` / `GDA_UNITY_BATCH_SIZE` / `GDA_MAX_COMPILE_MEM_MB` / `GDA_MAX_LINK_MEM_MB` / `GDA_UNITY_MEM_MB` | Unity 与内存估算 | 仅 CMake 缓存参数（`-D`），**不支持环境变量** |

## 与 AGENTS.md / README 对照

逐条核对结论（一致，另有一处精度补充）：

- `uv run build.py` / `--release` 语义、手动 `cmake --preset` 命令 ✓；
- "切勿删除 `build/<preset>/_deps/`" ✓（`build.py` AUTO-CLEAN 保留 + `FetchDependencies.cmake` 头注释）；
- "添加新 .cpp 时必须在 `add_library()` 中加入" ✓（Unity 构建只编译列出的文件）；**精度补充**：`src/util/skill_templates/*.md` 与 `registry.json` 为内容数据文件，经生成头机制（`skill_gen.cmake` + `tools/embed_skills.py`）进入编译，不进 add_library；
- 依赖版本 `godot-cpp 10.0.0-stable` / `mcp-cpp-sdk 0.3.4`、FetchContent 非子模块 ✓（`FetchDependencies.cmake:17-29`，含 `GODOTCPP_API_VERSION "4.7"` FORCE）；
- 编译器优先 Clang/clang-cl、MSVC/GCC 回退 ✓（根 CMakeLists 自动探测 + `CompilerOptions.cmake` 分发）；
- 优化自适应（sccache/ccache、LTO、Unity、Ninja 作业池）✓；AGENTS.md 与实现一致：仅 `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` 支持 CACHE（`-D`）与进程环境变量双通道，`GDA_UNITY_BUILD`/`GDA_UNITY_BATCH_SIZE` 等内存参数只接受 `-D` CACHE；
- **文档一致性**：`README.md` / `README.en.md` 工具数口径为 badge `tools-385+`（`README.md:8` / `README.en.md:8`）与“385+ 个工具 / Over 385 tools”（`README.md:26` / `README.en.md:26`，域工具约数口径，数量随版本增长以 `search_tools` 查到的为准），wiki 实测为 [overview.md](./overview.md) 的 **391 域工具**——398/399（MCP 可达 / catalog-index）含 7 元工具与 `system_status`，README 未列这两个数字属“约数 vs 实测”口径差异；新版 README 已无独立构建章节（`README.md:70` 指向 wiki [build.md](./build.md) 与 `tests/README.md`），`--package` / `--debug` 参数以本页命令速查表为准。

## 关联页面

- [modules/core.md](./modules/core.md) — 核心模块（ServerContext、命令队列、模式检测）
- [tests.md](./tests.md) — L1/L2 测试体系与 `GDA_ENABLE_TESTS` 用法
- [overview.md](./overview.md) — 项目总览与工具统计
- [example.md](./example.md) — Example 示例工程
