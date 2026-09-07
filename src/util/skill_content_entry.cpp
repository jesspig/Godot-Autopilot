#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kUsageDescription =
    R"gda_skill(Entry point for using the godot-autopilot MCP server in a Godot project: connection prerequisites, tool discovery protocol (search first, never guess tool names), and the error protocol (new_errors_since_last_call watermark, retryable soft errors, export lockout). Use whenever working on a Godot project through the godot-autopilot MCP tools.)gda_skill";

const char *kUsageBody = R"gda_skill(# Godot Autopilot Usage

Entry point for working on a Godot project through the godot-autopilot MCP server. Read this first: it explains how to connect, how to discover tools (never guess names), and how to read the error protocol every response follows.

## Prerequisites

- The Godot editor must be open with the godot-autopilot plugin enabled. The MCP server starts automatically with the plugin and stops when the plugin is disabled or the editor closes.
- The MCP endpoint is `http://127.0.0.1:9527/mcp` by default, but the port is configurable. Never assume 9527; see godot-autopilot-direct-http for the four-level port discovery order.
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

The 363 domain tools (scene, property, resource, script, physics, render, audio, and so on) plus `system_status` are not registered as MCP tools directly. Call every domain tool through `call_tool`, passing the domain tool name and its arguments object.

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

## Batching with batch_execute

`batch_execute` orchestrates tools that already exist, in order:

- `operations` is an ordered array of `{"tool": ..., "args": {...}}` items, at most 256 per call.
- `stop_on_error` defaults to true: the first failing operation skips the rest.
- `rollback_on_error` (default false) undoes the editor global undo history changes made by the batch on failure; the response then reports how many operations were rolled back and whether that rollback was only partial.
- The response lists per-operation `results` plus `succeeded` and `failed` counts.

Use `batch_execute` for deterministic sequences of existing tools. Use `code_execute` when you need loops or computation (see godot-autopilot-tool-map for the decision guide).

## Error protocol

Every tool failure is returned as a JSON object with an `error` key. For `call_tool` and `code_execute`, a top-level `error` also sets the MCP-level error flag on the response.

In addition, every meta-tool response carries a top-level `new_errors_since_last_call` integer - the error watermark:

- It counts new errors recorded since your previous call, including errors the call itself just produced. A failed call reports at least its own error.
- It is one-shot: the value is consumed when you read it and the counter resets. Never skip reading it, even when the call succeeded.
- Errors printed by a running game are recorded too, so after fixing a problem, make any meta-tool call and confirm the watermark comes back 0.

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

## Limits

- Default operation timeout is 5000 ms; the maximum accepted timeout is 30000 ms.
- JSON responses are capped at 4 MiB. Large results are truncated with detectable fields (such as a `truncated` flag) rather than silently dropped - check them before trusting completeness.
- `batch_execute` accepts at most 256 operations per call.

## If MCP tools are missing

If your MCP client shows an empty tool list or calls fail right after the editor restarted, the server is usually fine and the client harness simply did not reconnect. See godot-autopilot-direct-http to keep working by sending JSON-RPC over HTTP directly.

## See also

- godot-autopilot-direct-http
- godot-autopilot-tool-map
)gda_skill";

const char *kDirectHttpDescription =
    R"gda_skill(Direct HTTP fallback for the godot-autopilot MCP server: when the MCP client lost its tools after an editor restart (empty tool list, failed calls, reconnect failed), send JSON-RPC tools/call straight to http://127.0.0.1:<port>/mcp using curl (bash) or PowerShell. Always discover the actual port first from MCP config files - never assume the default 9527.)gda_skill";

const char *kDirectHttpBody = R"gda_skill(# Direct HTTP Access to the Godot Autopilot MCP Server

Fallback protocol for talking to the godot-autopilot MCP server with plain HTTP requests (curl or PowerShell) when the normal MCP client channel is broken. The server itself is almost always fine.

## When to use this

- Your MCP client lost its tools after an editor restart: the tool list is empty, calls fail, or reconnecting did not help.
- The server starts automatically with the editor plugin, so it is typically still online - the break is usually in the client harness, not the server.
- You can bypass the harness by POSTing JSON-RPC straight to the server endpoint.

## Step 0 - Discover the actual port

Never assume the default 9527. The port is configurable and any of these sources can override it. Check in this order:

