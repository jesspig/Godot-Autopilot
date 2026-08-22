---
type: 测试体系指南
title: 测试体系
description: L1/L2 双层测试体系、遍历排除清单、数值统计与已知引擎副作用
tags:
  - 测试
  - L1
  - L2
timestamp: "2026-08-22T15:10:00+08:00"
resource: tests/
---

# 测试体系（tests/）

> 审计日期：2026-08-22（2026-08-12 初稿；08-17 随客户端配置生成器测试同步并补 YAML frontmatter；08-20 随 +4 工具与新 L2 用例 `05_rename_references` 同步；08-21 随 ToolRegistry 单测复核；08-22 随测试瘦身同步——L1 78→71、SCHEMA 静态口径失效；08-22 15 时全量一致性审计——L1 分文件计数修正、L2 用例数 6、排除清单分组 12+22+1、步数对齐实测 546），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`tests/` 全部（unit 10 文件、runner 7 实现 + 6 头文件、integration、config 6 JSON、`tests/CMakeLists.txt`），对照 `tests/README.md` 与仓库根 `AGENTS.md` 测试段逐条核算。未运行任何测试，所有数值均来自源码静态统计。

## 架构总览

测试分两层，由根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 引入。该开关已固化在 `CMakePresets.json` 的 debug/release 预设（cacheVariables 默认 `ON`），**清理/重建 `build/` 后重新配置（`uv run build.py` 或 `cmake --preset debug`）自动恢复测试目标，无需手动传参**；直接以裸 `cmake`（不带 preset）配置时仍为默认 OFF（`CMakeLists.txt:139`）：

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
- **L2 依赖**：Godot 可执行文件。路径解析 `GodotProcess::resolve_godot_path()`（`godot_process.cpp:528-534`）：进程环境变量 `GODOT_PATH` 优先，为空才回退解析仓库根 `.env`；二者皆无 → 执行器退出码 2。
- **L2 平台限制**：`godot_process.cpp` / `mcp_test_client.cpp` 的进程管理与 Winsock 实现均为 `#ifdef _WIN32`，非 Windows 平台返回"仅支持 Windows"（失败/不可用）。
- L2 产物目录：`build/debug/tests/`（ctest 报告输出到 `build/debug/tests/output/`，由 `tests/CMakeLists.txt:112` 指定 `--report-dir`）。

### CLI 与退出码（`tests/runner/main.cpp`）

参数：`--config-dir`、`--file`（可含或不含 `.json` 后缀）、`--headless|--gui`（互斥；用例 JSON 的 `headless` 字段优先）、`--no-auto`（不启动进程，端口取 `GODOT_AUTOPILOT_PORT` 直连外部 MCP）、`--keep-open`、`--report-dir`、`--help`。

退出码：0 全部通过；1 存在 FAIL/ERROR；2 参数或环境错误（含 `GODOT_PATH` 未配置、`--no-auto` 端口未就绪、`--file` 无匹配、未捕获异常）。

### 执行闭环（`godot_process.cpp`）

1. 随机空闲端口（`bind 127.0.0.1:0` 取回，`pick_free_port`）
2. 首次 `--editor --import` 幂等同步执行（120s 超时，失败/超时不致命，仅记日志）
3. 临时注入 `GODOT_AUTOPILOT_PORT` 后常驻启动 `--editor`（`--headless` 由用例决定；另注入 `GDA_FORCE_HEADLESS=1`）
4. 就绪轮询（200ms 间隔）：TCP 端口探测 → MCP `initialize` 握手（裸 `POST /mcp` JSON-RPC，响应含 `serverInfo` 即确认）
5. 执行 `before_all` → stages → `after_all`（`pipeline_executor.cpp`；`before_all`/`after_all` 失败即整体 `fatal_error` 不判断言；stages 失败按 `on_failure` fail_fast/continue）
6. 停止：`taskkill /PID` 软杀 → 5s 宽限 → `TerminateProcess` 兜底；崩溃时截取最近 2000 字符日志（`CRASH_LOG_LIMIT`）

