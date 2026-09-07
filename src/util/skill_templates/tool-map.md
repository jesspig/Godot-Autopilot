# Godot Autopilot Tool Map

Task-to-tool routing for the 363 domain tools of the godot-autopilot MCP server. Use it when you know the task but not which tool to call.

## How to use

1. Find your task category below and pick a candidate tool.
2. Confirm the exact name with `search_tools` (never guess names).
3. Read the schema with `get_tool_detail`, including its side-effect marker for anything that writes files or config.
4. Execute through `call_tool`, or orchestrate several with `batch_execute`.

## Read (inspect what exists)

| Task | Tools |
|---|---|
| Edited scene tree | `get_scene_tree` (optional `max_depth`, `include_properties`), `get_editor_edited_scene_root` |
| Node properties | `property_get`, `property_get_list` |
| Editor selection | `get_editor_selection` |
| Running game scene tree | `get_debugger_scene_tree` |
| Running game UI | `get_game_ui_elements` |
| Resources on disk | `get_resource_dir_files`, `get_resource_type`, `has_resource`, `get_resource_dependencies`, `get_resource_references` |
| Project file system | `get_editor_file_system_tree`, `get_editor_file_system_status` |
| Project / editor settings | `get_project_settings`, `has_project_settings`, `get_editor_settings` |
| Godot API reflection | `get_docs_class`, `find_docs_class`, `get_docs_method`, `get_docs_property` - prefer these over recalling API signatures from memory |
| Viewports | `capture_editor_viewport` |

## Build & modify

| Task | Tools |
|---|---|
| Nodes | `create_scene_node`, `rename_scene_node`, `reparent_node`, `delete_scene_node`, `instantiate_scene`, `add_group_node` |
| Properties | `property_set` (JSON value shapes and Godot 3-to-4 renames: see godot-autopilot-properties-signals) |
| Signals | `signal_connect`, `signal_disconnect`, `trace_signal_flow` |
| Resources | `create_resource`, `load_resource`, `save_resource`, `duplicate_resource`, `set_resource_property`, `get_resource_property` |
| Files | `rename_resource_file`, `move_resource_file` (both rewrite references), `write_file`, `read_file`, `find_in_files` |
| Scripts | `create_script`, `attach_script_to_node`, `call_script_node`, `reload_script` |
| Theme | `create_theme_resource`, `set_theme_color`, `set_theme_stylebox_flat`, `apply_theme_to_control`, `set_control_anchor_preset` |
| Tilemap | `create_tilemap`, `create_tilemap_tileset`, `set_tilemap_cell`, `set_tilemap_cells` (at most 64 per call) |
| Animation | `create_scene_animation_player`, `create_animation`, `create_animation_track`, `insert_animation_keyframe`, `create_scene_animation_tree`, `add_animation_machine_state`, `connect_animation_states` |
| SpriteFrames | `create_spriteframes`, `add_spriteframes_animation`, `add_spriteframes_frame` |
| Undo grouping | `create_editor_undo_redo_action`, `add_editor_undo_redo_do`, `add_editor_undo_redo_undo`, `commit_editor_undo_redo` |

## Run & debug

| Task | Tools |
|---|---|
| Launch / stop | `play_editor_current_scene`, `stop_editor_playing` |
| Game channel status | `get_game_status` |
| Code in the game | `execute_game_script`, `reload_game_scripts` |
| Input into the game | `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `get_game_input_status` - editor-side input tools do not reach the game (see godot-autopilot-running-games) |
| Game screenshots / UI | `capture_game_viewport`, `get_game_ui_elements` |
| Logs (three paths) | `get_debugger_log` (editor engine log, works without a game), `get_debugger_errors` and `get_debugger_output` (runtime channel), `get_game_log_entries` (on-disk log, works without a session) |
| Performance | `get_debug_monitor_catalog`, `get_debug_monitors`, `get_debug_memory_usage`, `get_debug_object_count`, `get_debug_node_count` |
| Pause / reload | `set_scene_tree_pause`, `reload_scene_tree_current_scene` |
| Inline tests | `run_gdscript_tests`, `run_gdscript_test_files` |
| Project analysis | `validate_scene_file`, `find_unused_resources` |

## Project admin

| Task | Tools |
|---|---|
| Project settings | `get_project_settings`, `set_project_settings`, `save_project_settings` - set is memory-only until you save |
| Editor settings | `get_editor_settings`, `set_editor_settings` (persists automatically) |
| InputMap | `get_input_map_actions`, `add_input_map_action`, `add_input_map_action_event`, `save_input_map` |
| Main scene | `set_editor_main_scene` |
| Scene files | `create_editor_scene`, `open_editor_scene`, `save_editor_scene`, `save_editor_scene_as`, `close_editor_scene` |
| File system refresh | `scan_editor_file_system` |

## batch_execute vs code_execute

- `batch_execute` orchestrates existing tools: an ordered `operations` list, run in sequence, at most 256 items, with `stop_on_error` (default true) and optional `rollback_on_error`. Choose it for deterministic sequences such as "set these five properties, then save".
- `code_execute` runs GDScript in the editor: the source is wrapped in a script extending Node, and the edited scene root is exposed as SceneRoot. Default mode inlines your code inside a single `_run()` function - top-level func definitions are unsupported (pass `function_name` for multi-function mode). `timeout_ms` defaults to 5000 with a 30000 maximum; timeout is checked before execution, not used to interrupt. `auto_owner` defaults to true so created nodes are saved with the scene.
- Decision rule: expressible as a handful of existing tool calls - use `batch_execute`. Needs loops, math or conditions (laying out hundreds of tiles, bulk renames, computed values) - use `code_execute`. Very large payloads also favor `code_execute` to avoid huge JSON arguments.
- Both are meta tools: call them directly at the MCP layer, not through `call_tool`.

## Full tool index

references/CATEGORY_INDEX.md lists all 363 domain tools grouped by their 30 source modules, one line each. Browse it when `search_tools` does not surface a tool you suspect exists.

## See also

- godot-autopilot-usage
- godot-autopilot-direct-http
- godot-autopilot-scene-building
- godot-autopilot-properties-signals
- godot-autopilot-resources-files
- godot-autopilot-scripting
- godot-autopilot-running-games
- godot-autopilot-debugging
- godot-autopilot-inspection
- godot-autopilot-tips-gotchas
