---
type: 基线文档
title: 工具面基线
description: ST-03 中间快照（308 catalog / 302 域 / 5 元）与 T-01 严格冗余基线的静态审计记录
tags:
  - 工具面
  - 基线
  - 审计
timestamp: "2026-11-16T06:00:00+08:00"
resource: devtools/commands/tool-audit.ts
---

# 工具面基线

> 审计日期：2026-11-16。数据由 `devtools/commands/tool-audit.ts` 静态解析源码生成；本页数字均可在 [tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json) 中找到来源。

> ST-04 中间快照：catalog 268 = 域 262 + 元 5 + system 1，生产别名 263，schema 247/21，能力矩阵缺失 0。下文的 459 计数与域分布表是 T-01 历史基线，最终收口计数由 T-25 重写。

## 数据来源与命令

- 域工具：`src/tools/domains/*/*_tools.hpp` 的 451 个 `ToolSpec` 块。
- 系统与元工具：`src/tools/base/register_all.cpp` 的 `system_status` 与 7 个 `kMeta` 工具。
- 基线：`bun run gda-dev-tools audit-tools --baseline` → [tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)。
- 目标：`bun run gda-dev-tools audit-tools --target` → [tool_surface_target.json](../../tests/data/tool_surface_target.json) 与 [capability_matrix.json](../../tests/data/capability_matrix.json)。
- 查看：`bun run gda-dev-tools audit-tools --json` 直接输出完整基线。

## 基线计数

| 指标 | 数值 | 来源 |
| --- | ---: | --- |
| catalog 总数 | 459 | `catalog_total` |
| 域工具 | 451 | `domain_total` |
| 元工具 | 7 | `meta_total` |
| `system_status` | 1 | `system_total` |
| 域文件数 | 33 | `domain_count` |
| 严格冗余候选 | 32 | `strict_redundant_count` |
| 生产别名 | 0 | `alias_count` |
| schema 非空 | 393 | `schema_non_empty` |
| schema 空 | 66 | `schema_empty` |
| 能力矩阵域行 | 451 | `capability_matrix.rows` |
| 能力矩阵系统/元行 | 8 | `capability_matrix.meta_rows` |

## 域分布

| 域 | 当前 | 有效目标 | 计划目标 | 合并组 |
| --- | ---: | ---: | ---: | --- |
| Analysis | 3 | 1 | 1 | analyze_project |
| Animation | 11 | 6 | 6 | animation_clip / animation_track / animation_player / animation_tree |
| Audio | 24 | 13 | 13 | audio_bus / audio_player / audio_device / audio_capture / audio_wav |
| Capture | 2 | 2 | 2 | capture_editor_viewport / review_scene_visually |
| C# | 6 | 3 | 3 | csharp_member / csharp_event / dotnet |
| Config | 17 | 11 | 11 | config_read / config_write / autoload |
| Debug | 15 | 9 | 10 | debug_info / debug_stack / debug_log / debug_visual / debug_physics_fps / debug_monitors |
| Debugger | 6 | 4 | 4 | debugger_stream / debugger_plugin_log / debugger_game_log / debugger_session |
| Display | 25 | 14 | 14 | display_info / display_clipboard / display_dialog / display_mouse / display_tts / display_window / display_screen |
| Docs | 4 | 1 | 1 | get_docs_class |
| Editor | 33 | 24 | 24 | editor_state / editor_ui / editor_files / editor_scene_control / editor_save / editor_selection / editor_undo / editor_ui_action / editor_config / editor_inspect / editor_csharp / editor_script / editor_breakpoints / editor_session / editor_info |
| Game | 19 | 13 | 13 | game_state / game_job / game_ui / game_evidence / game_script / game_input / game_tween / game_localization |
| GridMap | 5 | 4 | 4 | create_gridmap / gridmap_item / set_gridmap_cells / get_gridmap_used_cells |
| Group | 3 | 1 | 1 | node_group |
| Input | 15 | 7 | 7 | input_state / input_action / input_key / input_mouse / input_text / input_gamepad |
| InputMap | 9 | 5 | 5 | get_input_map_actions / set_input_map_action_bundle / erase_input_map / save_input_map |
| Log | 1 | 1 | 1 | get_game_log_entries |
| Nav | 15 | 6 | 6 | create_navigation / nav_query / nav_control |
| OS | 23 | 11 | 11 | process / env / time / system / file / file_manager / alert / path |
| Physics | 51 | 32 | 32 | body / area / space / joint / shape / query / soft_body / debug / bundle |
| Properties | 7 | 6 | 6 | property_get / property_set / set_node_transform / property_get_list / signal |
| Render | 52 | 30 | 30 | environment / instance / viewport / camera / canvas / light / particles / resource / inspect / free |
| Resources | 34 | 20 | 20 | load / query / create / property / file / save / uid / import / curve / mesh |
| Scene | 15 | 9 | 9 | get_scene_tree / create_scene_node / edit_scene_node / delete_scene_node / camera / instantiate |
| SceneTree | 8 | 1 | 1 | game_scene_tree |
| Scripts | 11 | 6 | 6 | query / file / attach / execute / call |
| SpriteFrames | 3 | 2 | 2 | update / get |
| Testing | 2 | 1 | 1 | run_tests |
| Text | 10 | 4 | 4 | font / shaped / query |
| Theme | 10 | 4 | 4 | info / update_theme / update_layout |
| TileMap | 5 | 3 | 3 | create / set_cells / tileset |
| Tileset | 3 | 1 | 1 | update_tileset |
| Tween | 4 | 2 | 2 | game_tween / editor_preview |

