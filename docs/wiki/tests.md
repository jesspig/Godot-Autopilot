---
type: 测试体系指南
title: 测试体系
description: L1/L2 双层测试体系、遍历排除清单、数值统计与已知引擎副作用
tags:
  - 测试
  - L1
  - L2
timestamp: "2026-09-20T23:05:34+08:00"
resource: tests/
---

# 测试体系（tests/）

> 审计日期：2026-09-19（2026-08-29 0.2.2 发布审计——L1 77、L2 7 份；09-02 安全与并行硬化——L1 77→96（新增 security_parallel_hardening 17 项，覆盖队列/路径/鉴权/限额/日志）、核心路径/扫描/响应边界与构建大小写修复；09-08 技能生成器——L1 96→103（新增 skill_gen 7 项），ctest 注册点 103→110，同日 skill 内容外置化——测试零改动仍 103；09-10 skill 体系 19→7 册重构——用例改名与白名单扩充，仍 103；09-13 上午随反馈修复批次同步——L1 103→114（新增 mcp_image_content 11 项），L2 7→8 份（新增 07_scene_tabs），ctest 110→122；09-13 下午随收口批次同步——L1 114→126（新增 variant_json_strict 12 项），L2 8→9 份（新增 08_property_readback），ctest 122→135，L2 运行需 `GODOT_AUTOPILOT_ALLOW`；技能体系 8 册；09-13 晚随 A 组知识库审计修复批次同步——02/03/04/05/06 用例步骤与内容重核（02=before_all 2+39、03=1+2、04=after_all 1+28、05=13、06=26）、traversal/register_all_test 行号修正；09-13 晚随 0.2.4 版知识库全量审计同步——after_all 失败语义修正（工具报错不改变整体判定，仅进程死亡补 fatal_error）、CRASH_LOG_LIMIT 表述改为“截断至 2000 字符”、L2 用例发现机制改为“9 份均由 GLOB 自动发现”、历史事故出处修正并补 todo；09-14 随修复批次同步——L1 126→140（variant_json_strict 12→17、security_parallel_hardening 17→20、runtime_ops 1→3、新增 log_ops_test 4 项），unit 测试文件 14→15，L2 仍 9 份，ctest 注册点 135→149；09-15 随 Computer Use grounding 批次同步——L1 140→172（新增 editor_coords_test 32 项），unit 测试文件 15→16，L2 9→11 份（新增 09_editor_ui、10_editor_input），ctest 注册点 149→183；03_tools_contract 枚举 322 个非 SIDE 工具、0 失败、warnings 实测 4 条；09-16 随失败修复批次（feature/failure-remediation）同步——L1 172→229（新增 editor_ui_tree_test 6 / inline_resource_json_test 12 / skill_resources_test 12 / tilemap_ops_test 13 项，editor_coords_test +5、runtime_ops_test +9），unit 测试文件 16→20，L2 11→17 份（新增 11_editor_tree、12_capture_params、13_inline_subresource、14_tilemap_rect、15_scene_path、16_game_jobs），ctest 注册点 183→246；03 遍历 324 个非 SIDE 工具、0 失败、warnings 仍为基线 4 条；L2 授权前置改述：capability 由 `GODOT_AUTOPILOT_ALLOW` env 或 `user://godot_autopilot/config.json` 的 `allow` 字段提供（env 优先），16_game_jobs 的 start_game_job 需 `game_runtime`；09-16 随客户端配置生成器扩容批次同步——L1 229→238（client_config_gen_test 11→20，覆盖 20 客户端渲染与双 TOML 形态），ctest 注册点 246→255，基于当前工作树代码逐行核对（不依赖 git 历史）；09-16 随视觉辅助场景批次同步——L1 238→246（新增 `vision_assist_test` 8 项），unit 测试文件 20→21，L2 17→18 份（新增 17_vision_assist），ctest 注册点 255→264；09-16 随坐标换算/键名统一/脚本新鲜度/UID 守卫/CJK 往返/open 幂等/属性两轮批次同步——L1 246→267（新增 game_ui_coords_test 6 / keycode_alias_test 6 / script_freshness_test 7 / uid_guard_test 2 项），unit 测试文件 21→25，L2 18→25 份（新增 18_script_freshness、19_cjk_roundtrip、22_uid_guard、23_click_ui_coords、24_keycode_alias、25_open_scene_idempotent、28_sprite_frames_animation；20/21/26/27 号段空缺），ctest 注册点 264→292；03 遍历候选 324→325 个非 SIDE 工具（`review_scene_visually` 计入）、上限 650 步，warnings 仍为基线 4 条（本轮未动相关工具口径）；09-17 随 L1 精简批次同步——L1 267→243（`editor_coords_test` 37→15 删仿射子集用例、`vision_assist_test` 8→7、`tool_catalog_test` 7→6 删 `PopulateDefaultToolsOnce`，`populate_default_tools` 函数与声明整体删除），ctest 注册点 292→268，构建实测 L1 243/243 全绿；09-19 收尾第二轮随 traits/invoke/rescan/batch 同步——L1 255→269（新增 `tool_pipeline_traits_test` 4 / `tool_invoke_test` 4 / `batch_refs_test` 6 三文件 14 项，unit 文件 26→29），L2 27→28 份（新增 `27_user_tools_rescan`），ctest 注册点 283→298（主代理全量实测 298/298 全绿）。
> 覆盖范围：`tests/` 全部（unit 31 文件、runner 7 实现 + 6 头文件、integration、config 29 JSON、`tests/guard/` 迁移守卫与注释守卫、`tests/CMakeLists.txt`），对照 `tests/README.md` 与仓库根 `AGENTS.md` 测试段逐条核算。285 项 L1 为 `TEST`/`TEST_F` 宏逐行统计口径（09-20 可重放监控批次 269→285：新增 `trace_recorder_test` 11 / `log_persist_test` 5，unit 文件 29→31；09-19 数据化重构 243→255：新增 `tool_args_test` 11 项、`tool_registry_test` 5→6；收尾第二轮 255→269：新增 `tool_pipeline_traits_test` 4 / `tool_invoke_test` 4 / `batch_refs_test` 6；历史 09-17 精简批次 267→243：editor_coords 37→15、vision_assist 8→7、tool_catalog 删 `PopulateDefaultToolsOnce`；09-16 历史：新增 vision_assist 8 项 + 4 文件 21 项、client_config_gen 11→20）；ctest 注册点 316 = 285 gtest + 2 守卫 + 29 份 L2（L2 与守卫计数可静态核实，最终以 `ctest -N` 为准）。
> 09-19 ToolSpec 数据化重构：工具声明全量改为 `ToolSpec`（`make_spec_tool`，30 域 391 工具 + `system_status` + 7 元），删除 `GDA_TOOL_CLASS`/`tool_decl.hpp`/`fn_tool.hpp`/`meta_tools.hpp`/`IMetaTool`/8 个 `schema_*_ops.cpp`/`schema_fills.hpp`/`tool_input_schema`；新增 `tool_args`（L1 `tool_args_test` 11 项）/`tool_pipeline`（`kObserve` 截图后处理）/`dynamic_spec_store`/`autopilot_tools`（用户脚本动态工具，`user_tools` 授权门）与 `tests/guard/` 迁移守卫（30 域 + strict，实测 `OK (domains: 30, strict: on)`）；L2 新增 `26_user_tools_code_mode`（26→27 份，含 09-18 的 20 号）；遍历改为运行时经 `search_tools` 空 query 枚举 392 条后按 `side_effect`/`mutating`/`dynamic` 排除（实测 330 个进入空参+冒烟两步骤全过）；`tests/config/99_user_tools_diag.json` 诊断用例已删除。
> 09-18 CJK 传输链收敛批次：L2 新增 `20_cjk_text_roundtrip`（11/11，零 C++，编辑器侧中文文本往返），L2 配置 25→26 份；L1 用例数不变（新增 5 个 CJK 单测因无 Godot 运行时整体回退，覆盖移交 L2-20）。
> 09-18 E2E 优化批次：域 385→391（非 SIDE 325→329、SIDE 60→62，`patch_script` 计 writes_file 15→16、`build_nodes_from_spec` 计 None 1→2）、catalog 393→399；03 实测 603 步全过（空参 329 + 冒烟 273 + 56 跳过，warnings 基线 4 条不变）；L1 243 不变；本轮新增 L2 用例 0 份（25 份）。
> 09-18 本地 L2 环境约束（残余风险）：`Example/` 含 E2E 残留游戏（`scenes/main.tscn` + `run/main_scene`）时，无头编辑器在首次文件扫描完成后约 0.5–0.8s 自开主场景，与用例 `before_all` 干净场景竞态（只读探针复现，移开 `scenes/` 后消失）；污染态 7 份（01/02/11/14/15/18/28）失败、干净态 25 份全绿，failing handler 所在域本批次零改动。本地跑 L2 前建议确认 `Example/.godot/editor/editor_layout.cfg` 无残留 open_scenes；`Example/project.godot` 的 34 行 E2E 增量保持原样。
> 09-18 补记：`GDA_ENABLE_TESTS` 开关行号 `CMakeLists.txt:164`→`:168`（以根 `CMakeLists.txt` 实测为准，`option(GDA_ENABLE_TESTS` 在 `:168`）；09-20 行号漂移修正——当前在 `:171`。
> 09-19 收尾第二轮（traits/invoke/rescan/batch）：traits 收编终态（`kCaptureImage` 附件按 flag 判定/`kSceneTarget` 成功响应幂等补全/`kUndoable` 纯标记）+ `src/tools/tool_invoke.{hpp,cpp}`（`invoke_tool`/`invoke_depth`，深度上限 8）+ `AutopilotTools::rescan`（默认 `res://addons/godot-autopilot-tools`，256 文件/深度 8/逐文件隔离）+ `batch_execute` 最小变量串联（`$prev`/`$steps[N].result`/`$steps[N].error` 整字符串精确替换，`unresolvable reference` 前缀）；L1 新增 3 文件 14 项（269，29 文件），L2 新增 `27_user_tools_rescan`（28 份），ctest 注册点 298（269 + 1 + 28，主代理全量实测 298/298 全绿）；`samples/user-tools/echo_tool.gd` + `echo_tool.cs`（C# 文档级，未经本仓库 CI 验证）位于 `samples/user-tools/`，不在测试工程内。
> 09-20 代码-文档一致性审计：ctest 注册点 298→299（补计 09-19 注释清理批新增的 `comment_guard`；`ctest -E "^gda_runner_"` 非 runner 口径 270→271）、新增"守卫"小节；269/28/391/399/62/330 等其余数值逐项复核一致。
> 09-20 可重放监控与双目录持久化批次：新增 L1 `trace_recorder_test` 11 项 + `log_persist_test` 5 项（269→285 gtest，unit 文件 29→31），L2 新增 `29_trace_persistence`（28→29 份，headless，需 `code_execute`）；`ctest -E "^gda_runner_"` 271→287、全量 ctest 注册点 299→316；`ctest --preset debug -E "^gda_runner_"` 287 项与 L2 `29_trace_persistence` 均为实测通过。

