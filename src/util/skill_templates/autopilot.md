# Godot Autopilot

Read this skill before using the godot-autopilot plugin: it is the required entry point for every session on a Godot project through the godot-autopilot MCP server. It covers how to connect, how to route a task to a tool family, when to consult the engine documentation instead of memory, how to watch engine state while working, and the change/observe/verify loop that keeps edits honest. For engine-level operations load the domain skills instead: godot-autopilot-scene-system, godot-autopilot-resources, godot-autopilot-scripting, godot-autopilot-runtime, godot-autopilot-servers, godot-autopilot-content and godot-autopilot-csharp. Protocol mechanics (discovery, batch and code execution, errors, limits, gotchas) live in the godot-autopilot-tools skill.

## Prerequisites

- The Godot editor must be open with the godot-autopilot plugin enabled. The MCP server starts automatically with the plugin and stops when the plugin is disabled or the editor closes.
- The MCP endpoint is `http://127.0.0.1:9527/mcp` by default, but the port is configurable. Never assume 9527; see the godot-autopilot-tools skill (references/script-access.md) for the port discovery order.
- The server listens on the loopback interface only, with no authentication. It is meant for trusted local MCP clients on the same machine.

## What the server exposes

The MCP layer exposes the meta tools `ping`, `search_tools`, `list_categories`, `get_tool_detail`, `call_tool`, `batch_execute` and `code_execute`.

Every domain tool is executed through `call_tool` by passing the domain tool name and its arguments object. A few high-effect groups sit behind capability gates; the enable paths are in the godot-autopilot-tools skill.

## Consult the engine documentation

Never call engine APIs from memory. Property names, method signatures, enum values, defaults and class members change between Godot versions, and recalled signatures are a common source of silent failures. The server exposes a documentation family - `find_docs_class`, `get_docs_class`, `get_docs_method`, `get_docs_property` - that answers straight from the offline documentation cache bundled with the running editor.

Consult it before:

- Writing or patching a script that calls an engine API you have not read this session.
- Setting a property whose exact name or accepted values you are not sure of - check the property type and enum names first.
- Using a class or method that is new to you, or that may have been renamed between Godot versions.
- Chasing a failure whose message mentions an unknown property, unknown method or parse error.

Because the cache ships inside the editor, the answers follow the engine version the user actually runs: whichever Godot version is open, the documentation matches that exact build. There is no network access and no stale recall involved - read the docs, do not remember them.

## Task routing

### Choosing the right tool

Reduce the task to its object and intent first; the first matching row is usually the right family:

- Change a property -> `property_set`, then read it back with `property_get` to confirm the value landed.
- Inspect properties or their metadata -> `property_get`, `property_get_list`.
- Build or edit tilemaps -> `create_tilemap_tileset` plus `set_tilemap_cell` / `set_tilemap_cells` for cells.
- Add, rename, move or delete nodes -> the scene tools (`create_scene_node`, `rename_scene_node`, `reparent_node`, `delete_scene_node`).
- Work with resources in memory or on disk -> the resource tools (`create_resource`, `load_resource`, `save_resource`).
- Write, attach or call GDScript -> the script tools (`create_script`, `attach_script_to_node`, `call_script_node`).
- Need an engine API fact -> the documentation tools (`find_docs_class`, `get_docs_class`, `get_docs_method`, `get_docs_property`) - never memory; see "Consult the engine documentation".
- Automate the editor UI -> enumerate with `get_editor_ui_elements` or preview with `hit_test_editor_point`, then act with `click_editor_element`, `type_editor_element_text` or `run_editor_shortcut`; raw-coordinate fallbacks (`click_input_mouse`, `scroll_input_mouse`, `drag_input_mouse`, `type_input_text`) are for the 2D canvas, 3D viewport and custom-drawn controls (see godot-autopilot-runtime).
- Run the game and observe it -> the runtime tools (`play_editor_current_scene`, `capture_game_viewport`, `queue_game_input`).
- Diagnose from logs -> the log tools; see "Watch the engine state".
- Repeat a fixed sequence of calls -> `batch_execute`; need loops, math or computed values -> `code_execute`; the semantics of both are in the godot-autopilot-tools skill.