stdout/stderr 各接独立管道读线程持续消费，防 64KB 缓冲写满阻塞子进程。

### 报告（`runner_report.cpp`）

- 控制台表格：文件名称 / 通过步骤数 / 耗时（<10s 显毫秒，否则显秒）/ 状态 `PASS|FAIL|ERROR`（`fatal_error` 非空 → ERROR）
- JSON 报告：`tests/output/report-YYYYmmdd_HHMMSS.json`，字段 `generated_at` / `total_files` / `passed_files` / `files[].{name,passed,duration_ms,fatal_error,steps[]}`

## L1 单元测试

**10 个测试文件，实际 71 个 TEST/TEST_F**（`TEST`/`TEST_F` 宏逐行统计，2026-08-22 复核）：

| 文件 | 数量 | 主题 |
|---|---|---|
| `bm25_index_test.cpp` | 14 | BM25 检索：命中排序、分类/标签过滤、空查询、上限截断 |
| `register_all_test.cpp` | 14 | 注册管线：7 元工具 ListTools 往返、catalog 覆盖、schema 基线、未知工具错误、幂等重注册、双服务器一致性 |
| `client_config_gen_test.cpp` | 11 | 8 客户端配置文件渲染快照（URL/type/enabled）、JSON 合并三态（新建/保留其他键/非法）、TOML 追加与跳过 |
| `command_queue_test.cpp` | 7 | 跨线程 submit/drain、异常经 future 传播、主线程记录 |
| `tool_catalog_test.cpp` | 7 | add/get、重复覆盖、并发安全、默认工具填充一次 |
| `log_system_test.cpp` | 5 | 单例、级别/分类/文本过滤、环形缓冲覆盖（08-22 清理删回调用例） |
| `tool_registry_test.cpp` | 5 | ToolBase/FnTool/ToolRegistry：add/find/categories、execute echo、角色接口助手、meta 注册与路由断言 |
| `schema_builder_test.cpp` | 4 | schema 构造形状、required 位置、未知类型不崩溃（08-22 清理后 API 收敛为三件，用例收缩） |
| `error_util_test.cpp` | 3 | 错误 JSON 形状、四字段详情 |
| `runtime_ops_test.cpp` | 1 | 编辑器队列注入往返 |
| **合计** | **71** | |

**链接来源**（`tests/CMakeLists.txt:31-46` 的 `GDA_UNIT_BUSINESS_SOURCES`，共 14 个显式 + 1 组 glob）：`src/core/` 的 `log_system`、`resource_registry`、`scene_dirty_tracker`、`export_guard`；`src/util/` 的 `bm25_index`、`error_util`、`readback_util`、`variant_json`、`client_config_gen`；`src/tools/` 的 `tool_catalog`、`schema_builder`、`register_all`、`dispatch`、`debugger_access`；`src/tools/*_ops.cpp`（`GDA_TOOLS_OPS_SOURCES` glob，`register_all.cpp` 引用全部 `handle_xxx` 符号故必须链接）。

**约束**：L1 禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃）；`register_all_test` 仅测注册/分发/未知工具错误路径。`SchemaStatisticsBaseline`（`register_all_test.cpp:133-147`）为运行时统计：断言 schema 非空数 > 空数 > 0，**不硬编码具体数值**（历史观测值如 "283 非空 / 73 空" 仅为某次运行的快照，非断言常量）。`client_config_gen` 为纯 C++（仅 std + `mcp::JsonValue`），不触碰 Godot API，可安全纳入 L1。

## L2 配置驱动用例

**6 个用例文件 → 6 条 ctest 用例**（`tests/CMakeLists.txt:105-114`：`gda_runner_<文件名去后缀>`，`TIMEOUT 600`）：

