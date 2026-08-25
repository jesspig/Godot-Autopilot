---
type: 模块文档
title: 工具注册表
description: 工具注册管线、三层结构、schema 统计口径、契约缺口与遍历排除清单
tags:
  - 模块
  - 工具注册
  - schema
timestamp: "2026-08-24T05:10:00+08:00"
resource: src/tools/
---

# 工具注册表（src/tools/ 注册管线）

> 审计日期：2026-08-24（2026-08-12 初稿；08-17 补 frontmatter；08-20 registry 单一来源重构；08-21 真类化 + def 删除、元工具接口化、副作用驱动遍历排除；08-22 死代码清理与全量一致性审计；08-24 随竞品对齐批次同步——领域工具扩至 363/27 类（+Animation 10、Theme 8、Testing 2、Analysis 3）、副作用 42、删除 get_debugger_stack_dump/get_debugger_monitors、reimport_resource_files 字段名缺口修复、catalog 口径 371）。
> 覆盖范围：`register_all.cpp/hpp`、`dispatch.cpp/hpp`、`tool_catalog.cpp/hpp`、`schema_builder.cpp/hpp`、`schema_fills.hpp`、8 个 `schema_*_ops.cpp`（含 08-24 新增 `schema_animation_ops.cpp`/`schema_theme_ops.cpp`）、`tool_base.hpp`、`tool_registry.hpp`、`fn_tool.hpp`、`tool_decl.hpp`、`meta_tools.hpp`、30 个域 `*_tools.hpp`，对照 `tests/runner/traversal.cpp`、`tests/unit/register_all_test.cpp`、`tests/config/03_tools_contract.json` 与仓库根 `AGENTS.md` 工具段。
> 相关页面：[测试体系](../tests.md) · [工具实现 B 组](../modules/tools_ops_b.md) · [工具实现 A 组](../modules/tools_ops_a.md) · [入口与运行时](../modules/entry_runtime.md) · [架构总览](../overview.md)

## 注册管线流程（ToolRegistry 单一来源 + 全量真类，自 2026-08-21）

工具注册发生在 `register_all_tools()`（`src/tools/register_all.cpp`），现为 **ToolRegistry 单一来源**且 **363 域工具全部为独立 `ToolBase` 子类**。`tool_defs.def` 已删除；域工具定义在 `src/tools/<域>_tools.hpp`（30 个域文件），每个用宏 `GDA_TOOL_CLASS(ClassName, "tool_name", desc, "Category", std::vector<std::string>({tags}), handler, basic)`（宏在 `tool_decl.hpp`）生成 `public ToolBase` 真类，`make_tools()` 返回 `vector<unique_ptr<ToolBase>>`。`register_all` 先 add `system_status`（FnTool），再逐一注册 30 个域的 `make_tools()`，再 `add()` 7 个元工具（`dynamic_cast<IMetaTool*>` 自动归类）；catalog/index/g_handlers/g_meta_handlers/RegisterTool 均从 registry 派生。

```mermaid
flowchart TD
    A[register_all_tools] --> B[g_active_registry = 新建 ToolRegistry]
    B --> C[add FnTool: system_status]
    B --> D[add 30 域 make_tools(): 363 个真类]
    B --> E[add MetaTool: 7 个元工具]
    E --> F[派生 catalog.add_tool(all_any)]
    E --> G[派生 server.RegisterTool(all_meta)]
    E --> H[派生 BM25 index.add_entry(all_any)]
    E --> I[派生 g_handlers(all) + g_meta_handlers(all_meta)]
```

