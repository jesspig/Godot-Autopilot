---
name: godot-autopilot-tools
description: "CLI-first calling reference for Godot Autopilot: the gda command and flag surface, target discovery through the plugin instance registry, live gda search/describe/tools list plus gda call, native MCP tools as the compatibility face, confirmation and MCP annotations for high-risk side effects, the error watermark protocol and error codes, batch_execute/code_execute semantics, response limits, and dynamic user tools. Use for calling-protocol facts."
---

# Godot Autopilot Tools

This volume is the cross-domain calling knowledge for GDA: the channel ladder,
the CLI surface, discovery, confirmation, the error protocol and the limits. It
carries no domain knowledge; scene, resource, scripting, runtime, server,
content and C# facts live in their own volumes.

## Channel ladder

1. **GDA CLI.** Whenever a shell works, use `gda`. It is the first-class
   interface: it discovers the plugin's port from the instance registry, prefers
   the local IPC endpoint, falls back to HTTP, and prints JSON. It works on
   Windows, macOS and Linux, and its exit codes separate tool failures from
   transport failures.
2. **Native MCP tools.** When the harness lists the GDA tools, use them
   directly. They are the compatibility face: full schemas, structured errors
   and image content. The semantics are identical; only the transport differs.
3. **Manual editor instructions.** When neither a shell nor an MCP client is
   available, hand the user precise editor steps and ask them to report the
   result. Do not fabricate tool output.

There is no HTTP API to call by hand and no proxy command. Every call goes
through `gda` or the harness MCP tools.

## The `gda` command surface

Run `gda --help` for the current text; the table below is the stable shape.

| Command | Purpose |
|---|---|
| `gda ping` | Check connectivity to the MCP server. |
| `gda status` | Show server status, project, port, transport and `mcp_url`. |
| `gda categories` | List tool categories. |
| `gda search <query>` | Search tools through the live server. |
| `gda describe <tool>` | Show one live tool's schema and `side_effect`. |
| `gda call <tool>` | Call one tool. |
| `gda list` | List known Godot instances and projects. |
| `gda start` | Launch a Godot editor and wait for it. |
| `gda stop` | Stop an editor by id, port or project. |
| `gda restart` | Restart an editor with the same settings. |
| `gda logs` | Show an instance log. |
| `gda skills list` | List installed skill books. |
| `gda skills show <name>` | Print one book; `--file <rel>` reads an auxiliary file. |
| `gda tools list` | List all direct tools from a live instance; `--category`/`--tags` narrow it. |
| `gda projects` | List known Godot projects offline. |
| `gda cache list` | List cache manifests. |
| `gda cache info` | Show cache root, entry count and size. |
| `gda cache clear` | Remove old entries; requires `--force`. |
| `gda install skills` | Install skill books to `~/.agents/skills`. |
| `gda install plugin` | Install the addon into a project. |
| `gda update <target>` | Update `cli`, `skills`, `plugin` or `all`; `--check` reports only. |

Global options that matter on most calls:

- Target selection: `--port <n>` (connection override only), `--id <id>`,
  `--project <dir>`, `--socket <path>`, `--transport auto|ipc|http`,
  `--no-fallback`.
- Input: `--args <json>`, `--args-file <path>`, `--confirm`, `--timeout <sec>`.
- Discovery filters: `--category <name>`, `--tags <a,b>` (`gda search` and `gda tools list`).
- Output: `--json`, `--pretty`, `--raw`, `--save-images <dir>`.
- Lifecycle: `--godot <path>`, `--headless`, `--play`, `--no-import`,
  `--wait <sec>`, `--force`, `--tail <n>`, `--probe <light|strict>`.
- Install and update: `--path <dir>`, `--source auto|online|cache|builtin`,
  `--offline`, `--zh`, `--all`, `--check`.

`--port` is a connection override, never a listen port. The plugin owns the
port. `--offline` skips the network and uses the cache or the builtin skills
archive. `--zh` selects the Chinese skills archive. `--force` is required for
destructive cache clears and overwrites.

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Success. |
| 1 | The tool ran and returned a failure envelope. |
| 2 | Transport or environment failure: no instance, endpoint down, timeout. |
| 3 | Usage error: unknown command, flag or malformed arguments. |

