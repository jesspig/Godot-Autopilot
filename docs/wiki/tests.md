---
type: 测试体系指南
title: 测试体系
description: L1/L2 双层测试体系、遍历排除清单、数值统计与已知引擎副作用
tags:
  - 测试
  - L1
  - L2
timestamp: "2026-09-13T03:48:49+08:00"
resource: tests/
---

# 测试体系（tests/）

> 审计日期：2026-09-13（2026-08-29 0.2.2 发布审计——L1 77、L2 7 份；09-02 安全与并行硬化——L1 77→96（新增 security_parallel_hardening 17 项，覆盖队列/路径/鉴权/限额/日志）、核心路径/扫描/响应边界与构建大小写修复；09-08 技能生成器——L1 96→103（新增 skill_gen 7 项），ctest 注册点 103→110，同日 skill 内容外置化——测试零改动仍 103；09-10 skill 体系 19→7 册重构——用例改名与白名单扩充，仍 103；09-13 随反馈修复批次同步——L1 103→114（新增 mcp_image_content 11 项），L2 7→8 份（新增 07_scene_tabs），ctest 110→122；技能体系 8 册），基于当前工作树代码逐行核对（不依赖 git 历史）。
> 覆盖范围：`tests/` 全部（unit 13 文件、runner 7 实现 + 6 头文件、integration、config 8 JSON、`tests/CMakeLists.txt`），对照 `tests/README.md` 与仓库根 `AGENTS.md` 测试段逐条核算。114 项 L1 已本地验证通过（见 changelog 2026-09-13-02）；09-13 实测 `ctest -N`：L1 114 / 总注册 122（L2 需 `GODOT_PATH`，未计入运行验证）。

## 架构总览

测试分两层，由根 `CMakeLists.txt` 的 `GDA_ENABLE_TESTS` 引入。该开关已固化在 `CMakePresets.json` 的 debug/release 预设（cacheVariables 默认 `ON`），**清理/重建 `build/` 后重新配置（`uv run build.py` 或 `cmake --preset debug`）自动恢复测试目标，无需手动传参**；直接以裸 `cmake`（不带 preset）配置时仍为默认 OFF（`CMakeLists.txt:164`）：

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
- L2 产物目录：`build/debug/tests/`（ctest 报告输出到 `build/debug/tests/output/`，由 `tests/CMakeLists.txt:116` 指定 `--report-dir`）。

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

**13 个测试文件，实际 114 个 TEST/TEST_F**（`TEST`/`TEST_F` 宏逐行统计，2026-09-13 复核；09-02 bm25 14→20 + security_parallel_hardening 17 项，09-08 新增 skill_gen 7 项，09-13 新增 mcp_image_content 11 项）：

| 文件 | 数量 | 主题 |
|---|---|---|
| `bm25_index_test.cpp` | 20 | BM25 检索：命中排序、分类/标签过滤、空查询、上限截断 |
| `register_all_test.cpp` | 14 | 注册管线：7 元工具 ListTools 往返、catalog 覆盖、schema 基线、未知工具错误、幂等重注册、双服务器一致性 |
| `client_config_gen_test.cpp` | 11 | 8 客户端配置文件渲染快照（URL/type/enabled）、JSON 合并三态（新建/保留其他键/非法）、TOML 追加与跳过 |
| `command_queue_test.cpp` | 7 | 跨线程 submit/drain、异常经 future 传播、主线程记录 |
| `tool_catalog_test.cpp` | 7 | add/get、重复覆盖、并发安全、默认工具填充一次 |
| `log_system_test.cpp` | 7 | 单例、级别/分类/文本过滤、环形缓冲覆盖、值拷贝快照与 recent 上限（09-02 增补） |
| `tool_registry_test.cpp` | 5 | ToolBase/FnTool/ToolRegistry：add/find/categories、execute echo、角色接口助手、meta 注册与路由断言 |
| `schema_builder_test.cpp` | 4 | schema 构造形状、required 位置、未知类型不崩溃（08-22 清理后 API 收敛为三件，用例收缩） |
| `error_util_test.cpp` | 3 | 错误 JSON 形状、四字段详情 |
| `runtime_ops_test.cpp` | 1 | 编辑器队列注入往返 |
| `security_parallel_hardening_test.cpp` | 17 | 安全与并行硬化：队列关闭/满/线程校验、路径/鉴权、配置与截断、日志并发 |
| `skill_gen_test.cpp` | 7 | 技能生成器：8 册清单、name/description 规范、文件布局、frontmatter 渲染、反引号词回验（详见下节） |
| `mcp_image_content_test.cpp` | 11 | 截图 MCP image content（header-only `src/util/mcp_image_content.hpp`）：3 个 capture_* 白名单、顶层/嵌套 data 提取与优先级、data 替换为 `<attached-as-image-content>`、非白名单/非对象/缺 data/非 png 不注入 |
| **合计** | **114** | |

