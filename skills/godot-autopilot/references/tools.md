# Calling Protocol and Docs CLI

This reference is the cross-domain calling knowledge: channels, the CLI
surface, engine docs, confirmation, errors, limits, and the tool surface. Domain
behavior lives in the other references.

## Channels

- **GDA CLI.** Use `gda` whenever a shell works. It discovers the plugin port,
  prefers IPC, falls back to HTTP, and prints JSON.
- **Native MCP tools.** When the harness lists GDA tools, use them directly.
  Semantics match the CLI; only the transport differs.
- **Manual editor steps.** When neither is available, give the user a short,
  checkable sequence and ask for one observation. Never invent tool output.

## Core CLI flow

```bash
gda search "tilemap cell"
gda describe set_tilemap_cells
gda call set_tilemap_cells --args '{"node_path": "Level", "cells": [{"x": 0, "y": 0, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}]}'
gda tools list
```

- `gda search <query>` searches the live server by name, description, category,
  and tags; `--category` and `--tags` narrow it. It returns at most 20 results.
- `gda describe <name>` prints the schema and `side_effect` marker. Read it
  before the first write; never infer parameter names.
- `gda call <name> --args '<json>'` or `--args-file <path>` executes one call.
- `gda tools list` prints every directly callable tool. Discovery is live-only;
  start an editor with `gda start --project <dir>` when no instance is running.

Target resolution: explicit `--socket` or `--port`, then `--id`, then the live
instance matching `--project` or the current directory. The plugin owns the
port; `--port` is a connection override only. `gda status --json` reports the
resolved `mcp_url` and transport. If resolution fails, `gda list --all` shows
known projects.

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Success |
| 1 | The tool ran and returned a failure envelope |
| 2 | Transport or environment failure: no instance, endpoint down, timeout |
| 3 | Usage error: unknown command, flag, or malformed arguments |

Stdout is JSON; diagnostics go to stderr. Exit 2 means the channel is down;
never report a fabricated result after it.

## Tool surface and deprecated aliases

- The live catalog carries 252 canonical tools; `gda tools list` and
  `search_tools` expose only those. `get_tool_detail` also resolves an alias, but
  returns the canonical tool detail plus the replacement fields.
- Another 325 deprecated aliases resolve to a canonical tool through
  `call_tool`, `batch_execute`, and the CLI's `gda call`. They are hidden from
  search and the catalog so a name search returns one entry per capability.
- Every alias carries the replacement fields in its response:
  deprecated_alias, canonical_name, deprecated_since, alias_removal_version. The
  fields are informational; branch on the canonical tool, not the alias.
- Aliases are deprecated from the 0.4.x line and are removed in the 0.6.x line. Migration means calling the canonical tool with the action below, not
  waiting for removal. If a tool is missing from `gda tools list`, search by
  object and call the canonical name.
- Alias calls keep the confirmation gate: a high-risk alias still returns
  `confirmation_required` and is never a bypass.

Common replacements:

```bash
gda describe resource_query
gda call resource_query --args '{"action": "references", "path": "res://assets/icon.png"}'
gda call get_game_state --args '{"action": "status"}'
```

