# C# Tool Reference

The tools used by the C# build/verify loop, one line each. Domain tools are
invoked through `call_tool` unless your MCP client exposes them directly; see
the godot-autopilot skill for the protocol. `build_csharp_assembly` takes no
parameters; the other tools accept only the parameters named here.

| Tool | Purpose |
|---|---|
| `write_file` | Write or append text (`path`, `content`, optional `mode`); .cs writes under res:// are engine-managed and return a `diagnostics` object, so this is how C# scripts are created and edited. |
| `copy_resource_file` | Byte-for-byte duplicate of a file (`path`, `dest_path`); use it to start a new .cs from one that already compiles. |
| `build_csharp_assembly` | Trigger an async `dotnet build --nologo <project>` on the res:// root .csproj/.sln; returns the project file, command, started and pid, does not capture compiler output and does not hot-reload the assembly. |
| `get_debugger_log` | Read the editor-process log buffer (`limit`, default 50); the place where editor-side C# compile and parse errors are recorded. |
| `get_plugin_log` | Read the plugin's own in-process log (`limit`, default 100, max 1000; optional `level`, `category`, `filter`, `since_index`); authorization denials, timeout diagnostics and dropped late game responses. |
| `play_editor_current_scene` | Launch the edited scene as a game; save the scene first. |
| `get_game_status` | Query the running game (`timeout_ms`); a half-dead process is stopped and restarted, not debugged. |
| `get_debugger_errors` | Read script errors from the running game as a structured list (`limit`, default 20); empty without an active debug session. |
| `get_debugger_output` | Read stdout/stderr captured from the running game over the runtime channel (`limit`, default 50). |
| `get_game_log_entries` | Read the tail of the on-disk game log (`limit`, default 50, max 500); works without a debug session. |
| `property_get` | Read one node property (`path`, `property`); the read-back check after any property write. |
| `property_get_list` | List a node's properties with metadata (`path`, plus optional `only_script_variables`, `property_filter`); discover the exact C# `[Export]` field names before reading or writing them. |
| `property_set` | Set a node property (`path`, `property`, `value`, optional `type_hint`); converts node paths for C# `[Export]` references, errors "value not applied" and attempts to restore the old value when the engine rejects the assignment. |
| `get_script_property` | Read one `property` from a script (`script_path`) or a node (`node_path`); only fields visible to GDScript come back with a value. |
| `create_resource` | Instantiate a resource by `type` - a ClassDB class, a GDScript class_name or a C# `[GlobalClass]` - and register it as memory://name with `name`. |
| `set_resource_property` | Set a field on a located resource (`value`, optional `type_hint`); the resource-side counterpart of `property_set`. |
| `save_resource` | Save an in-memory resource (`name` or `object_id`) to disk; required before a memory:// resource can be referenced by a node property. |
| `attach_script_to_node` | Attach a Script resource to a node (`node_path`, `script_path`); registered with the undo history, and replacing a script does not migrate exported references. |
| `reload_game_scripts` | Reload GDScript files in the running game (`paths`); it does not reload C# code. |
| `stop_editor_playing` | Stop the game started by `play_editor_current_scene`. |
| `capture_game_viewport` | Capture the running game as a base64 PNG (`timeout_ms`); the visual verification step of the loop. |

The `new_errors_since_last_call` field is not a tool: after every call it
reports the errors collected since your previous call - consume it to confirm
that a fix produced nothing new (see the godot-autopilot skill).

## See also

- SKILL.md - the C# development loop these tools implement
- godot-autopilot-runtime - capture, log and injection semantics in depth
- godot-autopilot-resources - resource locators and save flows
- godot-autopilot - the discovery protocol and the error watermark