**链接来源**（`tests/CMakeLists.txt:31-49` 的 `GDA_UNIT_BUSINESS_SOURCES`，共 17 个显式 + 1 组 glob）：`src/core/` 的 `log_system`、`resource_registry`、`scene_dirty_tracker`、`export_guard`、`editor_readiness`；`src/util/` 的 `bm25_index`、`error_util`、`readback_util`、`variant_json`、`client_config_gen`、`skill_gen`、`skill_content_generated`（构建期嵌入薄胶水）；`src/tools/` 的 `tool_catalog`、`schema_builder`、`register_all`、`dispatch`、`debugger_access`；`src/tools/*_ops.cpp`（`GDA_TOOLS_OPS_SOURCES` glob，`register_all.cpp` 引用全部 `handle_xxx` 符号故必须链接）。

**约束**：L1 禁止调用任何已注册工具 handler（无引擎时 godot-cpp 接口指针为 nullptr 会崩溃）；`register_all_test` 仅测注册/分发/未知工具错误路径。`SchemaStatisticsBaseline`（`register_all_test.cpp:133-147`）为运行时统计：断言 schema 非空数 > 空数 > 0，**不硬编码具体数值**（历史观测值如 "283 非空 / 73 空" 仅为某次运行的快照，非断言常量）。`client_config_gen` 为纯 C++（仅 std + `mcp::JsonValue`），不触碰 Godot API，可安全纳入 L1。

### skill_gen 技能生成器测试（`skill_gen_test.cpp`，09-08 新增）

生成器 `src/util/skill_gen.cpp`；8 册正文外置为 `src/util/skill_templates/`（31 个文件——30 个 .md + registry.json，registry 8 条映射），经 `tools/embed_skills.py` 构建期嵌入为生成头 `skill_content_embedded.h`（入 `build/<preset>/generated/`，gitignore 覆盖），渲染目标 `.agents/skills/<name>/SKILL.md`。纯 C++ 不触碰 Godot API，L1 可跑无需引擎。09-08 内容外置化重构（7 个 `skill_content_*.cpp` → 模板目录 + 构建期嵌入）测试零改动全绿；**09-10 skill 体系 19→7 册重构（每册带 references/ 渐进披露 + 84 条 Godot 4.8.0-dev 源码研究发现织入引擎六册）后，用例改名与白名单扩充；09-13 新增 `godot-autopilot-csharp` 册（8 册）并同步 `SKILL_COUNT = 8`，ctest L1 114/114 全绿**。7 个用例：

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

**8 个用例文件 → 8 条 ctest 用例**（`tests/CMakeLists.txt:112-119`：`gda_runner_<文件名去后缀>`，`TIMEOUT 600`）：

