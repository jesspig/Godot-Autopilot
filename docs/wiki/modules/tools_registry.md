# 工具注册表（src/tools/ 注册管线）

> 审计日期：2026-08-12，基于当前工作树源码逐行核算（未运行测试、未引用 git 历史）。
> 覆盖范围：`register_all.cpp/hpp`、`dispatch.cpp/hpp`、`tool_catalog.cpp/hpp`、`schema_builder.cpp/hpp`、`schema_fills.hpp`、6 个 `schema_*_ops.cpp`、`tool_defs.def`，对照 `tests/runner/traversal.cpp`、`tests/unit/register_all_test.cpp`、`tests/config/03_tools_contract.json` 与仓库根 `AGENTS.md` 工具段。
> 相关页面：[测试体系](../tests.md) · [工具实现 B 组](../modules/tools_ops_b.md) · [工具实现 A 组](../modules/tools_ops_a.md) · [入口与运行时](../modules/entry_runtime.md) · [架构总览](../overview.md)

## 注册管线流程

工具注册发生在 `register_all_tools()`（`src/tools/register_all.cpp:268`），单函数内完成四路登记：

```mermaid
flowchart TD
    A[register_all_tools] --> B[g_handlers['system_status'] 手写 lambda<br/>register_all.cpp:270]
    A --> C[TOOL_ENTRY 宏展开 tool_defs.def 331 条<br/>register_all.cpp:283-286 → g_handlers]
    A --> D[RegisterTool 直连 7 个元工具<br/>register_all.cpp:296/342/367/398/435/497/548]
    A --> E[TOOL_ENTRY 宏再展开<br/>register_all.cpp:566-570 → catalog.add_tool]
    A --> F[populate_default_tools 5 个默认条目<br/>tool_catalog.cpp:48-139]
    A --> G[meta 快照 3 个条件添加<br/>register_all.cpp:601-621]
    A --> H[g_meta_handlers 7 个元工具<br/>register_all.cpp:573-598]
    A --> I[BM25 索引全量入库<br/>register_all.cpp:624-626]
    A --> J[一致性对账：handler↔catalog 双向缺失检查<br/>register_all.cpp:633-653]
```

- `tool_defs.def` 被 `#include` 两次（`register_all.cpp:285` 与 `:569`），宏 `TOOL_ENTRY` 在两次展开中分别产出 `g_handlers` 赋值与 `catalog.add_tool` 调用，同一数据源驱动两张表。
- 一致性对账（`register_all.cpp:633-653`）：handler 缺 catalog 条目 → 以 `"Auto"` 分类补录；catalog 条目缺 handler → 记 Error 日志。当前配置下两方向均无缺失，无 Auto 补录。

## 三层结构

| 层 | 载体 | 数量 | 内容 |
|---|---|---|---|
| MCP 服务器层 | `server.RegisterTool()` | **7** | 元工具（见下） |
| 分发层 | `dispatch::g_handlers` | **332** | 331 领域工具 + `system_status`（`register_all.cpp:270` 手写） |
| 目录层 | `ToolCatalog` | **342** | 331 领域 + 5 默认 + 3 meta 快照 + system_status 等（口径见下） |

领域工具不注册进 MCP 工具列表，只能经 `call_tool` 元工具代理调用；`g_handlers` 中 332 个 handler 与 331 条 def 一一对应，`system_status` 是唯一非 def 来源。`ToolCatalog` 342 = 331 领域 + 7 元工具 + `system_status` + 3 个 meta 快照（batch_execute/call_tool/code_execute 的目录条目）。

## 数值核算总表（与 AGENTS.md 逐项对比）