## 架构总览

测试分两层，由根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 引入。该开关已固化在 `CMakePresets.json` 的 debug/release 预设（cacheVariables 默认 `ON`），**清理/重建 `build/` 后重新配置（`uv run build.py` 或 `cmake --preset debug`）自动恢复测试目标，无需手动传参**；直接以裸 `cmake`（不带 preset）配置时仍为默认 OFF（`CMakeLists.txt:171`）：

| 层 | 目标 | 可执行文件 | 启动引擎 | 判定方式 |
|---|---|---|---|---|
| L1 | 纯单元测试（不触碰 Godot API） | `gda_unit_tests`（gtest） | 否 | gtest 断言，`gtest_discover_tests` 逐条注册到 ctest |
| L2 | 配置驱动引擎内测试 | `gda_test_runner` | 是（headless 编辑器） | C++ 执行器对 `call_tool` 响应做断言（`assert_engine.cpp`） |

L2 的链路：执行器自管 Godot headless 编辑器进程 → 注入 `GODOT_AUTOPILOT_PORT` → 经真实 MCP HTTP（`integration/mcp_test_client.cpp`，Streamable HTTP 传输）驱动领域工具，每 step 一次 HTTP 往返。**每份 `config/*.json` = 一次独立的编辑器生命周期最小闭环**（启动 → MCP 就绪 → before_all → stages → after_all → 停止），文件间不共享状态。

## 构建与运行

```powershell
# 配置（测试开关已固化在预设，无需传参；下述命令为显式写法）
cmake --preset debug

# 构建
cmake --build --preset debug --target gda_unit_tests gda_test_runner

# 全量运行（ctest；L2 需 GODOT_PATH）
ctest --preset debug

# 单跑
ctest --preset debug -R gda_runner_01_scene
build\debug\tests\gda_test_runner.exe --file 01_scene
```

- **L1 依赖**：googletest 由 FetchContent 拉取（v1.15.2，GIT_SHALLOW，`tests/CMakeLists.txt:10-13`）；仅 L1 链接。
- **L2 依赖**：Godot 可执行文件。路径解析 `GodotProcess::resolve_godot_path()`（`godot_process.cpp:513-519`）：进程环境变量 `GODOT_PATH` 优先，为空才回退解析仓库根 `.env`；二者皆无 → 执行器退出码 2。
- **L2 授权前置**：涉及任意脚本/游戏运行时的用例需要 capability 授权——`00_meta`（code_execute）、`04_resources_scripts`（execute_script）、`16_game_jobs`（start_game_job 需 `game_runtime`）、`18_script_freshness` 与 `22_uid_guard`（execute_script，需 `code_execute`）、`23_click_ui_coords` 与 `24_keycode_alias`（游戏侧路径，需 `game_runtime`）、`26_user_tools_code_mode`（`code_execute` 驱动 + 注册/执行用户工具需 `user_tools`；before_all 经 `AutopilotTools.set_enabled(true)` 写配置自包含，但 env `GODOT_AUTOPILOT_ALLOW` 若已设置且不含 `user_tools`（也无 `all`）则 env 优先、set_enabled 无效，用例步骤 1 失败，此时需在 env 中补 `user_tools` 或取消该 env）、`27_user_tools_rescan`（同 26 号：`code_execute` 驱动 + rescan 扫描与注册需 `user_tools`，env 优先陷阱相同）、`29_trace_persistence`（`execute_script` 扫描落盘目录，需 `code_execute`）；授权由 `GODOT_AUTOPILOT_ALLOW` 环境变量（如 `code_execute`、`game_runtime`、`user_tools` 或 `all`）或 `user://godot_autopilot/config.json` 的 `allow` 字段提供（env 优先），执行器不注入授权变量，缺失时相应用例被授权门拒绝而 FAIL。
- **L2 平台限制**：`godot_process.cpp` / `mcp_test_client.cpp` 的进程管理与 Winsock 实现均为 `#ifdef _WIN32`，非 Windows 平台返回"仅支持 Windows"（失败/不可用）。
- L2 产物目录：`build/debug/tests/`（ctest 报告输出到 `build/debug/tests/output/`，由 `tests/CMakeLists.txt:130` 指定 `--report-dir`）。

### CLI 与退出码（`tests/runner/main.cpp`）

参数：`--config-dir`、`--file`（可含或不含 `.json` 后缀）、`--headless|--gui`（互斥；用例 JSON 的 `headless` 字段优先）、`--no-auto`（不启动进程，端口取 `GODOT_AUTOPILOT_PORT` 直连外部 MCP）、`--keep-open`、`--report-dir`、`--help`。

退出码：0 全部通过；1 存在 FAIL/ERROR；2 参数或环境错误（含 `GODOT_PATH` 未配置、`--no-auto` 端口未就绪、`--file` 无匹配、未捕获异常）。

### 执行闭环（`godot_process.cpp`）