| ctest 用例 | 文件 | name | 步骤数 | 内容 |
|---|---|---|---|---|
| `gda_runner_00_meta` | `00_meta.json` | meta_tools | 15 | 7 个元工具语义：ping / search_tools（关键词、分类、标签三查）/ list_categories / get_tool_detail（命中、缺失、缺参）/ call_tool（未知、缺参）/ batch_execute（成功、含失败）/ code_execute（返回值、语法错、缺参） |
| `gda_runner_01_scene` | `01_scene.json` | 01_scene | before_all 1 + 19 | 场景节点：创建（显式根路径/默认值/子节点/非法类型/双根）、删除（成功/缺失/根禁删）、树查询、未保存拒绝切换、group 四步、缺参报错 |
| `gda_runner_02_property` | `02_property.json` | property_tools | before_all 2 + 17 | 属性读写：int/string/Vector2/Color 写读回、缺属性/缺节点报错、readback MATCHED、int 字符串静默转 0、property_get_list、缺 value 报错 |
| `gda_runner_03_tools_contract` | `03_tools_contract.json` | tools_contract | 2 个 traverse 步骤 | 全量遍历：empty_args + heuristic_smoke（详见下节） |
| `gda_runner_04_resources_scripts` | `04_resources_scripts.json` | resources_scripts | 7 | execute_script 四行为（单表达式自返/多行显式 return/语法错/缺参）+ get_resource_extensions / has_resource + search_tools 发现性 |
| `gda_runner_05_rename_references` | `05_rename_references.json` | 05_rename_references | 12 | rename 事务化端到端（08-20 新增）：create/save 源资源取 uid → write_file 手工依赖（按 path 引用）→ 前向反查命中 → rename（断言 updated_files 非空 + uid_preserved）→ find_in_files 证明旧路径引用消失、新路径引用存在、marker 可搜索 → get_resource_references 按新路径命中、按旧路径为空。在 res://gda_tmp_rename/ 临时建删，跑后需清理该临时目录 |
| `gda_runner_06_move_references` | `06_move_references.json` | 06_move_references | 16 | 事务化 move_resource_file（08-24 新增）：create_directory 源/目标 → write_file 最小 tres（move_asset.tres，engine_managed update_file）→ get_resource_uid 源 → write_file 依赖 tscn（referrer.tscn）→ get_resource_references 前向命中 → open_editor_scene 保持打开 → move_resource_file 至新目录（断言 moved 非空、failed/stale_references 为空、reloaded_scenes 非空）→ get_resource_uid 新路径 → find_in_files 新旧路径双向证明 + has_resource_dependency/get_resource_references 新旧反查 → close_editor_scene + move_os_file_to_trash 清理临时目录 |
| `gda_runner_07_scene_tabs` | `07_scene_tabs.json` | 07_scene_tabs | 16 | 场景脏标签闭环（09-13 新增）：create_editor_scene(close_current) 建未命名 TabA → save_editor_scene_as 落盘 → property_set 致脏 → open_editor_scene(tab_b) 被拒绝并提示 save/reload → reload_editor_scene 返回 reloaded/scene_path/observed:true 清脏 → 再次 open 成功且 get_scene_tree 显示 TabB；另覆盖 reload 对未打开场景报 `scene is not open`、create_editor_scene 的 timeout_ms 非整数/越界报错且不影响当前场景；cleanup 关闭场景并回收 `res://gda_tmp_tabs/`。幽灵标签分支（多标签共存时移动依赖）未覆盖：runner 无打开第二个标签的步骤，无法制造陈旧标签状态，故放弃该分支 |

**JSON schema 冻结规则**（`config_loader.cpp` 强制校验，违规抛 `runtime_error` 且错误消息含字段路径）：顶层必填 `name`/`pipeline`；`headless` 默认 true；`on_failure` 仅 `fail_fast`（默认）| `continue`；`before_all`/`after_all` 仅 tool+args+id（不支持 traverse/expect）；`stages[].steps` 平铺保留顺序；步骤 `tool` 与 `traverse` 二选一；`traverse.mode` 仅 `empty_args`（默认）| `heuristic_smoke`。

**断言语义**（`assert_engine.cpp`）：`has_keys` 顶层键存在性；`field_checks[].key` 点路径分段查找；`value` 数值兼容（int/double 统一转 double）；`not_empty`（string 非空 / array、object `Size()>0` / null 判空失败）；无 `expect` 时仅检查 `last_call_was_error()` 为假（即工具调用未报错）。

## 全量遍历（`traversal.cpp`）

- **工具来源**：运行时遍历 `src/tools/*_tools.hpp` 解析 `GDA_TOOL_CLASS(` 第 2 参，**实测域工具 365 个（30 个域文件）**；解析器仅精确匹配 `GDA_TOOL_CLASS(`，以 `GDA_TOOL_CLASS_SIDE(` 声明的 **49 个**副作用工具不进入枚举，**实际枚举 316 个**；纳入枚举的工具再经 `get_tool_detail` 的 `side_effect` 字段兜底判定排除（当前 SIDE 已由宏前缀排除，兜底用于防回归）
- **副作用排除**：每工具先 `get_tool_detail`（响应含 `side_effect` 字段）；字段非空即视为副作用工具，记录 excluded 并跳过（不再硬编码名单）；再校验存在性与工具名一致性（响应非对象或名字不匹配 → FAIL）
- **`empty_args`**：空对象调用；响应非 JSON 对象 → FAIL；含 `error` 字段算"有错误响应"（统计 error 数，不 FAIL）；schema `required` 非空但空参未报错 → 记 **warnings**（不 FAIL）
- **`heuristic_smoke`**：按 schema properties 类型生成启发值（integer→0、number→0.0、boolean→false、array→`[]`、object→`{}`、其余→`"test"`）；无 properties 的工具跳过（不产生步骤）
- **崩溃检测**：每步调用后查编辑器进程存活，进程死亡 → `fatal_error`（附 2000 字符日志）
- 统计输出：调用总数 / 通过 / 失败 / result / error / "missing required" / 跳过（无 properties）/ 排除（副作用）