- **真类（非 FnTool）**：`src/tools/*_tools.hpp` 共 30 个域文件（另有 `meta_tools.hpp` 承载 7 元工具），命名空间 `godot_autopilot::<域>_tools`，提供 `make_tools()`。类内 `execute` 委托既有 domain handler（`scene_ops::handle_*` 等），`input_schema` 调 `::godot_autopilot::tool_input_schema(ToolName, basic)`（复用 `build_schema_for`，与旧 def 路径**零漂移**）。
- **schema 单一源**：`tool_input_schema(name, basic)` 在 `register_all.hpp/cpp` 暴露，转发匿名命名空间 `build_schema_for`；`basic` 参数保留签名但已为 no-op（fill 表直接给出最终 schema）。
- **宏要点**：`GDA_TOOL_CLASS` 的 TagsList 实参必须用圆括号 `std::vector<std::string>({...})`（花括号逗号会被预处理器拆散）；`make_tools()` 必须用 `push_back(std::make_unique<Class>())`（vector 花括号 initializer_list 触发 unique_ptr 拷贝-已删除）。
- **元工具 = 接口 + 组合**：`IMetaTool` 标记接口（`tool_base.hpp`）；元工具类 `MetaTool : ToolBase, IMetaTool`（`meta_tools.hpp`），依赖（index/catalog/schema/处理逻辑）经构造函数组合注入；`ToolRegistry::add()` 用 `dynamic_cast<const IMetaTool*>` 自动归类——**实现 `IMetaTool` 接口即元工具**（Java 心智）。7 个元工具以 `MetaTool` 注册，在域工具之后，由派生循环统一 `RegisterTool`；导出保护在 RegisterTool 回调统一前置 `ExportGuard::is_exporting()` 检查。
- **生命周期**：`g_active_registry` 为文件级 `static unique_ptr<ToolRegistry>`（程序存活期），`register_all_tools` 每次调用重建内容并 `index.clear()` 重置 BM25 索引，避免悬垂与重复注册。
- **副作用驱动遍历排除**：`SideEffect` 枚举含 6 值（`None/WritesFile/WritesConfig/ShowsAlert/ModifiesWindow/Process`）；42 个副作用工具用 `GDA_TOOL_CLASS_SIDE` 宏（实现 `ISideEffect` 返回对应值）；`meta_get_tool_detail_impl` 经 `g_active_registry->find_any(name)` + `side_effect_of` + `side_effect_name` 在 `get_tool_detail` 响应补 `side_effect` 字段；遍历 runner 读该字段排除（删除硬编码清单）。
- **分类边界**：`get_debug_object_info`（def 标 Debug、handler `physics_ops::handle_resolve_object`）归 `physics_tools`；`read_file/find_in_files/write_file`（def 归 OS、handler `text_ops`）归 `os_tools`；Scene Tree 类工具 Category 均为 "Scene"、按 handler 分 `scene_tools`/`scene_tree_tools`；InputMap 工具 Category "Input"、归 `input_map_tools`。

## 三层结构

| 层 | 载体 | 数量 | 内容 |
|---|---|---|---|
| 单一来源层 | `ToolRegistry`（`g_active_registry`） | **371** | `all()`=364（363 域 + system_status）+ `all_meta()`=7 |
| MCP 服务器层 | `server.RegisterTool()`（派生自 registry） | **7** | 元工具（见下） |
| 分发层 | `dispatch::g_handlers`+`g_meta_handlers`（派生） | **371** | 364 域/系统 + 7 元 |
| 目录层 | `ToolCatalog`（派生） | **371** | 363 域 + system_status + 7 元 |

领域工具不注册进 MCP 工具列表，只能经 `call_tool` 元工具代理调用；`ToolCatalog` 371 = 363 域工具 + `system_status` + 7 元工具（单一来源 `all_any()` 逐一 `make_tool_info` 派生）。`system_status` 是全 registry 唯一非真类来源。MCP 工具总数 = 7 元 + 363 域 = **370**（经 `call_tool` 代理可达）。

## 数值核算总表（与 AGENTS.md 逐项对比）

