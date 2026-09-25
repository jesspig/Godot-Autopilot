# Godot Autopilot Tools

This volume is the cross-domain calling knowledge for the godot-autopilot plugin: the channel ladder, the discovery protocol, meta-tool semantics, the error protocol and the limits. The MCP server is the product; skills and scripts are the experience layer on top of it. Whichever channel you use, the same rules hold: discover every tool name with `search_tools` and never guess one; call every domain tool through `call_tool`; read the `new_errors_since_last_call` watermark after every call.

This volume carries no domain knowledge. Scene and property work, resources and files, GDScript, runtime and debugging, server RID tools, content pipelines and C# each have their own volume (see See also). Come here to learn how to reach any of them; go there to learn what to do once you arrive.

## Channel ladder

Three channels, in preference order. Move one step down only when the channel above is unavailable.

1. Native MCP tools. Whenever the client harness lists the MCP tools, use them directly: `ping`, `search_tools`, `list_categories`, `get_tool_detail`, `call_tool`, `batch_execute`, `code_execute`. This is the primary channel with full schemas, structured errors and image content.
2. Script bridge. When the MCP client lost its tool list (typically right after an editor restart) but a shell with Node 18+ or Bun exists, use the bundled script `node .agents/skills/godot-autopilot-tools/scripts/gda_mcp.mjs` documented in references/script-access.md. It speaks the same protocol, resolves the port the same way and reports the same errors.
3. Manual guide. When there is neither a working MCP client nor a script runtime, or the harness has no shell at all, stop automating and hand the user step-by-step editor instructions as documented in references/manual-fallback.md.

Never hand-write HTTP requests in any channel: no raw requests, no ad-hoc scripts against the endpoint. Every call goes through the harness MCP tools or the bundled script.

## Discovery protocol — search first, never guess

Tool names are snake_case with the verb first, but segment counts vary, so guessing is unreliable. Always discover:

1. `ping` to confirm the server responds.
2. `search_tools` with a `query` (optionally filtered by `category` or `tags`), or `list_categories` to browse by domain.
3. `get_tool_detail` with the exact `name` to read the input schema, description and side-effect marker before calling anything that writes files or config.
4. `call_tool` with `name` and `arguments` to execute. The seven meta tools are called directly at the MCP layer; every domain tool goes through `call_tool`.

Calling `call_tool` with an unknown name fails with a message telling you to use `search_tools`. Use it instead of improvising similar names.

```json
{"name": "create_scene_node", "arguments": {"type": "Node2D", "name": "Player", "parent_path": "Root/Actors"}}
```

### Search techniques

`search_tools` ranks candidates over tool names, descriptions, categories and tags, so the words you send shape the order you get back:

- Search by the object first, then by the operation: "tilemap cell" finds the cell tools, "animation track" finds the track tools. Add a verb only when the object query is too broad.
- Describe, do not name. While you do not know the real tool name, describe the outcome ("connect a signal from a button") instead of guessing a plausible-looking name.
- Rerun with synonyms before concluding a tool is missing: tile/cell, sprite/texture, log/output, delete/remove/erase, create/add/new.
- Narrow with filters: pass `category` for a single domain or `tags` for required tags, and browse `list_categories` when you only know the domain.
- Read `get_tool_detail` for the candidate schema and side-effect marker before the first write.
- When a query still comes back empty, browse the tool catalog reference in the godot-autopilot volume (every domain tool grouped by source module) before inventing anything. The exact tool set is whatever `search_tools` returns — never assert a count from memory.

## Meta-tool semantics

