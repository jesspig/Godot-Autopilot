---
type: 构建部署指南
title: 构建体系
description: 构建/部署/打包流程、CMake 模块职责、产物清单与环境变量
tags:
  - 构建
  - CMake
  - 部署
timestamp: "2026-08-22T15:10:00+08:00"
resource:
  - CMakeLists.txt
  - CMakePresets.json
  - build.py
  - cmake/
---

# 构建体系（build）

> 审计日期：2026-08-22（2026-08-12 初稿；08-16 随 mcp-cpp-sdk 0.3.1 升级同步；08-17 补 YAML frontmatter 并复核 add_library 源数量；08-22 随版本号收敛为根 `VERSION` 单一来源同步；08-22 随代码清理同步——Unity 构建接线生效、Lto.cmake 删 `GDA_LTO` 死变量；08-22 15 时全量一致性审计——CMakeLists 行数 157、端口覆盖行号、README ~343 口径对齐），基于当前工作树文件逐项核对（不依赖 git 历史）。
> 事实来源：`build.py`（205 行）、`CMakeLists.txt`（157 行）、`CMakePresets.json`、`cmake/` 全部 6 个模块、`.env.template`、根 `README.md` / `README_zh.md` / `AGENTS.md` 构建段。

## 命令速查表

| 命令 | 行为 |
|---|---|
| `uv run build.py` | Debug：配置 + 构建 + 部署到 `Example/addons/godot-autopilot/` |
| `uv run build.py --release` | 先清理，再 Release 配置 + 构建 + 部署 |
| `uv run build.py --release --package` | Release 构建部署后，再打包 `dist/godot-autopilot-<version>.zip` |
| `uv run build.py --package` | 仅打包已部署的 addons（不触发构建，未部署则报错退出） |
| `uv run build.py --debug` | 显式 Debug（默认即为 Debug） |
| `cmake --preset debug && cmake --build --preset debug` | 手动构建（不部署） |

约束：`--release` 与 `--debug` 互斥，同时指定报错退出（`build.py:174`）。

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

`_clean()`（`build.py:157-164`）删除两类目录，**不触碰 `build/`**：

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

### 5. 打包 `_package_addon()`（`build.py:141-155`)

将 `Example/addons/godot-autopilot/` 打包为 `dist/godot-autopilot-<version>.zip`（`ADDON_VERSION` 读自根目录 `VERSION` 文件——版本号单一来源，与 `project()` 版本一致）。

## 产物清单

| 产物 | 位置 | 说明 |
|---|---|---|
| `godot-autopilot.dll` | `build/{debug,release}/` → 部署目录 | Windows 平台库 |
| `libgodot-autopilot.so` | 同上 | Linux |
| `libgodot-autopilot.dylib` | 同上 | macOS |
| `godot-autopilot.pdb` | 同上（Windows，存在时复制） | 调试符号 |
| `godot-autopilot.gdextension` | 部署目录（每次部署重新生成） | 入口 `entry_symbol = "GDExtensionEntryPoint"`、`compatibility_minimum = "4.3"`、`[libraries]` 6 条平台路径（windows/linux/macos × debug/release，均为 `x86_64`） |
| `dist/godot-autopilot-<version>.zip` | `dist/` | `--package` 产物（版本号取自根 `VERSION` 文件） |

部署目录：`Example/addons/godot-autopilot/`（README 的 `Example/` 与 build.py 内部路径 `example/` 在 Windows 大小写不敏感文件系统下为同一目录）。

## CMake 目标

`add_library(godot-autopilot SHARED ...)`（`CMakeLists.txt:59-129`）共 **70 个 .cpp**：

| 目录 | 数量 | 目录 | 数量 |
|---|---:|---|---:|
| `src/main.cpp` | 1 | `src/tools/` | 41 |
| `src/core/` | 7 | `src/util/` | 5 |
| `src/resources/` | 2 | `src/ui/` | 2 |
| `src/prompts/` | 9 | `src/runtime/` | 3 |