### Read (inspect what exists)

| Task | Tools |
|---|---|
| Edited scene tree | `get_scene_tree`, `get_editor_edited_scene_root` |
| Node properties | `property_get`, `property_get_list` |
| Editor selection | `get_editor_selection` |
| Running game scene tree | `get_debugger_scene_tree` |
| Running game UI | `get_game_ui_elements` |
| Editor UI elements | `get_editor_ui_elements`, `hit_test_editor_point` |
| Resources on disk | `get_resource_dir_files`, `get_resource_type`, `has_resource`, `get_resource_dependencies`, `get_resource_references` |
| Project file system | `get_editor_file_system_tree`, `get_editor_file_system_status` |
| Project / editor settings | `get_project_settings`, `has_project_settings`, `get_editor_settings` |
| Godot API reflection | `get_docs_class`, `find_docs_class`, `get_docs_method`, `get_docs_property` - prefer these over recalling API signatures from memory |
| Viewports | `capture_editor_viewport` |

### Build and modify

| Task | Tools |
|---|---|
| Nodes | `create_scene_node`, `rename_scene_node`, `reparent_node`, `delete_scene_node`, `instantiate_scene`, `add_group_node` |
| Properties | `property_set` (JSON value shapes and Godot 3-to-4 renames: see godot-autopilot-scene-system) |
| Signals | `signal_connect`, `signal_disconnect`, `trace_signal_flow` |
| Resources | `create_resource`, `load_resource`, `save_resource`, `duplicate_resource`, `set_resource_property`, `get_resource_property` |
| Files | `rename_resource_file`, `move_resource_file` (both rewrite references), `write_file`, `read_file`, `find_in_files` |
| Scripts | `create_script`, `attach_script_to_node`, `call_script_node`, `reload_script` |
| Theme | `create_theme_resource`, `set_theme_color`, `set_theme_stylebox_flat`, `apply_theme_to_control`, `set_control_anchor_preset` |
| Tilemap | `create_tilemap`, `create_tilemap_tileset`, `set_tilemap_cell`, `set_tilemap_cells` (bulk) |
| Animation | `create_scene_animation_player`, `create_animation`, `create_animation_track`, `insert_animation_keyframe`, `create_scene_animation_tree`, `add_animation_machine_state`, `connect_animation_states` |
| SpriteFrames | `create_spriteframes`, `add_spriteframes_animation`, `add_spriteframes_frame` |
| Undo grouping | `create_editor_undo_redo_action`, `add_editor_undo_redo_do`, `add_editor_undo_redo_undo`, `commit_editor_undo_redo` |
| Editor UI automation | `click_editor_element`, `type_editor_element_text`, `run_editor_shortcut`; raw-coordinate fallbacks `click_input_mouse`, `scroll_input_mouse`, `drag_input_mouse`, `type_input_text` |

### Run and debug

