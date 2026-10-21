---
name: godot-autopilot-tools
description: "Calling the GDA (godot-autopilot) plugin: the invocation ladder (native MCP tools, the gda CLI, the plain-HTTP REST face, manual fallback), what to do when the plugin is installed but no MCP client is configured, search-first tool discovery, meta-tool semantics including batch_execute/code_execute and the three-tier capability gates, the error protocol and error watermark, limits, the full domain tool catalog, dynamic user tools, and the manual fallback guide for machines without a usable CLI. Load this skill whenever MCP tools are missing or you need calling-protocol facts."
---

# Godot Autopilot Tools

This volume is the cross-domain calling knowledge for the godot-autopilot plugin: the channel ladder, the discovery protocol, meta-tool semantics, the error protocol and the limits. The MCP server is the product; skills and scripts are the experience layer on top of it. Whichever channel you use, the same rules hold: discover every tool name with `search_tools` and never guess one; call every domain tool through `call_tool`; read the `new_errors_since_last_call` watermark after every call.

This volume carries no domain knowledge. Scene and property work, resources and files, GDScript, runtime and debugging, server RID tools, content pipelines and C# each have their own volume (see See also). Come here to learn how to reach any of them; go there to learn what to do once you arrive.

## Channel ladder

Four channels, in preference order. Drop to the next only when the current one is unavailable.

**When to enter the ladder.** Either of these:

- **The plugin is installed but no MCP client is configured.** The Godot project contains `addons/godot-autopilot` and the server may already be listening on loopback, yet your harness lists no GDA tools. Nothing is broken — you simply have no native channel, so start at rung 2.
- **The client lost its tools.** The list went empty or calls fail right after an editor restart, and reconnecting did not help.

Probe before choosing a rung: `bun --version`, then `node --version`. Take the first rung whose precondition holds, and never jump to the manual guide while a shell still works.

1. Native MCP tools. Whenever the client harness lists the MCP tools, use them directly: `ping`, `search_tools`, `list_categories`, `get_tool_detail`, `call_tool`, `batch_execute`, `code_execute`. This is the primary channel with full schemas, structured errors and image content.
2. GDA CLI. When the MCP channel is down, drive the server from a shell with the `gda` command. It is the only scripted channel: it speaks the same MCP protocol, accepts the same subcommands and shares the same exit codes as the native channel. Pick the first distribution that works on the machine:
   - **bunx** (Bun present, nothing installed): run `bunx --no-install "github:jesspig/Godot-Autopilot#0.3.0" <cmd>` first (instant when the bunx cache is warm; a fast failure otherwise); if not cached, `bunx "github:jesspig/Godot-Autopilot#0.3.0" <cmd>` downloads once. Quotes are required on PowerShell because `#` starts a comment.
   - **global install** (Bun, used repeatedly): `bun install -g` once, then run plain `gda <cmd>`.
   - **standalone binary** (no JavaScript runtime at all): download the `gda` release asset for the user's OS and CPU, verify it against the release's `sha256sums.txt`, and run it exactly like the CLI. No runtime needed; one download per version. Asset names, checksum verification and per-platform caveats (macOS quarantine, musl, SmartScreen) are in references/cli-access.md.
   Calling commands: ping/status/categories/search/describe/call; lifecycle: list/start/stop/logs; REST family: api list/api describe/api call/endpoints. Run `<prefix> --help` for the full reference; stdout is compact JSON, diagnostics go to stderr. Known limitation: the pinned release tag was cut before the CLI existed, so that tag's tree carries no `cli/` directory and no root `package.json` — the pinned command cannot resolve a `gda` binary and fails every time, so do not spend retries on it. Use `bunx "github:jesspig/Godot-Autopilot#develop" <cmd>` instead (that branch does ship the CLI, but it is an unreleased moving target, not a pinned version), or the standalone `gda` release asset once a release ships it (asset names and checksum verification in references/cli-access.md), or drop to rung 3.