| ctest 用例 | 文件 | name | 步骤数 | 内容 |
|---|---|---|---|---|
| `gda_runner_00_meta` | `00_meta.json` | meta_tools | 15 | 7 个元工具语义：ping / search_tools（关键词、分类、标签三查）/ list_categories / get_tool_detail（命中、缺失、缺参）/ call_tool（未知、缺参）/ batch_execute（成功、含失败）/ code_execute（返回值、语法错、缺参） |
| `gda_runner_01_scene` | `01_scene.json` | 01_scene | before_all 1 + 19 | 场景节点：创建（显式根路径/默认值/子节点/非法类型/双根）、删除（成功/缺失/根禁删）、树查询、未保存拒绝切换、group 四步、缺参报错 |
| `gda_runner_02_property` | `02_property.json` | property_tools | before_all 2 + 17 | 属性读写：int/string/Vector2/Color 写读回、缺属性/缺节点报错、readback MATCHED、int 字符串静默转 0、property_get_list、缺 value 报错 |
| `gda_runner_03_tools_contract` | `03_tools_contract.json` | tools_contract | 2 个 traverse 步骤 | 全量遍历：empty_args + heuristic_smoke（详见下节） |
| `gda_runner_04_resources_scripts` | `04_resources_scripts.json` | resources_scripts | 7 | execute_script 四行为（单表达式自返/多行显式 return/语法错/缺参）+ get_resource_extensions / has_resource + search_tools 发现性 |
| `gda_runner_05_rename_references` | `05_rename_references.json` | 05_rename_references | 12 | rename 事务化端到端（08-20 新增）：create/save 源资源取 uid → write_file 手工依赖（按 path 引用）→ 前向反查命中 → rename（断言 updated_files 非空 + uid_preserved）→ find_in_files 证明旧路径引用消失、新路径引用存在、marker 可搜索 → get_resource_references 按新路径命中、按旧路径为空。在 res://gda_tmp_rename/ 临时建删，跑后需清理该临时目录 |

**JSON schema 冻结规则**（`config_loader.cpp` 强制校验，违规抛 `runtime_error` 且错误消息含字段路径）：顶层必填 `name`/`pipeline`；`headless` 默认 true；`on_failure` 仅 `fail_fast`（默认）| `continue`；`before_all`/`after_all` 仅 tool+args+id（不支持 traverse/expect）；`stages[].steps` 平铺保留顺序；步骤 `tool` 与 `traverse` 二选一；`traverse.mode` 仅 `empty_args`（默认）| `heuristic_smoke`。

**断言语义**（`assert_engine.cpp`）：`has_keys` 顶层键存在性；`field_checks[].key` 点路径分段查找；`value` 数值兼容（int/double 统一转 double）；`not_empty`（string 非空 / array、object `Size()>0` / null 判空失败）；无 `expect` 时仅检查 `last_call_was_error()` 为假（即工具调用未报错）。

## 全量遍历（`traversal.cpp`）

- **工具来源**：运行时遍历 `src/tools/*_tools.hpp` 解析 `GDA_TOOL_CLASS(` 第 2 参，**实测 336 个**；解析失败（缺逗号/引号未闭合）抛异常
- **副作用排除**：每工具先 `get_tool_detail`（响应含 `side_effect` 字段）；字段非空即视为副作用工具，记录 excluded 并跳过（不再硬编码名单）；再校验存在性与工具名一致性（响应非对象或名字不匹配 → FAIL）
- **`empty_args`**：空对象调用；响应非 JSON 对象 → FAIL；含 `error` 字段算"有错误响应"（统计 error 数，不 FAIL）；schema `required` 非空但空参未报错 → 记 **warnings**（不 FAIL）
- **`heuristic_smoke`**：按 schema properties 类型生成启发值（integer→0、number→0.0、boolean→false、array→`[]`、object→`{}`、其余→`"test"`）；无 properties 的工具跳过（不产生步骤）
- **崩溃检测**：每步调用后查编辑器进程存活，进程死亡 → `fatal_error`（附 2000 字符日志）
- 统计输出：调用总数 / 通过 / 失败 / result / error / "missing required" / 跳过（无 properties）/ 排除（副作用）