| Order | Where to look | What to read |
|---|---|---|
| 1 | MCP client config files in the project root: .mcp.json, .cursor/mcp.json, .codex/config.toml, opencode.json and similar | The URL the panel's Generate button wrote, of the form http://127.0.0.1:\<port\>/mcp - this is the actual address |
| 2 | Plugin config in the Godot user data folder (Windows): %APPDATA%\Godot\app_userdata\<ProjectName>\godot_autopilot\config.json | The "port" key; \<ProjectName\> is the config/name value from project.godot |
| 3 | Environment variable | GODOT_AUTOPILOT_PORT |
| 4 | Fallback | 9527 |

Whichever source you use, the endpoint path is always /mcp on 127.0.0.1.

## Protocol - two mandatory steps

All requests are POSTs to http://127.0.0.1:\<port\>/mcp with these headers:

- Content-Type: application/json
- Accept: application/json, text/event-stream
- No Authorization header and no session id header - the server is stateless.
- The Host must be localhost (127.0.0.1); the server rejects other Host values.

Step 1, one-time handshake: send a JSON-RPC notification (no id):

```json
{"jsonrpc":"2.0","method":"notifications/initialized"}
```

The server answers 202 with an empty object. Skipping this step is the most common failure: every later tools/call returns 400 "Server not initialized". Repeat step 1 once per editor session if the editor was restarted meanwhile; sending it again is harmless.

Step 2: call a tool. The JSON-RPC `id` is required and must not be null. Domain tools are proxied through `call_tool`:

```json
{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}
```

## curl example (bash)

```bash
URL="http://127.0.0.1:9527/mcp"  # discover the real port first!
# Step 1: one-time handshake (required, returns 202)
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","method":"notifications/initialized"}'
# Step 2: call a domain tool through call_tool
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}'
```

`ping` is the safest first call. Creating a node works the same way - only the inner arguments change (omit `parent_path` only while the edited scene has no root):

```bash
curl -s -X POST "$URL" -H "Content-Type: application/json" \
  -H "Accept: application/json, text/event-stream" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"create_scene_node","arguments":{"type":"Node2D","name":"Player"}}}}'
```

## PowerShell example

