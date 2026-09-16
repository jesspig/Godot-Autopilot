# AGENTS.md

## 构建

- `uv run build.py` — Debug 构建并部署到 `Example/addons/godot-autopilot/`；`--release` 先清理 `.godot`/`addons` 再构建；`--package` 打包 `dist/godot-autopilot-<version>.zip`；`--package --libs-dir <dir>` 从目录递归收集三平台库合并打包（须与 `--package` 同用）
- 手动：`cmake --preset debug && cmake --build --preset debug`；预设 `debug`/`release`（Ninja，`CMAKE_OSX_ARCHITECTURES=x86_64;arm64` universal，gdextension 用 `macos.{debug,release}.universal`）
- **勿删 `build/<preset>/_deps/`**（godot-cpp/mcp-cpp-sdk/googletest 缓存）；**新增 `.cpp` 必须加入 `CMakeLists.txt:65` 的 `add_library()`**（如 `src/core/editor_coords.cpp`、`src/tools/input_click_ops.cpp`、`src/tools/editor_ui_ops.cpp`、`src/tools/editor_ui_actions.cpp`），业务源码另需同步 `tests/CMakeLists.txt:31` 的 `GDA_UNIT_BUSINESS_SOURCES`（漏了会 undefined symbol，header-only 除外；`src/tools/*_ops.cpp` 由 `tests/CMakeLists.txt:24-25` 的 GLOB 纳入，core 侧 `editor_coords.cpp` 与 `editor_ui_actions.cpp` 需显式加入）；`src/util/skill_templates/*.md` 为内容数据文件不进 add_library（经 `cmake/skill_gen.cmake` 构建期嵌入生成头）
- 版本单一来源：根 `VERSION`（当前 `0.2.5`）→ `CMakeLists.txt:33 file(READ)` 喂 `project()` + `configure_file` 生成 `GDA_VERSION`（`server_info`/`system_status.version`）+ `build.py:34` 打包名；升版只改该文件后重新 configure

## 架构