1. 随机空闲端口（`bind 127.0.0.1:0` 取回，`pick_free_port`）
2. 首次 `--editor --import` 幂等同步执行（120s 超时，失败/超时不致命，仅记日志）
3. 临时注入 `GODOT_AUTOPILOT_PORT` 后常驻启动 `--editor`（`--headless` 由用例决定；另注入 `GDA_FORCE_HEADLESS=1`）
4. 就绪轮询（200ms 间隔）：TCP 端口探测（connect 5s 超时）→ MCP `initialize` 握手（裸 `POST /mcp` JSON-RPC，响应含 `serverInfo` 即确认；读侧 5s 超时，服务器可能保持连接、超时退出而非挂死）
5. 执行 `before_all` → stages → `after_all`（`pipeline_executor.cpp`；`before_all` 失败即整体 `fatal_error`；`before_all`/`after_all` 为 plain 步骤不支持 expect 断言，`after_all` 工具报错不改变整体判定（仅进程死亡导致跳过时补 `fatal_error` 说明）；stages 失败按 `on_failure` fail_fast/continue）
6. 停止：`taskkill /PID` 软杀 → 5s 宽限 → `TerminateProcess` 兜底；崩溃时附编辑器日志（`pipeline_executor.cpp` 的 `CRASH_LOG_LIMIT` 截断至 2000 字符）

stdout/stderr 各接独立管道读线程持续消费，防 64KB 缓冲写满阻塞子进程。

### 报告（`runner_report.cpp`）

- 控制台表格：文件名称 / 通过步骤数 / 耗时（<10s 显毫秒，否则显秒）/ 状态 `PASS|FAIL|ERROR`（`fatal_error` 非空 → ERROR）
- JSON 报告：`tests/output/report-YYYYmmdd_HHMMSS.json`，字段 `generated_at` / `total_files` / `passed_files` / `files[].{name,passed,duration_ms,fatal_error,steps[]}`

## L1 单元测试

**31 个测试文件，实际 285 个 TEST/TEST_F**（`TEST`/`TEST_F` 宏逐行统计，2026-09-20 复核；09-20 可重放监控批次新增 `trace_recorder_test` 11 项、`log_persist_test` 5 项；09-19 数据化重构新增 `tool_args_test` 11 项、`tool_registry_test` 5→6（flags 路由用例）；收尾第二轮新增 `tool_pipeline_traits_test` 4 / `tool_invoke_test` 4 / `batch_refs_test` 6 三文件 14 项；历史：09-02 bm25 14→20 + security_parallel_hardening 17 项，09-08 新增 skill_gen 7 项，09-13 上午新增 mcp_image_content 11 项、下午新增 variant_json_strict 12 项、09-14 修复批次 variant_json_strict +5 / security_parallel_hardening +3 / runtime_ops +2 / log_ops_test +4、09-15 新增 editor_coords_test 32 项、09-16 新增 editor_ui_tree/inline_resource_json/skill_resources/tilemap_ops 4 文件 43 项且 editor_coords +5 / runtime_ops +9、09-16 晚 client_config_gen 11→20、09-16 视觉辅助批次新增 vision_assist_test 8 项、09-16 本轮新增 game_ui_coords 6 / keycode_alias 6 / script_freshness 7 / uid_guard 2 四文件 21 项；09-17 精简批次 editor_coords_test 37→15（删仿射子集用例）、vision_assist_test 8→7、tool_catalog_test 7→6（删 `PopulateDefaultToolsOnce`））：

| 文件 | 数量 | 主题 |
|---|---|---|
| `bm25_index_test.cpp` | 20 | BM25 检索：命中排序、分类/标签过滤、空查询、上限截断 |
| `register_all_test.cpp` | 14 | 注册管线：7 元工具 ListTools 往返、catalog 覆盖、schema 基线、未知工具错误、幂等重注册、双服务器一致性 |
| `client_config_gen_test.cpp` | 20 | 20 客户端配置文件渲染（URL/type/enabled/顶层键/ZCode 双层嵌套/Crush `$schema`/共享 `.mcp.json` 三客户端同构/Reasonix `[[plugins]]`，09-16 扩至 20 客户端）、JSON 合并三态（新建/保留其他键/非法）、TOML 追加与跳过（Codex 形态与 Reasonix 形态） |
| `command_queue_test.cpp` | 7 | 跨线程 submit/drain、异常经 future 传播、主线程记录 |
| `tool_catalog_test.cpp` | 6 | ToolCatalog：add/get、重复覆盖、并发安全（`PopulateDefaultToolsOnce` 已随 `populate_default_tools` 整体删除，09-17 精简批次） |
| `tool_args_test.cpp` | 11 | `Args` 取参器与 vector 版 `build_schema`（09-19 新增）：缺失/类型错误消息、`null` 视为缺失、整数接受整数值 double 拒绝小数、默认值、object/array 取用、`reject_unknown`、vector 与 initializer_list 两版 schema 一致 |
| `log_system_test.cpp` | 7 | 单例、级别/分类/文本过滤、环形缓冲覆盖、值拷贝快照与 recent 上限（09-02 增补） |
| `tool_registry_test.cpp` | 6 | `SpecTool`/ToolRegistry（09-19 更新）：add/find/categories、execute echo、`side_effect_of`、`kMeta` flags 归类、flags 路由与 `make_tool_info` 的 dynamic/mutating |
| `schema_builder_test.cpp` | 4 | schema 构造形状、required 位置、未知类型不崩溃（08-22 清理后 API 收敛为三件，用例收缩） |
| `error_util_test.cpp` | 3 | 错误 JSON 形状、四字段详情 |
| `runtime_ops_test.cpp` | 12 | 编辑器队列注入往返、`payload_is_pending` 识别（非对象/非整数拒绝）、`discard_pending` 未命中返回 false（09-14 增补 3 项；09-16 失败修复批次扩至 12 项） |
| `security_parallel_hardening_test.cpp` | 20 | 安全与并行硬化：队列关闭/满/线程校验、路径/鉴权、配置与截断、日志并发 |
| `skill_gen_test.cpp` | 7 | 技能生成器：8 册清单、name/description 规范、文件布局、frontmatter 渲染、反引号词回验（详见下节） |
| `mcp_image_content_test.cpp` | 11 | 截图 MCP image content（header-only `src/util/mcp_image_content.hpp`）：3 个 capture_* 白名单、顶层/嵌套 data 提取与优先级、data 替换为 `<attached-as-image-content>`、非白名单/非对象/缺 data/非 png 不注入 |
| `variant_json_strict_test.cpp` | 17 | `VariantJson::deserialize_strict` 严格形状：Rect2/Rect2i 的 `size` 别名（`w/h` 与 `x/y`）与冲突报错、缺 size、AABB 缺深度、Transform2D 缺 `columns`、Transform3D 缺 `basis.rows`、Vector2/Vector2i 对象要求；含完整错误消息文本断言 |
| `log_ops_test.cpp` | 4 | 游戏日志过滤（`log_ops::filter_log_lines`）：空 filter 返回全部、子串命中、保留末尾 limit 条、limit ≤0 返回全部命中 |
| `editor_coords_test.cpp` | 15 | 编辑器坐标纯逻辑（`src/core/editor_coords.hpp/cpp`）：`fit_within` 缩放（等比缩小不放大/最少 1 像素/非法输入）、`scale_size` 缩放链（恒等/整数倍/非法与溢出拒绝/与 fit_within 组合封顶）、截图 `diff_sample` 采样比较（阈值/网格采样/无效输入）与 bbox；09-17 精简批次删仿射子集用例（37→15） |
| `editor_ui_tree_test.cpp` | 6 | 编辑器场景树行纯逻辑（09-16 新增，`editor_ui_ops` 侧）：filter 匹配（空过滤/大小写不敏感跨 path 与 name/selected_only）、`max_items` 钳制上限 1000、ASCII 折叠仅限 ASCII 字母、相对路径分段拼接 |
| `inline_resource_json_test.cpp` | 12 | 资源属性内联描述解析（09-16 新增）：`{"type": ..., "properties": {...}}` 一步创建 `[sub_resource]` 的解析/校验与错误路径 |
| `skill_resources_test.cpp` | 12 | `godot://skills` MCP 资源解析（09-16 新增）：目录/单册/单文件 URI 解析与未知 URI 错误 |
| `tilemap_ops_test.cpp` | 13 | `fill_tilemap_rect` 矩形铺砖纯逻辑（09-16 新增）：角点归一/格数上限（>100000 报错）/擦除分支与 set_count 统计 |
| `vision_assist_test.cpp` | 7 | 视觉辅助截图参数纯逻辑（09-16 新增）：`scale`/`max_dimension` 组合封顶与小图不放大、`scale` 越界拒绝、`region` 左上交集裁剪、`diff_image` bbox 与阈值噪声忽略、`annotate_nodes_max` 先于绘制强制（09-17 精简批次 8→7） |
| `game_ui_coords_test.cpp` | 6 | `click_game_ui_element` 注入坐标换算纯逻辑（09-16 新增，`core/editor_coords.hpp`）：4x 拉伸、letterbox 边距、画布平移复合、rect 中心映射 |
| `keycode_alias_test.cpp` | 6 | 键名→键码统一判定纯逻辑（09-16 新增，`input_map_ops::resolve_key_name_code`）：裸名/`KEY_` 前缀/大小写不敏感/数字码等价、未知名判无效 |
| `script_freshness_test.cpp` | 7 | 脚本取用口径纯逻辑（09-16 新增，`script_ops.cpp`）：`script_load_cache_mode` 缓存模式选择、`wants_fresh_load` 只认字面 true |
| `uid_guard_test.cpp` | 2 | UID 写选择守卫纯逻辑（09-16 新增，`resource_ops::uid_needs_add`）：未注册走 `add_id`、已注册走 `set_id` |
| `tool_pipeline_traits_test.cpp` | 4 | traits 图片附件判定纯逻辑（收尾第二轮新增）：顶层 data 附加 image content、非 png/缺 data 拒绝、无 flag 透传（有 flag 的场景补全需编辑器，L1 未覆盖） |
| `tool_invoke_test.cpp` | 4 | 工具间组合调用纯逻辑（收尾第二轮新增，`tools::invoke_tool`）：未知/成功透传、异步 pending 嵌套拦截、深度守卫（导出分支无 setter，L1 未覆盖，注释已声明） |
| `batch_refs_test.cpp` | 6 | `batch_execute` 最小变量串联纯逻辑（收尾第二轮新增）：`$prev` 链、`$steps[N].result` 嵌套替换与字面量保持原样、前向引用/首操作 `$prev`/状态错配报错前缀、stop_on_error 截断 |
| `trace_recorder_test.cpp` | 11 | `TraceRecorder` 纯逻辑（09-20 可重放监控批次新增）：id 前缀/唯一性、seq 严格递增、`query_recent` 尾取与 0/超量边界、`query_by_trace` 过滤、`query_since` 边界与 `next_seq`、`clear_for_test` 重置、脱敏剥离 `data`/`base64` 值保留其他键、脱敏/原始两种截断口径、FNV-1a 稳定值与区分、base64 解码（padding 省略/噪声字符）、`to_json_line` 核心键与转义 |
| `log_persist_test.cpp` | 5 | `LogPersist` 纯函数（09-20 可重放监控批次新增）：`format_human_line` 三态（含 detail/不含/空 detail）、`session_stamp_from_ticks` UTC 转换、`should_prune` 边界（>20 文件或 >50MB）、`level_name`/`category_name` 全枚举与未知值 |
| **合计** | **285** | |