### 步数核算（与 AGENTS.md "约 546 步" 对齐）

- 每次遍历候选 = 336 − 35 排除 = **301 个非排除工具**
- empty_args：301 个调用、每个产生 1 个步骤
- heuristic_smoke：301 个候选中跳过无 properties 的工具，步骤数取决于运行时空 schema 数
- **08-21 全量真类化后实测 ≈546 步**（AGENTS.md/README 同口径）；精确值随运行时统计浮动，以 `03_tools_contract` 运行输出为准
- 耗时：`tests/README.md` 称 "约 2-3 分钟"（两次全量遍历 + 两次全量 get_tool_detail 的 HTTP 往返量级）。

### 35 工具排除清单（`GDA_TOOL_CLASS_SIDE` 标记驱动，实测 35 = 12 + 22 + 1；08-21 起由 `side_effects()` 自动排除，不再硬编码名单）

**持久磁盘副作用（12）**——写 project.godot / editor_settings / .tscn / 文件：

```
set_editor_main_scene  set_editor_plugin_enabled  save_project_settings
add_input_map_action_event  save_input_map  set_editor_settings
save_editor_scene  save_editor_scenes  save_editor_scene_as
write_file  create_script  save_resource
```

**用户可见副作用（22）**——弹窗/进程/环境变量/音频/剪贴板/鼠标/窗口：

| 类别 | 数量 | 工具 |
|---|---|---|
| 弹窗与对话框 | 2 | `show_os_alert`、`show_display_dialog` |
| 进程与系统执行 | 5 | `create_os_process`、`execute_os_process`、`kill_os_process`、`open_os_path`、`move_os_file_to_trash` |
| 环境变量 | 1 | `set_os_environment` |
| 音频/语音 | 2 | `speak_display_tts`、`stop_display_tts` |
| 剪贴板/鼠标 | 3 | `set_display_clipboard`、`set_display_mouse_mode`、`warp_display_mouse` |
| 窗口操作 | 9 | `set_display_window_title`、`set_display_window_position`、`set_display_window_size`、`set_display_window_mode`、`set_display_window_flag`、`move_display_window_to_foreground`、`request_display_window_attention`、`create_display_window`、`delete_display_window` |

**进程副作用（1）**：`build_csharp_assembly`（08-20 新增）。

