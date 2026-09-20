# AGENTS.md

## 构建

- `uv run build.py` — Debug 构建并部署到 `Example/addons/godot-autopilot/`；`--release` 先清理 `.godot`/`addons` 再构建；`--package` 打包 `dist/godot-autopilot-<version>.zip`；`--package --libs-dir <dir>` 从目录递归收集三平台库合并打包（须与 `--package` 同用）
- 手动：`cmake --preset debug && cmake --build --preset debug`；预设 `debug`/`release`（Ninja，`CMAKE_OSX_ARCHITECTURES=x86_64;arm64` universal，gdextension 用 `macos.{debug,release}.universal`）
- **勿删 `build/<preset>/_deps/`**（godot-cpp/mcp-cpp-sdk/googletest 缓存）；**新增 `.cpp` 必须加入 `CMakeLists.txt:65` 的 `add_library()`**（如 `src/core/editor_coords.cpp`、`src/core/trace_recorder.cpp`、`src/core/log_persist.cpp`、`src/core/sanitize_policy.cpp`、`src/tools/input_click_ops.cpp`、`src/tools/editor_ui_ops.cpp`、`src/tools/editor_ui_actions.cpp`），业务源码另需同步 `tests/CMakeLists.txt:37` 的 `GDA_UNIT_BUSINESS_SOURCES`（漏了会 undefined symbol，header-only 除外；`src/tools/*_ops.cpp` 由 `tests/CMakeLists.txt:24-25` 的 GLOB 纳入，core 侧 `editor_coords.cpp`/`editor_ui_actions.cpp`/`trace_recorder.cpp`/`log_persist.cpp` 需显式加入，`sanitize_policy.cpp` 仅 `main.cpp` 调 `initialize()`、不进 L1）；`src/util/skill_templates/*.md` 为内容数据文件不进 add_library（经 `cmake/skill_gen.cmake` 构建期嵌入生成头）
- 版本单一来源：根 `VERSION`（当前 `0.2.6`）→ `CMakeLists.txt:33 file(READ)` 喂 `project()` + `configure_file` 生成 `GDA_VERSION`（`server_info`/`system_status.version`）+ `build.py:34` 打包名；升版只改该文件后重新 configure

## 架构