**链接来源**（`tests/CMakeLists.txt` 的 `GDA_UNIT_BUSINESS_SOURCES`，共 25 个显式 + 1 组 glob）：`src/core/` 的 `log_system`、`log_persist`、`resource_registry`、`scene_dirty_tracker`、`export_guard`、`editor_readiness`、`editor_coords`、`trace_recorder`；`src/util/` 的 `bm25_index`、`error_util`、`readback_util`、`scene_verify`、`variant_json`、`client_config_gen`、`skill_gen`、`skill_content_generated`（构建期嵌入薄胶水）；`src/tools/` 的 `tool_catalog`、`schema_builder`、`register_all`、`dispatch`、`debugger_access`、`editor_ui_actions`、`tool_args`、`tool_pipeline`、`tool_invoke`；`src/tools/*_ops.cpp`（`GDA_TOOLS_OPS_SOURCES` glob，`register_all.cpp` 引用全部 `handle_xxx` 符号故必须链接）。

**约束**：L1 禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃）；`register_all_test` 仅测注册/分发/未知工具错误路径。`SchemaStatisticsBaseline`（`register_all_test.cpp:131-148`）为运行时统计：当前基线断言 catalog 399 总条 = 336 非空 + 63 空（09-19 复核；随工具集变化，以运行时统计为准）。`client_config_gen` 为纯 C++（仅 std + `mcp::JsonValue`），不触碰 Godot API，可安全纳入 L1。`sanitize_policy.cpp` 未列入 L1 业务源——`enabled()`/`set_enabled()` 为 header 内联原子缓存（L1 经 `TraceRecorder::sanitize_args(text, bool)` 直接传参断言脱敏口径），`initialize()` 仅由入口调用、不参与 L1 链接。

### skill_gen 技能生成器测试（`skill_gen_test.cpp`，09-08 新增）

生成器 `src/util/skill_gen.cpp`；8 册正文外置为 `src/util/skill_templates/`（31 个文件——30 个 .md + registry.json，registry 8 条映射；模板为平铺命名：主册 `<name>.md` + 参考 `<name>--<ref>.md`，渲染时经 `files[].path` 映射为 `<name>/SKILL.md` + `<name>/references/*.md`），经 `tools/embed_skills.py` 构建期嵌入为生成头 `skill_content_embedded.h`（入 `build/<preset>/generated/`，gitignore 覆盖），渲染目标 `.agents/skills/<name>/SKILL.md`。纯 C++ 不触碰 Godot API，L1 可跑无需引擎。09-08 内容外置化重构（7 个 `skill_content_*.cpp` → 模板目录 + 构建期嵌入）测试零改动全绿；**09-10 skill 体系 19→7 册重构（每册带 references/ 渐进披露 + 84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册）后，用例改名与白名单扩充；09-13 新增 `godot-autopilot-csharp` 册（8 册）并同步 `SKILL_COUNT = 8`（用例仍为 7 项，未随收口批次变化）**。7 个用例：

| 用例 | 内容 |
|---|---|
| `AllEightSkillsPresent` | 8 册 name 定稿清单逐一核对（数量 = 8，双向集合比对：多出与缺失均 FAIL） |
| `SkillNamesFollowSpecRules` | name 匹配 `^[a-z0-9]+(-[a-z0-9]+)*$`、≤64 字符、路径 `.agents/skills/<name>/SKILL.md` |
| `DescriptionsValid` | description 非空、≤1024 字符 |
| `FilesLayout` | `files[0]` 必为 SKILL.md，其余须 `references/` 前缀；body 非空、无 PLACEHOLDER 残留 |
| `RenderedFrontmatterWellFormed` | 渲染产物 frontmatter 完整：name/author/`version`（取 `GDA_VERSION`）/description 含 `: ` 必加引号/`metadata:` 与收尾 `---` |
| `SkillRegistryFixture.ToolNamesExistInRegistry` | 工具名回验（机制见下） |
| `EverySkillDeclaresReferences` | 每册须至少 1 个 `references/` 文件（files.size() ≥ 2，渐进披露结构契约） |