- 私有头文件目录：`${CMAKE_SOURCE_DIR}/src`；
- 链接库（PRIVATE）：`godot-cpp`、`mcp-server`、`mcp-http`（后两者来自 mcp-cpp-sdk）；
- MSVC（含 clang-cl）额外 `target_link_options "/WHOLEARCHIVE:$<TARGET_FILE:godot-cpp>"`（`CMakeLists.txt:133`）—— 强制导出 godot-cpp 全部符号，防止 GDExtension 入口符号被链接器裁剪；
- **Unity 构建已接线生效**：`set_target_properties(godot-autopilot PROPERTIES UNITY_BUILD ${GDA_UNITY_ENABLED} UNITY_BUILD_BATCH_SIZE ${GDA_UNITY_BATCH})`（`CMakeLists.txt`，add_library 之后）——实测 Debug batch=8（16 核）；此前仅 BuildOptimization.cmake 计算参数、未挂到 target；
- `option(GDA_ENABLE_TESTS ... OFF)`，开启后 `add_subdirectory(tests)`（详见 [tests.md](./tests.md)）。

## cmake/ 模块职责

按根 `CMakeLists.txt:49-54` 的 include 顺序：

| 文件 | 职责 | 关键变量 |
|---|---|---|
| `Platform.cmake` | 架构与 CI 探测 | `GDA_ARCH`（`x86_64`/`x86`，按指针宽度）、`GDA_IS_CI`（进程环境存在 `CI` 即 ON） |
| `BuildOptimization.cmake` | 硬件感知并行度（核心规则） | `GDA_COMPILE_JOBS`、`GDA_LINK_JOBS`（CACHE 优先、其次 ENV）、`GDA_UNITY_BUILD`（ON 且核数 >1 时启用）、`GDA_UNITY_BATCH_SIZE`（0=auto）、`GDA_MAX_COMPILE_MEM_MB`（默认 1500）、`GDA_MAX_LINK_MEM_MB`（默认 4000）、`GDA_UNITY_MEM_MB`（默认 500） |
| `CompilerOptions.cmake` | 按编译器分发 flags | Clang/clang-cl 与 MSVC：`/utf-8 /bigobj /W4 /EHsc` + `_CRT_SECURE_NO_WARNINGS`、`_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`、`_WIN32_WINNT=0x0A00`；MSVC 调试信息 `Embedded`；GCC/Clang 非 Windows 非 CI 追加 `-march=native`；`find_package(Threads REQUIRED)`；`CMAKE_POSITION_INDEPENDENT_CODE ON` |
| `Cache.cmake` | 编译缓存自动探测 | 优先 sccache（支持 MSVC），回退 ccache（仅 GCC/Clang）；命中则设 `CMAKE_C/CXX_COMPILER_LAUNCHER` |
| `FetchDependencies.cmake` | FetchContent 依赖 | `godot-cpp` @ `10.0.0-rc1`（godotengine/godot-cpp，GIT_SHALLOW）、`mcp-cpp-sdk` @ `0.3.1`（jesspig/modelcontextprotocol-cpp-sdk，GIT_SHALLOW）；`FETCHCONTENT_QUIET OFF`；文件头部注释明确"禁止删除 _deps/" |
| `Lto.cmake` | 仅 Release 的链接优化 | 优先级 Clang ThinLTO（`-flto=thin`）> MSVC LTCG（`CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE`）> GCC IPO（`CheckIPOSupported`）；非 Release 直接跳过 |

### 并行度计算规则（`BuildOptimization.cmake`）