| Deprecated alias | Canonical call |
|---|---|
| `get_game_status` | `get_game_state` action=status |
| `capture_game_viewport` | `capture_editor_viewport` target=game |
| `get_debugger_log` / `get_debugger_errors` / `get_debugger_output` | `get_debugger_stream` action=log / errors / output |
| `validate_scene_file` | `analyze_project` action=validate_scene |
| `verify_scene_saved` | `get_scene_tree` verify=true |
| `load_resource` / `reload_resource` | `resource_load` action=load / reload |
| `save_resource` | `resource_save` action=save |
| `create_resource` / `duplicate_resource` | `resource_create` action=create / duplicate |
| `get_resource_type` / `get_resource_dependencies` / `get_resource_references` / `get_resource_uid` | `resource_query` action=type / deps / references / uid |
| `get_resource_property` / `set_resource_property` | `resource_property` action=get / set |
| `rename_resource_file` / `move_resource_file` / `remove_resource_file` / `copy_resource_file` | `resource_file` action=rename / move / remove / copy |
| `write_file` / `read_file` / `find_in_files` | `os_file_write` action=write / `os_file_text` action=read / find |
| `create_editor_scene` / `open_editor_scene` / `close_editor_scene` / `reload_editor_scene` / `play_editor_current_scene` / `stop_editor_playing` | `editor_scene_control` action=create / open / close / reload / play / stop |
| `save_editor_scene_as` / `save_editor_scenes` | `save_editor_scene` action=save_as / save_all |
| `get_editor_selection` / `get_editor_edited_scene_root` | `get_editor_state` action=selection / root |
| `set_editor_selection` | `editor_selection_set` action=set |
| `scan_editor_file_system` / `get_editor_file_system_status` | `get_editor_files` action=scan / status |
| `instantiate_scene` / `build_nodes_from_spec` | `create_scene_node` mode=scene / spec |
| `rename_scene_node` / `reparent_node` | `edit_scene_node` action=rename / reparent |
| `add_group_node` / `remove_group_node` / `has_group_node` | `node_group` action=add / remove, or `get_scene_tree` include_groups=true |
| `set_tilemap_cell` | `set_tilemap_cells` with a `cells` entry |
| `add_tilemap_atlas_source` / `add_tilemap_physics_layer` / `set_tilemap_tile_collision` | `update_tileset` action=add_atlas_source / add_physics_layer / set_tile_collision |
| `create_spriteframes` / `add_spriteframes_animation` / `add_spriteframes_frame` | `update_spriteframes` action=create / add_animation / add_frame |
| `create_scene_animation_tree` / `add_animation_machine_state` / `connect_animation_states` | `animation_tree` action=create / add_node / connect |
| `get_audio_bus_layout` / `set_audio_bus_volume_db` / `add_audio_bus_effect` | `audio_bus` action=get_layout / set_volume / add_effect |
| `play_audio_player` / `set_audio_player_volume_db` / `seek_audio_player` | `audio_player` action=play / set_volume / seek |
| `set_audio_device_output` / `set_audio_device_input` | `audio_device` action=set_output / set_input |
| `create_theme_resource` / `set_theme_item_bundle` / `set_theme_color` / `set_theme_stylebox_flat` | `update_theme` action=create / bundle |
| `set_control_anchor_preset` / `apply_theme_to_control` | `update_control_layout` action=layout / apply |
| `queue_game_input` / `wait_game_input` / `sequence_game_inputs` | `game_input` action=queue / wait / sequence |
| `execute_game_script` | `game_script` action=execute with `script_action` |
| `set_scene_tree_pause` / `is_scene_tree_paused` / `reload_scene_tree_current_scene` | `game_scene_tree` action=set_pause / is_paused / reload_current_scene |
| `get_nav_2d_map_path` / `get_nav_3d_map_path` | `nav_query` mode=path_query with dimension=2 / 3 |
| `get_nav_3d_map_closest_point_to_segment` | `nav_query` mode=closest_point_to_segment, dimension=3 |
| `intersect_physics_2d_ray` / `intersect_physics_3d_ray` | `physics_query` mode=intersect_ray, dimension=2 / 3 |
| `create_render_camera` / `create_render_light` / `create_render_mesh` / `create_render_canvas_item` | `render_camera` / `render_light` / `render_mesh` / `render_canvas_item` action=create |
| `csharp_list_members` / `csharp_get_property` | `csharp_member` action=list / get |

The same rule covers every alias: call the canonical tool and pass the action,
mode, or parameters shown above.

## Engine documentation CLI

`gda docs` is the offline engine reference. It reads the local engine dump,
caches it by engine minor, and searches classes, members, and globals.

```bash
gda docs search "tween"
gda docs class Node2D --member position
gda docs member Node2D look_at
gda docs sync
```

- `status` is read-only: project pin, resolved engine, cache path, staleness.
- `search` defaults to 20 results (`--limit` up to 200) and accepts `--kind`
  (`class`, `method`, `property`, `signal`, `enum`, `constant`, `utility`) and
  `--class`. `class` returns brief, description, and counts by default; use
  `--section`, `--member`, or `--full` to expand. `member` auto-detects the
  kind, and an ambiguous name returns all matches.
- `sync` always exports. A missing cache or a version mismatch exports on the
  next query. A failed export is an error; it never falls back to another
  version.