Stdout is JSON; diagnostics go to stderr. Exit 2 means the channel is down, not
that the tool returned nothing. Never report a fabricated result after exit 2.

## Target discovery

The CLI resolves the target in this order: explicit `--socket`/`--port`, the
instance index by `--id`, the live instance matching `--project` or the current
directory, and the persisted plugin record. The port comes from the plugin, not
from a default. `gda status --json` shows the resolved values: `port`,
`port_source`, `mcp_url`, `transport`, `ipc_kind`, `plugin_version`,
`toolset_hash`.

If resolution fails, the error names the reason. Run `gda list --all` to see
known projects, then `gda start --project <dir>` or open the editor. Do not
guess a port and do not reuse one from an earlier session.

## Discovery protocol — search first, never guess

1. `gda ping` confirms the server responds.
2. `gda search <query>` searches the live server by name, description, category
   and tags. Use `--category` or `--tags` to narrow; the server returns at most
   20 results by default.
3. `gda describe <name>` prints the schema, description and `side_effect`
   marker. Read it before the first write.
4. `gda call <name> --args '<json>'` executes. Use `--args-file` for large
   payloads.
5. `gda tools list` prints every directly callable tool as `{tools, total}`,
   excluding the five MCP protocol tools. Use it for a full inventory, not for
   keyword search.

Discovery is live-only: the CLI keeps no offline tool catalog.
Without a running instance, start one with `gda start --project <dir>`.

Search by the object first, then by the operation: "tilemap cell",
"animation track", "import sidecar". Add a verb only when the object is too
broad. Rerun with synonyms before concluding that a tool is missing: tile/cell,
sprite/texture, log/output, delete/remove/erase, create/add/new. Never invent a
plausible name; an unknown tool fails and tells you to search.

Example:

```bash
gda describe create_scene_node
gda call create_scene_node --args '{"parent_path": "Root", "name": "Player", "type": "CharacterBody2D", "properties": {"position": {"x": 0, "y": -10}}}'
```

## Native MCP tools

When the harness lists GDA tools, the meta tools are `ping`, `search_tools`,
`list_categories`, `get_tool_detail`, `call_tool`, `batch_execute` and
`code_execute`. Every domain tool goes through `call_tool` with `name` and
`arguments`; `call_tool` also accepts a top-level `confirm` boolean. Use
`search_tools` / `get_tool_detail` exactly as you would use `gda search` /
`gda describe`. `search_tools` defaults to 20 results; pass `limit: 0` only when
you explicitly need the full enumeration.

## Confirmation and annotations

High-risk side effects require explicit confirmation. The server returns
`{"code": "confirmation_required", "confirm_required": true, "side_effect":
"...", "hint": "..."}` before executing. Review the tool description and the
target, then retry with `--confirm` (CLI) or `confirm: true` (MCP). The classes
that require confirmation are:

- `destructive`: irreversibly removes an object that already exists in the
  editor, such as a node, animation, audio bus effect or display window.
- `process`: starts, stops or otherwise controls an OS process.
- `code_execute`: runs arbitrary GDScript in the editor (`code_execute`,
  `batch_execute`, `execute_script`).
- `game_runtime`: reaches into the running game (`execute_game_script`,
  `queue_game_input`, `reload_game_scripts`).

File, config and scene writes (`writes_file`, `writes_config`, `writes_scene`,
`shows_alert`, `modifies_window`) do not require confirmation, but they still
change content. Read the target back after every write. MCP clients also
receive tool annotations; treat a destructive or runtime annotation as a
stop-and-review signal.

Do not work around a confirmation error by choosing another tool with the same
effect. Do not batch a high-risk call to hide it from review.

## `batch_execute` and `code_execute`

- `batch_execute` runs an ordered `operations` list (at most 256) in sequence.
  Each operation takes `tool` and optional `args`; `stop_on_error` defaults to
  true and `rollback_on_error` is optional. Use it for deterministic sequences
  such as "set these five properties, then save". A batch containing async game
  tools is refused unless you pass `await_async` and call the batch directly.
- `code_execute` runs GDScript in the editor. Without a line-start `func ` it
  inlines the source in one entry function; with a `func ` line it preserves
  the source and uses `function_name` (default `_run`). `timeout_ms` defaults to
  5000 and is capped by the schema; the deadline is enforced while the script
  runs. Use it for loops, math and computed values that a batch cannot express.