### 步数核算（与 AGENTS.md "316 个做空参+冒烟" 对齐）

- 每次遍历候选 = 域工具 365 中解析器枚举的 **316 个非 SIDE 工具**（49 个 `GDA_TOOL_CLASS_SIDE` 不进入枚举）
- empty_args：316 个调用、每个产生 1 个步骤
- heuristic_smoke：316 个候选中跳过无 properties 的工具，步骤数取决于运行时空 schema 数
- **上限 632 步（316×2 减空 schema 跳过，以运行时报告为准）**，精确值以 `03_tools_contract` 运行输出为准
- 耗时：`tests/README.md` 称 "约 2-3 分钟"（两次全量遍历 + 两次全量 get_tool_detail 的 HTTP 往返量级，随步数增长略有增加）。

### 49 工具排除清单（`GDA_TOOL_CLASS_SIDE` 标记驱动，实测 49 = 15 + 6 + 4 + 12 + 6 + 5 + 1；08-21 起由 `side_effects()` 自动排除，不再硬编码名单）

| side_effect | 数量 | 工具 |
|---|---:|---|
| `writes_file` | 15 | `save_editor_scene`、`save_editor_scene_as`、`save_editor_scenes`、`create_script`、`save_resource`、`copy_resource_file`、`move_resource_file`、`create_directory`、`create_theme_resource`、`set_theme_color`、`set_theme_constant`、`set_theme_font_size`、`set_theme_stylebox_flat`、`write_file`、`move_os_file_to_trash` |
| `writes_config` | 6 | `save_project_settings`、`set_editor_settings`、`set_editor_main_scene`、`set_editor_plugin_enabled`、`save_input_map`、`add_input_map_action_event` |
| `shows_alert` | 4 | `show_os_alert`、`show_display_dialog`、`speak_display_tts`、`stop_display_tts` |
| `modifies_window` | 12 | `create_display_window`、`delete_display_window`、`move_display_window_to_foreground`、`request_display_window_attention`、`set_display_clipboard`、`set_display_mouse_mode`、`set_display_window_flag`、`set_display_window_mode`、`set_display_window_position`、`set_display_window_size`、`set_display_window_title`、`warp_display_mouse` |
| `process` | 6 | `build_csharp_assembly`、`create_os_process`、`execute_os_process`、`kill_os_process`、`open_os_path`、`set_os_environment` |
| `game_runtime` | 5 | `execute_game_script`、`reload_game_scripts`、`queue_game_input`、`wait_game_input`、`sequence_game_inputs` |
| `code_execute` | 1 | `execute_script` |

历史事故（README 记载）：`set_editor_main_scene` 曾把 `application/run/main_scene` 写成 `"test"` 写入 `Example/project.godot`；`save_editor_scene` 空参生成 `Example/NewNode.tscn`。**新增工具若写配置/文件/弹窗/改窗口，必须同步加入此清单**，否则遍历会污染 Example 项目或干扰桌面。

### warnings 语义（3 个已知契约缺口）

`traversal.cpp:300-304` 的判定是通用的（任何"schema 声明必填但空参未报错"都记 warning 不 FAIL），实际命中以下 3 个（与 AGENTS.md/README 一致，业务代码未改）：

| 工具 | 缺口 |
|---|---|
| `create_scene_node` | name / type 有默认值，不校验必填 |
| `get_resource_extensions` | 缺 type 时返回全类型列表 |
| `reimport_resource_files` | 空参时 count=0 静默成功 |

## 数值核算总表（vs AGENTS.md / README）

