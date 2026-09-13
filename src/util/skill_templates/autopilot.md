# Godot Autopilot

Entry point for working on a Godot project through the godot-autopilot MCP server. This skill is the plugin usage overview: how to connect, how to discover tools (never guess names), how to route a task to a tool, how to read the error protocol every response follows, and the change/observe/verify development loop that keeps edits honest. For engine-level operations load the domain skills instead: godot-autopilot-scene-system, godot-autopilot-resources, godot-autopilot-scripting, godot-autopilot-runtime, godot-autopilot-servers and godot-autopilot-content.

## Prerequisites

- The Godot editor must be open with the godot-autopilot plugin enabled. The MCP server starts automatically with the plugin and stops when the plugin is disabled or the editor closes.
- The MCP endpoint is `http://127.0.0.1:9527/mcp` by default, but the port is configurable. Never assume 9527; see references/http-fallback.md for the four-level port discovery order.
- The server listens on the loopback interface only, with no authentication. It is meant for trusted local MCP clients on the same machine.

## What the server exposes

The MCP layer intentionally exposes only seven meta tools:

- `ping` - health check
- `search_tools` - search tools by keyword, category or tags
- `list_categories` - list all tool categories with counts
- `get_tool_detail` - full schema and side-effect marker for one tool
- `call_tool` - execute any domain tool by name
- `batch_execute` - run several tool calls in sequence
- `code_execute` - run GDScript in the editor

The 366 domain tools (scene, property, resource, script, physics, render, audio, and so on) plus `system_status` are not registered as MCP tools directly. Call every domain tool through `call_tool`, passing the domain tool name and its arguments object.