| # | 声称（AGENTS.md） | 核算结果 | 结论 |
|---|---|---|---|
| 1 | 元工具 7 个：ping/search_tools/list_categories/get_tool_detail/call_tool/batch_execute/code_execute，均作为顶层工具提供 | 7 个经 registry `add()`（`dynamic_cast<IMetaTool*>` 自动入 meta_），统一在 `register_all.cpp` 循环 `server.RegisterTool`（描述/schema 取自 registry 工具）；名单由 `g_active_registry->all_meta()` 派生，与 `register_all_test.cpp` 的 `kMetaToolNames` 一致；L1 断言 `ListTools` 恰好 7 个 | **一致** |
| 2 | 领域工具 363 个 | 30 个 `*_tools.hpp` 的 `GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE` 共 363 个（无重复名）；registry `all()`=**364** = 363 + system_status | **一致**（system_status 是唯一非真类来源） |
| 3 | 工具总体 = 7 元 + 363 域 | registry `all_any()`=**371**（364 域/系统 + 7 元）；MCP 顶层 7 元工具，MCP 可达总数 370 | **一致** |
| 4 | ToolCatalog 371 条 | 由 registry `all_any()` 逐一 `make_tool_info` 派生 371 = 363 域 + system_status + 7 元；原 `populate_default_tools`/meta 快照/`Auto` 补录链路整体废弃 | **一致**（单一来源派生，无独立填表） |
| 5 | schema 283 非空 / 73 空（旧值，已随重命名变化） | def/SCHEMA_NONE 静态口径已随 08-21 真类化与 08-22 清理整体废除（fill 表直接给出最终 schema，`tool_input_schema` 的 basic 参数为 no-op）；catalog 级非空/空数**以运行时 `SchemaStatisticsBaseline` 观测为准**，不硬编码 | **运行时统计口径** |
| 6 | SchemaStatisticsBaseline 运行时统计断言，不硬编码 | `register_all_test.cpp:133-147` 只断言 `non_empty > empty`、`empty > 0`、总和 = `catalog.size()`；旧 283/73 为运行时实测值 | **一致** |
| 7 | 3 个契约缺口 | 见下表（reimport_resource_files 的 schema 字段名不一致已随 08-24 批次修复，空参静默成功仍在） | **一致** |
| 8 | 42 个副作用工具遍历排除 | 30 个 `*_tools.hpp` 中 42 个工具用 `GDA_TOOL_CLASS_SIDE` 标记；runner 读 `side_effect` 字段自动排除 | **一致** |
| 9 | 遍历步数与工具数联动 | 步数随工具数/排除集变化（363 − 42 = **321** 个参与空参契约遍历，副作用工具仍参与枚举但不做空参调用）；**以运行时 `03_tools_contract` 统计为准** | **运行时统计口径** |
| 10 | 命名约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首） | 363 个名字全部小写 snake_case；首段均为动词（create/get/set/add/remove/apply/intersect/play/stop/save/…），符合规范 | **一致** |

## 元工具与 g_meta_handlers

- 7 个元工具经 registry `add()` 注册（`IMetaTool` 接口自动归类）、由 `register_all.cpp` 派生循环 `RegisterTool`，不经 `call_tool`；`g_meta_handlers`（`register_all.cpp` 从 `all_meta()` 派生）保留同一组实现供分发层复用——即 `call_handler` 未命中 `g_handlers` 时直接查 `g_meta_handlers`（`dispatch.cpp`）。
- `call_tool`/`code_execute` 的错误翻转在 RegisterTool 回调统一处理：结果 JSON 含 `error` 字段时置 `is_error = true`，与领域工具 `{"error": ...}` 错误模式衔接。
- `call_tool` 对带 `__gda_pending` 的异步结果做等待与 `capture_game_viewport` 结果定型；等待实现为 `runtime_ops::wait_pending_response`。

## Schema 统计口径

