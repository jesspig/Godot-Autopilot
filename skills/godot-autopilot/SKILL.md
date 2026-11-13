---
name: godot-autopilot
description: "Entry point for using the GDA (godot-autopilot) plugin in a Godot project: read setup first, connect through gda status --json and mcp_url, discover tools with live gda search/describe/tools list, call them with gda call, observe the editor and game, respect the confirm protocol, and follow the change-observe-verify loop. Use for any godot-autopilot work after installation."
---

# Godot Autopilot

This is the entry point for every session on a Godot project driven through
Godot Autopilot (GDA). Read `godot-autopilot-setup` first on a machine or a
project where GDA is not installed yet. This volume covers the connection, tool
discovery, task routing, observation and the change/observe/verify loop. The
calling protocol (CLI reference, confirm, errors, limits, batch and code
execution) lives in `godot-autopilot-tools`; engine-level operations live in the
seven domain volumes.

## Connection model

- The `gda` CLI is the first-class interface. It discovers the plugin's random
  port, uses IPC when available and falls back to HTTP, and prints JSON.
- The plugin owns the port and persists it. The CLI never chooses a listen port
  and never assumes a default one.
- `gda status --json` is the connection record. `mcp_url` is the only
  authoritative endpoint for an MCP client; `transport` and `ipc_kind` tell you
  which local channel the CLI used.
- The MCP face is the compatibility channel for harnesses that list native
  tools. It exposes the same operations, with full schemas, structured errors
  and image content. Use it when the client already has the tools; use the CLI
  otherwise.
- The server is loopback-only. Never forward, proxy or publish its port.

If no instance is visible, `gda list --all` shows known projects and
`gda start --project <dir>` launches the editor. A stale port is never worth
reusing: re-read `mcp_url` after every editor restart.

## Discover tools, never guess names