3. REST face. The plugin serves a plain-HTTP API on the same port as the MCP endpoint, under the `/api` prefix. It needs no MCP handshake and no session, so a client that can only send ordinary HTTP can still reach every registered tool. It is **not** a scripted channel: it is the interface for an HTTP-only client. Its endpoints are `GET /api` (service and endpoint list), `GET /api/tools` (filtered, paged tool list), `GET /api/tools/<tool>` (one tool's schema), `POST /api/tools/<tool>` (call one tool — the request body is the argument object itself), `POST /api/batch` (ordered operations), `GET /api/health` (the only endpoint that needs no token) and `GET /api/openapi.json`. Every endpoint except the health probe requires an `Authorization: Bearer <token>` header; the token is in the plugin's instance registry and the `gda api` subcommands read it for you. Read references/rest-api.md for the endpoint contract, the token sources, the status-code table and two minimal snippets (Python and JavaScript). Prefer rung 2 whenever a shell exists — the CLI resolves the port and the token for you.
4. Manual guide. When there is neither a working MCP client nor any usable CLI nor an HTTP-capable client, hand the user step-by-step editor instructions per references/manual-fallback.md.

Never hand-write HTTP requests against the MCP endpoint in any channel: no raw requests, no ad-hoc scripts against `/mcp`. Every MCP call goes through the harness MCP tools or the gda CLI. The one sanctioned exception is the REST face in rung 3, which has its own reference and its own examples.

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
- `batch_execute` orchestrates existing tools: an ordered `operations` list (at most 256 items) run in sequence, with `stop_on_error` (default true) and optional `rollback_on_error`. Each operation takes only `tool` (required) and `args` (optional object, defaults to `{}`); the `call_tool`-style `name`/`arguments` keys are not accepted there. Result counts: `total` = executed operations (`succeeded` + `failed` + `pending`), `skipped` = operations never run after an early stop, and `total` + `skipped` always equals the length of `operations`; failed items carry their message under `error`. Choose it for deterministic sequences such as "set these five properties, then save". Async game tools inside a plain batch are refused with status error (the pending record is released, so `pending` counts 0) — call those tools one at a time via `call_tool` when you need their response, or pass `await_async: true` and invoke `batch_execute` directly.
- `code_execute` runs GDScript in the editor: the source is wrapped in a script extending Node, and the edited scene root is exposed as SceneRoot. Without a line-start `func ` definition the code is inlined inside a single entry-function body — write straight-line code ending in return. When any non-comment line starts with `func ` (leading whitespace stripped, comment lines ignored) the whole source is preserved verbatim and `function_name` (default `_run`) selects the entry point. The detector needs `func ` with a trailing space. `timeout_ms` defaults to 5000 and is capped at the maximum the server reports in the `code_execute` schema - that ceiling follows the host's long-blocking dispatch budget and can be lowered per deployment, so read it from the schema rather than assuming a fixed number; `auto_owner` defaults to true so created nodes are saved with the scene. The timeout is enforced while the script runs, not only before it — a long loop is interrupted when the deadline passes, and the call comes back as an exec_timeout failure instead of freezing the editor. Details in the gotchas reference.
- `side_effect` is the risk marker `get_tool_detail` reports, and it is never empty: a tool that only reads reports `none`. The classes are `none`, `writes_file`, `writes_config`, `shows_alert`, `modifies_window`, `process`, `code_execute`, `game_runtime` and `destructive`. The two script-running meta tools, `code_execute` and `batch_execute`, both report `code_execute`. The authorization gate is derived from that marker alone — the server keeps no per-tool-name exception list — so a tool can start requiring a capability because its class changed, not because its name is special.
- Decision rule: expressible as a handful of existing tool calls — use `batch_execute`. Needs loops, math or conditions (laying out hundreds of tiles, bulk renames, computed values) — use `code_execute`. Very large payloads also favor `code_execute` to avoid huge JSON arguments.
- Authorization gates, checked on every call, in three tiers. The gate follows the tool's `side_effect` class. **Read** tools (class `none`) are allowed by default. **Write** tools (`writes_file`, `writes_config`, `shows_alert`, `modifies_window`) are **denied by default** and need the `write` capability. **Danger** tools are denied by default and need their own capability: `process`, `code_execute`, `game_runtime`, `destructive`. `user_tools` is a seventh capability, independent of the class, guarding user-registered dynamic tools. The full capability list is `read`, `write`, `process`, `code_execute`, `game_runtime`, `destructive` and `user_tools`.
  - Concretely: `code_execute` and `execute_script` need the `code_execute` capability, and so does `batch_execute` — a batch is gated as a whole, so a client that enabled only `process` or `game_runtime` can no longer run a batch of ordinary domain tools and gets an authorization_required error instead. The tools that reach the running game (`execute_game_script`, `queue_game_input`, `wait_game_input`, `sequence_game_inputs`, `reload_game_scripts`) need the `game_runtime` capability. User-registered dynamic tools (see References and extension points) need the `user_tools` capability.
  - **A write-class tool now needs `write`.** Before this change a write-class tool needed no capability at all. If your calls start failing with an authorization error on a file or config write, that is the cause: add `write` to the allow list. Do not work around it by switching to a different tool. Destructive tools — the ones that destroy what already exists, such as deleting the main window — need the `destructive` capability and are denied by default too.
  - Enable a capability either by setting the `GODOT_AUTOPILOT_ALLOW` environment variable to the capability name (or `all`) and restarting the engine, or by ticking the matching checkbox ("Allow code_execute", "Allow game_runtime", "Allow user tools", "Allow write", "Allow destructive") in the plugin MCP Config dock, which saves to the plugin config and takes effect on the next tool call with no restart. Un-ticking a box removes only that one capability and leaves the others alone, even when the allow list was `all`. `process` has no dock checkbox — it needs the environment variable and a restart. When `GODOT_AUTOPILOT_ALLOW` is set it wins over the config, so do not rely on the dock checkbox while the variable is present.

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

Export lockout: while the editor is exporting the project, all tool calls are rejected with an "editor is exporting" error. Wait for the export to finish instead of retrying in a tight loop. That rejection carries a machine-readable code as well as the message, so branch on the code when you can.

### Error codes

Some failures carry a machine-readable `code` field next to the `error` text. Branch on the code, not on the message: the messages are prose and get reworded, the codes are stable. The codes you can receive:

| Code | Raised when | Retry? |
|---|---|---|
| exec_timeout | a `code_execute` script outran its `timeout_ms` | no — the script was interrupted; split the work or raise `timeout_ms` |
| eval_timeout | a game-side `eval` script outlived the 25000 ms host wait, or the editor gave up waiting for the eval answer | not immediately — the game may still be running that script |
| eval_cancelled | the eval request was torn down by a cancel handle | no — re-issue it if the result is still needed |
| main_thread_timeout | the editor main thread did not process the tool inside its dispatch budget | **no** — the call was cancelled and the side effect may still land; read the editor state first |
| queue_unavailable | the editor main-thread queue was full or already closed | yes, later — the editor may be shutting down |

The two eval codes arrive nested under a structured_error object inside the tool payload; the other three come back as the top-level result of the call. The dispatch codes carry a `retryable` flag: it is **false** for main_thread_timeout and **true** for queue_unavailable, while exec_timeout also carries it false and the eval codes carry no flag at all — treat a missing flag as "do not retry immediately". A main-thread timeout means the call was abandoned: the editor may still run the work afterwards, so read the state back before repeating it. main_thread_timeout also reports how long it waited, and on the dispatch path the queue depth and the age of the oldest pending call; exec_timeout reports the measured execution time next to the `timeout_ms` it exceeded. A failure with no code is a plain tool error: read its error text.

### Confirming a fix with the error watermark

Use the watermark as the closing step of every fix instead of assuming a fix worked:

1. A call fails, or the watermark reports new errors.
2. Diagnose and fix the cause.
3. Re-run the failing call. A success with the watermark at 0 confirms the fix produced no new errors.
4. A non-zero value means new errors were recorded since your previous call — errors printed by the running game count too. Read them with `get_debugger_errors` or `get_debugger_log`, fix again, then make any meta-tool call (even `ping`) until the watermark comes back 0.

## Observing while working

Never chain write operations blind. After a write, confirm with a read-back (`property_get`, `get_scene_tree`, `get_resource_property`), a screenshot (`capture_editor_viewport`, `capture_game_viewport`) or output (`get_debugger_log`; with a running game `get_debugger_errors` and `get_debugger_output`, on-disk fallback `get_game_log_entries`). When the problem is inside the plugin itself (authorization denials, timeouts, dropped responses), read `get_plugin_log`. Prefer observed output over assuming a call did what its name suggests; `ok` only means the call went through.

## Limits

- Default operation timeout is 5000 ms. The maximum accepted timeout is the value the tool schema reports as its maximum: for editor-only operations it follows the host's long-blocking dispatch budget and can be lowered per deployment, so read it from the schema instead of assuming a fixed number. Game operations cap `timeout_ms` at 25000 ms, keeping the host wait plus a grace interval below the transport limit.
- Editor-side calls are handed to the editor main thread under a wait budget with three tiers: 27000 ms for ordinary tools, 5000 ms for the health probes (`ping` and `system_status`), and 29000 ms for the long-blocking meta tools (`code_execute` and `batch_execute`). The long-blocking tier used to wait forever; it is bounded now, so a long task can still end with a main_thread_timeout instead of hanging forever, but it will end. The three tiers can be overridden with the `GODOT_AUTOPILOT_DISPATCH_TIMEOUT_MS`, `GODOT_AUTOPILOT_HEALTH_TIMEOUT_MS` and `GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS` environment variables, each clamped to 1000-29000 ms.
- JSON responses are capped at 4 MiB. Large results are truncated with detectable fields (such as a truncated flag) rather than silently dropped — check them before trusting completeness.
- `batch_execute` accepts at most 256 operations per call.
- At most 8 concurrent in-flight requests. Exceeding the limit fails fast — reduce parallelism instead of retrying blindly.
- On the REST face the tool list is paged (500 per page by default, 2000 maximum), a batch accepts at most 256 operations, a tool name is at most 128 characters, and a request body over 4 MiB is refused with 413 before it reaches the tool.

## References and extension points

- references/cli-access.md — the GDA CLI, the only scripted channel: which distribution to pick when no MCP client is configured, every command and global option (including the REST family `api list` / `api describe` / `api call` / `endpoints`), the seven-step port and target resolution chain, engine resolution for `start`, exit codes, the output contract, image handling, release asset names, checksum verification and per-platform caveats. Read it before running `gda` or downloading a binary.
- references/rest-api.md — the plain-HTTP face on the same port, for a client that cannot do an MCP handshake: the endpoint list, the request and response shapes, where the bearer token comes from, the status-code table, and minimal Python and JavaScript snippets. Read it only when the MCP channel and the CLI are both unavailable and the client can still send HTTP requests.
- references/manual-fallback.md — the manual guide: trigger criteria, hard rules, the handoff template and recovery steps. Read it when neither the MCP channel nor any CLI is available.
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