- 进程内 GDExtension，`MODULE_INITIALIZATION_LEVEL_EDITOR` 加载；`src/main.cpp:GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 启动 `ServerContext`，`_exit_tree()` 停止
- 线程：mcp-cpp-sdk HTTP 线程 → `CommandQueue::submit()`/`execute_sync()` → 主线程 `_process():s_queue.drain()`；**所有 Godot API 必须经队列执行**，直接调用必崩
- 端口 9527 `/mcp`，优先级 `GODOT_AUTOPILOT_PORT` env > `user://godot_autopilot/config.json`（`PluginConfig`）> 9527；默认绑 `127.0.0.1`，`ServerContext::start()` 拒绝非环回 host；mcp-cpp-sdk 0.3.2 起 Host 仅允许环回主机名/IP
- 工具注册 `ToolRegistry` 单一来源：385 域工具（30 个 `src/tools/<域>_tools.hpp`，`GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE` + `make_tools()`）+ `system_status`（FnTool）+ 7 元工具（`MetaTool` 需 `IMetaTool`，`add()` 自动归类）；catalog/BM25/`g_handlers`/`RegisterTool` 全派生
- 计数：392 = 7 元 + 385 域；393 = 385 域 + `system_status` + 7 元（catalog/index）；7 元 = `ping/search_tools/list_categories/get_tool_detail/call_tool/batch_execute/code_execute`
- 新增 13 个工具（Display 1 / Scene 1 / Editor 6 / Input 4 / Game 1）：`get_display_window_rect`、`get_scene_node_screen_rect`、`get_editor_viewport_geometry`、`get_editor_ui_elements`、`hit_test_editor_point`、`click_editor_element`、`type_editor_element_text`、`run_editor_shortcut`、`click_input_mouse`、`scroll_input_mouse`、`drag_input_mouse`、`type_input_text`、`click_game_ui_element`；`capture_editor_viewport` 新增 `region`/`max_dimension`/`space`/`annotate`/`diff_against_last`，游戏侧新增 wheel 注入与单步 mouse_motion，`click_game_ui_element` 走游戏侧匹配+注入（单次协议往返）
- 新增 5 个工具（Editor 2 / TileMap 1 / Game 2；非 SIDE 322→324、SIDE 57→60）：`scene_tree_items`、`select_scene_tree_node`（编辑器场景树行枚举与按路径选中）、`fill_tilemap_rect`（矩形铺砖，>100000 格报错、单次 undo）、`start_game_job`、`get_game_job`（长游戏脚本异步提交+轮询，job 表上限 16、过期宽限 2s；`start_game_job` 需 `game_runtime` 授权）；视觉辅助批次 +1（Capture；非 SIDE 324→325）：`review_scene_visually`（编辑器图+游戏图+节点表+映射只读组合）；行为变更细节见 `docs/wiki/modules/tools_ops_a.md`/`docs/wiki/modules/tools_ops_b.md`/`docs/wiki/modules/entry_runtime.md` 与 `docs/wiki/changelog/2026-09-16-log.md`
- 坐标/键名/脚本新鲜度批次（无新增工具）：`click_game_ui_element` 补窗口坐标换算（画布→窗口客户区，`window_position`/`viewport_position` 回显）；键名判定编辑器/游戏两侧统一（`input_map_ops::resolve_key_name_code`，裸名/`KEY_` 前缀/大小写不敏感/数字码等价）；脚本只读工具新增 `fresh` 参数（`CACHE_MODE_IGNORE` 读盘），`attach`/`reload` 恒重读，`create_script` 响应 `cache_invalidated`→`cache_refreshed`；`open_editor_scene` 幂等成功（`already_open`）；`create_scene_node` 属性两轮应用（OBJECT 型先）；`set_resource_uid`/保存路径按表成员选 `add_id`/`set_id`；文本链路全 `String::utf8`（CJK 往返）
- 错误水印：响应顶层 `new_errors_since_last_call` 一次性消费（`error_watermark.hpp`）；`editor_readiness` 导入中返回 `retryable` 软错误
- 互转：`VariantJson::serialize/deserialize`（`util/variant_json.hpp`）；错误 `{"error": "msg"}`，`call_tool` 置 `is_error=true`

## 添加工具

1. `src/tools/<cat>_ops.hpp` 声明 `handle_xxx`，`<cat>_ops.cpp` 实现
2. `<cat>_tools.hpp` 用 `GDA_TOOL_CLASS`（副作用用 `GDA_TOOL_CLASS_SIDE` + `SideEffect` 枚举值）声明子类并入 `make_tools()`（`register_all.cpp` 自动注册）
3. schema 由 `tool_input_schema(name, basic)` 单一源（`basic` 已 no-op）；命名 `<动词>_<类别>_<维度>_<对象>_<修饰>` snake_case 动词置首（如 `create_scene_node`）

## 约定

- 命名空间 `godot_autopilot`；日志仅 `System/Transport/Tools/Resources/Prompts`
- 编译器优先 Clang/clang-cl，sccache/LTO/Unity/Ninja 池自适应；依赖 godot-cpp 10.0.0-rc1、mcp-cpp-sdk 0.3.3（FetchContent，无子模块）

## 测试