Two groups are denied by default behind an authorization gate: `code_execute` and `execute_script` (arbitrary GDScript in the editor) need the `code_execute` capability, and the tools that reach the running game (`execute_game_script`, `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `reload_game_scripts`) need the `game_runtime` capability. The gate is checked on every call; the "batch_execute versus code_execute" section covers the enable paths.

## Discovery protocol - search first, never guess

Tool names follow the pattern verb_category_dimension_object_modifier in snake_case with the verb first, but segment count varies (2-6 segments), so guessing is unreliable. Always discover:

1. `ping` to confirm the server responds.
2. `search_tools` with a `query` (optionally `category` or `tags` filters), or `list_categories` to browse by domain.
3. `get_tool_detail` with the exact `name` to read its input schema, description and side-effect marker before calling anything that writes files or config.
4. `call_tool` with `name` and `arguments` to execute.

Calling `call_tool` with an unknown name fails with a message telling you to use `search_tools`. Use it instead of improvising similar names.

Example domain call through `call_tool`:

```json
{"name": "create_scene_node", "arguments": {"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}}
```

If `search_tools` does not surface a tool you suspect exists, browse references/tool-catalog.md: it lists all 366 domain tools grouped by their 30 source modules, one line each.

## Task routing

1. Find your task category below and pick a candidate tool.
2. Confirm the exact name with `search_tools` (never guess names).
3. Read the schema with `get_tool_detail`, including its side-effect marker for anything that writes files or config.
4. Execute through `call_tool`, or orchestrate several with `batch_execute`.

### Read (inspect what exists)

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
| Tilemap | `create_tilemap`, `create_tilemap_tileset`, `set_tilemap_cell`, `set_tilemap_cells` (bulk; `source_id` -1 clears a cell) |
| Animation | `create_scene_animation_player`, `create_animation`, `create_animation_track`, `insert_animation_keyframe`, `create_scene_animation_tree`, `add_animation_machine_state`, `connect_animation_states` |
| SpriteFrames | `create_spriteframes`, `add_spriteframes_animation`, `add_spriteframes_frame` |
| Undo grouping | `create_editor_undo_redo_action`, `add_editor_undo_redo_do`, `add_editor_undo_redo_undo`, `commit_editor_undo_redo` |

### Run and debug

| Task | Tools |
|---|---|
| Launch / stop | `play_editor_current_scene`, `stop_editor_playing` |
| Game channel status | `get_game_status` |
| Code in the game | `execute_game_script`, `reload_game_scripts` |
| Input into the game | `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `get_game_input_status` - editor-side input tools do not reach the game (see godot-autopilot-runtime) |
| Game screenshots / UI | `capture_game_viewport`, `get_game_ui_elements` |
| Logs (four sources) | `get_debugger_log` (editor engine log, works without a game), `get_debugger_errors` and `get_debugger_output` (runtime channel), `get_game_log_entries` (on-disk log, works without a session), `get_plugin_log` (plugin-internal diagnostics: authorization denials, timeout bookkeeping, dropped late game responses) |
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

## Game development workflow

The routing tables cover single calls; real work is a loop. `ok` means the call went through, not that the effect is what you wanted, so every iteration is change, observe, diagnose, fix, verify - and a fix is confirmed by the error watermark (see "Confirming a fix with the error watermark"), not by assumption:

1. Change the project through tools (`property_set`, `create_script`, `write_file`, and so on), or orchestrate several through `batch_execute` / `code_execute`.
2. Observe the result with a screenshot (`capture_editor_viewport`, `capture_game_viewport`), a read-back (`property_get`, `get_scene_tree`, `get_resource_property`) or output (`get_debugger_log`; with a running game `get_debugger_errors` and `get_debugger_output`, disk fallback `get_game_log_entries`).
3. Diagnose from what you observed, fix, and re-observe until the observation shows the intended effect.

references/development-workflow.md expands this into complete loops for UI and scene appearance, script logic, resources and imports, and runtime debugging with input injection - including how screenshots arrive as image content through `call_tool`.

## batch_execute versus code_execute

- `batch_execute` orchestrates existing tools: an ordered `operations` list, run in sequence, at most 256 items, with `stop_on_error` (default true) and optional `rollback_on_error`. Choose it for deterministic sequences such as "set these five properties, then save".
- `code_execute` runs GDScript in the editor: the source is wrapped in a script extending Node, and the edited scene root is exposed as SceneRoot. Default mode inlines your code inside a single `_run()` function - top-level func definitions are unsupported (pass `function_name` for multi-function mode). `timeout_ms` defaults to 5000 with a 30000 maximum; timeout is checked before execution, not used to interrupt. `auto_owner` defaults to true so created nodes are saved with the scene.
- Decision rule: expressible as a handful of existing tool calls - use `batch_execute`. Needs loops, math or conditions (laying out hundreds of tiles, bulk renames, computed values) - use `code_execute`. Very large payloads also favor `code_execute` to avoid huge JSON arguments.
- Both are meta tools: call them directly at the MCP layer, not through `call_tool`.
- Authorization: `code_execute` and `execute_script` are denied by default. Enable them either by setting the `GODOT_AUTOPILOT_ALLOW` environment variable to `code_execute` (or `all`) and restarting the engine, or by ticking "Allow code_execute" in the plugin's MCP Config dock, which saves to the plugin config and takes effect on the next tool call - no restart needed. When `GODOT_AUTOPILOT_ALLOW` is set it wins over the config, so do not rely on the dock checkbox while the variable is present. The game runtime tools need the `game_runtime` capability, enabled the same way via the environment variable or the `allow` key in the plugin config (the dock checkbox manages code_execute only).

## Error protocol

Every tool failure is returned as a JSON object with an `error` key. For `call_tool` and `code_execute`, a top-level `error` also sets the MCP-level error flag on the response.

In addition, every meta-tool response carries a top-level `new_errors_since_last_call` integer - the error watermark:

- It counts new errors recorded since your previous call, including errors the call itself just produced. A failed call reports at least its own error.
- It is one-shot: the value is consumed when you read it and the counter resets. Never skip reading it, even when the call succeeded.
- Errors printed by a running game are recorded too.

Soft (retryable) errors: while the editor is importing or scanning resources, affected calls return a structured error with `retryable` set to true and a suggested `retry_after_ms`:

```json
{"error": "editor is currently importing/scanning resources; retry shortly", "retryable": true, "retry_after_ms": 500}
```

Wait at least `retry_after_ms`, then retry the same call.

Export lockout: while the editor is exporting the project, all tool calls are rejected with:

```json
{"error": "editor is exporting; retry after export completes"}
```

Wait for the export to finish instead of retrying in a tight loop.

## Confirming a fix with the error watermark

Use the watermark as the closing step of every fix instead of assuming a fix worked:

1. A call fails, or the watermark reports new errors.
2. Diagnose and fix the cause.
3. Re-run the failing call. A success with `new_errors_since_last_call` at 0 confirms the fix produced no new errors.
4. A non-zero value means new errors were recorded since your previous call - errors printed by the running game count too. Read them with `get_debugger_errors` or `get_debugger_log`, fix again, then make any meta-tool call (even `ping`) until the watermark comes back 0.

## Tool gotchas in brief

The behaviors below bite most often; the full catalogue lives in references/tool-gotchas.md.

- `set_tilemap_cells` has no fixed per-call entry cap, but oversized JSON arguments can hit client-side request limits and fail as a parse error before the tool runs. Keep batches reasonably sized and use a `code_execute` loop for bulk layouts. An entry with `source_id` -1 clears the cell at those coordinates.
- Never assign memory:// resources to node properties: `property_set` rejects them, because writing one into the scene file would corrupt it. Save with `save_resource` first, then assign the `res://` path.
- Non-numeric strings into int properties become 0: `property_set` returns ok and stores 0. After any property write you are unsure about, read it back with `property_get` and assert the expected value.
- Where the built-in prompt text and the implementation disagree, the implementation wins: trust `get_tool_detail` over prompt-quoted tool counts, keycodes or serialization claims.
- `code_execute` has four traps: single-function mode is the default (top-level func definitions are rejected), `close_scene` on the editor interface is refused outright (use `close_editor_scene`), mixed tabs and spaces are rejected, and `SceneRoot` node paths carry no root-name prefix.

## Limits

- Default operation timeout is 5000 ms; the maximum accepted timeout is 30000 ms.
- JSON responses are capped at 4 MiB. Large results are truncated with detectable fields (such as a `truncated` flag) rather than silently dropped - check them before trusting completeness.
- `batch_execute` accepts at most 256 operations per call.
- The full size-limit table lives in references/tool-gotchas.md.

## If MCP tools are missing

If your MCP client shows an empty tool list or calls fail right after the editor restarted, the server is usually fine and the client harness simply did not reconnect. See references/http-fallback.md to keep working by sending JSON-RPC over HTTP directly.

## See also

- godot-autopilot-scene-system - scene lifecycle, nodes, properties, signals
- godot-autopilot-resources - resource and file operations, rename and move
- godot-autopilot-scripting - GDScript execution channels
- godot-autopilot-runtime - running the game, input injection, debugging
- godot-autopilot-servers - RenderingServer, PhysicsServer and NavigationServer RID tools
- godot-autopilot-content - tilemap, animation, audio, UI theming