Tool names are snake_case and usually verb-first, but segment counts vary and
names change as the toolset grows. Discovery is mandatory:

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"path": "Level", "cells": []}'
```

1. `gda search <query>` searches the live server by name, description, category
   and tags; it returns at most 20 results by default.
2. `gda describe <name>` prints the schema and the `side_effect` marker. Read
   the schema before the first call; never infer parameter names.
3. `gda call <name> --args '<json>'` (or `--args-file <path>`) executes one
   tool against the live editor.
4. `gda tools list` prints every directly callable tool when you need the full
   inventory instead of a keyword search.

Discovery is live-only; there is no offline catalog. Start an
editor with `gda start --project <dir>` when no instance is running.

Search by the object first ("tilemap cell", "animation track", "import
sidecar"), then add a verb only when the result is too broad. Rerun with
synonyms before concluding that a tool is missing: tile/cell, sprite/texture,
log/output, delete/remove/erase, create/add/new. The exact tool set is whatever
discovery returns; never assert a count or a name from memory.

When the native MCP tools are available, the same protocol applies:
`search_tools`, `get_tool_detail`, `call_tool`, plus the meta tools
`batch_execute` and `code_execute`. Discover first; call second.

## What the plugin exposes

The MCP layer has seven meta tools: `ping`, `search_tools`,
`list_categories`, `get_tool_detail`, `call_tool`, `batch_execute` and
`code_execute`. Every domain tool is executed through `call_tool` with its name
and arguments object; the CLI wraps that path as `gda call`.

- `batch_execute` runs an ordered list of existing tool calls (at most 256),
  with `stop_on_error` and optional rollback. Use it for deterministic
  sequences such as "set these five properties, then save".
- `code_execute` runs GDScript in the editor for loops, math and computed
  values. The timeout is enforced while the script runs and is capped by the
  value reported in the schema; split long work rather than assuming a fixed
  ceiling.
- `side_effect` classifies risk: `none`, `writes_file`, `writes_config`,
  `shows_alert`, `modifies_window`, `writes_scene`, `process`,
  `code_execute`, `game_runtime`, `destructive`. High-risk classes require
  explicit confirmation; see below.

## Consult the engine documentation

Never call engine APIs from memory. Property names, method signatures, enum
values and defaults change between Godot versions, and a recalled signature is
a common source of silent failures. Use the documentation family
(`find_docs_class`, `get_docs_class`, `get_docs_method`, `get_docs_property`)
before writing or patching a script that touches an unfamiliar API, setting a
property whose exact name or accepted values are unclear, or chasing an
"unknown property" or parse error. The documentation cache ships with the
running editor, so it matches the exact engine build in use.

## Task routing

Reduce the task to its object and intent, then discover the family. The rows
below name representative tools; confirm the current names and schemas with
`gda search` and `gda describe`.

### Read and inspect

| Task | Representative tools |
|---|---|
| Edited scene tree | `get_scene_tree`, `get_editor_edited_scene_root` |
| Node properties | `property_get`, `property_get_list` |
| Editor selection | `get_editor_selection` |
| Running game tree and UI | `get_debugger_scene_tree`, `get_game_ui_elements` |
| Editor UI elements | `get_editor_ui_elements`, `hit_test_editor_point` |
| Resources on disk | `get_resource_dir_files`, `get_resource_type`, `has_resource`, `get_resource_dependencies`, `get_resource_references` |
| Project file system | `get_editor_file_system_tree`, `get_editor_file_system_status` |
| Project and editor settings | `get_project_settings`, `has_project_settings`, `get_editor_settings` |
| API reflection | `get_docs_class`, `find_docs_class`, `get_docs_method`, `get_docs_property` |
| Visual state | `capture_editor_viewport`, `capture_game_viewport` |

### Build and modify

| Task | Representative tools |
|---|---|
| Nodes | `create_scene_node`, `rename_scene_node`, `reparent_node`, `delete_scene_node`, `instantiate_scene`, `add_group_node` |
| Properties | `property_set` (single and batch forms) |
| Signals | `signal_connect`, `signal_disconnect`, `trace_signal_flow` |
| Resources | `create_resource`, `load_resource`, `save_resource`, `duplicate_resource`, `set_resource_property` |
| Files | `rename_resource_file`, `move_resource_file`, `write_file`, `read_file`, `find_in_files` |
| Scripts | `create_script`, `attach_script_to_node`, `call_script_node`, `reload_script` |
| Theme and layout | `create_theme_resource`, `set_theme_color`, `apply_theme_to_control`, `set_control_anchor_preset` |
| Tilemap | `create_tilemap`, `create_tilemap_tileset`, `set_tilemap_cell`, `set_tilemap_cells` |
| Animation | `create_animation`, `create_animation_track`, `insert_animation_keyframe`, `create_scene_animation_tree` |
| Editor UI automation | `click_editor_element`, `type_editor_element_text`, `run_editor_shortcut`; raw-coordinate fallbacks `click_input_mouse`, `type_input_text` |

### Run and debug

| Task | Representative tools |
|---|---|
| Launch and stop | `play_editor_current_scene`, `stop_editor_playing` |
| Game channel status | `get_game_status` |
| Code in the game | `execute_game_script`, `reload_game_scripts` |
| Input into the game | `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `get_game_input_status` |
| Game screenshots and UI | `capture_game_viewport`, `get_game_ui_elements` |
| Logs | `get_debugger_log`, `get_debugger_errors`, `get_debugger_output`, `get_game_log_entries`, `get_plugin_log` |
| Performance | `get_debug_monitor_catalog`, `get_debug_monitors`, `get_debug_memory_usage`, `get_debug_object_count` |
| Pause and reload | `set_scene_tree_pause`, `reload_scene_tree_current_scene` |
| Inline tests | `run_gdscript_tests`, `run_gdscript_test_files` |
| Project analysis | `validate_scene_file`, `find_unused_resources` |

### Project administration