- `call_tool` executes one tool: pass the domain tool name as `name` and its inputs as `arguments`. Meta tools themselves (`ping`, `search_tools`, `list_categories`, `get_tool_detail`, `batch_execute`, `code_execute`) are called directly, not through `call_tool`. Server status is a domain tool: reach `system_status` through `call_tool`.
- `batch_execute` orchestrates existing tools: an ordered `operations` list run in sequence, at most 256 items, with `stop_on_error` (default true) and optional `rollback_on_error`. Choose it for deterministic sequences such as "set these five properties, then save". Async game tools inside a batch report status pending — the request was sent but not awaited, and the batch discards it — so call those tools one at a time when you need their response.
- `code_execute` runs GDScript in the editor: the source is wrapped in a script extending Node, and the edited scene root is exposed as SceneRoot. Without a line-start `func ` definition the code is inlined inside a single entry-function body — write straight-line code ending in return. When any non-comment line starts with `func ` (leading whitespace stripped, comment lines ignored) the whole source is preserved verbatim and `function_name` (default `_run`) selects the entry point. The detector needs `func ` with a trailing space. `timeout_ms` defaults to 5000 with a 30000 maximum; `auto_owner` defaults to true so created nodes are saved with the scene.
- Decision rule: expressible as a handful of existing tool calls — use `batch_execute`. Needs loops, math or conditions (laying out hundreds of tiles, bulk renames, computed values) — use `code_execute`. Very large payloads also favor `code_execute` to avoid huge JSON arguments.
- Authorization gates, checked on every call. `code_execute` and `execute_script` need the `code_execute` capability. The tools that reach the running game (`execute_game_script`, `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `reload_game_scripts`) need the `game_runtime` capability. User-registered dynamic tools (see References and extension points) need the `user_tools` capability. Enable a capability either by setting the `GODOT_AUTOPILOT_ALLOW` environment variable to the capability name (or `all`) and restarting the engine, or by ticking the matching checkbox ("Allow code_execute", "Allow game_runtime", "Allow user tools") in the plugin MCP Config dock, which saves to the plugin config and takes effect on the next tool call with no restart. When `GODOT_AUTOPILOT_ALLOW` is set it wins over the config, so do not rely on the dock checkbox while the variable is present.

## Error protocol

Every tool failure is returned as a JSON object with an `error` key. For `call_tool` and `code_execute`, a top-level `error` also sets the MCP-level error flag on the response.

In addition, every meta-tool response carries a top-level integer watermark field counting new errors recorded since your previous call, including errors the call itself just produced. A failed call reports at least its own error. The semantics:

- It is one-shot: the value is consumed when you read it and the counter resets. Never skip reading it, even when the call succeeded.
- Errors printed by a running game are recorded too.
- After every write call (`property_set`, `create_scene_node`, `write_file`, `save_resource`, scene, script or config edit), read the watermark before issuing the next call. A non-zero value means new errors were recorded — stop, pull the real log lines, fix or explain them, then continue.

Soft (retryable) errors: while the editor is importing or scanning resources, affected calls return a structured error with a retryable flag set to true and a suggested `retry_after_ms` wait:

```json
{"error": "editor is currently importing/scanning resources; retry shortly", "retryable": true, "retry_after_ms": 500}
```

Wait at least the suggested interval, then retry the same call.

Export lockout: while the editor is exporting the project, all tool calls are rejected with an "editor is exporting" error. Wait for the export to finish instead of retrying in a tight loop.

### Confirming a fix with the error watermark

Use the watermark as the closing step of every fix instead of assuming a fix worked:

1. A call fails, or the watermark reports new errors.
2. Diagnose and fix the cause.
3. Re-run the failing call. A success with the watermark at 0 confirms the fix produced no new errors.
4. A non-zero value means new errors were recorded since your previous call — errors printed by the running game count too. Read them with `get_debugger_errors` or `get_debugger_log`, fix again, then make any meta-tool call (even `ping`) until the watermark comes back 0.

## Observing while working

Never chain write operations blind. After a write, confirm with a read-back (`property_get`, `get_scene_tree`, `get_resource_property`), a screenshot (`capture_editor_viewport`, `capture_game_viewport`) or output (`get_debugger_log`; with a running game `get_debugger_errors` and `get_debugger_output`, on-disk fallback `get_game_log_entries`). When the problem is inside the plugin itself (authorization denials, timeouts, dropped responses), read `get_plugin_log`. Prefer observed output over assuming a call did what its name suggests; `ok` only means the call went through.

## Limits

- Default operation timeout is 5000 ms; the maximum accepted timeout is 30000 ms. Game operations cap `timeout_ms` at 25000 ms, keeping the host wait plus a grace interval below the transport limit.
- JSON responses are capped at 4 MiB. Large results are truncated with detectable fields (such as a truncated flag) rather than silently dropped — check them before trusting completeness.
- `batch_execute` accepts at most 256 operations per call.
- At most 8 concurrent in-flight requests. Exceeding the limit fails fast — reduce parallelism instead of retrying blindly.

## References and extension points

- references/script-access.md — the script bridge: subcommands, options, port resolution, exit codes, image rules and the troubleshooting table. Read it when the native MCP channel is down but a shell exists.
- references/manual-fallback.md — the manual guide: trigger criteria, hard rules, the handoff template and recovery steps. Read it when neither MCP nor script channels exist.
- The tool catalog, tool gotchas and development-workflow references live in the godot-autopilot volume; this volume does not duplicate them. When a `search_tools` query comes back empty, browse the catalog there; when a call behaves surprisingly, check the gotchas there.
- User-defined dynamic tools (the `user_tools` mechanism): project scripts can register extra domain tools that then surface through the same discovery protocol and the same `call_tool` path, marked as dynamic. Registration and calls both sit behind the `user_tools` capability (environment variable wins over config; "Allow user tools" dock checkbox; see Meta-tool semantics). The directory-scan convention defaults to res://addons/godot-autopilot-tools, and the plugin ships samples under its user-tools sample directory. For writing or debugging such tools, load the godot-autopilot-scripting volume.

## See also

- godot-autopilot — connection prerequisites, usage overview and the shared references (tool catalog, gotchas, development workflow)
- godot-autopilot-scene-system — scene lifecycle, nodes, properties, signals
- godot-autopilot-resources — resource and file operations, rename and move
- godot-autopilot-scripting — GDScript execution channels and user-defined tools
- godot-autopilot-runtime — running the game, input injection, debugging
- godot-autopilot-servers — RenderingServer, PhysicsServer and NavigationServer RID tools
- godot-autopilot-content — tilemap, animation, audio, UI theming
- godot-autopilot-csharp — C#/.NET projects, assembly build loop and script interop