- 进程内 GDExtension，`MODULE_INITIALIZATION_LEVEL_EDITOR` 加载；`src/main.cpp:GDExtensionEntryPoint` 注册 `GodotAutopilotPlugin`，`_enter_tree()` 启动 `ServerContext`，`_exit_tree()` 停止
- 线程：mcp-cpp-sdk HTTP 线程 → `CommandQueue::submit()`/`execute_sync()` → 主线程 `_process():s_queue.drain()`；**所有 Godot API 必须经队列执行**，直接调用必崩
- 端口 9527 `/mcp`，优先级 `GODOT_AUTOPILOT_PORT` env > `user://godot_autopilot/config.json`（`PluginConfig`）> 9527；默认绑 `127.0.0.1`，`ServerContext::start()` 拒绝非环回 host；mcp-cpp-sdk 0.3.2 起 Host 仅允许环回主机名/IP
- 工具统一数据层 `ToolSpec`（`src/tools/tool_spec.hpp`）：`{name, description, category, tags, side_effect, flags, params, handler, raw_schema}`；`ParamSpec = schema::ParamDef`（name/type/description/required）；`make_spec_tool(ToolSpec)` 产 `SpecTool : ToolBase, ISideEffect`，schema 由 `params` 经 `schema::build_schema` 派生、`raw_schema` 非空时优先；`tool_flags` 位标志 kMeta/kDynamic/kMutating/kObserve/kCaptureImage/kSceneTarget/kUndoable；取参统一 `Args`（`tool_args.hpp`：opt_/require_/get_ + `reject_unknown`，空值视为缺失）
- 注册 `ToolRegistry` 单一来源（`src/tools/register_all.cpp`）：`build_registry` = 30 域 `make_tools()`（391 工具）+ `system_status` + 7 元工具（SpecTool，手写 schema 存 `raw_schema`）+ 动态 spec store；按 `flags & kMeta` 归 meta_；`refresh_derived` 派生 catalog/BM25/`dispatch` handlers；`refresh_dynamic_tools()` 在用户脚本注册/注销后重建；`get_tool_detail` 回显 `side_effect`/`dynamic`/`mutating`
- 计数：399 = 391 域 + `system_status` + 7 元（catalog/index/分发）；MCP 只暴露 7 元工具，域工具经 `call_tool` 代理可达 398；7 元 = `ping/search_tools/list_categories/get_tool_detail/call_tool/batch_execute/code_execute`
- 执行管线 `src/tools/tool_pipeline.cpp`：`SpecTool::execute()` 先授权门 `deny_if_unauthorized` 再 handler，最后 `pipeline::run_post` 按 `kObserve` 合并编辑器截图（observe=true；Input 域 4 个合成输入工具已收编，重复实现已删）
- traits 收编终态：`kCaptureImage` 9 工具（`register_all.cpp` 按 flag 判定图片附件单一来源，旧白名单仅作未知工具兜底）；`kSceneTarget` 25 工具（`tool_pipeline.cpp::run_post` 对成功响应缺 `scene_path` 时幂等补全，不做脏标记）；`kUndoable` 10 工具（纯标记，各 handler 自管 undo，无 post 动作）
- 内部组合调用 `src/tools/tool_invoke.{hpp,cpp}`：`tools::invoke_tool(name, args)`（导出拦截→深度上限 8→`dispatch::call_handler`→异步 pending 嵌套拦截）+ `invoke_depth()`；导出分支无公开 setter，L1 未覆盖（注释已声明）
- 目录扫描 `AutopilotTools::rescan(directory)`（默认 `res://addons/godot-autopilot-tools`；门禁/主线程 marshal/`res://`/`user://` 校验/递归 256 文件/深度 8/逐文件错误隔离/返回 scanned/registered/failed/errors；约定 `register_autopilot_tools(api)` 实例方法；无父节点 Node 注册后释放、仅 RefCounted 安全）；示例 `samples/user-tools/echo_tool.gd` + `echo_tool.cs`（C# 文档级，未经本仓库 CI 验证）
- `batch_execute` 最小变量串联（`src/tools/code_exec_ops.cpp` 内）：`$prev`、`$steps[N].result`/`$steps[N].error` 整字符串精确替换，错误前缀 `unresolvable reference`，走既有 stop_on_error；无 schema 改动、无条件/并行
- 动态工具（用户脚本 API）：`AutopilotTools` 单例（`src/tools/autopilot_tools.{hpp,cpp}`，GDScript 侧 `Engine.get_singleton("AutopilotTools")`）提供 register_tool/unregister_tool/has_tool/list_tools/get_tool/call_tool/is_enabled/set_enabled；spec 存 `dynamic_spec_store.hpp` 的 store() + mutex，注册即 `refresh_dynamic_tools()`；执行受 `user_tools` 授权门（env `GODOT_AUTOPILOT_ALLOW` 优先于配置 `allow`，dock 复选框 "Allow user tools"）
- 新增 13 个工具（Display 1 / Scene 1 / Editor 6 / Input 4 / Game 1）：`get_display_window_rect`、`get_scene_node_screen_rect`、`get_editor_viewport_geometry`、`get_editor_ui_elements`、`hit_test_editor_point`、`click_editor_element`、`type_editor_element_text`、`run_editor_shortcut`、`click_input_mouse`、`scroll_input_mouse`、`drag_input_mouse`、`type_input_text`、`click_game_ui_element`；`capture_editor_viewport` 新增 `region`/`max_dimension`/`space`/`annotate`/`diff_against_last`，游戏侧新增 wheel 注入与单步 mouse_motion，`click_game_ui_element` 走游戏侧匹配+注入（单次协议往返）
- 新增 5 个工具（Editor 2 / TileMap 1 / Game 2；非 SIDE 322→324、SIDE 57→60）：`scene_tree_items`、`select_scene_tree_node`（编辑器场景树行枚举与按路径选中）、`fill_tilemap_rect`（矩形铺砖，>100000 格报错、单次 undo）、`start_game_job`、`get_game_job`（长游戏脚本异步提交+轮询，job 表上限 16、过期宽限 2s；`start_game_job` 需 `game_runtime` 授权）；视觉辅助批次 +1（Capture；非 SIDE 324→325）：`review_scene_visually`（编辑器图+游戏图+节点表+映射只读组合）；行为变更细节见 `docs/wiki/modules/tools_ops_a.md`/`docs/wiki/modules/tools_ops_b.md`/`docs/wiki/modules/entry_runtime.md` 与 `docs/wiki/changelog/2026-09-16-log.md`
- 坐标/键名/脚本新鲜度批次（无新增工具）：`click_game_ui_element` 补窗口坐标换算（画布→窗口客户区，`window_position`/`viewport_position` 回显）；键名判定编辑器/游戏两侧统一（`input_map_ops::resolve_key_name_code`，裸名/`KEY_` 前缀/大小写不敏感/数字码等价）；脚本只读工具新增 `fresh` 参数（`CACHE_MODE_IGNORE` 读盘），`attach`/`reload` 恒重读，`create_script` 响应 `cache_invalidated`→`cache_refreshed`；`open_editor_scene` 幂等成功（`already_open`）；`create_scene_node` 属性两轮应用（OBJECT 型先）；`set_resource_uid`/保存路径按表成员选 `add_id`/`set_id`；文本链路全 `String::utf8`（CJK 往返）
- 错误水印：响应顶层 `new_errors_since_last_call` 一次性消费（`error_watermark.hpp`）；`editor_readiness` 导入中返回 `retryable` 软错误
- 可观测与持久化（09-20 批次）：`src/core/log_system.{hpp,cpp}` 增 `LogEntry.detail` + `LogSystem::log_detailed`（query 的 `filter_text` 大小写不敏感匹配 message∪detail，detail 上限 8192 字符）；中心埋点在 `src/tools/tool_spec.hpp`（`SpecTool::execute` 记 `TraceEvent` + 人类 detail，慢工具阈值 `kSlowToolMs=2000ms`、`error_code` 取 `structured_error.code`）、`src/tools/tool_invoke.{hpp,cpp}`（thread_local span 栈 + `capture_trace_context`/`ScopedTraceContext` 跨线程传递，22 处 `execute_sync` 调用点已包装）、`src/tools/dispatch.cpp`（异常分支 `record_dispatch_exception`）；`src/core/trace_recorder.{hpp,cpp}`（环缓冲 20000、`sanitize_args` 脱敏 4000/原始 64000 字符、`to_json_line`、`decode_base64`）与 `src/core/log_persist.{hpp,cpp}`（`user://godot_autopilot/logs/gda-*.log` 人类全量、`traces/trace-*.jsonl` 结构化事件，各保留最近 20 文件/50MB，主线程 `_process` flush + 游标增量桥接，写失败降级一次告警）；图片仅脱敏关闭时写 `traces/images/`，jsonl 只记 `image_ref`/`image_bytes`/`image_hash`/`image_width`/`image_height`；脱敏开关 `src/core/sanitize_policy.*`（env `GODOT_AUTOPILOT_DESENSITIZE` > config `desensitize` > 默认 true，即时生效）；已知修复（防回归）：`FileAccess::READ_WRITE` 打开不存在的文件会失败（不创建），持久化/追加写入必须回退 `WRITE`——`log_persist.cpp` 与 `text_ops.cpp` 的 file_write APPEND 已按此处理
- Dock/UI：`McpLogDock` 新增 Detail 切换与 Open Logs 按钮（打开 `user://godot_autopilot/logs`）；`McpConfigDock` 新增 "Desensitize data" 复选框（写 config + `sanitize_policy::set_enabled`）
- 互转：`VariantJson::serialize/deserialize`（`util/variant_json.hpp`）；错误 `{"error": "msg"}`，`call_tool` 置 `is_error=true`