| Task | Representative tools |
|---|---|
| Project settings | `get_project_settings`, `set_project_settings`, `save_project_settings`; set is memory-only until saved |
| Editor settings | `get_editor_settings`, `set_editor_settings` |
| InputMap | `get_input_map_actions`, `add_input_map_action`, `add_input_map_action_event`, `save_input_map` |
| Main scene | `set_editor_main_scene` |
| Scene files | `create_editor_scene`, `open_editor_scene`, `save_editor_scene`, `save_editor_scene_as`, `close_editor_scene` |
| File system refresh | `scan_editor_file_system` |

## Confirmation and safety

High-risk side effects return a structured `confirmation_required` error
instead of executing. The classes that require confirmation are `destructive`,
`process`, `code_execute` and `game_runtime`. Review the tool description and
the exact target, then retry:

```bash
gda call code_execute --args '{"source_code": "return 1"}' --confirm
```

With native MCP tools, pass `confirm: true` in the arguments object. File and
scene writes (`writes_file`, `writes_config`, `writes_scene`) do not require
confirmation, but they still change project content: read the target back after
every write. Never work around a confirmation error by switching to a different
tool with the same effect.

MCP clients see tool annotations and the `side_effect` marker. Treat
`destructive` and `game_runtime` annotations as a stop-and-review signal, not
as a formality.

## Watch the engine state

Never chain writes blind. Every change lands in a live editor with an import
queue, a debugger and an engine log; state you did not observe is state you do
not know.

- Read the `new_errors_since_last_call` watermark after every call. A non-zero
  value means new errors were recorded; pull the real log lines, fix or explain
  them, then continue. The counter is one-shot and resets when read.
- While a game runs, `get_debugger_errors` and `get_debugger_output` carry its
  errors and prints. Without a running game, `get_debugger_log` reads the editor
  output and engine log, and `get_game_log_entries` reads the on-disk log after
  the session ends.
- When the problem is inside the plugin itself (confirmation denials, timeouts,
  dropped responses), read `get_plugin_log`.
- Prefer observed output — a log line, a screenshot, a read-back — over
  assuming a call did what its name suggests. `ok` only means the call went
  through.

## The change/observe/verify loop

The routing tables cover single calls; real work is a loop:

1. Change the project through tools, or orchestrate a deterministic sequence
   with `batch_execute` / `code_execute`.
2. Observe with a screenshot (`capture_editor_viewport`,
   `capture_game_viewport`), a read-back (`property_get`, `get_scene_tree`,
   `get_resource_property`) or output (`get_debugger_log`, and while a game
   runs `get_debugger_errors` / `get_debugger_output`).
3. Diagnose from the observation, fix, and observe again until the result
   matches the intent.
4. Confirm the fix with the error watermark at zero, then run the relevant
   validation tool (`validate_scene_file`, inline GDScript tests, or a game
   probe) before declaring success.

## Where the protocol details live

- CLI command and flag reference, exit codes, output contract:
  `godot-autopilot-tools`.
- Discovery, `batch_execute` and `code_execute` semantics:
  `godot-autopilot-tools`.
- Confirm protocol, MCP annotations and error codes:
  `godot-autopilot-tools`.
- Timeout, response-size, concurrency and batch limits:
  `godot-autopilot-tools`.
- Engine-specific behavior: the seven domain volumes below.

## See also

- `godot-autopilot-setup` - install, start, connect, generate the MCP config.
- `godot-autopilot-tools` - CLI-first calling protocol, confirm, errors, limits.
- `godot-autopilot-scene-system` - scenes, nodes, properties, signals, undo.
- `godot-autopilot-resources` - resources, imports, rename and move.
- `godot-autopilot-scripting` - GDScript lifecycle and execution channels.
- `godot-autopilot-runtime` - running the game, input injection, debugging.
- `godot-autopilot-servers` - RenderingServer, PhysicsServer, NavigationServer.
- `godot-autopilot-content` - tilemap, animation, audio, UI theming.
- `godot-autopilot-csharp` - C# build loop, exports, reflection bridge.
