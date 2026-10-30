# C# Tool Reference

The tools used by the C# bootstrap/build/verify loop, one line each. Domain
tools are invoked through `call_tool` unless your MCP client exposes them
directly; see the godot-autopilot skill for the protocol. Parameter lists here
are the ones that matter for C# work; use `get_tool_detail` for the full schema.

| Tool | Purpose |
|---|---|
| `dotnet_environment` | Probe the project and SDK without building: `dotnet_available`, `dotnet_version`, `project_file`, `is_dotnet_editor`, `assembly_loaded`, `engine_version`, features and `hint`. On a plain project the error carries a ready-to-write `template {path, content, target_framework, assembly_name}`. `assembly_loaded` means the .NET module is loaded, not the project assembly. |
| `create_csharp_script` | Create a `.cs` file (`path` ending in `.cs`, `source_code`, optional `overwrite`). Gates on stem==class name, `partial` + Godot base, and root csproj coverage. Returns `{path, class_name, csproj_ref_exists}`, `verified`/`readback` and `diagnostics {ok, instantiable, hint}`. |
| `write_file` | Generic write (`path`, `content`, optional `mode`); `.cs` writes under `res://` return a `diagnostics` object but enforce no C# gates. |
| `patch_script` | Patch one anchored spot (`path`, `anchor`, `replacement`, optional `mode`); compiles before writing and rolls back on failure. |
| `copy_resource_file` | Byte-for-byte duplicate (`path`, `dest_path`); start a new `.cs` from one that already compiles. |
| `build_csharp_assembly` | Build the root `.csproj`/`.sln`. `{wait: true, timeout_ms?: 1..25000}` returns `BuildResult {exit_code, success, errors, output_digest, output_empty, output_truncated?, errors_truncated?, game_running, restart_required, next actions}`; without `wait` it returns `{pid, started, game_running, restart_required, next actions}`. `restart_required` is true once the build succeeds or is launched (nothing hot-reloads loaded assemblies). Optional `probe_script` (a `res://` `.cs` path) loads that script after the build without instantiating it and adds an editor assembly probe object (path, `is_dotnet_editor`, state, `can_instantiate`, base class, `hint`) — `can_instantiate: false` on an available probe means the editor still runs the old assembly (click Build or restart the editor). |
| `get_debugger_log` | Read the editor-process log buffer (`limit`, default 50); editor-side C# compile and parse errors land here. |
| `get_plugin_log` | Read the plugin's own log (`limit`, `level`, `category`, `filter`, `since_index`); authorization denials, timeout diagnostics, dropped game responses. |
| `play_editor_current_scene` | Launch the edited scene as a game; save the scene first. |
| `get_game_status` | Query the running game (`timeout_ms`): scene, `node_count` (excludes the reserved `__gda_runtime` subtree), `instrumentation_node_count`, FPS, `paused`, `physics_frame`. |
| `get_debugger_errors` | Read running-game script errors (`limit`, optional `group`, optional `since_seq`); use the top-level `next_seq` as the next `since_seq`. |
| `get_debugger_output` | Read stdout/stderr captured from the running game (`limit`). |
| `get_game_log_entries` | Read the tail of the on-disk game log (`limit`, optional case-sensitive filter). |
| `property_get` / `property_get_list` | Read one property / list properties with metadata; use the list to discover exact C# `[Export]` names. |
| `property_set` | Set a node property (`path`, `property`, `value`, optional `type_hint`); converts node paths and typed arrays for C# exports and reports rejected writes. |
| `get_script_property` | Read one `property` from a script (`script_path`) or node (`node_path`); only GDScript-visible members return values. |
| `create_resource` | Instantiate a ClassDB, GDScript or C# `[GlobalClass]` resource (`type`, `name`). |
| `set_resource_property` / `save_resource` | Set a field on a located resource / save a `memory://` resource to disk. |
| `attach_script_to_node` | Attach a script resource (`node_path`, `script_path`) in one undo step; exported references are not migrated. |
| `reload_game_scripts` | Reload GDScript files in the running game (`paths`); it never reloads C# code. |
| `stop_editor_playing` | Stop the game started by `play_editor_current_scene`. |
| `capture_game_viewport` | Capture the running game as a base64 PNG (`timeout_ms`, optional `when`/`after_frames`). |
| `csharp_list_members` | List C# members of a locator (`runtime` editor/game). Optional `kind` (`method`/`property`/`signal`/`all`, default all), `name_contains` (case-insensitive), `limit` (1-1000, **default 200**) and `offset` page the matched members across methods → properties → signals; with `runtime: "game"` filtering and paging run inside the game so inherited tables never cross the channel. Returns `{methods, properties, signals, counts: {methods, properties, signals, returned, total}, truncated}` — `truncated: true` means more matches remain after limit/offset. |
| `csharp_get_property` / `csharp_set_property` | Read / write a C# property or field through the reflection bridge (`runtime` editor/game). |
| `csharp_call_method` | Call a C# method; overloads match by argument count. |
| `csharp_subscribe_event` / `csharp_unsubscribe_event` | Wire/unwire a receiver method to a C# event by subscription token. |

`new_errors_since_last_call` is not a tool: after a tool call it reports the
errors collected from tool responses and game `runtime_error` payloads since
your previous call. It does **not** include compiler output or engine script
errors; for those use `build_csharp_assembly.errors`, `get_debugger_log` and
`get_debugger_errors(since_seq)`.

## See also

- SKILL.md - the C# development loop these tools implement
- csharp-bridge.md - the reflection bridge endpoints and limits
- godot-autopilot-runtime - capture, log and injection semantics in depth
- godot-autopilot-resources - resource locators and save flows
- godot-autopilot - the discovery protocol and the error watermark