| 条目 | 权威口径（本轮实测） | 源码核算 | 结论 |
|---|---|---|---|
| L1 gtest 数量 | 114（含 09-02 安全并行硬化 17 项 + 09-08 skill_gen 7 项 + 09-13 mcp_image_content 11 项） | 114（逐文件宏统计见上表；`ctest -N -E "^gda_runner_"` 实测 114） | 一致 |
| L2 用例文件数 | 8（00_meta / 01_scene / 02_property / 03_tools_contract / 04_resources_scripts / 05_rename_references / 06_move_references / 07_scene_tabs；09-13 新增 07） | 8 | 一致 |
| ctest L2 用例 | gda_runner_<name> | 一致（`tests/CMakeLists.txt:112-119`，TIMEOUT 600；06/07 由 GLOB 自动发现） | 一致 |
| 遍历工具数 | 365 | 365（30 个 `*_tools.hpp` 的 `GDA_TOOL_CLASS(_SIDE)` 计数；解析器仅枚举 `GDA_TOOL_CLASS(` 的 316 个） | 一致 |
| 排除工具数 | 49 | 49（`GDA_TOOL_CLASS_SIDE` 标记 49 个：writes_file 15 / writes_config 6 / shows_alert 4 / modifies_window 12 / process 6 / game_runtime 5 / code_execute 1；不进入枚举，`side_effect` 字段兜底判定保留） | 一致 |
| 03 遍历步数 | 上限 632 步（316×2 减空 schema 跳过） | **上限 632 步**（域 365 中解析器枚举 316，49 个 SIDE 不枚举，以运行时为准） | 运行时统计口径 |
| 03 耗时 | 约 2-3 分钟（随步数增长略有增加） | README：约 2-3 分钟 | 一致 |
| schema 非空/空数 | 运行时观测 | `SchemaStatisticsBaseline` 仅断言非空>空>0（08-22 起 SCHEMA_NONE/BASIC 静态枚举已删，`tool_input_schema` 的 basic 参数为 no-op） | 无法静态精确核算，属运行时观测值 |
| 工具总结构 | 7 元 + 365 领域 | 7 元工具经 `ToolRegistry::add()`（IMetaTool 自动归类）+RegisterTool；365 领域/系统经 30 域 `make_tools()`；`g_handlers` 派生=366（365+system_status） | 单一来源 |
| ToolCatalog 373 条目 | 365 领域 + system_status + 7 元 | 全部由 registry `all_any()` 逐一 `make_tool_info` 派生，无独立填表 | 373 自洽 |

> 对照说明：本表"源码核算"为逐文件/逐宏统计的本轮实测值；仓库 AGENTS.md 与 README 的对应数值由"维护入口"统一同步，若仍有旧值以本页实测为准。

## 已知引擎副作用

`tests/README.md` 第 8 节声称：L2 每次运行后 `Example/project.godot` 可能被引擎自动追加 `[audio]` 段（`buses/default_bus_layout="uid://c6hb2nshs2igl"`），并生成 `Example/default_bus_layout.tres`。**审计结论：执行器代码无写库逻辑**（`godot_process.cpp` 仅管理进程与管道），此行为属 Godot headless 编辑器自身自动保存，仓库代码无法直接佐证，标注**待现场验证**。清理方式（README 记载）：`git checkout -- Example/project.godot` + `Remove-Item Example/default_bus_layout.tres`。

## L2 环境漂移（编辑器场景恢复）

当 `Example/project.godot` 含 `run/main_scene`（如报告测试产物或手动配置）且 `Example/.godot/editor` 缓存记录最近场景时，headless 编辑器启动后会**异步恢复主场景**，覆盖用例 `before_all` 创建的 Root 场景，导致 `01_scene`/`02_property` 用例失败。已确证与插件代码无关（基线项目状态下一次通过）。

运行 L2 前确保 `Example/project.godot` 不含 `run/main_scene`，或删除 `Example/.godot/editor` 缓存；用 `git status` 检查 `Example/project.godot` 是否被测试/编辑器改动过。

## 审计发现的不一致点清单

1. **03 遍历步数**：历史值 560（348 工具口径）→ ≈408 → ≈410 → ≈546（336 工具含冒烟，08-21 全量真类化后 run）→ 640-660（363 域/321 枚举口径）→ **当前上限 632 步（316 枚举×2 减空 schema 跳过，以运行时为准，历史值均为旧口径）**；精确值随运行时空 schema 数变化，属运行时统计口径。
2. **schema 空/非空数**：旧 283/73 为运行时观测值，`SchemaStatisticsBaseline` 不硬编码；08-21 真类化 + 08-22 清理后 def/SCHEMA_NONE 静态口径整体废除（fill 表直出），catalog 级非空/空数以运行时观测为准。
3. **引擎副作用**（非数值）：README 的 `[audio]` 段 / `default_bus_layout.tres` 声称无执行器代码佐证，属引擎行为，待验证。

## 相关页面

- [工具注册表（modules/tools_registry.md）](modules/tools_registry.md)
- [构建说明（build.md）](build.md)
- [核心模块（modules/core.md）](modules/core.md)
- [测试体系说明（tests/README.md，仓库内权威文档）](../../tests/README.md)