| # | 声称（AGENTS.md） | 核算结果 | 结论 |
|---|---|---|---|
| 1 | 元工具 7 个：ping/search_tools/list_categories/get_tool_detail/call_tool/batch_execute/code_execute，直接 `RegisterTool` 注册 | 7 处 `RegisterTool`（`register_all.cpp:296/342/367/398/435/497/548`），名单与 `dispatch.hpp:11-13` 的 `meta_tool_names`、`register_all_test.cpp:28-30` 完全一致；L1 测试断言 `ListTools` 恰好 7 个（`register_all_test.cpp:86-89`） | **一致** |
| 2 | 领域工具 331 个（g_handlers 映射） | `tool_defs.def` 中 `TOOL_ENTRY(` 331 条（无重复名）；`g_handlers` 实际 **332** = 331 + `system_status` | **一致**（331 准确；g_handlers 另有 system_status，AGENTS.md 未提及） |
| 3 | 工具总数 338 = 7 元 + 331 领域 | MCP 层 7 + 分发层领域 331 = 338（系统口径）；目录层 342 | **一致** |
| 4 | ToolCatalog 342 条 | 342 = 331 领域 + 7 元工具 + `system_status` + 3 个 meta 快照；默认表含 5 项：ping/system_status/search_tools/list_categories/get_tool_detail，快照表 3 项：batch_execute/call_tool/code_execute（`tool_catalog.cpp:48-139` + `register_all.cpp:601-621`） | **数字一致** |
| 5 | schema 283 非空 / 73 空（旧值，已随重命名变化） | def 层面静态可数：SCHEMA_NONE=208、SCHEMA_BASIC=123（旧 222/126）；catalog 级非空/空数随 6 个 fill 函数与默认表变化，**以运行时 `SchemaStatisticsBaseline` 观测为准**，不硬编码 | **运行时统计口径** |
| 6 | SchemaStatisticsBaseline 运行时统计断言，不硬编码 | `register_all_test.cpp:133-147` 只断言 `non_empty > empty`、`empty > 0`、总和 = `catalog.size()`；旧 283/73 为运行时实测值 | **一致** |
| 7 | 3 个契约缺口 | 见下表 | **一致** |
| 8 | 34 个副作用工具遍历排除 | `traversal.cpp:24-63` 数组恰 34 项（12 持久磁盘 + 22 用户可见），全部存在于 def | **一致** |
| 9 | 遍历约 408 步（297 空参 + 111 冒烟） | 步数可由逻辑复现 = 408（331 − 34 = 297；冒烟 = 297 − 空 schema 且未排除 186 = 111）；耗时约 2-3 分钟 | **步数一致，耗时以分钟计**（见不一致点 1） |
| 10 | 命名约定 `<动词>_<类别>_<维度>_<对象>_<修饰>`（动词置首） | 331 个名字全部小写 snake_case（0 大写、0 非法字符）；首段均为动词（create/get/set/add/remove/apply/intersect/play/stop/save/…），符合规范 | **一致** |

## 元工具与 g_meta_handlers

- 7 个元工具直接 `RegisterTool`，不经 `call_tool`；`g_meta_handlers`（`register_all.cpp:573-598`）保留同一组实现供分发层复用——即 `call_handler` 收到元工具名时走 `g_meta_handlers` 分支（`dispatch.cpp:35-39`）。
- `call_tool` 元工具检查结果 JSON 的 `error` 字段并置 `is_error = true`（`register_all.cpp:447`），与领域工具 `{"error": ...}` 错误模式衔接。
- `call_tool` 对带 `__gda_pending` 的异步结果做等待与 `capture_game_viewport` 结果定型（`register_all.cpp:243-258`）。

## Schema 统计口径

- `has_schema_params` 判定：`input_schema.properties` 存在且为非空对象（`register_all_test.cpp:40-43`）；空 schema 仍保留空 `properties` 对象。
- 330 余条 schema 全部由 `schema::build_schema({ParamDef...})` 生成（`schema_builder.cpp:116-136`）；无 schema 条目的工具走 `build_schema_for_none_by_name`（`register_all.cpp:73-90`）——`scene_tree_*` 补 `group` 参数、`tilemap_*` 补 `node_path`，其余返回空表。当前 331 个 def 工具全部在 fill 表中有条目，该回退路径未实际命中。
- 3 个 meta 快照（batch_execute/call_tool/code_execute）在 `schema_debug_sys_ops.cpp:166-181` 中有非空 schema 条目，因此 `SchemaSampledTools` 将其断言为非空（`register_all_test.cpp:149-168`）。

## 契约缺口表（遍历记 warnings，不 FAIL）

| 工具 | schema 必填声明 | 实际行为 | 来源 |
|---|---|---|---|
| `create_scene_node` | name/type 标 required，但描述注明默认值 NewNode/Node | 空参被默认值吞掉，不报 "missing required" | `schema_scene_ops.cpp:8-12`；warning 逻辑 `traversal.cpp:302-304` |
| `get_resource_extensions` | type 标 required | handler 缺省时传空串给 `get_recognized_extensions_for_type("")`，返回全类型而非报错 | `resource_ops.cpp:814-836` |
| `reimport_resource_files` | path 标 required | 空参时 files 缺失走 count=0 静默成功（另注意：schema 字段为 `path`，handler 实际读取 `files`，字段名不一致） | `resource_ops.cpp:1122-1146`；`schema_scene_ops.cpp:151-153` |

历史背景注释见 `traversal.cpp:300-301`。

## 遍历排除清单摘要（34 个）

- **12 个持久磁盘副作用**：set_editor_main_scene、set_editor_plugin_enabled、save_project_settings、add_input_map_action_event、save_input_map、set_editor_settings、save_editor_scene、save_editor_scenes、save_editor_scene_as、write_file、create_script、save_resource。
- **22 个用户可见副作用**：show_os_alert、show_display_dialog、create_os_process、execute_os_process、kill_os_process、open_os_path、move_os_file_to_trash、set_os_environment、speak_display_tts、stop_display_tts、set_display_clipboard、set_display_mouse_mode、warp_display_mouse、set_display_window_title、set_display_window_position、set_display_window_size、set_display_window_mode、set_display_window_flag、move_display_window_to_foreground、request_display_window_attention、create_display_window、delete_display_window。
- 清单定义在 `traversal.cpp:24-63`；其中 22 个（SCHEMA_NONE=22）同时属于空 schema 集合，冒烟阶段跳过。
- 遍历新增工具时必须同步维护此清单，否则会污染 `Example/` 项目或干扰桌面（注释见 `traversal.cpp:18-23`）。