- `has_schema_params` 判定：`input_schema.properties` 存在且为非空对象（`register_all_test.cpp`）；空 schema 仍保留空 `properties` 对象。
- 全部 schema 由 `schema::build_schema({ParamDef...})` 生成（8 个 `schema_*_ops.cpp` fill 表 + register_all 内联的元工具 schema）；未命中 fill 表的名字返回空表。schema_builder API 面收敛为三件：`ParamDef`、`build_schema(initializer_list<ParamDef>)`、`add_required_flag`（原 `make_object_schema`/`make_empty_schema`/`add_param` 等辅助函数已删除）。非空/空数以运行时 `SchemaStatisticsBaseline` 观测为准，不硬编码。
- 元工具 schema：ping/list_categories/get_tool_detail/call_tool 五个转换 `build_schema`；search_tools/batch_execute/code_execute 保留手写 JSON（嵌套结构 ParamDef 不可表达）。batch_execute/code_execute 处理逻辑内联于 `code_exec_ops::handle_batch_execute`/`handle_code_execute`，由 `SchemaSampledTools` 断言非空（`register_all_test.cpp`）。

## 契约缺口表（遍历记 warnings，不 FAIL）

| 工具 | schema 必填声明 | 实际行为 | 来源 |
|---|---|---|---|
| `create_scene_node` | name/type 标 required，但描述注明默认值 NewNode/Node | 空参被默认值吞掉，不报 "missing required" | `schema_scene_ops.cpp`；warning 逻辑 `traversal.cpp:302-304` |
| `get_resource_extensions` | type 标 required | handler 缺省时传空串给 `get_recognized_extensions_for_type("")`，返回全类型而非报错 | `resource_ops.cpp` |
| `reimport_resource_files` | path 标 required（files 可选，handler 两者皆读——08-24 字段名不一致已修复） | 空参时 files/path 均缺失走 count=0 静默成功 | `resource_ops.cpp:1760-1818`；`schema_scene_ops.cpp:170-173` |

历史背景注释见 `traversal.cpp:300-301`。

## 遍历排除清单摘要（42 个）

- **20 个持久磁盘副作用**（WritesFile/WritesConfig）：save_editor_scene、save_editor_scenes、save_editor_scene_as、write_file、create_script、save_resource、move_resource_file、create_directory、create_theme_resource、set_theme_color、set_theme_constant、set_theme_font_size、set_theme_stylebox_flat、move_os_file_to_trash、set_editor_main_scene、set_editor_plugin_enabled、save_project_settings、add_input_map_action_event、save_input_map、set_editor_settings。
- **16 个用户可见副作用**：show_os_alert、show_display_dialog、speak_display_tts、stop_display_tts、set_display_clipboard、set_display_mouse_mode、warp_display_mouse、set_display_window_title、set_display_window_position、set_display_window_size、set_display_window_mode、set_display_window_flag、move_display_window_to_foreground、request_display_window_attention、create_display_window、delete_display_window。
- **6 个进程副作用**：build_csharp_assembly、create_os_process、execute_os_process、kill_os_process、open_os_path、set_os_environment。
- 清单不硬编码：42 个工具用 `GDA_TOOL_CLASS_SIDE` 标记，`get_tool_detail` 响应携带 `side_effect` 字段，`tests/runner/traversal.cpp` 依该字段自动排除并从 30 个域 `*_tools.hpp` 枚举全部工具名；其中空 schema 的副作用工具在冒烟阶段同样跳过。

## 遍历步数推导

> 步数随工具数变化：域工具 363、排除 42，空参 363−42=**321**，冒烟步数以运行时 `03_tools_contract.json` 统计为准。08-24 扩容后枚举与排除集同步扩大，历史实测值（546）已过期，以最近一次运行为准。

遍历经 `call_tool` 元工具代理，每个非排除工具产生一次调用 = 一个 StepResult；步数随工具集与空 schema 数浮动（当前运行实测量见 `03_tools_contract` 报告）。

## 分发与错误路径（dispatch.cpp）

- `call_handler`：编辑器队列存在且不在主线程时，先 `queue.submit` 排到主线程执行，保证 Godot API 线程安全。
- `call_handler_impl` 查找顺序：`g_handlers` → 命中即执行，异常捕获后返回 `internal error in tool 'X': unexpected C++ exception`（参数 dump 截断 256 字节）；未命中再查 `g_meta_handlers`（元名单不再硬编码，由 registry `all_meta()` 派生）；否则返回 `domain tool 'X' not found — use search_tools to discover available tools`。
- 导出保护：全部 7 个元工具回调前置 `ExportGuard::is_exporting()` 检查，命中返回 `export_blocked_result()`。
- `debugger_access.hpp` 为 runtime_ops 提供的自由函数接口（capture/broadcast/cancel/continue/breaked/reload_scripts），实现于 `debugger_access.cpp`；依赖方向 runtime_ops.cpp → debugger_access.hpp、debugger_ops.cpp → runtime_ops.hpp，无环。

