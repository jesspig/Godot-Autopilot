---
name: project-building-gda
description: 构建、部署或打包 Godot Autopilot GDExtension，新增或删除 C++ 源文件，以及升级版本号时使用；纯测试配置改动与引擎内运行验证分别交给 project-running-tests。
---

# 构建与源文件登记

## 适用范围

- 用于：`cmake` 配置/编译/部署/打包不通过或需要跑一次；新增 `.cpp`/`.hpp`；`VERSION` 升版；构建产物缺失或陈旧。
- 不适用：跑 L1/L2 测试与判定口径（`project-running-tests`）；只改 `tests/config/*.json`（零 C++，不需要重新登记源文件）；Godot 工程侧的 `demo/` 内容改动。

## 前置条件

- `uv` 可用（`main.py` 入口）；CMake ≥ 3.28 与 Ninja（预设用 Ninja，缺 Ninja 时 CMake 会 WARNING 并拖慢构建）。
- `build/debug/` 或 `build/release/` 已配置过；首次配置用 `cmake --preset debug`。

## 流程

1. 日常改代码后：`uv run main.py build`（Debug 配置 + 编译），再 `uv run main.py deploy`（部署到 `demo/*/addons/godot-autopilot/`，`--demos` 可只部署子集；L2 测试床 `tests/testbed/addons/godot-autopilot/` 恒被部署，不受子集影响；交互式用 `uv run main.py tui`）。想验证编译但不部署：`cmake --build --preset debug`。
2. 发布物：`uv run main.py build --release` → `uv run main.py deploy --release` → `uv run main.py package [--libs-dir <dir>]` → `dist/godot-autopilot-<version>.zip`。`package` 子命令单独使用时只打包已部署的 addons，不触发构建。三平台合并打包用 `package --libs-dir <dir>`（缺任一平台库直接报错退出）。
3. 升版：只改根 `VERSION`，然后重新 configure（`cmake --preset debug`）+ 构建。不要在任何源文件、CMake 或 README 里另写版本号副本。
4. 构建后先部署再确认落位：`uv run main.py deploy` 后，各 `demo/*/addons/godot-autopilot/` 与 `tests/testbed/addons/godot-autopilot/` 下 `.gdextension` 与对应平台库的修改时间是本次构建时间（`main.py tui` 的状态表可直观核对，表内已含测试床一行）。

## 规则与边界

- **新增 `.cpp` 必须登记两处**，漏一处就在链接期炸：
  - `CMakeLists.txt:65` 起的 `add_library(godot-autopilot SHARED ...)` —— 漏了整个共享库不编译它，表现为 `handle_xxx` 未解析外部符号。
  - `tests/CMakeLists.txt:37` 的 `GDA_UNIT_BUSINESS_SOURCES` —— 漏了只有 `gda_unit_tests` 链接失败（undefined symbol），主库却是好的，容易误判成"测试代码有问题"。
  - `src/tools/*_ops.cpp` 由 `tests/CMakeLists.txt:24-25` 的 GLOB（`CONFIGURE_DEPENDS`）自动纳入，不必手加；`*_tools.hpp` 与 `*_ops.hpp` 是 header-only，两处都不用加。
  - core 侧要手加的现有例子：`editor_coords.cpp`、`trace_recorder.cpp`、`log_persist.cpp`、`monitor.cpp`（`editor_ui_actions.cpp` 属 `src/tools/`，不是 core）。
  - 有意**不进 L1** 的三个：`sanitize_policy.cpp`（只在 `main.cpp` 调 `initialize()`）、`monitor_env.cpp`（仅主线程）、`perf_sampler.cpp`（依赖引擎 tick）。别为了"覆盖"把它们塞进业务源，会在无引擎环境链接或运行失败。
- **勿删 `build/<preset>/_deps/`**：里面是 godot-cpp / mcp-cpp-sdk / googletest 的 FetchContent 缓存，删了要重新联网拉取；网络受限时用 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=build/debug/_deps/googletest-src` 指回已有缓存。
- `--release`（`deploy` 下）会先清理各 demo 与 L2 测试床的 `.godot` 与 `addons`，之后引擎要重新导入，别在同事正在编辑器里跑的时候顺手 `--release`；床被清过之后跑 L2 必须先 `deploy` 重新写入插件。
- `skills/*/*.md` 是内容数据文件，**不进 `add_library`**，由 `cmake/skill_gen.cmake` 构建期经 `tools/embed_skills.py` 嵌入生成头（落在 `build/<preset>/generated/`，已被 gitignore）。
- `GDA_ENABLE_TESTS` 已固化在 `CMakePresets.json` 的 debug/release 预设；裸 `cmake`（不带 `--preset`）配置默认是 OFF，此时看不到测试目标不是 bug。

## 常见错误

- 只看"编译过了"就宣布完成：注释守卫与迁移守卫跑在 ctest 阶段，编译通过≠守卫通过。
- 新加 `.cpp` 后只改根 `CMakeLists.txt`，然后被 `gda_unit_tests` 的 undefined symbol 卡住又回不去思路。
- 把 `CMakeLists.txt` 的行号当永恒常量：`add_library` 现起 `:65`、`option(GDA_ENABLE_TESTS` 现起 `:174`，文件增删后一律重新 grep 定位。
- 手改生成的 `skill_content_embedded.h`，或把它提交进仓库。

## 检查清单

- [ ] `cmake --build --preset debug` 与（必要时）`uv run main.py build` 均无错误。
- [ ] 新增/删除的 `.cpp` 已在两处登记，且 header-only 未被误登记。
- [ ] 若改了 `VERSION`：重新 configure 过，产物 zip 名与 `system_status.version` 一致。
- [ ] `build/<preset>/_deps/` 未被清理。

## 协同与参考

- 详细构建体系与产物清单：[docs/wiki/build.md](../../../docs/wiki/build.md)
- 测试目标与运行：`project-running-tests`；新增工具的源文件联动：`project-adding-domain-tools`