## 添加工具

1. `src/tools/<域>_ops.hpp` 声明 `handle_xxx`，`<域>_ops.cpp` 实现（返回 `{"error": ...}` 或结果对象）
2. `<域>_tools.hpp`：参数写成 `const std::vector<ParamSpec>` 表，`make_tools()` 里 `v.push_back(make_spec_tool(ToolSpec{name, description, category, {tags}, side_effect, flags, kXxxParams, handler}))` 一行（`side_effect`/`flags` 按需：只读用 `SideEffect::None` + `tool_flags::kNone`，有副作用/可变用对应值 + `kMutating`，合成输入类另加 `kObserve`；图片交付/场景目标/可撤销维度按需加 `kCaptureImage`/`kSceneTarget`/`kUndoable`，口径见架构段 traits 行）；`register_all.cpp` 经各域 `make_tools()` 自动注册
3. schema 由参数表自动派生（`raw_schema` 仅手写嵌套结构时使用），**无须另建 schema 文件**；仅新增 `.cpp` 才需动 `CMakeLists.txt:65` 的 `add_library()`（header-only 无需），迁移守卫（`tests/guard/`）拒绝旧宏/schema 文件残留；命名 `<动词>_<类别>_<维度>_<对象>_<修饰>` snake_case 动词置首（如 `create_scene_node`）