## 遍历步数推导（约 408 步）

遍历经 `call_tool` 元工具代理，每个非排除工具产生一次调用 = 一个 StepResult；步数全部由 def 与排除/空表集合决定，可精确复现：

| 阶段 | 计算 | 步数 |
|---|---|---|
| empty_args（全量空参契约） | 331 − 34（排除） | 297 |
| heuristic_smoke（按 schema 属性类型生成参数冒烟） | 297 − 186（空 schema 且未被排除 = 208 − 22） | 111 |
| **合计** | | **408** |

`tests/config/03_tools_contract.json` 未显式写入步数或耗时，仅描述"一次跑完 331 工具，共耗时约 2-3 分钟"。

## 分发与错误路径（dispatch.cpp）

- `call_handler`（`dispatch.cpp:57-66`）：编辑器队列存在且不在主线程时，先 `queue.submit` 排到主线程执行，保证 Godot API 线程安全。
- `call_handler_impl`（`dispatch.cpp:17-53`）查找顺序：`g_handlers` → 命中即执行，异常捕获后返回 `internal error in tool 'X': unexpected C++ exception`（参数 dump 截断 256 字节）；未命中但属 `meta_tool_names` → `g_meta_handlers`；否则返回 `domain tool 'X' not found — use search_tools to discover available tools`。
- 导出保护：全部 7 个元工具回调前置 `ExportGuard::is_exporting()` 检查，命中返回 `export_blocked_result()`（`dispatch.cpp:68-74`）。

## 命名约定抽查

- 331 个工具名全部匹配 `^[a-z0-9_]+$`（小写 snake_case，无大写、无连字符），且**首段均为动词**（create/get/set/add/remove/apply/intersect/play/stop/save/seek/move/warp/…）——符合 `<动词>_<类别>_<维度>_<对象>_<修饰>` 动词置首规范。
- 段数随粒度自然变化：2 段（`instantiate_scene`、`property_get`、`signal_connect`…）、3-4 段（`create_physics_2d_body`、`intersect_physics_2d_ray`…）、5 段+（`get_scene_tree_nodes_in_group`、`set_input_map_action_deadzone`、`get_nav_3d_map_closest_point_to_segment`…）。规范约束动词置首与 snake_case，段数与类别段选取以表达清晰为准。

## def 分类分布（331 领域工具，23 个类别）

| 类别 | 数量 | 类别 | 数量 |
|---|---|---|---|
| Render | 49 | Scene | 12 |
| Physics | 47 | Scripts | 10 |
| Display | 24 | Text | 10 |
| Editor | 22 | TileMap | 7 |
| Resources | 21 | Debugger | 7 |
| Audio | 20 | Game | 6 |
| Input | 19 | Properties | 5 |
| OS | 16 | Docs | 4 |
| Debug | 16 | Group | 3 |
| Navigation | 15 | SpriteFrames | 3 |
| Config | 13 | System | 1 |
| | | Capture | 1 |

注：InputMap 类别已并入 Input（原 `input_map_persist` → `save_input_map` 归 Input）；`get_debug_object_info` 挂 physics_ops 模块但归 Debug 类；`write_file` 归 OS 类；`capture_editor_viewport` 归 Capture。

## 与 AGENTS.md 不一致点清单

1. **耗时声称不符（旧）**：AGENTS.md 旧写"560 步，约 15s"——560 步已随重命名更新为 ≈408（297 + 111）；"约 15s"与 `03_tools_contract.json` 描述"共耗时约 2-3 分钟"矛盾（且 L2 需启动 headless 编辑器，秒级耗时不现实），以分钟级为准。
2. **ToolCatalog 构成口径**：342 = 331 领域 + 7 元工具 + `system_status` + 3 个 meta 快照；默认表 5 项（ping/system_status/search_tools/list_categories/get_tool_detail）与快照 3 项（batch_execute/call_tool/code_execute）中，7 个元工具均有目录条目。
3. **g_handlers 口径**：实际 332（331 def + system_status，`register_all.cpp:270`），AGENTS.md 未提及 system_status 属于 g_handlers。
4. **3 个契约缺口补充**：`reimport_resource_files` 的 schema 字段名（path）与 handler 实际读取字段（files）不一致，AGENTS.md 未记载。

## 出站链接

- [测试体系](../tests.md)
- [工具实现 B 组](../modules/tools_ops_b.md)
- [工具实现 A 组](../modules/tools_ops_a.md)
- [入口与运行时](../modules/entry_runtime.md)
- [架构总览](../overview.md)