**工具名回验机制**：`SkillRegistryFixture` 照抄 `register_all_test.cpp` 的服务端构造方式（`register_all_tools` 填充 catalog，仅注册不调用任何 handler），允许集 = 运行时 catalog 全部工具名 ∪ 各工具 `input_schema.properties` 参数名 ∪ 176 项手动白名单（`kManualWhitelist`，实测 176 条，均为明确非工具名的词：Godot API 方法、Godot 属性名、内部机制/返回字段、枚举值等）；随后提取各册正文反引号包裹的全小写含下划线词逐一比对（``` 围栏代码块整体跳过），未命中即 FAIL。保证技能文档中出现的工具名/参数名与运行时注册始终一致。

## L2 配置驱动用例

**29 个用例文件 → 29 条 ctest 用例**（`tests/CMakeLists.txt` 的 config GLOB 注册段：`gda_runner_<文件名去后缀>`，`TIMEOUT 600`；21 号段空缺，99 号诊断用例已删；数量以 `tests/config/*.json` 实测为准）：

| ctest 用例 | 文件 | name | 步骤数 | 内容 |
|---|---|---|---|---|
| `gda_runner_00_meta` | `00_meta.json` | meta_tools | 15 | 7 个元工具语义：ping / search_tools（关键词、分类、标签三查）/ list_categories / get_tool_detail（命中、缺失、缺参）/ call_tool（未知、缺参）/ batch_execute（成功、含失败）/ code_execute（返回值、语法错、缺参） |
| `gda_runner_01_scene` | `01_scene.json` | 01_scene | before_all 1 + 19 | 场景节点：创建（显式根路径/默认值/子节点/非法类型/双根）、删除（成功/缺失/根禁删）、树查询、未保存拒绝切换、group 四步、缺参报错 |
| `gda_runner_02_property` | `02_property.json` | property_tools | before_all 2 + 39 | 属性读写：int/string/Vector2/Color 写读回、缺属性/缺节点报错、readback MATCHED、int 字符串静默转 0、property_get_list、缺 value 报错；另覆盖脚本属性引用与 Node 引用、数组/PackedArray 严格形状校验、property_get_list 过滤（script 变量命中/未命中、engine 属性）与 fixture 清理 |
| `gda_runner_03_tools_contract` | `03_tools_contract.json` | tools_contract | 1 + 2 个步骤 | reload_resource 缺 path 契约断言 + 全量遍历：empty_args + heuristic_smoke（详见下节） |
| `gda_runner_04_resources_scripts` | `04_resources_scripts.json` | resources_scripts | after_all 1 + 28 | execute_script 四行为（单表达式自返/多行显式 return/语法错/缺参）+ get_resource_extensions / has_resource + search_tools 发现性 + save_resource copy-on-write（dest_path 落盘、源缓存不被改写）+ reload_resource（replaced/磁盘值/缺失报错）+ duplicate_resource 按 name 跨调用读写 + copy_resource_file（成功/类型/缺失源/同路径报错）；fixture 位于 res://gda_tmp_res_ops/，after_all 回收 |
| `gda_runner_05_rename_references` | `05_rename_references.json` | 05_rename_references | 13 | rename 事务化端到端（08-20 新增）：目录 rename 被拒绝并提示改用 move_resource_file → create/save 源资源取 uid → write_file 手工依赖（按 path 引用）→ 前向反查命中 → rename（断言 updated_files 非空 + uid_preserved）→ find_in_files 证明旧路径引用消失、新路径引用存在、marker 可搜索 → get_resource_references 按新路径命中、按旧路径为空。在 res://gda_tmp_rename/ 临时建删，跑后需清理该临时目录 |
| `gda_runner_06_move_references` | `06_move_references.json` | 06_move_references | 26 | 事务化 move_resource_file（08-24 新增）：create_directory 源/目标 → write_file 最小 tres（move_asset.tres，engine_managed update_file）→ get_resource_uid 源 → write_file 依赖 tscn（referrer.tscn）→ get_resource_references 前向命中 → open_editor_scene 保持打开 → move_resource_file 至新目录（断言 moved 非空、failed/stale_references 为空、reloaded_scenes 非空）→ get_resource_uid 新路径 → find_in_files 新旧路径双向证明 + has_resource_dependency/get_resource_references 新旧反查 → close_editor_scene + move_os_file_to_trash 清理临时目录；另含目录整体 move 用例（res://gda_tmp_move_dir/ 的 src/ 子目录移动后断言 moved/failed/stale_references/filesystem_scanned，并以 find_in_files 与 get_resource_references 双向证明磁盘引用重写） |
| `gda_runner_07_scene_tabs` | `07_scene_tabs.json` | 07_scene_tabs | 16 | 场景脏标签闭环（09-13 新增）：create_editor_scene(close_current) 建未命名 TabA → save_editor_scene_as 落盘 → property_set 致脏 → open_editor_scene(tab_b) 被拒绝并提示 save/reload → reload_editor_scene 返回 reloaded/scene_path/observed:true 清脏 → 再次 open 成功且 get_scene_tree 显示 TabB；另覆盖 reload 对未打开场景报 `scene is not open`、create_editor_scene 的 timeout_ms 非整数/越界报错且不影响当前场景；cleanup 关闭场景并回收 `res://gda_tmp_tabs/`。幽灵标签分支（多标签共存时移动依赖）未覆盖：runner 无打开第二个标签的步骤，无法制造陈旧标签状态，故放弃该分支 |
| `gda_runner_08_property_readback` | `08_property_readback.json` | property_readback | before_all 3 + 17 | 严格 JSON 形状回读（09-13 收口批次新增）：property_set 的 Rect2 文档形状（`{w,h}`，position 可省）与别名形状（`{x,y}`）成功并读回、size 轴冲突/缺失报错、Transform2D 缺 columns 报错；create_scene_node 属性应用（applied_properties 精确匹配 + 读回证明已生效）与严格错误路径；错误消息按 dispatch 包装后的完整字符串精确断言 |
| `gda_runner_09_editor_ui` | `09_editor_ui.json` | editor_ui | 5 | 编辑器 UI 只读正向路径 + 缺参错误（09-15 新增，headless、无副作用）：get_editor_ui_elements（max_elements 5，断言 result.space=window 且 count 非空）、get_editor_viewport_geometry（space=window）、hit_test_editor_point（position 10,10）、get_display_window_rect、get_scene_node_screen_rect 缺 paths 报 `missing required parameter: paths (array)`；headless 下元素数与命中数不确定，只断言结构字段存在 |
| `gda_runner_10_editor_input` | `10_editor_input.json` | editor_input | 6 | 编辑器输入/UI 动作参数校验（09-15 新增，headless）：全部在注入 warp/事件之前返回 error，不产生真实点击、拖拽、滚轮或文本输入副作用——click_input_mouse 缺 position、scroll_input_mouse 非法 direction（`invalid scroll direction: sideways`）、drag_input_mouse 缺 to、type_input_text 缺 text、click_editor_element 缺 path、run_editor_shortcut 未知键（`invalid shortcut key: notakey`），错误消息精确断言 |
| `gda_runner_11_editor_tree` | `11_editor_tree.json` | editor_tree | before_all 4 + 14 | 编辑器场景树行枚举与选中（09-16 新增，headless，含选中副作用）：`scene_tree_items` 行结构断言（path/name/type/depth/selected/collapsed/visible/rect）与 filter/selected_only/max_items/truncated、`select_scene_tree_node` 按 path 选中并联动 Inspector（inspect/focus/add 追加选中、selected_count），缺参与无场景报错 |
| `gda_runner_12_capture_params` | `12_capture_params.json` | capture_params | 7 | capture 新增参数的参数校验路径（09-16 新增，headless）：`after_frames` 负数与非整数、`when` 表达式错误码（`when_parse_error` 等）、`scale` 越界，编辑器/游戏两目标均在执行前报 error |
| `gda_runner_13_inline_subresource` | `13_inline_subresource.json` | 13_inline_subresource | 36 | 资源属性内联描述一步创建 `[sub_resource]`（09-16 新增）：property_set 与 create_scene_node 的 `properties` 内联 `{"type": ..., "properties": {...}}`（RectangleShape2D/Material 等典型形状）、响应回显 `inline_resources`、嵌套内联与形状错误路径 |
| `gda_runner_14_tilemap_rect` | `14_tilemap_rect.json` | tilemap_rect | before_all 1 + 11（after_all 1） | `fill_tilemap_rect` 矩形铺砖（09-16 新增）：from/to 角点（含逆序）成功铺砖与 erase 擦除、alternative/layer 参数、>100000 格上限报错、单次 undo；fixture 经 create_tilemap_tileset + add_tilemap_atlas_source 构建，after_all 回收 |
| `gda_runner_15_scene_path` | `15_scene_path.json` | 15_scene_path | 17 | 写工具响应回显目标场景路径（09-16 新增）：create_scene_node/delete_scene_node/property_set/rename_scene_node/attach_script_to_node 的 `scene_path`/`scene_unsaved` 字段——未保存场景上误建可被立刻发现，另有缺参错误路径 |
| `gda_runner_16_game_jobs` | `16_game_jobs.json` | game_jobs | 9 | `start_game_job`/`get_game_job` 参数校验与 headless 运行时通道路径（09-16 新增）：op 仅支持 eval、job 表上限 16、过期宽限 2s、cancel 语义；**需 `game_runtime` capability 授权**（`GODOT_AUTOPILOT_ALLOW` env 或 `user://godot_autopilot/config.json` 的 `allow` 字段，env 优先），未授权时用例 FAIL |
| `gda_runner_17_vision_assist` | `17_vision_assist.json` | vision_assist | 以 `17_vision_assist.json` 为准 | 视觉辅助截图参数（09-16 新增）：`annotate_nodes`/`annotate_nodes_max`、`diff_image`、`review_scene_visually` 参数校验与只读组合 |
| `gda_runner_18_script_freshness` | `18_script_freshness.json` | script_freshness | 13 | 脚本取用口径回归（09-16 新增，headless）：`create_script` 写盘后 `cache_refreshed=true` → `reload_script` 结果 0 → 覆写含 `@export var probe_marker=7` 版本 → `attach_script_to_node` 取磁盘版（断言 `instantiated=false` + `note` 非空——非 `@tool` 脚本在编辑器内 `can_instantiate=false` 系引擎语义，新鲜度以源码 MARKER 与 property 链路为准，09-17-23 起） → `execute_script` 读源码含新 marker → `get_script_property {fresh:true}` 得 7 → 节点 `property_get/set` 读写回；需 `code_execute` 授权（`GODOT_AUTOPILOT_ALLOW` env 或 `allow` 字段，env 优先） |
| `gda_runner_19_cjk_roundtrip` | `19_cjk_roundtrip.json` | cjk_roundtrip | 4 | CJK 文本落盘往返（09-16 新增，headless）：含中文注释脚本 `create_script`（`verified`/`readback`/`cache_refreshed` 全真）→ `read_file` 全量内容比对 → `find_in_files` 中文 query 命中 1 文件；after_all 回收 `res://tests_tmp` |
| `gda_runner_20_cjk_text_roundtrip` | `20_cjk_text_roundtrip.json` | cjk_text_roundtrip | 11 | 编辑器侧中文文本往返（09-18 新增，headless，11/11）：CJK 字符串写入/读回与传输链长度口径；零 C++，覆盖移交自原计划的 L1 单测（L1 无 Godot 运行时不支持 `godot::` 构造） |
| `gda_runner_22_uid_guard` | `22_uid_guard.json` | uid_guard | 13 | 资源 UID 守卫（09-16 新增，headless）：保存资源与 `set_resource_uid` 显式/省略 uid 三分支回验 + `get_game_log_entries` 配 `filter=unique_ids.has` 断言 `matched_lines=0`；需 `code_execute` 授权；fixture 位于 `res://gda_tmp_uid_guard/` |
| `gda_runner_23_click_ui_coords` | `23_click_ui_coords.json` | click_ui_coords | 8 | `click_game_ui_element` 参数校验与无游戏通道错误（09-16 新增，headless）：缺 path/path 非串/button 越界/double 非布尔/max 非正/timeout 越界各精确报错；合法参数无游戏时返 `error+hint` 通道错误；坐标换算不断言（须真实游戏进程，留待示例游戏恢复后补测，换算由 L1 `game_ui_coords_test` 覆盖）；需 `game_runtime` 授权 |
| `gda_runner_24_keycode_alias` | `24_keycode_alias.json` | keycode_alias | 13 | 键名口径统一验收（09-16 新增，headless）：编辑器侧裸名 `P`/`KEY_P`/大小写归一/裸数字/`SPACE` 全 `ok`、未知名精确报错；游戏侧 `queue_game_input` 入参接受路径（无游戏时 `error+hint` 通道错误）；探针动作 `gda_keycode_alias_probe` 前后清理，需 `game_runtime` 授权 |
| `gda_runner_25_open_scene_idempotent` | `25_open_scene_idempotent.json` | 25_open_scene_idempotent | 9 | `open_editor_scene` 幂等成功（09-16 新增，headless）：首次打开 `result=ok` → `get_scene_tree` 确认 → 同路径二次打开 `already_open=true` → 无效路径仍报错；fixture 位于 `res://gda_tmp_open_idem/` |
| `gda_runner_26_user_tools_code_mode` | `26_user_tools_code_mode.json` | user_tools_code_mode | before_all 1 + 7（after_all 1） | 用户脚本动态工具端到端（09-19 新增，headless）：`code_execute` 内 `AutopilotTools.register_tool` 注册 `user_echo_probe` → MCP `call_tool` 调用回显 → `search_tools`/`get_tool_detail` 标记 `dynamic=true` → `code_execute` 内 `api.call_tool` 直连（code-mode 桥）→ `get_tool` 取 handle 后 `unregister_tool` → 再调报 `domain tool ... not found`；**需 `code_execute` + `user_tools` 授权**（env 优先陷阱见"构建与运行"节）；after_all 兜底注销并关闭 user_tools |
| `gda_runner_27_user_tools_rescan` | `27_user_tools_rescan.json` | user_tools_rescan | before_all 1 + 5（after_all 1） | 目录扫描 rescan 端到端（收尾第二轮新增，headless）：`code_execute` 写 probe 文件（`register_autopilot_tools(api)` 约定）→ `api.rescan` 注册非空 → MCP `call_tool` 回显 → 二次 rescan 幂等（`registered` 空、`failed` 存在）→ 清理；夹具目录 `res://gda_tmp_user_tools/`；**需 `code_execute` + `user_tools` 授权**（env 优先陷阱同 26 号）；after_all 兜底注销并关闭 user_tools |
| `gda_runner_28_sprite_frames_animation` | `28_sprite_frames_animation.json` | sprite_frames_animation | 10 | 属性两轮应用回归（09-16 新增，headless）：`create_scene_node` 同传 `sprite_frames`+`animation`，断言 `applied_properties==["sprite_frames","animation"]` 且读回 `animation=="idle"`；fixture 内存 SpriteFrames 落盘 `res://gda_tmp_sprite_frames_anim/frames.tres` |
| `gda_runner_29_trace_persistence` | `29_trace_persistence.json` | trace_persistence | 3 | 日志/trace 双目录持久化端到端（09-20 新增，headless）：`system_status` 热身 → `execute_script` 扫描 `user://godot_autopilot/traces` 最新 `trace-*.jsonl`（断言含 `session_start` 与 `server_ready` 启动标记，并遍历全部 trace 文件断言不含 PNG base64 前缀——图片只记 `image_ref`/`image_hash`）→ 扫描 `user://godot_autopilot/logs` 最新 `gda-*.log`（断言含启动日志行 "Godot Self-Driving plugin starting"）；不调用 `capture_editor_viewport`（headless 无视口渲染）；**需 `code_execute` 授权**（env 优先） |

**JSON schema 冻结规则**（`config_loader.cpp` 强制校验，违规抛 `runtime_error` 且错误消息含字段路径）：顶层必填 `name`/`pipeline`；`headless` 默认 true；`on_failure` 仅 `fail_fast`（默认）| `continue`；`before_all`/`after_all` 仅 tool+args+id（不支持 traverse/expect）；`stages[].steps` 平铺保留顺序；步骤 `tool` 与 `traverse` 二选一；`traverse.kind` 仅 `domain_tools`；`traverse.mode` 仅 `empty_args`（默认）| `heuristic_smoke`；`args` 值任意 JSON、嵌套原样保留。

**断言语义**（`assert_engine.cpp`）：`has_keys` 顶层键存在性；`field_checks[].key` 点路径分段查找（仅对象点路径，不支持数组下标；任一段缺失判失败）；`value` 数值兼容（int/double 统一转 double）；`not_empty`（string 非空 / array、object `Size()>0` / null 判空失败）；无 `expect` 时仅检查 `last_call_was_error()` 为假（即工具调用未报错）。

## 全量遍历（`traversal.cpp`）

- **工具来源（运行时枚举）**：遍历经 `search_tools` 空 query 枚举全部 catalog 工具名（`list_tool_names`），响应形如 `{"results":[{"name","score"},...]}`；**实测 392 条 = 391 域工具 + `system_status`**，跳过 7 个协议级元工具；解析失败（非 JSON 对象/缺 results 数组/清单为空）抛 `std::runtime_error`
- **排除判定**：每工具先 `get_tool_detail`（响应含 `side_effect`/`mutating`/`dynamic` 字段），`side_effect` 非空 / `mutating=true` / `dynamic=true` 任一命中即记录 excluded 并跳过（不再解析源码、不再硬编码名单；动态工具在日常 L2 无注册，`dynamic` 分支为防回归保留）；再校验存在性与工具名一致性（响应非对象或名字不匹配 → FAIL）
- **`empty_args`**：空对象调用；响应非 JSON 对象 → FAIL；含 `error` 字段算"有错误响应"（统计 error 数，不 FAIL）；schema `required` 非空但空参未报错 → 记 **warnings**（不 FAIL）
- **`heuristic_smoke`**：按 schema properties 类型生成启发值（integer→0、number→0.0、boolean→false、array→`[]`、object→`{}`、其余→`"test"`）；无 properties 的工具跳过（不产生步骤）
- **崩溃检测**：每步调用后查编辑器进程存活，进程死亡 → `fatal_error`（附 2000 字符日志）
- 统计输出：调用总数 / 通过 / 失败 / result / error / "missing required" / 跳过（无 properties）/ 排除（副作用/可变/dynamic）

### 步数核算（与 AGENTS.md "330 个做空参+冒烟" 对齐）

- 每次遍历候选 = 运行时枚举 392 条中排除 **62 个**（`side_effect` 非空 60 个 + `side_effect=None` 但 `mutating` 的 `fill_tilemap_rect`/`build_nodes_from_spec` 2 个）后余 **330 个**
- empty_args：330 个调用、每个产生 1 个步骤
- heuristic_smoke：330 个候选中跳过无 properties 的工具，步骤数取决于运行时空 schema 数
- **上限 660 步（330×2 减空 schema 跳过，以运行时报告为准）**，精确值以 `03_tools_contract` 运行输出为准（09-19 实测两步骤全过）
- 耗时：`tests/README.md` 称 "约 2-3 分钟"（两次全量遍历 + 两次全量 get_tool_detail 的 HTTP 往返量级，随步数增长略有增加）。

### 62 个排除工具清单（`side_effect`/`mutating` 字段驱动，实测 62 = 16 + 6 + 4 + 20 + 6 + 7 + 1 + 2；按 `get_tool_detail` 字段自动排除，不再硬编码名单）

| side_effect | 数量 | 工具 |
|---|---:|---|
| `writes_file` | 16 | `save_editor_scene`、`save_editor_scene_as`、`save_editor_scenes`、`create_script`、`patch_script`、`save_resource`、`copy_resource_file`、`move_resource_file`、`create_directory`、`create_theme_resource`、`set_theme_color`、`set_theme_constant`、`set_theme_font_size`、`set_theme_stylebox_flat`、`write_file`、`move_os_file_to_trash` |
| `writes_config` | 6 | `save_project_settings`、`set_editor_settings`、`set_editor_main_scene`、`set_editor_plugin_enabled`、`save_input_map`、`add_input_map_action_event` |
| `shows_alert` | 4 | `show_os_alert`、`show_display_dialog`、`speak_display_tts`、`stop_display_tts` |
| `modifies_window` | 20 | 原 12 个（`create_display_window`、`delete_display_window`、`move_display_window_to_foreground`、`request_display_window_attention`、`set_display_clipboard`、`set_display_mouse_mode`、`set_display_window_flag`、`set_display_window_mode`、`set_display_window_position`、`set_display_window_size`、`set_display_window_title`、`warp_display_mouse`）+ 09-15 新增 7 个（`click_editor_element`、`type_editor_element_text`、`run_editor_shortcut`、`click_input_mouse`、`scroll_input_mouse`、`drag_input_mouse`、`type_input_text`）+ 09-16 新增 1 个（`select_scene_tree_node`） |
| `process` | 6 | `build_csharp_assembly`、`create_os_process`、`execute_os_process`、`kill_os_process`、`open_os_path`、`set_os_environment` |
| `game_runtime` | 7 | `execute_game_script`、`start_game_job`（09-16 新增）、`reload_game_scripts`、`queue_game_input`、`wait_game_input`、`sequence_game_inputs`、`click_game_ui_element`（09-15 新增） |
| `code_execute` | 1 | `execute_script` |
| `None`（仅 `mutating`） | 2 | `fill_tilemap_rect`（09-16 新增）、`build_nodes_from_spec`（09-18 新增）：`side_effect` 为 None，仅靠 `mutating` 排除——批量铺砖/声明式建树可撤销，仍不进入自动冒烟 |

历史事故（早期文档记载，当前工作树未检索到原始出处）：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`；`save_editor_scene` 空参生成 `Example/NewNode.tscn`。**新增工具若写配置/文件/弹窗/改窗口，必须在 `ToolSpec` 声明对应 `side_effect` + `kMutating`**，否则遍历会污染 Example 项目或干扰桌面。

> [!todo] 待补充：上述两起历史事故的原始出处（当前 `tests/README.md`、根 `README.md`、`AGENTS.md` 均无此段记载）。

### warnings 语义（历史基线 4 条，以运行时报告为准）

`traversal.cpp` 的判定是通用的（任何"schema 声明必填但空参未报错"都记 warning 不 FAIL）。09-19 数据化重构后完整遍历 330 个候选两步骤全过、0 失败；warnings 清单以运行时报告为准，历史基线为以下 4 条（09-15 与 09-16 两轮一致）：

| 工具 | 缺口 |
|---|---|
| `start_input_gamepad_vibration` | device / weak / strong 标必填但描述声明默认值（0 / 0.5 / 0.5），空参不报 "missing required" |
| `stop_input_gamepad_vibration` | device 标必填但默认 0，空参不报 "missing required" |
| `get_resource_extensions` | 缺 type 时返回全类型列表 |
| `reimport_resource_files` | 空参时 count=0 静默成功 |

`create_scene_node`（name / type 有默认值，不校验必填）属历史已知项：仅当运行期存在编辑场景时命中，遍历未告警。

## 数值核算总表（vs AGENTS.md / README）

| 条目 | 权威口径（本轮实测） | 源码核算 | 结论 |
|---|---|---|---|
| L1 gtest 数量 | 285（31 个 unit 文件；09-20 新增 `trace_recorder_test` 11 / `log_persist_test` 5；09-19 新增 `tool_args_test` 11 项、`tool_registry_test` 5→6 增 flags 路由用例；收尾第二轮新增 `tool_pipeline_traits_test` 4 / `tool_invoke_test` 4 / `batch_refs_test` 6；历史见上方 L1 表） | 285（逐文件宏统计见上表；`ctest --preset debug -E "^gda_runner_"` 实测 287 = 285 + 2 守卫全绿） | 一致 |
| L2 用例文件数 | 29（00-10 共 11 份 + 11_editor_tree / 12_capture_params / 13_inline_subresource / 14_tilemap_rect / 15_scene_path / 16_game_jobs / 17_vision_assist / 18_script_freshness / 19_cjk_roundtrip / 20_cjk_text_roundtrip / 22_uid_guard / 23_click_ui_coords / 24_keycode_alias / 25_open_scene_idempotent / 26_user_tools_code_mode / 27_user_tools_rescan / 28_sprite_frames_animation / 29_trace_persistence；21 号段空缺；99 号诊断用例已删） | 29（`tests/config/*.json` 实测；`29_trace_persistence` 实测 PASS） | 一致 |
| ctest L2 用例 | gda_runner_<name> | 一致（`tests/CMakeLists.txt` 的 config GLOB 注册段，TIMEOUT 600；29 份均由 GLOB 自动发现，新增文件零配置） | 一致 |
| ctest 注册点 | 316（285 L1 + 2 守卫 + 29 L2） | 316（其中 L2 与守卫可静态核实；最终以 `ctest -N` 实测为准） | 以运行时统计为准 |
| 遍历工具数 | 391 域 + `system_status` = 392 枚举 | 391（30 个 `*_tools.hpp` 的 `ToolSpec` 条目）+ system_status；运行时经 `search_tools` 空 query 枚举 | 一致 |
| 排除工具数 | 62 | 62（`side_effect` 非空 60 = writes_file 16 / writes_config 6 / shows_alert 4 / modifies_window 20 / process 6 / game_runtime 7 / code_execute 1 + `side_effect=None` 但 `mutating` 2；运行时经 `get_tool_detail` 字段判定） | 一致 |
| 03 遍历步数 | 上限 660 步（330 候选×2 减空 schema 跳过） | **上限 660 步**（392 枚举 - 62 排除 = 330 候选，以运行时为准；09-19 实测两步骤全过） | 运行时统计口径 |
| 03 耗时 | 约 2-3 分钟（随步数增长略有增加） | README：约 2-3 分钟 | 一致 |
| schema 非空/空数 | 运行时观测（当前精确基线 399 = 336 非空 + 63 空） | `SpecTool` 构造期从参数表/`raw_schema` 生成；`SchemaStatisticsBaseline` 精确断言（09-19） | 运行时统计口径 |
| 工具总结构 | 7 元 + 391 领域 | 全部 `SpecTool`；7 元经 `kMeta` 归类 + RegisterTool；391 域经 30 域 `make_tools()` + `system_status`；`g_handlers` 派生=392 | 单一来源 |
| ToolCatalog 399 条目 | 391 领域 + system_status + 7 元 | `refresh_derived` 对 `all_any()` 逐一 `make_tool_info` 派生，无独立填表 | 399 自洽 |

> 对照说明：本表"源码核算"为逐文件/逐宏统计的本轮实测值；仓库 AGENTS.md 与 README 的对应数值由"维护入口"统一同步，若仍有旧值以本页实测为准。

## 守卫（tests/guard/，ctest 常驻）

- `migration_guard`（`migration_guard.cmake` + `migrated_domains.txt`，30 域 + `strict`）：零依赖源码断言，防旧宏/`schema_*_ops.cpp`/`tool_input_schema` 回退；迁移规则见 [ToolSpec 数据层与执行管线](tool_base_design.md)。
- `comment_guard`（09-19 注释清理批新增，`comment_guard.py`）：递归扫描 `src/` 与 `tests/` 下 `*.cpp/*.hpp`，除 `// namespace` 结尾标记外禁止任何 `//`/`/*…*/` 注释（正确处理字符串/字符字面量与 `R"TAG(…)TAG"` raw string）；违规退出 1，判定时机为 ctest 而非编译。
- 两守卫均不随 L2 引擎环境变化；`ctest --preset debug -E "^gda_runner_"` = 285 gtest + 2 守卫 = 287，全量 ctest = 287 + 29 L2 = 316（以 `ctest -N` 为准）。

## 已知引擎副作用

`tests/README.md` 第 8 节声称：L2 每次运行后 `Example/project.godot` 可能被引擎自动追加 `[audio]` 段（`buses/default_bus_layout="uid://c6hb2nshs2igl"`），并生成 `Example/default_bus_layout.tres`。**审计结论：执行器代码无写库逻辑**（`godot_process.cpp` 仅管理进程与管道），此行为属 Godot headless 编辑器自身自动保存，仓库代码无法直接佐证，标注**待现场验证**。清理方式（README 记载）：`git checkout -- Example/project.godot` + `Remove-Item Example/default_bus_layout.tres`。

## L2 环境漂移（编辑器场景恢复）

当 `Example/project.godot` 含 `run/main_scene`（如报告测试产物或手动配置）且 `Example/.godot/editor` 缓存记录最近场景时，headless 编辑器启动后会**异步恢复主场景**，覆盖用例 `before_all` 创建的 Root 场景，导致 `01_scene`/`02_property` 用例失败。已确证与插件代码无关（基线项目状态下一次通过）。

运行 L2 前确保 `Example/project.godot` 不含 `run/main_scene`，或删除 `Example/.godot/editor` 缓存；用 `git status` 检查 `Example/project.godot` 是否被测试/编辑器改动过。

## 审计发现的不一致点清单

1. **03 遍历步数**：历史值 560（348 工具口径）→ ≈408 → ≈410 → ≈546（336 工具含冒烟）→ 640-660（363 域/321 枚举口径）→ 634（366 域/317 枚举口径）→ 644（322 枚举×2）→ 648（324×2）→ 650（325×2）→ 658（329×2，09-18 实测 603 步全过）→ **当前上限 660 步（330 候选×2 减空 schema 跳过，以运行时为准，历史值均为旧口径）**；精确值随运行时空 schema 数变化，属运行时统计口径。
2. **schema 空/非空数**：由 `SpecTool` 构造期从参数表/元工具 `raw_schema` 生成，catalog 级非空/空数以运行时观测为准（当前精确基线 399 = 336 非空 + 63 空）。
3. **引擎副作用**（非数值）：README 的 `[audio]` 段 / `default_bus_layout.tres` 声称无执行器代码佐证，属引擎行为，待验证。

## 已知遗留（待跟进）

- **mcp-cpp-sdk 客户端偶发响应等待缺陷（未修复，待跟进）**：L2 执行器（`tests/integration/mcp_test_client.cpp`）在 runner 场景下对特定响应的等待偶发退化为约 60s（现场约 50% 触发）；同一时刻服务端对独立请求完全健康，指向 SDK **客户端**侧响应分发/匹配缺陷。本轮在测试侧规避（移除触发步骤 + `call_tool` 的 transport 错误重试 + L1 消息断言），SDK 侧根因待跟进——不要按"已修复"处理。
- **游戏侧新增路径未做 L3 双进程 E2E**：`queue_game_input` 的 wheel/mouse_motion、`click_game_ui_element`（09-16 起含 `window_position` 窗口坐标换算）、`capture_game_viewport` 的 region/max_dimension/annotate 在游戏进程内执行，本轮验证范围为编译通过与代码级路径检查，未编写编辑器 + 游戏双进程的 L3 端到端用例。`click_game_ui_element` 的换算端到端断言（320x180 视口 → 1280x720 窗口 4x 下 `window_position={640,360}`）留待示例游戏恢复后补测，补测骨架见 `23_click_ui_coords.json` 的覆盖力声明；换算正确性现由 L1 `game_ui_coords_test` 覆盖。
- **编辑器 UI 动作未自动化**：L2 执行器不支持跨步骤变量引用（上一步返回的元素 path 无法传给下一步），`click_editor_element` / `type_editor_element_text` 的真实点击/输入未纳入自动用例；`10_editor_input` 仅覆盖参数校验错误路径（在注入前返回 error，无副作用）。

## 相关页面

- [工具注册表（modules/tools_registry.md）](modules/tools_registry.md)
- [构建说明（build.md）](build.md)
- [核心模块（modules/core.md）](modules/core.md)
- [测试体系说明（tests/README.md，仓库内权威文档）](../../tests/README.md)