```powershell
$Url = "http://127.0.0.1:9527/mcp"   # discover the real port first!
$accept = "application/json, text/event-stream"
# Step 1: one-time handshake (required, returns 202)
Invoke-WebRequest -Method Post -Uri $Url -ContentType "application/json" -Headers @{ Accept = $accept } -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}'
# Step 2: call a domain tool through call_tool
$resp = Invoke-WebRequest -Method Post -Uri $Url -ContentType "application/json" -Headers @{ Accept = $accept } -Body '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"call_tool","arguments":{"name":"ping","arguments":{}}}}'
# The response body is an SSE frame: keep the first "data:" line and parse it.
$line = ($resp.Content -split "`n" | Where-Object { $_ -like 'data:*' } | Select-Object -First 1)
$json = ($line.Substring(5) | ConvertFrom-Json)
$inner = ($json.result.content[0].text | ConvertFrom-Json)
$inner.result                        # -> "pong"
$inner.new_errors_since_last_call    # error watermark, see godot-autopilot-usage
```

PowerShell single-quoted strings need no JSON double-quote escaping. If your PowerShell version renders Invoke-WebRequest output differently, cross-check against the curl example above.

## Reading the response

A successful call returns HTTP 200 with an SSE frame: lines of `event: message` followed by `data: <JSON>`. To get the business payload:

1. Take the line starting with `data:` and strip the prefix (in bash: pipe through `sed -n 's/^data: *//p'`).
2. Parse the remaining JSON - the JSON-RPC envelope. The tool result lives at result.content[0].text.
3. That text field is itself a JSON string; parse it again (inner layer).
4. The business payload is the inner "result" key; an inner "error" key means the tool call failed, and an outer isError of true marks the same thing.
5. Read the inner top-level `new_errors_since_last_call` - same one-shot semantics as on the MCP channel (see godot-autopilot-usage).

Full shape of a `ping` response (whitespace added for readability):

```json
{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text",
"text":"{\"new_errors_since_last_call\":0,\"result\":\"pong\"}"}],"isError":false}}
```

## Troubleshooting

| HTTP status | Meaning | Fix |
|---|---|---|
| 400 "Server not initialized" | Step 1 handshake missing | Send the notifications/initialized notification first |
| 400 "tool not found: ..." or "domain tool '...' not found" | Wrong tool name in tools/call or call_tool | Discover exact names with search_tools |
| 403 | Host header missing or not localhost | Keep Host as 127.0.0.1 / localhost |
| 404 | GET request used | The endpoint only accepts POST |
| 413 | Request body over 4 MiB | Shrink the payload (split into batches) |
| 503 | More than 8 concurrent requests, or server not running | Is the editor open with the plugin enabled? Reduce parallel calls |
| 504 | Processing exceeded 30 seconds | Split the work into smaller calls |

Tool-level failures (bad arguments, missing nodes) are not HTTP errors - they arrive as HTTP 200 with an "error" key in the inner JSON.

## Constraints

- Loopback only, no authentication: never expose the port to the network and never use this against a remote machine.
- At most 8 concurrent in-flight requests.
- The handshake of step 1 is per editor session; redo it after the editor restarts.

## See also

- godot-autopilot-usage
- godot-autopilot-tool-map
)gda_skill";

const char *kToolMapDescription =
    R"gda_skill(Task-to-tool routing table for the godot-autopilot MCP server: which domain tools fit reading, building, modifying, running or debugging a Godot project, and when to prefer batch_execute vs code_execute. Use when you know the task but not which of the 363 tools to call.)gda_skill";

const char *kToolMapBody = R"gda_skill(# Godot Autopilot Tool Map

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
)gda_skill";

const char *kCategoryIndexBody = R"gda_skill(# Category Index

All 363 domain tools of the godot-autopilot MCP server, grouped by their 30 source modules. Call domain tools through `call_tool`; confirm schemas with `get_tool_detail`.

## Analysis - analyze_tools (3)

- `validate_scene_file` - dry-run validate a scene file on disk without touching the edited scene
- `find_unused_resources` - find project files never referenced by any other file's dependencies
- `trace_signal_flow` - trace signal wiring around a node of the edited scene

## Animation - animation_tools (10)

- `create_scene_animation_player` - create an AnimationPlayer node in the edited scene
- `create_animation` - create an Animation resource in a player's default library
- `remove_animation` - remove a named animation from an AnimationPlayer
- `get_animation_list` - list a player's animations with length and loop mode
- `create_animation_track` - add a value or method track to an animation
- `insert_animation_keyframe` - insert one key into a track at a given time
- `remove_animation_track` - remove a whole track from an animation
- `create_scene_animation_tree` - create an AnimationTree with an empty state machine root
- `add_animation_machine_state` - append a state to the AnimationTree state machine
- `connect_animation_states` - connect two states, optionally with a transition condition

## Audio - audio_tools (20)

- `get_audio_bus_layout` - read the complete audio bus layout including effect chains
- `set_audio_bus_layout` - replace the whole audio bus layout
- `get_audio_bus_count` - count audio buses including Master
- `get_audio_bus_name` - read a bus name by zero-based index
- `set_audio_bus_volume_db` - set a bus volume in decibels
- `set_audio_bus_mute` - mute or unmute a bus
- `set_audio_bus_bypass_effects` - bypass a bus's whole effect chain without removing it
- `add_audio_bus_effect` - add an audio effect to a bus by class name
- `remove_audio_bus_effect` - remove a bus effect by index
- `set_audio_bus_solo` - solo a bus so only it is audible
- `play_audio_player` - play a stream on an audio player node
- `stop_audio_player` - stop an audio player node
- `set_audio_player_volume_db` - set a player's volume in decibels
- `set_audio_player_pitch_scale` - set a player's pitch scale
- `get_audio_player_playback_position` - read a player's playback position in seconds
- `seek_audio_player` - seek a player to a position in seconds
- `get_audio_device_outputs` - list available audio output devices
- `set_audio_device_output` - switch the editor's audio output device
- `get_audio_device_inputs` - list available audio input devices
- `set_audio_device_input` - switch the editor's audio input device

## Capture - capture_tools (1)

- `capture_editor_viewport` - screenshot the editor viewport (or the running game with target=game) as base64 PNG

## Config - config_tools (13)

- `get_project_settings` - read one project.godot setting
- `set_project_settings` - set a project setting in memory only (call save afterwards)
- `has_project_settings` - check whether a project setting exists
- `save_project_settings` - persist in-memory project settings to project.godot
- `get_engine_version` - read the running engine version
- `get_engine_fps` - measure the current frames per second
- `get_engine_frames_drawn` - read the total frames-drawn counter
- `set_engine_time_scale` - scale the game loop speed (slow motion or fast forward)
- `get_engine_time_scale` - read the current time scale
- `set_engine_max_fps` - cap the frame rate (0 disables the cap)
- `get_editor_settings` - read one editor preference
- `set_editor_settings` - set an editor preference (persists automatically)
- `has_editor_settings` - check whether an editor preference exists

## Debug - debug_tools (15)

- `print_debug_log` - log a message through the plugin log system
- `get_debug_stack` - capture script backtraces in the editor process
- `get_debug_monitor` - read one built-in performance monitor by id
- `get_debug_monitor_catalog` - list built-in performance monitors with ids
- `get_debug_monitors` - read all built-in monitors in one snapshot
- `get_debug_custom_monitor` - read one custom performance monitor by id
- `get_debug_custom_monitor_names` - list registered custom monitor ids
- `remove_debug_custom_monitor` - remove a custom monitor by id
- `get_debug_object_count` - read the live Object count
- `get_debug_node_count` - read the live Node count
- `get_debug_memory_usage` - read current static memory usage in bytes
- `set_debug_physics_fps` - set the physics ticks-per-second rate
- `set_debug_collision_visual` - toggle collision shape visualization
- `set_debug_navigation_visual` - toggle navigation geometry visualization
- `set_debug_performance_visual` - toggle the performance overlay

## Debugger - debugger_tools (5)

- `get_debugger_log` - read the editor engine log buffer (works without a running game)
- `get_debugger_errors` - read script errors captured from the running game
- `get_debugger_output` - read game stdout/stderr over the runtime channel
- `get_debugger_scene_tree` - read the running game's scene tree as text
- `get_debugger_session_info` - read debug session state (active or stopped at a breakpoint)

## Display - display_tools (24)

- `get_display_clipboard` - read the system clipboard text
- `set_display_clipboard` - write text to the system clipboard
- `show_display_dialog` - show a native modal dialog (user-visible side effect)
- `get_display_mouse_position` - read the mouse cursor position in screen coordinates
- `set_display_mouse_mode` - set cursor behavior (visible, captured, hidden)
- `warp_display_mouse` - move the mouse cursor to a screen position
- `capture_display_screen` - screenshot a physical screen as PNG
- `get_display_screen_count` - count connected screens
- `get_display_screen_dpi` - read a screen's DPI
- `get_display_screen_position` - read a screen's desktop position
- `get_display_screen_refresh_rate` - read a screen's refresh rate in hertz
- `get_display_screen_size` - read a screen's pixel size
- `get_display_tts_voices` - list available text-to-speech voices
- `speak_display_tts` - speak text aloud via text-to-speech
- `stop_display_tts` - stop ongoing text-to-speech playback
- `create_display_window` - create an editor sub-window and return its id
- `delete_display_window` - close a previously created sub-window
- `move_display_window_to_foreground` - raise a window and give it focus
- `request_display_window_attention` - flash a window's taskbar entry
- `set_display_window_flag` - set a window flag such as always-on-top
- `set_display_window_mode` - set a window mode such as fullscreen or maximized
- `set_display_window_position` - move a window to screen coordinates
- `set_display_window_size` - resize a window in pixels
- `set_display_window_title` - set a window's title bar text

## Docs - doc_tools (4)

- `get_docs_class` - reflect a Godot class signature from ClassDB (no docstrings)
- `find_docs_class` - substring search over registered class names
- `get_docs_method` - look up one method's reflected signature on a class
- `get_docs_property` - look up one property's reflected info on a class

## Editor - editor_tools (23)

- `get_editor_selection` - list the currently selected nodes
- `set_editor_selection` - replace the editor selection with given paths
- `get_editor_edited_scene_root` - fetch the edited scene's root node info
- `save_editor_scene` - save the currently edited scene to disk
- `save_editor_scenes` - save every open scene tab at once
- `save_editor_scene_as` - save the current scene to a specific path
- `reload_editor_scene` - reload a scene from disk, discarding unsaved edits
- `close_editor_scene` - close the currently edited scene
- `create_editor_scene` - create a new empty scene with a root node of a given type
- `open_editor_scene` - open an existing scene file in the editor
- `inspect_editor_resource` - open a resource file in the Inspector panel
- `create_editor_undo_redo_action` - start a custom undo/redo action
- `add_editor_undo_redo_do` - register a do-step method call in the open action
- `add_editor_undo_redo_undo` - register an undo-step method call in the open action
- `commit_editor_undo_redo` - commit the open action as a single undo step
- `get_editor_file_system_tree` - fetch the project file system directory tree
- `get_editor_file_system_status` - report file system scan state and progress
- `scan_editor_file_system` - trigger a full file system rescan
- `set_editor_main_scene` - set the project's main scene
- `set_editor_plugin_enabled` - enable or disable an editor plugin
- `play_editor_current_scene` - launch the edited scene as a game
- `stop_editor_playing` - stop the running game process
- `build_csharp_assembly` - trigger an async dotnet build of the C# project

## Game - game_tools (9)

- `get_game_status` - query the running game's engine stats over the runtime channel
- `execute_game_script` - run code inside the running game process
- `reload_game_scripts` - reload GDScripts in the running game without restarting it
- `queue_game_input` - inject input into the running game (the editor-side input tools do not reach it)
- `wait_game_input` - wait for a transient input state on an action in the game
- `get_game_input_status` - query pressed and just-pressed state in the game
- `sequence_game_inputs` - schedule up to 256 input events on exact physics frame offsets
- `capture_game_viewport` - screenshot the running game as base64 PNG
- `get_game_ui_elements` - enumerate Control nodes of the running game for click automation

## Group - group_tools (3)

- `add_group_node` - add a node to a group persistently (saved with the scene, undo-tracked)
- `remove_group_node` - remove a node from a group (undo-tracked)
- `has_group_node` - check whether a node belongs to a group

## Input - input_tools (11)

- `press_input_action` - press an input action in the editor process
- `release_input_action` - release a previously pressed input action
- `is_input_action_pressed` - check whether an action is currently held in the editor
- `is_input_action_just_pressed` - check the single-frame just-pressed state in the editor
- `press_input_key` - inject a key press into the editor (restricted key set)
- `release_input_key` - inject a key release into the editor
- `move_input_mouse` - inject mouse motion into the editor process
- `press_input_mouse_button` - inject a left, right or middle mouse press into the editor
- `release_input_mouse_button` - inject a mouse button release into the editor
- `start_input_gamepad_vibration` - start gamepad vibration on the editor process
- `stop_input_gamepad_vibration` - stop gamepad vibration

## Input - input_map_tools (8)

- `get_input_map_actions` - list all input action names of the editor project
- `has_input_map_action` - check whether an input action exists
- `add_input_map_action` - add an empty input action (deadzone defaults to 0.5)
- `add_input_map_action_event` - bind an input event object to an action
- `erase_input_map_action_event` - remove one bound event from an action by index
- `set_input_map_action_deadzone` - set an action's analog deadzone
- `erase_input_map_action` - remove an action and its persisted setting
- `save_input_map` - persist the editor InputMap to project settings

## Navigation - nav_tools (15)

- `create_nav_2d_map` - create a 2D navigation map and return its RID
- `create_nav_2d_region` - create a 2D navigation region on a map
- `get_nav_2d_map_path` - query a navigation path across a 2D map
- `create_nav_2d_agent` - create a 2D avoidance agent on a map
- `set_nav_2d_agent_velocity` - drive a 2D agent's velocity each frame
- `create_nav_3d_map` - create a 3D navigation map and return its RID
- `set_nav_3d_map_cell_size` - set a 3D map's cell size
- `create_nav_3d_region` - create a 3D navigation region on a map
- `set_nav_3d_region_navigation_mesh` - assign a NavigationMesh resource to a 3D region
- `get_nav_3d_map_path` - query a navigation path across a 3D map
- `get_nav_3d_map_closest_point_to_segment` - find the closest map point to a segment
- `create_nav_3d_agent` - create a 3D avoidance agent on a map
- `set_nav_3d_agent_velocity` - drive a 3D agent's velocity each frame
- `get_nav_3d_agent_state` - read a 3D agent's position and velocity
- `create_nav_3d_obstacle` - create a 3D obstacle avoidance agents steer around

## OS - os_tools (18)

- `show_os_alert` - show a blocking native alert dialog (blocks the editor UI)
- `create_os_process` - start a detached background process and return its PID
- `execute_os_process` - run a command synchronously and capture stdout/stderr
- `kill_os_process` - force-kill a process by PID
- `open_os_path` - open a path or URL with the default application
- `get_os_datetime` - read current date and time fields
- `get_os_unix_time` - read Unix time as floating-point seconds
- `get_os_locale` - read the system locale string
- `get_os_system_fonts` - list installed system font names
- `get_os_system_info` - read host machine and OS information
- `get_os_unique_id` - read the machine unique identifier
- `get_os_user_data_dir` - read the project's user data directory path
- `get_os_environment` - read an editor process environment variable
- `set_os_environment` - set an editor process environment variable
- `write_file` - write or append text to a file under res:// or user://
- `read_file` - read a text file from disk
- `find_in_files` - recursively search text files for a query string
- `move_os_file_to_trash` - move a file or folder to the system trash

## Physics - physics_tools (48)

- `get_physics_2d_space_direct_state` - read a 2D physics space's direct state before queries
- `intersect_physics_2d_ray` - cast a 2D ray and return the first hit (space auto-detected from the edited scene)
- `intersect_physics_2d_shape` - intersect a 2D space with a shape RID
- `intersect_physics_2d_point` - query colliders touching a 2D point
- `create_physics_2d_body` - create a server-side 2D body and return its RID
- `set_physics_2d_body_mode` - set a 2D body's simulation mode
- `set_physics_2d_body_state` - set a 2D body state field such as transform or velocity
- `get_physics_2d_body_state` - read a 2D body state field
- `apply_physics_2d_body_force` - apply a continuous force to a 2D body
- `apply_physics_2d_body_impulse` - apply an instantaneous impulse to a 2D body
- `create_physics_2d_joint` - create a 2D joint (pin, groove or damped spring)
- `create_physics_2d_area` - create a 2D physics area and return its RID
- `set_physics_2d_area_monitorable` - set whether a 2D area can be monitored
- `create_physics_2d_circle_shape` - create a 2D circle shape and return its RID
- `set_physics_2d_shape_data` - set a 2D circle shape's geometry
- `get_physics_3d_space_direct_state` - read a 3D physics space's direct state before queries
- `intersect_physics_3d_ray` - cast a 3D ray and return the first hit (space RID required)
- `intersect_physics_3d_shape` - intersect a 3D space with a shape RID
- `intersect_physics_3d_point` - query colliders touching a 3D point
- `create_physics_3d_body` - create a server-side 3D body and return its RID
- `set_physics_3d_body_mode` - set a 3D body's simulation mode
- `set_physics_3d_body_state` - set a 3D body state field
- `get_physics_3d_body_state` - read a 3D body state field
- `set_physics_3d_body_transform` - teleport a 3D body to a transform
- `apply_physics_3d_body_force` - apply a continuous force to a 3D body
- `apply_physics_3d_body_impulse` - apply an instantaneous impulse to a 3D body
- `apply_physics_3d_body_torque` - apply rotational torque to a 3D body
- `set_physics_3d_body_axis_lock` - lock a movement axis on a 3D body
- `set_physics_3d_body_param` - set a 3D body parameter such as mass by enum index
- `add_physics_3d_body_shape` - attach a shape RID to a 3D body
- `add_physics_3d_body_collision_exception` - stop a 3D body colliding with another
- `remove_physics_3d_body_collision_exception` - re-enable collision between two bodies
- `create_physics_3d_joint` - create a 3D joint (pin, hinge, slider, cone twist or 6-DOF)
- `set_physics_3d_joint_param` - set solver parameters on a 3D joint
- `create_physics_3d_area` - create a 3D physics area and return its RID
- `set_physics_3d_area_monitorable` - set whether a 3D area can be monitored
- `set_physics_3d_area_param` - set a 3D area parameter such as gravity by enum index
- `set_physics_3d_area_space` - attach a 3D area to a physics space
- `set_physics_3d_area_transform` - place a 3D area with a global transform
- `set_physics_3d_space_param` - set a 3D space parameter by enum index
- `set_physics_3d_space_solver_iterations` - set a 3D space's solver iteration count
- `set_physics_3d_space_solver_params` - set solver iterations and penetration limits together
- `create_physics_3d_soft_body` - create a 3D soft body and return its RID
- `set_physics_3d_soft_body_mesh` - assign a mesh to a 3D soft body
- `create_physics_3d_sphere_shape` - create a sphere shape and return its RID
- `set_physics_3d_shape_data` - set a sphere shape's geometry
- `get_physics_node_rid` - get the RID of a scene physics node for server-side tools
- `get_debug_object_info` - resolve an ObjectID to its class and identity

## Properties - property_tools (5)

- `property_get` - read one property value from a scene node
- `property_get_list` - list every property of a node with metadata
- `property_set` - set a node property (editor undo, errors carry candidate suggestions)
- `signal_connect` - connect a signal to a target method (persisted with the scene by default)
- `signal_disconnect` - remove a signal connection (idempotent)

## Render - render_tools (49)

- `create_render_canvas_item` - create a server-side 2D canvas item and return its RID
- `add_render_canvas_item_rect` - draw a filled rectangle on a canvas item
- `add_render_canvas_item_circle` - draw a filled circle on a canvas item
- `add_render_canvas_item_texture_rect` - draw a texture into a rectangle on a canvas item
- `add_render_canvas_item_line` - draw a line on a canvas item
- `set_render_canvas_item_transform` - set a canvas item's 2D transform
- `set_render_canvas_item_visible` - set a canvas item's visibility
- `get_render_canvas_item_rid` - get the canvas item RID of a CanvasItem node
- `create_render_scenario` - create a server-side 3D scenario and return its RID
- `set_render_scenario_environment` - attach an environment to a 3D scenario
- `create_render_camera` - create a server-side 3D camera and return its RID
- `set_render_camera_transform` - set a 3D camera's position
- `set_render_camera_perspective` - set a perspective projection on a camera
- `set_render_camera_orthogonal` - set an orthogonal projection on a camera
- `create_render_light` - create a directional, omni or spot light and return its RID
- `set_render_light_param` - set a numeric parameter on a light
- `set_render_light_color` - set a light's color
- `create_render_mesh` - create a server-side mesh and return its RID
- `add_render_mesh_surface` - add a vertex-array surface to a mesh
- `set_render_mesh_surface_material` - assign a material to one mesh surface
- `create_render_material` - create a server-side material and return its RID
- `set_render_material_param` - set a named parameter on a material
- `create_render_viewport` - create a server-side viewport and return its RID
- `set_render_viewport_size` - set a viewport's pixel size
- `set_render_viewport_clear_mode` - set how a viewport clears each frame
- `create_render_particles` - create a 2D or 3D particle system and return its RID
- `set_render_particles_emitting` - start or stop particle emission
- `restart_render_particles` - restart particle emission from the beginning
- `set_render_particles_lifetime` - set the particle lifetime in seconds
- `set_render_environment_bg_color` - set an environment's background color
- `set_render_environment_ambient_light` - set an environment's ambient light
- `set_render_environment_glow` - configure glow (bloom) post-processing
- `set_render_environment_ssr` - configure screen-space reflections
- `set_render_environment_tonemap` - configure tone mapping and exposure
- `set_render_environment_sdfgi` - configure SDFGI global illumination
- `set_render_environment_volumetric_fog` - configure volumetric fog
- `create_render_fog_volume` - create a volumetric fog volume and return its RID
- `set_render_fog_volume_shape` - set a fog volume's shape after creation
- `create_render_shader` - create a server-side shader, optionally with source code
- `set_render_shader_code` - replace a shader's source code
- `set_render_shader_parameter_global` - set a global shader parameter
- `create_render_texture_from_image` - create a texture from an image file and return its RID
- `create_render_sky` - create a server-side sky and return its RID
- `set_render_sky_material` - set a sky's material
- `create_render_reflection_probe` - create a reflection probe and return its RID
- `create_render_decal` - create a decal and return its RID
- `set_render_instance_visible` - set a rendering instance's visibility
- `set_render_instance_layer_mask` - set a rendering instance's camera layer mask
- `find_render_node_from_rid` - validate an RID and find scene nodes using it

## Resources - resource_tools (24)

- `load_resource` - load a resource file from disk into memory
- `load_resource_threaded` - start an async background load (step 1 of the threaded chain)
- `get_resource_load_threaded_status` - poll a threaded load's status (step 2)
- `get_resource_load_threaded` - fetch a threaded load's result (step 3)
- `save_resource` - save a resource to disk, including memory:// instances
- `create_resource` - instantiate a Resource subclass in memory, optionally registered as memory://name
- `duplicate_resource` - load and duplicate a file-backed resource
- `get_resource_type` - report the engine class name of the resource at a path
- `get_resource_types` - list every instantiable Resource subclass
- `get_resource_extensions` - list file extensions recognized for a resource type
- `has_resource` - check whether a path is loadable as a resource
- `get_resource_dir_files` - list entries of a res:// or user:// directory
- `get_resource_uid` - read the numeric UID of a resource file
- `set_resource_uid` - assign or auto-generate a UID for a resource file
- `remove_resource_file` - delete a resource file (dry-run scan first, force to actually delete)
- `rename_resource_file` - rename or move a file with automatic reference rewriting
- `move_resource_file` - move a file to another res:// directory through the same transaction pipeline
- `create_directory` - create directories inside the project (res:// only)
- `get_resource_dependencies` - list the external files a resource references
- `has_resource_dependency` - check whether a resource depends on a specific file
- `get_resource_references` - reverse lookup: which files reference a resource
- `reimport_resource_files` - queue an async reimport through the editor file system
- `set_resource_property` - set a property on a located resource
- `get_resource_property` - read a property from a located resource

## Scene - scene_tools (6)

- `create_scene_node` - create a node in the edited scene with optional type, name, parent and inline properties
- `delete_scene_node` - delete a node via editor undo/redo (refuses the scene root)
- `rename_scene_node` - rename a node, refusing duplicate sibling names
- `reparent_node` - move a node under a new parent as one undo step
- `get_scene_tree` - walk the edited scene tree with optional depth and properties
- `instantiate_scene` - instantiate a .tscn PackedScene into the edited scene

## Scene - scene_tree_tools (8)

- `call_scene_tree_group` - call a method on every node in a group
- `notify_scene_tree_group` - send a Godot notification to every node in a group
- `get_scene_tree_nodes_in_group` - list node paths of all nodes in a group
- `create_scene_tree_timer` - create a SceneTreeTimer for delayed logic
- `is_scene_tree_paused` - check whether the running scene tree is paused
- `set_scene_tree_pause` - pause or unpause the running scene tree
- `set_scene_tree_debug_collisions_hint` - toggle collision shape debug visualization
- `reload_scene_tree_current_scene` - reload the currently running scene from disk

## Scripts - script_tools (10)

- `execute_script` - execute GDScript synchronously in the editor process
- `create_script` - create a GDScript file on disk after compile-checking it
- `load_script` - load a GDScript resource from a res:// path
- `reload_script` - reload a script from disk, optionally keeping instance state
- `attach_script_to_node` - attach a GDScript file to a node (undo-tracked)
- `detach_script_from_node` - remove a node's script (undo-tracked)
- `call_script_node` - call a method on a node whose script is marked as a tool script
- `get_script_property` - read a script's declared default or a node's current property value
- `set_script_property` - set a node property without undo entry or validation
- `get_script_property_list` - list the declared variables of a GDScript file

## SpriteFrames - spriteframes_tools (3)

- `create_spriteframes` - create an in-memory SpriteFrames resource (built-in default animation removed)
- `add_spriteframes_animation` - add a named animation to a SpriteFrames resource
- `add_spriteframes_frame` - add frames to an animation, optionally splitting a sprite sheet

## System - system_tools (1)

- `get_game_log_entries` - read the tail of the game's on-disk log file (works without a debug session)

## Testing - test_tools (2)

- `run_gdscript_tests` - run inline GDScript tests with the built-in assertion framework
- `run_gdscript_test_files` - run GDScript test files from a project directory

## Text - text_tools (10)

- `create_text_font` - create a TextServer font and return its id
- `create_shaped_text` - create a shaped text object and return its id
- `set_text_font_data` - load a font file into a font id
- `set_text_font_antialiasing` - set a font's antialiasing mode
- `set_text_font_hinting` - set a font's hinting mode
- `get_text_font_system_path` - resolve a system-installed font to an absolute file path
- `has_text_feature` - check whether the active TextServer supports a shaping feature
- `is_text_locale_right_to_left` - check whether a locale uses right-to-left direction
- `add_shaped_text_string` - add a text string to a shaped text object
- `get_shaped_text_size` - measure a shaped text object's rendered size in pixels

## Theme - theme_tools (8)

- `create_theme_resource` - create an empty .tres theme file on disk
- `set_theme_color` - set a color item on a saved theme
- `set_theme_constant` - set an integer constant item on a saved theme
- `set_theme_font_size` - set a font size item on a saved theme
- `set_theme_stylebox_flat` - build and register a StyleBoxFlat item on a saved theme
- `get_theme_info` - inspect a saved theme's types and items
- `apply_theme_to_control` - assign a .tres theme to a Control node in the edited scene
- `set_control_anchor_preset` - apply one of the 16 Control layout presets

## TileMap - tilemap_tools (4)

- `create_tilemap` - create a TileMap node with a default TileSet of a given tile size
- `create_tilemap_tileset` - create a TileSet resource in memory and register it as memory://name
- `set_tilemap_cell` - set a single cell on a TileMap or TileMapLayer node
- `set_tilemap_cells` - set up to 64 cells in one call (larger batches: use code_execute)

## TileMap - tileset_tools (3)

- `add_tilemap_atlas_source` - add an atlas source with a texture to an in-memory TileSet
- `add_tilemap_physics_layer` - add a physics layer to an in-memory TileSet
- `set_tilemap_tile_collision` - set collision polygons for one tile of a TileSet
)gda_skill";

} // namespace

std::vector<SkillSpec> make_entry_skills() {
  return {
      {"godot-autopilot-usage", kUsageDescription,
       {{"SKILL.md", kUsageBody}}},
      {"godot-autopilot-direct-http", kDirectHttpDescription,
       {{"SKILL.md", kDirectHttpBody}}},
      {"godot-autopilot-tool-map", kToolMapDescription,
       {{"SKILL.md", kToolMapBody},
        {"references/CATEGORY_INDEX.md", kCategoryIndexBody}}},
  };
}

} // namespace godot_autopilot::skill_gen