历史事故（README 记载）：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`；`save_editor_scene` 空参生成 `Example/NewNode.tscn`。**新增工具若写配置/文件/弹窗/改窗口，必须同步加入此清单**，否则遍历会污染 Example 项目或干扰桌面。

### warnings 语义（3 个已知契约缺口）

`traversal.cpp:300-304` 的判定是通用的（任何"schema 声明必填但空参未报错"都记 warning 不 FAIL），实际命中以下 3 个（与 AGENTS.md/README 一致，业务代码未改）：

| 工具 | 缺口 |
|---|---|
| `create_scene_node` | name / type 有默认值，不校验必填 |
| `get_resource_extensions` | 缺 type 时返回全类型列表 |
| `reimport_resource_files` | 空参时 count=0 静默成功 |

## 数值核算总表（vs AGENTS.md / README）

| 条目 | AGENTS.md 声称 | 源码核算 | 结论 |
|---|---|---|---|
| L1 gtest 数量 | 71（历史：61→78→71，08-22 清理删 log_system 回调用例、schema_builder 用例收缩） | 71（逐文件宏统计见上表） | 一致 |
| L2 用例文件数 | 6（00_meta / 01_scene / 02_property / 03_tools_contract / 04_resources_scripts / 05_rename_references；08-20 新增 05） | 6 | 一致 |
| ctest L2 用例 | gda_runner_<name> | 一致（`tests/CMakeLists.txt:108-114`，TIMEOUT 600；05 由 GLOB 自动发现） | 一致 |
| 遍历工具数 | 336 | 336（26 个 `*_tools.hpp` 的 `GDA_TOOL_CLASS(_SIDE)` 计数） | 一致 |
| 排除工具数 | 35 | 35（`get_tool_detail` 的 `side_effect` 字段非空即排除，由 `GDA_TOOL_CLASS_SIDE` 驱动，不硬编码） | 一致 |
| 03 遍历步数 | 约 546 步 | **实测 546**（08-21 全量真类化后 run，336 工具含冒烟） | 运行时统计口径 |
| 03 耗时 | 约 2-3 分钟 | README：约 2-3 分钟 | 一致 |
| schema 非空/空数 | 运行时观测 | `SchemaStatisticsBaseline` 仅断言非空>空>0（08-22 起 SCHEMA_NONE/BASIC 静态枚举已删，`tool_input_schema` 的 basic 参数为 no-op） | 无法静态精确核算，属运行时观测值 |
| 工具总结构 | 7 元 + 336 领域 | 7 元工具经 `ToolRegistry::add()`（IMetaTool 自动归类）+RegisterTool；336 领域/系统经 26 域 `make_tools()`；`g_handlers` 派生=337（336+system_status） | 单一来源 |
| ToolCatalog 344 条目 | 336 领域 + system_status + 7 元 | 全部由 registry `all_any()` 逐一 `make_tool_info` 派生，无独立填表 | 344 自洽 |

## 已知引擎副作用

`tests/README.md` 第 8 节声称：L2 每次运行后 `Example/project.godot` 可能被引擎自动追加 `[audio]` 段（`buses/default_bus_layout="uid://c6hb2nshs2igl"`），并生成 `Example/default_bus_layout.tres`。**审计结论：执行器代码无写库逻辑**（`godot_process.cpp` 仅管理进程与管道），此行为属 Godot headless 编辑器自身自动保存，仓库代码无法直接佐证，标注**待现场验证**。清理方式（README 记载）：`git checkout -- Example/project.godot` + `Remove-Item Example/default_bus_layout.tres`。

## L2 环境漂移（编辑器场景恢复）

当 `Example/project.godot` 含 `run/main_scene`（如报告测试产物或手动配置）且 `Example/.godot/editor` 缓存记录最近场景时，headless 编辑器启动后会**异步恢复主场景**，覆盖用例 `before_all` 创建的 Root 场景，导致 `01_scene`/`02_property` 用例失败。已确证与插件代码无关（基线项目状态下一次通过）。

运行 L2 前确保 `Example/project.godot` 不含 `run/main_scene`，或删除 `Example/.godot/editor` 缓存；用 `git status` 检查 `Example/project.godot` 是否被测试/编辑器改动过。

## 审计发现的不一致点清单

1. **03 遍历步数**：历史值 560（348 工具口径）→ ≈408 → ≈410 → **当前实测 ≈546**（336 工具含冒烟，08-21 全量真类化后 run）；精确值随运行时空 schema 数变化，属运行时统计口径。
2. **schema 空/非空数**：旧 283/73 为运行时观测值，`SchemaStatisticsBaseline` 不硬编码；08-21 真类化 + 08-22 清理后 def/SCHEMA_NONE 静态口径整体废除（fill 表直出），catalog 级非空/空数以运行时观测为准。
3. **引擎副作用**（非数值）：README 的 `[audio]` 段 / `default_bus_layout.tres` 声称无执行器代码佐证，属引擎行为，待验证。

## 相关页面

- [工具注册表（modules/tools_registry.md）](modules/tools_registry.md)
- [构建说明（build.md）](build.md)
- [核心模块（modules/core.md）](modules/core.md)
- [测试体系说明（tests/README.md，仓库内权威文档）](../../tests/README.md)