- Decision rule: a handful of existing calls → `batch_execute`; loops, math,
  conditions or very large payloads → `code_execute`.

## Error protocol and watermark

Every tool failure is a JSON object with an `error` key. Some carry a stable
`code`; branch on the code, not on the prose message. Every meta-tool response
also carries `new_errors_since_last_call`, an integer counting new errors since
your previous call, including errors the call itself produced.

- The watermark is one-shot: reading it resets the counter. Never skip it, even
  on success.
- Errors printed by a running game count too.
- After every write, read the watermark before the next call. A non-zero value
  means stop, pull the real log lines, fix or explain them, then continue.

Soft, retryable errors carry `retryable: true` and `retry_after_ms` while the
editor is importing or scanning resources. Wait at least the suggested interval
and retry the same call. During project export, all tool calls are rejected;
wait for the export to finish instead of retrying in a tight loop.

Stable error codes:

| Code | Raised when | Retry? |
|---|---|---|
| `exec_timeout` | A `code_execute` script outran `timeout_ms`. | No; split the work or raise the timeout within the schema cap. |
| `eval_timeout` | A game-side eval outlived the host wait. | Not immediately; the game may still be running it. |
| `eval_cancelled` | A game-side eval was torn down by a cancel handle. | No; re-issue if the result is still needed. |
| `main_thread_timeout` | The editor main thread did not process the call in its budget. | No; the call was cancelled but the side effect may still land. Read state first. |
| `queue_unavailable` | The editor main-thread queue was full or closing. | Yes, later. |
| `confirmation_required` | A high-risk tool was called without confirmation. | Only after reviewing the target; retry with `--confirm`. |

A failure with no code is a plain tool error: read its text. A
`main_thread_timeout` reports how long it waited and may report queue depth;
`exec_timeout` reports the measured execution time next to the exceeded limit.

Confirm a fix with the watermark: reproduce the failure, diagnose, fix, rerun
the failing call, and make meta calls until the watermark returns 0. Zero after
a successful call is the closing evidence that the fix produced no new errors.

## Limits

- Default operation timeout is 5000 ms. The maximum is the value the tool schema
  reports; editor dispatch budgets can be lowered per deployment, so read the
  schema rather than assuming a fixed number. Game operations cap `timeout_ms`
  at 25000 ms.
- Editor calls run under a wait budget with three tiers: ordinary tools,
  health probes (`ping`, `system_status`, 5000 ms) and long-blocking meta tools
  (`code_execute`, `batch_execute`). A long task can end with
  `main_thread_timeout` instead of hanging forever.
- JSON responses are capped at 4 MiB and truncation is detectable; check the
  truncation fields before trusting completeness.
- `batch_execute` accepts at most 256 operations.
- At most 8 concurrent in-flight requests; exceeding the limit fails fast.
- A tool name is at most 128 characters.

## Dynamic user tools

Project scripts can register extra domain tools through the `user_tools`
mechanism. They surface through the same discovery protocol and the same
`gda call` / `call_tool` path, marked as dynamic. The directory-scan convention
defaults to `res://addons/godot-autopilot-tools`, and the plugin ships samples
under its user-tools sample directory. Registration and calls use the same
confirmation rules as built-in tools.

## Manual fallback

When neither the CLI nor an MCP client is available, give the user a short,
checkable sequence: open the project, enable the plugin, perform the action in
the editor, then report the exact node path, property value or log line. Ask for
one observation at a time and never invent a screenshot, log line or watermark.
Restoring the CLI is the preferred recovery: install the standalone `gda`
binary, then continue normally.

## See also

- `godot-autopilot-setup` - installation, first connection, MCP configuration.
- `godot-autopilot` - connection model, routing, observation loop.
- `godot-autopilot-scene-system` - scenes, properties, signals, undo.
- `godot-autopilot-resources` - resources, imports, rename and move.
- `godot-autopilot-scripting` - GDScript execution channels.
- `godot-autopilot-runtime` - game process, input, logs, debugger.
- `godot-autopilot-servers` - server RID tools.
- `godot-autopilot-content` - tilemap, animation, audio, UI.
- `godot-autopilot-csharp` - C# build loop and interop.