- 域当前合计 451；有效目标合计 257，计划目标合计 258。
- 净新增 6 项：`pack_scene`、`animation_runtime`、`bake_navigation_mesh`、`astar_path`、`image_ops`、`mesh_ops`。
- 目标 catalog = 257 + 6 + `system_status` 1 + 元工具 5 = 269；计划声明值 270，执行期允许 250–300。
- Debug 差异：D-08 记当前 16，按域文件实测 15；Category 计数 16 是因为 `get_debug_object_info` 位于 physics 域但 Category 为 Debug。D-07 删除 6 项后有效目标为 9。

## 副作用分布（域工具）

| SideEffect | 数量 |
| --- | ---: |
| None | 276 |
| WritesScene | 43 |
| ModifiesWindow | 42 |
| WritesFile | 27 |
| WritesConfig | 18 |
| GameRuntime | 18 |
| Process | 10 |
| CodeExecute | 7 |
| Destructive | 6 |
| ShowsAlert | 4 |

## 严格冗余候选（32）

| 组 | 数量 | 旧工具 |
| --- | ---: | --- |
| Audio | 3 | get_audio_device_inputs / get_audio_bus_count / get_audio_bus_name |
| Debug | 6 | get_debug_monitor / get_debug_monitor_catalog / get_debug_object_count / get_debug_memory_usage / get_debug_node_count / get_debug_custom_monitor_names |
| Docs | 2 | get_docs_method / get_docs_property |
| Theme | 5 | set_theme_color / set_theme_constant / set_theme_font_size / set_theme_stylebox_flat / set_control_anchor_preset |
| TileMap/GridMap | 2 | set_tilemap_cell / set_gridmap_cell |
| InputMap | 4 | add_input_map_action / add_input_map_action_event / set_input_map_action_deadzone / has_input_map_action |
| Resource/Script/Property | 4 | has_resource_dependency / move_resource_file / load_script / set_properties_batch |
| Scene/Editor/Game/Meta | 6 | get_scene_snapshot / get_scene_node_screen_rect / verify_scene_saved / capture_game_viewport / ping / list_categories |

- 逐条参数适配与结果投影见 [capability_matrix.json](../../tests/data/capability_matrix.json) 的 `rows` 与 `meta_rows`；32 项均已登记 `status=mapped`，尚未从 catalog 删除。
- 矩阵域行 451、系统/元行 8，合计 459；域工具名唯一，系统/元工具名与域工具无重名。

## 副作用低报审查（启发式）

以下 8 条工具当前 `SideEffect::None`，但名称含删除、运行、播放、复制等强动作语义，需在对应合并任务中复核；这是审查候选，不是已确认缺陷。

| 工具 | 域 | 当前档位 |
| --- | --- | --- |
| play_audio_player | Audio | None |
| stop_audio_player | Audio | None |
| stop_audio_input_capture | Audio | None |
| remove_physics_3d_body_collision_exception | Physics | None |
| restart_render_particles | Render | None |
| duplicate_resource | Resources | None |
| run_gdscript_tests | Testing | None |
| run_gdscript_test_files | Testing | None |

## 相关文件

- 审计实现：[tool-audit.ts](../../devtools/commands/tool-audit.ts)
- 注册入口：[register_all.cpp](../../src/tools/base/register_all.cpp)
- 基线数据：[tool_catalog_baseline.json](../../tests/data/tool_catalog_baseline.json)
- 目标数据：[tool_surface_target.json](../../tests/data/tool_surface_target.json)
- 能力矩阵：[capability_matrix.json](../../tests/data/capability_matrix.json)