## 约定

- 命名空间 `godot_autopilot`；日志仅 `System/Transport/Tools/Resources/Prompts`
- 编译器优先 Clang/clang-cl，sccache/LTO/Unity/Ninja 池自适应；依赖 godot-cpp 10.0.0-stable、mcp-cpp-sdk 0.3.4（FetchContent，无子模块）
- 注释规范：除 `// namespace` 结尾标记外源码禁止注释，解释进 wiki；`ctest -R comment_guard` 门禁

## 测试

- 启用：`GDA_ENABLE_TESTS` 已在 `CMakePresets.json` debug/release 置 `ON`（裸 `cmake` 默认 `OFF`）
- 运行：`ctest --preset debug`（L1 秒级，L2 全量约 12 分钟需 `GODOT_PATH`）；单跑 `build/debug/tests/gda_test_runner.exe --file 01_scene`；CI 仅 `ctest --preset debug -E "^gda_runner_"`（L1）
- 结构：L1 `gda_unit_tests` 285 gtest（31 文件；`tool_args_test` 11 项、`tool_registry_test` 5→6 增 flags 路由用例，本轮新增 `tool_pipeline_traits_test` 4 项、`tool_invoke_test` 4 项、`batch_refs_test` 6 项，本批次新增 `trace_recorder_test` 11 项、`log_persist_test` 5 项）+ 迁移守卫 `migration_guard` 与注释守卫 `comment_guard` 各 1 项，`ctest --preset debug -E "^gda_runner_"` 共 287 项；L2 `gda_test_runner` + `tests/config/*.json` 29 份（`00_meta`…`10_editor_input` 11 份 + `11_editor_tree`/`12_capture_params`/`13_inline_subresource`/`14_tilemap_rect`/`15_scene_path`/`16_game_jobs`/`17_vision_assist`/`18_script_freshness`/`19_cjk_roundtrip`/`20_cjk_text_roundtrip`/`22_uid_guard`/`23_click_ui_coords`/`24_keycode_alias`/`25_open_scene_idempotent`/`26_user_tools_code_mode`/`27_user_tools_rescan`/`28_sprite_frames_animation`/`29_trace_persistence`；21 号段空缺，99 号诊断用例已删）；ctest 注册点 316（287 + 29；L2 需 `GODOT_PATH`；L2 授权 capability 由 `GODOT_AUTOPILOT_ALLOW` env 或 `user://godot_autopilot/config.json` 的 `allow` 字段提供，env 优先，`16_game_jobs` 的 `start_game_job`、23/24 游戏侧路径需 `game_runtime`，18/22 与 26/27/29 号脚本路径需 `code_execute`，26/27 号注册/执行用户工具另需 `user_tools`；计数以运行时统计为准）
- skill_gen：内容源 `src/util/skill_templates/`（30 个 .md + registry.json，8 册技能 = 1 总纲 `godot-autopilot` + 6 引擎指南 + 1 C# 专册 `godot-autopilot-csharp`，每册带 references/；84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册，4.7+/4.8 行内简注；编辑器 UI/输入自动化工具用法已补充进相关分册）+ `tools/embed_skills.py` 构建期嵌入（`SKILL_COUNT = 8`；5 项校验 name/description/files 结构与孤儿文件，生成头入 `build/<preset>/generated/`，gitignore 覆盖；渲染至 `.agents/skills/`，dock 按钮 Generate Skills/Update Skills 动态切换，Update 先递归清理 `godot-autopilot-` 前缀目录再重建），`tests/unit/skill_gen_test.cpp` 7 用例 L1 校验（8 册清单/name/description/文件布局/frontmatter/反引号词回验 catalog∪schema 参数名∪176 项白名单/每册 references 声明），无需 Godot；L1 计数 285 为 `TEST`/`TEST_F` 宏静态统计（31 文件，以运行时统计为准）
- 客户端一键配置（`client_config_gen` + McpConfigDock 下拉）：20 个客户端的项目级 MCP 配置生成/合并（09-16 由 8 扩至 20：新增 ZCode `.zcode/config.json` mcp.servers 双层 / Command Code、CodeBuddy 与 pi 共用 `.mcp.json` / Kilo `.kilo/mcp.json` streamable-http / Roo `.roo/mcp.json` / Grok Build `.grok/config.toml` / Kimi `.kimi-code/mcp.json` / Zed `.zed/settings.json` context_servers / Crush `.crush.json` mcp+$schema / Copilot VS Code `.vscode/mcp.json` servers 键 / Reasonix `reasonix.toml` `[[plugins]]` 数组；调研淘汰 Cline 与 Antigravity CLI——项目级可能不生效；TOML 判定 `uses_toml(id)`，merge_toml_config 带 ClientId 区分 mcp_servers 段与 plugins 数组两种 marker）
- 新增用例 = 新建 `tests/config/*.json` 零 C++；Godot 路径 `GODOT_PATH` env > `.env`（`.env.template` 复制），缺失则执行器以退出码 2 报错（不自动跳过）
- 遍历：`03_tools_contract` 运行时经 `search_tools` 空 query 枚举（392 条 = 391 域 + `system_status`，跳过 7 个协议级元工具），再按 `get_tool_detail` 的 `side_effect`/`mutating`/`dynamic` 字段排除，**330 个**进入空参契约 + 启发式冒烟两步骤（上限 660 步，实测全过；以运行时统计为准）；warnings 以运行时报告为准（历史基线 4 条：`start_input_gamepad_vibration`/`stop_input_gamepad_vibration`/`get_resource_extensions`/`reimport_resource_files`，`create_scene_node` 是否告警依赖运行期场景状态）
- 迁移守卫：`tests/guard/migration_guard.cmake` + `migrated_domains.txt`（30 域 + `strict`）零依赖读源码断言——域文件无 `GDA_TOOL_CLASS`、无 `schema_*_ops.cpp`、无 `tool_input_schema`/`tool_decl.hpp` 引用；`ctest --preset debug -R migration_guard` 或 `cmake -DSOURCE_DIR=<仓库根> -P tests/guard/migration_guard.cmake` 运行（实测输出 `OK (domains: 30, strict: on)`）
- 副作用：L2 后 `Example/project.godot` 可能追加 `[audio]/[input]` 并生成 `default_bus_layout.tres`，`git checkout -- Example/project.godot` 清理

## 知识局限

- schema 非空/空数以 `register_all_test.cpp:SchemaStatisticsBaseline` 运行时统计为准（当前基线断言 catalog 399 总条 = 336 非空 + 63 空）
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
- changelog 条目页为历史记录：链接约束按"新增条目至少 1 条相对链接"执行，历史页不回溯补链
- 数值口径以 `docs/wiki/tests.md` 的"数值核算总表"为准，AGENTS.md 测试段与之一致（当前 L1 285/31 文件、L2 29 份、ctest 316、工具 399）