- **编译作业数** = `min(总内存/1500MB, 核数-2)`，下限 1；同时写入 `CMAKE_BUILD_PARALLEL_LEVEL`；
- **链接作业数** = `min(总内存/4000MB, 2)`，下限 1（链接内存密集，刻意收紧）；
- 作业池命名 `gda_compile_pool` / `gda_link_pool`，加 `gda_` 前缀是**刻意规避**与 mcp-cpp-sdk 自带 `compile_pool`/`link_pool` 的全局同名冲突（`BuildOptimization.cmake:83` 注释）；
- **Unity batch** = `min(总内存/500MB, (核数+1)/2)`，下限 2。

### 预设（`CMakePresets.json`）

version 8；`debug`/`release` 两个 configure 预设：Ninja 生成器、`build/{debug,release}`、`CMAKE_BUILD_TYPE` 对应、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`；build/test 预设各两个。根 `CMakeLists.txt` 在 `project()` 之前自动探测编译器（Windows 找 `clang-cl`，非 Windows 找 `clang++-19/18/17/16` 回退 `clang++`），并用 `find_program(ninja)` 提示非 Ninja 生成器。

## 环境变量

| 变量 | 作用 | 生效方式 |
|---|---|---|
| `GODOT_PATH` | 定位 Godot 可执行文件（L2 引擎内测试） | 进程环境变量优先，为空才回退仓库根 `.env`（复制 `.env.template`，不入库）；二者皆缺 → L2 失败/跳过 |
| `GODOT_AUTOPILOT_PORT` | 覆盖默认 MCP 端口 9527 | 运行时 `std::getenv`（`src/core/server_context.cpp:20`） |
| `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` | 强制编译/链接并行度 | CACHE（`-D`）优先，其次进程环境变量 |
| `CI` | 存在即 `GDA_IS_CI=ON` | 隐式；关闭 `-march=native` 以保证可复现 |
| `GDA_UNITY_BUILD` / `GDA_UNITY_BATCH_SIZE` / `GDA_MAX_COMPILE_MEM_MB` / `GDA_MAX_LINK_MEM_MB` / `GDA_UNITY_MEM_MB` | Unity 与内存估算 | 仅 CMake 缓存参数（`-D`），**不支持环境变量** |

## 与 AGENTS.md / README 对照

逐条核对结论（全部一致，另有两点精度差异）：

- `uv run build.py` / `--release` 语义、手动 `cmake --preset` 命令 ✓；
- "切勿删除 `build/<preset>/_deps/`" ✓（`build.py` AUTO-CLEAN 保留 + `FetchDependencies.cmake` 头注释）；
- "添加新 .cpp 时必须在 `add_library()` 中加入" ✓（Unity 构建只编译列出的文件）；
- 依赖版本 `godot-cpp 10.0.0-rc1` / `mcp-cpp-sdk 0.3.1`、FetchContent 非子模块 ✓（`FetchDependencies.cmake:15,24`）；
- 编译器优先 Clang/clang-cl、MSVC/GCC 回退 ✓（根 CMakeLists 自动探测 + `CompilerOptions.cmake` 分发）；
- 优化自适应（sccache/ccache、LTO、Unity、Ninja 作业池）✓；**精度差异**：AGENTS.md 写"可通过 `GDA_COMPILE_JOBS` / `GDA_LINK_JOBS` 等环境变量覆盖"——实际仅这两个支持环境变量，`GDA_UNITY_BATCH_SIZE` 等内存参数只接受 `-D` CACHE；
- **文档一致性**：`README.md` / `README_zh.md` 工具数口径 "~343 / 23 类"（08-22 审计时由 "~339" 修正）及 [overview.md](./overview.md) 审计（343 = 7 元 + 336 领域；registry/catalog 344 含 `system_status`）一致；`--package` / `--debug` 两个 build.py 参数在 README 构建章节未提及。

## 关联页面

- [modules/core.md](./modules/core.md) — 核心模块（ServerContext、命令队列、模式检测）
- [tests.md](./tests.md) — L1/L2 测试体系与 `GDA_ENABLE_TESTS` 用法
- [overview.md](./overview.md) — 项目总览与工具统计
- [example.md](./example.md) — Example 示例工程