- 启用：`GDA_ENABLE_TESTS` 已在 `CMakePresets.json` debug/release 置 `ON`（裸 `cmake` 默认 `OFF`）
- 运行：`ctest --preset debug`（L1 秒级，L2 约 2 分钟需 `GODOT_PATH`）；单跑 `build/debug/tests/gda_test_runner.exe --file 01_scene`；CI 仅 `ctest --preset debug -E "^gda_runner_"`（L1）
- 结构：L1 `gda_unit_tests` 243 gtest（25 文件：vision_assist 7 / game_ui_coords 6 / keycode_alias 6 / script_freshness 7 / uid_guard 2 为本轮新增；另有 editor_ui_tree 6 / inline_resource_json 12 / skill_resources 12 / tilemap_ops 13；`editor_coords_test` 32→37→15（09-17 精简批次删除仿射子集用例）、`runtime_ops_test` 3→12、`client_config_gen_test` 11→20）；L2 `gda_test_runner` + `tests/config/*.json` 25 份（`00_meta`…`10_editor_input` 11 份 + `11_editor_tree`/`12_capture_params`/`13_inline_subresource`/`14_tilemap_rect`/`15_scene_path`/`16_game_jobs`/`17_vision_assist` + `18_script_freshness`/`19_cjk_roundtrip`/`22_uid_guard`/`23_click_ui_coords`/`24_keycode_alias`/`25_open_scene_idempotent`/`28_sprite_frames_animation`；20/21/26/27 号段空缺）；当前 ctest 注册点共 268（L1 243 + L2 25；L2 需 `GODOT_PATH`；L2 授权 capability 由 `GODOT_AUTOPILOT_ALLOW` env 或 `user://godot_autopilot/config.json` 的 `allow` 字段提供，env 优先，`16_game_jobs` 的 `start_game_job`、23/24 游戏侧路径需 `game_runtime`，18/22 脚本路径需 `code_execute`）
- skill_gen：内容源 `src/util/skill_templates/`（30 个 .md + registry.json，8 册技能 = 1 总纲 `godot-autopilot` + 6 引擎指南 + 1 C# 专册 `godot-autopilot-csharp`，每册带 references/；84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册，4.7+/4.8 行内简注；编辑器 UI/输入自动化工具用法已补充进相关分册）+ `tools/embed_skills.py` 构建期嵌入（`SKILL_COUNT = 8`；5 项校验 name/description/files 结构与孤儿文件，生成头入 `build/<preset>/generated/`，gitignore 覆盖；渲染至 `.agents/skills/`，dock 按钮 Generate Skills/Update Skills 动态切换，Update 先递归清理 `godot-autopilot-` 前缀目录再重建），`tests/unit/skill_gen_test.cpp` 7 用例 L1 校验（8 册清单/name/description/文件布局/frontmatter/反引号词回验 catalog∪schema 参数名∪176 项白名单/每册 references 声明），无需 Godot；L1 计数 243 为 `TEST`/`TEST_F` 宏静态统计（09-17 构建实测 243/243 全绿）
- 客户端一键配置（`client_config_gen` + McpConfigDock 下拉）：20 个客户端的项目级 MCP 配置生成/合并（09-16 由 8 扩至 20：新增 ZCode `.zcode/config.json` mcp.servers 双层 / pi `.pi/mcp.json` / Command Code 与 CodeBuddy 共用 `.mcp.json` / Kilo `.kilo/mcp.json` streamable-http / Roo `.roo/mcp.json` / Grok Build `.grok/config.toml` / Kimi `.kimi-code/mcp.json` / Zed `.zed/settings.json` context_servers / Crush `.crush.json` mcp+$schema / Copilot VS Code `.vscode/mcp.json` servers 键 / Reasonix `reasonix.toml` `[[plugins]]` 数组；调研淘汰 Cline 与 Antigravity CLI——项目级可能不生效；TOML 判定 `uses_toml(id)`，merge_toml_config 带 ClientId 区分 mcp_servers 段与 plugins 数组两种 marker）
- 新增用例 = 新建 `tests/config/*.json` 零 C++；Godot 路径 `GODOT_PATH` env > `.env`（`.env.template` 复制），缺失则执行器以退出码 2 报错（不自动跳过）
- 遍历：`03_tools_contract` 枚举 385 域工具（解析器仅匹配 `GDA_TOOL_CLASS(`），60 个 `GDA_TOOL_CLASS_SIDE` 不进入枚举（`side_effect` 字段兜底判定保留），325 个做空参+冒烟（09-16-00 实测 324 个 0 失败，视觉辅助 +1 后候选 325，以运行时为准，上限 650 步）；实测 4 条 warnings：`start_input_gamepad_vibration`/`stop_input_gamepad_vibration`/`get_resource_extensions`/`reimport_resource_files`（`create_scene_node` 是否告警依赖运行期场景状态）
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
- 语言简体中文；由本规则触发的自动维护以代码与用户讨论结果为事实，changelog 时间必须真实（禁止猜测）