- `--docs-file <path>` reads an external `extension_api.json` and bypasses the
  cache. If the project pin and the available binary disagree, pass a matching
  `--godot <path>`; the command refuses to mix minors.
- `clear --engine <x.y>` removes one bucket; `--all` requires `--force`.

## Confirmation

High-risk side effects return a structured `confirmation_required` error before
executing. Review the description and target, then retry with `--confirm` (CLI)
or `confirm: true` (MCP). The classes are `destructive` (removes an existing
object), `process` (controls an OS process), `code_execute` (runs arbitrary
GDScript), and `game_runtime` (reaches into the running game).

File, config, and scene writes do not require confirmation, but they change
content: read the target back. Never switch to another tool with the same effect
to bypass confirmation, and never hide a high-risk call inside a batch. A
deprecated alias runs the same gate as its canonical tool.

## Batch and code execution

- `batch_execute` runs an ordered `operations` list (at most 256). Each operation
  has `tool` and optional `args`; `stop_on_error` defaults to true and
  `rollback_on_error` is optional. Use it for deterministic sequences. An
  operation may use a canonical tool or a deprecated alias.
- `code_execute` runs GDScript in the editor for loops, math, and computed
  values. Without a line-start `func ` it wraps the source in one entry
  function; with `func ` it preserves the source and uses `function_name`.
  `timeout_ms` defaults to 5000 and is capped by the schema.
- Decision rule: a handful of existing calls -> `batch_execute`; loops, math,
  conditions, or very large payloads -> `code_execute`.

## Errors and the watermark

Every tool failure is a JSON object with an `error` key. Some carry a stable
`code`; branch on the code, not the prose. Every meta-tool response also carries
`new_errors_since_last_call`, which counts new errors since your previous call,
including errors the call itself produced. It is one-shot: reading resets it.

| Code | Raised when | Retry? |
|---|---|---|
| `exec_timeout` | A `code_execute` script outran `timeout_ms` | Split the work or raise the timeout within the cap |
| `eval_timeout` | A game-side eval outlived the host wait; `timeout_cause` names `script_error` or `channel_lost` | Not immediately; the game may still be running it |
| `game_ready_timeout` | A queued game call never saw `gda:ready` with scene-ready | Yes, once ready; check the extension loads |
| `startup_in_progress` | The editor is still restoring its scene after launch | Yes, after `retry_after_ms` |
| `no_scene_open` / `viewport_degenerate` | Editor capture: no scene open, or 2D and 3D viewports both degenerate | Open a scene or restore the viewport, then retry |
| `main_thread_timeout` | The editor main thread missed its budget | Read state first; the side effect may still land |
| `queue_unavailable` | The main-thread queue was full or closing | Yes, later |
| `confirmation_required` | A high-risk tool ran without confirmation | Review the target, then retry with `--confirm` |

Soft, retryable errors carry `retryable: true` and `retry_after_ms` while the
editor imports or scans resources. Wait at least the suggested interval and
retry. During project export, all tool calls are rejected; wait for the export
instead of retrying in a tight loop.

Confirm a fix with the watermark: reproduce, diagnose, fix, rerun the failing
call, and make meta calls until the watermark returns 0. Zero after a successful
call is closing evidence that the fix produced no new errors.

## Resources, prompts, and tasks

`gda resources read <uri>` reads editor and debugger state; `gda resources list`
is the URI authority. `gda prompts list|get` renders registered prompts.
`gda call --async` plus `gda tasks list|get|wait|cancel|clean` manages locally
recorded tasks.

## Limits

- Default operation timeout is 5000 ms; the schema reports the current cap.
  Game operations cap `timeout_ms` at 25000 ms.
- JSON responses are capped at 4 MiB with truncation fields; check them before
  trusting completeness. `batch_execute` accepts at most 256 operations, and at
  most 8 requests may be in flight concurrently.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| Exit 2 with no JSON | No live instance or endpoint down | `gda list --all`, then `gda start --project .` |
| Exit 3 | Unknown command, flag, or malformed JSON | `gda --help`; validate the `--args` JSON |
| `confirmation_required` | High-risk call without confirmation | Review the target, then `--confirm` |
| `main_thread_timeout` | Editor main thread missed its budget | Read state before retrying |
| A familiar tool name is not found | The name is a deprecated alias, already removed or not listed | Search by object; call the canonical tool with its action |