## 命名约定抽查

- 363 个工具名全部匹配 `^[a-z0-9_]+$`（小写 snake_case，无大写、无连字符），且**首段均为动词**（create/get/set/add/remove/apply/intersect/play/stop/save/seek/move/warp/…）——符合 `<动词>_<类别>_<维度>_<对象>_<修饰>` 动词置首规范。
- 段数随粒度自然变化：2 段（`instantiate_scene`、`property_get`、`signal_connect`…）、3-4 段（`create_physics_2d_body`、`intersect_physics_2d_ray`…）、5 段+（`get_scene_tree_nodes_in_group`、`set_input_map_action_deadzone`、`get_nav_3d_map_closest_point_to_segment`…）。规范约束动词置首与 snake_case，段数与类别段选取以表达清晰为准。

## 类别分布（363 领域工具，27 个类别）

> 权威总数由 30 个域 `*_tools.hpp` 的 `GDA_TOOL_CLASS`/`GDA_TOOL_CLASS_SIDE` 枚举为准（=363），registry `all()`=364（+system_status）；下表为按分类的细分。

| 类别 | 数量 | 类别 | 数量 |
|---|---|---|---|
| Render | 49 | Scene | 14 |
| Physics | 47 | Config | 13 |
| Resources | 24 | Text | 10 |
| Display | 24 | Animation | 10 |
| Editor | 23 | Scripts | 10 |
| Audio | 20 | Game | 9 |
| Input | 19 | Theme | 8 |
| OS | 18 | TileMap | 7 |
| Debug | 16 | Debugger | 5 |
| Navigation | 15 | Properties | 5 |
| Docs | 4 | Group | 3 |
| SpriteFrames | 3 | Analysis | 3 |
| Testing | 2 | System | 1 |
| Capture | 1 | | |

注：InputMap 类别已并入 Input；`get_debug_object_info` 挂 physics_ops 模块但归 **Debug** 类（`physics_tools.hpp` 生成、Category "Debug"）；`write_file` 归 OS 类；`capture_editor_viewport` 归 Capture。08-24 新增类别：**Analysis**（analyze_ops 三件套）、**Animation**、**Theme**、**Testing**。

## 与 AGENTS.md 不一致点清单

1. **耗时声称不符（旧）**：AGENTS.md 旧写"560 步，约 15s"；现遍历步数随工具数联动（见"遍历步数推导"），以分钟级为准。
2. **ToolCatalog 构成口径（已随本轮重构更新）**：现 371 = 363 域 + `system_status` + 7 元，全部由 registry `all_any()` 单一来源派生；原 343/332/5 默认 + 3 快照 + Auto 补录口径已废除。
3. **g_handlers 口径（已更新）**：现由 registry `all()` 派生共 364（363 域 + system_status），不再手写。
4. **reimport_resource_files 字段名不一致（08-24 已修复）**：schema 现声明 `path`（必填）与 `files`（可选）双字段，handler 两者皆读；剩余缺口仅"空参 count=0 静默成功"，已并入上方契约缺口表。
5. **新增生命周期约束**：registry `g_active_registry` 为文件级静态单例（程序存活期），register_all 每次调用重建内容；避免对局部 registry 的悬垂引用（此前重构曾触雷，已修复并被 L1 `RegistryIsSingleSourceOfTools` 断言护航）。

## 出站链接

- [测试体系](../tests.md)
- [工具实现 B 组](../modules/tools_ops_b.md)
- [工具实现 A 组](../modules/tools_ops_a.md)
- [入口与运行时](../modules/entry_runtime.md)
- [架构总览](../overview.md)
