# AGENTS.md

## 构建

- `uv run build.py` — Debug 构建并部署到 `Example/addons/godot-autopilot/`；`--release` 先清理 `.godot`/`addons` 再构建；`--package` 打包 `dist/godot-autopilot-<version>.zip`；`--package --libs-dir <dir>` 从目录递归收集三平台库合并打包（须与 `--package` 同用）
- 手动：`cmake --preset debug && cmake --build --preset debug`；预设 `debug`/`release`（Ninja，`CMAKE_OSX_ARCHITECTURES=x86_64;arm64` universal，gdextension 用 `macos.{debug,release}.universal`）
- **勿删 `build/<preset>/_deps/`**（godot-cpp/mcp-cpp-sdk/googletest 缓存）；**新增 `.cpp` 必须加入 `CMakeLists.txt:64` 的 `add_library()`**，业务源码另需同步 `tests/CMakeLists.txt:31` 的 `GDA_UNIT_BUSINESS_SOURCES`（漏了会 undefined symbol，header-only 除外）
- 版本单一来源：根 `VERSION`（当前 `0.2.2`）→ `CMakeLists.txt:33 file(READ)` 喂 `project()` + `configure_file` 生成 `GDA_VERSION`（`server_info`/`system_status.version`）+ `build.py:34` 打包名；升版只改该文件后重新 configure

## 架构

- 进程内 GDExtension，`MODULE_INITIALIZATION_LEVEL_EDITOR` 加载；`src/main.cpp:GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 启动 `ServerContext`，`_exit_tree()` 停止
- 线程：mcp-cpp-sdk HTTP 线程 → `CommandQueue::submit()`/`execute_sync()` → 主线程 `_process():s_queue.drain()`；**所有 Godot API 必须经队列执行**，直接调用必崩
- 端口 9527 `/mcp`，优先级 `GODOT_AUTOPILOT_PORT` env > `user://godot_autopilot/config.json`（`PluginConfig`）> 9527；默认绑 `127.0.0.1`，`ServerContext::start()` 拒绝非环回 host；mcp-cpp-sdk 0.3.2 起 Host 仅允许环回主机名/IP
- 工具注册 `ToolRegistry` 单一来源：363 域工具（30 个 `src/tools/<域>_tools.hpp`，`GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE` + `make_tools()`）+ `system_status`（FnTool）+ 7 元工具（`MetaTool` 需 `IMetaTool`，`add()` 自动归类）；catalog/BM25/`g_handlers`/`RegisterTool` 全派生
- 计数：370 = 7 元 + 363 域；371 = 363 域 + `system_status` + 7 元（catalog/index）；7 元 = `ping/search_tools/list_categories/get_tool_detail/call_tool/batch_execute/code_execute`
- 错误水印：响应顶层 `new_errors_since_last_call` 一次性消费（`error_watermark.hpp`）；`editor_readiness` 导入中返回 `retryable` 软错误
- 互转：`VariantJson::serialize/deserialize`（`util/variant_json.hpp`）；错误 `{"error": "msg"}`，`call_tool` 置 `is_error=true`

## 添加工具

1. `src/tools/<cat>_ops.hpp` 声明 `handle_xxx`，`<cat>_ops.cpp` 实现
2. `<cat>_tools.hpp` 用 `GDA_TOOL_CLASS`（副作用用 `GDA_TOOL_CLASS_SIDE` + `SideEffect` 枚举值）声明子类并入 `make_tools()`（`register_all.cpp` 自动注册）
3. schema 由 `tool_input_schema(name, basic)` 单一源（`basic` 已 no-op）；命名 `<动词>_<类别>_<维度>_<对象>_<修饰>` snake_case 动词置首（如 `create_scene_node`）

## 约定

- 命名空间 `godot_autopilot`；日志仅 `System/Transport/Tools/Resources/Prompts`
- 编译器优先 Clang/clang-cl，sccache/LTO/Unity/Ninja 池自适应；依赖 godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.3.2（FetchContent，无子模块）

## 测试

- 启用：`GDA_ENABLE_TESTS` 已在 `CMakePresets.json` debug/release 置 `ON`（裸 `cmake` 默认 `OFF`）
- 运行：`ctest --preset debug`（L1 秒级，L2 约 2 分钟需 `GODOT_PATH`）；单跑 `build/debug/tests/gda_test_runner.exe --file 01_scene`；CI 仅 `ctest --preset debug -E "^gda_runner_"`（L1）
- 结构：L1 `gda_unit_tests` 96 gtest；L2 `gda_test_runner` + `tests/config/*.json` 7 份（`00_meta`/`01_scene`/`02_property`/`03_tools_contract`/`04_resources_scripts`/`05_rename_references`/`06_move_references`）；当前 ctest 注册点共 103（L2 需 `GODOT_PATH`）
- 新增用例 = 新建 `tests/config/*.json` 零 C++；Godot 路径 `GODOT_PATH` env > `.env`（`.env.template` 复制），缺失则 L2 跳过
- 遍历：`03_tools_contract` 枚举 363 域工具，42 个 `GDA_TOOL_CLASS_SIDE` 经 `side_effect` 字段自动排除（仍参与枚举，321 个做空参+冒烟）；3 契约缺口 `create_scene_node`/`get_resource_extensions`/`reimport_resource_files` 记 warnings
- 副作用：L2 后 `Example/project.godot` 可能追加 `[audio]/[input]` 并生成 `default_bus_layout.tres`，`git checkout -- Example/project.godot` 清理

## 知识局限

- schema 非空/空数以 `register_all_test.cpp:SchemaStatisticsBaseline` 运行时统计为准
- 目标 Godot 4.7：`TileSetAtlasSource.get_tile_data(source_id→atlas_coords, alt)`、`AnimatedSprite2D.sprite_frames`（非 `frames`）、`motion_mode` GROUNDED=0/FLOATING=1
- 27 类（InputMap 并入 Input）、3 模块无独立 hpp：`environment_ops.cpp`→`render_ops.hpp`、`display_window_ops.cpp`→`display_ops.hpp`、`runtime_game_ops.cpp`→`runtime_ops.hpp`

## 协作

- `git status` 大量 `M` 多为 CRLF 伪变更，以 `git diff HEAD` 为准；提交 `<type>(<scope>): <中文描述>`
- 多代理并行时子代理只写代码不编译，主代理统一 `cmake --preset` + `cmake --build --preset` 再分批测（并行编译会锁）；L1 禁调已注册 handler（`nullptr` 崩溃）；网络受限时 `-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=build/debug/_deps/googletest-src`

## 知识库

- 位置 `docs/wiki/`，入口 `docs/wiki/index.md`；按需更新（完成功能/交付指南/提交前），只改受影响页，删过时描述不留废弃标记；安全与并发约束见 `docs/wiki/security_contract.md`
- 更新前以 `git diff HEAD` 核查实际变更为准，无法核实标 `> [!todo] 待补充`；数值以运行时统计为准；每页至少 1 条相对链接
- frontmatter 必含 `type/title/description/tags/timestamp/resource`（`index.md`/`log.md`/`changelog/*` 除外）；带审计日期头的页改后同步日期；改后在 `changelog/<YYYY-MM-DD>-log.md` 按小时追加，`log.md` 仅留最近 7 天