| Task | Tools |
|---|---|
| Launch / stop | `play_editor_current_scene`, `stop_editor_playing` |
| Game channel status | `get_game_status` |
| Code in the game | `execute_game_script`, `reload_game_scripts` |
| Input into the game | `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `get_game_input_status` - editor-side input tools do not reach the game (see godot-autopilot-runtime) |
| Game screenshots / UI | `capture_game_viewport`, `get_game_ui_elements` |
| Logs (four sources) | `get_debugger_log`, `get_debugger_errors`, `get_debugger_output`, `get_game_log_entries`, `get_plugin_log` |
| Performance | `get_debug_monitor_catalog`, `get_debug_monitors`, `get_debug_memory_usage`, `get_debug_object_count`, `get_debug_node_count` |
| Pause / reload | `set_scene_tree_pause`, `reload_scene_tree_current_scene` |
| Inline tests | `run_gdscript_tests`, `run_gdscript_test_files` |
| Project analysis | `validate_scene_file`, `find_unused_resources` |
| Development loop | `capture_editor_viewport`, `capture_game_viewport`, `get_debugger_log` - the full change/observe/verify cycle is in references/development-workflow.md |

### Project admin

| Task | Tools |
|---|---|
| Project settings | `get_project_settings`, `set_project_settings`, `save_project_settings` - set is memory-only until you save |
| Editor settings | `get_editor_settings`, `set_editor_settings` (persists automatically) |
| InputMap | `get_input_map_actions`, `add_input_map_action`, `add_input_map_action_event`, `save_input_map` |
| Main scene | `set_editor_main_scene` |
| Scene files | `create_editor_scene`, `open_editor_scene`, `save_editor_scene`, `save_editor_scene_as`, `close_editor_scene` |
| File system refresh | `scan_editor_file_system` |

## Watch the engine state

Never chain write operations blind. Every change lands in a live editor with an import queue, a debugger and an engine log; state you did not observe is state you do not know. Keep observation inside the loop:

- After every write call, check the error watermark in the response before issuing the next call. A non-zero value means new errors were recorded - stop, pull the real log lines, fix or explain them, then continue. Watermark mechanics and the fix-confirmation loop are in the godot-autopilot-tools skill.
- While a game is running, watch the debugger channel: `get_debugger_errors` and `get_debugger_output` carry the game's errors and prints. Without a running game, the editor output panel and engine log come through `get_debugger_log`, and `get_game_log_entries` reads the on-disk log even after the session ended.
- When the problem is inside the plugin itself (authorization denials, timeouts, dropped responses), read `get_plugin_log`.
- Prefer observing real output - a log line, a screenshot, a read-back - over assuming a call did what its name suggests.

## Game development workflow

The routing tables cover single calls; real work is a loop. A success means the call went through, not that the effect is what you wanted, so every iteration is change, observe, diagnose, fix, verify:

1. Change the project through tools (`property_set`, `create_script`, `write_file`, and so on), or orchestrate several through `batch_execute` / `code_execute`.
2. Observe the result with a screenshot (`capture_editor_viewport`, `capture_game_viewport`), a read-back (`property_get`, `get_scene_tree`, `get_resource_property`) or output (`get_debugger_log`; with a running game `get_debugger_errors` and `get_debugger_output`, disk fallback `get_game_log_entries`).
3. Diagnose from what you observed, fix, and re-observe until the observation shows the intended effect.

references/development-workflow.md expands this into complete loops for UI and scene appearance, script logic, resources and imports, and runtime debugging with input injection.

## Where the protocol details live

- Discovery protocol (search first, never guess names): godot-autopilot-tools.
- `batch_execute` versus `code_execute` semantics and the authorization enable paths: godot-autopilot-tools.
- Error protocol and the error watermark confirmation loop: godot-autopilot-tools.
- Timeout, size and batch limits: godot-autopilot-tools.
- Tool gotchas catalogue: godot-autopilot-tools.

## If MCP tools are missing

If your MCP client shows an empty tool list or calls fail right after the editor restarted, the server is usually fine and the client harness simply did not reconnect. Work down this ladder:

1. Native MCP tools - retry the connection first; this is the normal channel.
2. Script bridge - run `node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs` (requires Node 18+ or Bun) to reach the same server without a working MCP client.
3. Neither installed - fall back to a manual plan per `references/manual-fallback.md` of the godot-autopilot-tools skill.

## See also

- godot-autopilot-scene-system - scene lifecycle, nodes, properties, signals
- godot-autopilot-resources - resource and file operations, rename and move
- godot-autopilot-scripting - GDScript execution channels
- godot-autopilot-runtime - running the game, input injection, debugging
- godot-autopilot-servers - RenderingServer, PhysicsServer and NavigationServer RID tools
- godot-autopilot-content - tilemap, animation, audio, UI theming
- godot-autopilot-csharp - C# build loop, GlobalClass resources, Export conversion
- godot-autopilot-tools - discovery, execution, error and limit protocols
