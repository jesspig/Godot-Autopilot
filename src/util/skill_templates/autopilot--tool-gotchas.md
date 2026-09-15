# Tool Gotchas

Catalogue of plugin-side behaviors that surprise callers: silent failures,
hard size limits, places where the built-in prompt text and the actual
implementation disagree, scene refresh timing, `code_execute` traps, and the
side-effect classes reported by `get_tool_detail`. Engine-side silent failures
(dropped node owners, cleared tile cells, audio bus fallbacks and friends)
live in the domain skills, not here.

## Silent failure: non-numeric strings into int properties become 0

`property_set` on an int property with a non-numeric string value (e.g. "abc")
returns ok and stores 0. The call does not error. After any property write
with a value you are unsure about, read it back with `property_get` and assert
the expected value.

Details: godot-autopilot-scene-system.

## Silent failure: unrecognized key names become KEY_NONE

The editor-side input injection tools (key pressing) resolve key names through
a parser that returns KEY_NONE for unrecognized names - without reporting an
error. Pressing KEY_NONE is a no-op, so a misspelled key name silently does
nothing. Prefer single letters and digits (mapped directly) or names from the
known set (SPACE, ENTER, ESCAPE, SHIFT, CONTROL, ALT, TAB, BACKSPACE, DELETE,
arrow keys).

Details: godot-autopilot-runtime (editor-side injection does not reach
the running game; use the game input channel there).

## Silent failure: schema-required parameters that have defaults

Three tools declare parameters as required in their schema but fall back to
defaults instead of erroring when you omit them:

- `create_scene_node` - name/type are marked required but default to NewNode
  and Node; an empty call creates a node with those defaults.
- `get_resource_extensions` - type is marked required; when omitted the
  handler passes an empty type and returns the extensions for ALL types
  instead of an error.
- `reimport_resource_files` - path is marked required; with no files and no
  path the call succeeds silently with a zero count.

Consequence: an omitted-argument bug can look like a successful call. Verify
the effect after calls to these three tools.

Details: godot-autopilot-scene-system and godot-autopilot-resources.

## Silent failure: property writes are read back, and rejections now error

`property_set` reads each written property back and no longer reports a plain
ok when the engine drops the value:

- A non-nil value read back as nil, an object-typed property whose readback is
  not an object, or an array-typed property read back empty when a non-empty
  array was written (or with an element read back as nil) is now an error:
  `value not applied: ...`, and the old value is restored. Previously such
  writes could return ok with no effect.
- A non-array JSON value for an array property is refused before writing - the
  error explains that a non-array value would silently clear the array. Pass
  `[]` to clear it explicitly.
- An array element that is a JSON object or array is refused when the declared
  element type cannot be determined. Declare the element type in the script
  (`Array[T]` / `[Export] T[]`) or build the array with `code_execute`.
- Values the engine merely adjusts still return ok, with a `warning` field
  describing the adjustment.

`set_resource_property` performs the same readback and errors when the
resource rejects the value (it does not restore the old value). Read-back
remains the authoritative check after every write you are unsure about:
`property_get` / `get_resource_property` and assert the expected value.

## Debugger reads without a debug session return empty + note

`get_debugger_errors`, `get_debugger_output` and `get_debugger_scene_tree`
fetch their data from the running game over the runtime channel. Without an
active debug session they return an empty result plus a `note` - there is no
editor-process fallback and no stale "last captured" tree. Use
`get_debugger_log` for editor-process script errors and output,
`get_game_log_entries` for the game's on-disk log file, and `get_plugin_log`
for the plugin's own in-process diagnostics (authorization denials, timeout
bookkeeping and dropped late game responses). The same rule applies
to `get_game_input_status`: its responses never include
`recent_engine_errors`, so read errors through the debugger tools instead.

## Screenshots: image content through call_tool, raw base64 inside batch_execute

Through `call_tool`, `capture_editor_viewport`, `capture_game_viewport` and
`capture_display_screen` deliver the PNG as an MCP image content block: the
text JSON keeps `format`, `width` and `height`, while `data` becomes
`"<attached-as-image-content>"` and image_attached is `true`. Inside
`batch_execute` and `code_execute` no image block is attached - the JSON
`data` field keeps the full base64 and must be decoded by the caller.

## Async game tools inside batch_execute: refused, or awaited with await_async

Several game tools answer asynchronously: their immediate return value is a
pending marker, and only the awaiting call paths - call_tool, or
batch_execute with `await_async=true` - wait for the game's answer and merge
it into the response. Inside a plain batch_execute those tools are refused
instead of silently dropped:

- Default (`await_async=false`): the operation fails with status error and a
  message naming the alternatives; the pending record is still released so
  nothing leaks, and `pending` in the response counts 0. The default
  `stop_on_error=true` then skips the remaining operations, so a batch that
  must keep going needs `stop_on_error=false`.
- `await_async=true`: batch_execute waits for the game response (bounded by
  the op's `timeout_ms`) and writes the real response under result with
  status ok or error. The wait runs on the transport thread, so invoke
  batch_execute directly - calling it through call_tool with
  `await_async=true` is refused with an error.

If you need one game tool's result, call_tool is still the simplest path; use
`await_async=true` when a later operation in the same batch depends on it.

## Long game scripts: start_game_job submits, get_game_job polls

`start_game_job` wraps a game op (currently only the `execute_game_script` op,
with `params` carrying that op's arguments verbatim) and returns a `job_id`
immediately instead of waiting - the right shape for in-game scripts that
would exceed the 25 s per-call budget. Poll `get_game_job` with the `job_id`:
a pending status means check again later, a done status carries the game's
response verbatim (including its own error fields), expired means the job
exceeded `timeout_ms` + 2000 ms after start and was dropped, and cancelled
means `cancel=true` tore the request down after sending an idempotent engine
cancel. A positive `timeout_ms` on the poll call waits internally instead of
returning the pending status right away (0, the default, never blocks). The
job table holds at most 16 jobs; entries leave it on collection, on expiry
(2 s grace past the deadline) or on cancel, and each of those paths releases
the eval error-break suppression, so abandoning a job never leaves error
breaks disabled in the game.

## Timeout budget chain: host wait = timeout_ms + 2000 ms, below the 30 s transport

Game ops time out in three layers, and the smallest layer wins:

1. The HTTP transport kills a stateless request after 30 s.
2. The plugin waits `timeout_ms + 2000` ms on the host side for the game
   answer (the extra 2000 ms is the response grace).
3. Accepted `timeout_ms` for game ops is capped at 25000 ms, so the host wait
   never exceeds 27000 ms - below the transport's 30 s.

Values above 25000 are rejected by the tools that validate `timeout_ms`
explicitly and clamped by the tools that only read it; the low-level game
sender also clamps every request. Either way the effective budget is at most
25000 ms and the returned `timeout_ms` reports the clamped host wait. Longer
work must be split into shorter calls or polled with `get_game_status`. The
30000 ms maximum still applies to editor-only tools (code_execute,
run_gdscript_tests, create_editor_scene and editor-target captures), which
never round-trip through the game.

## After a game timeout: engine continue and late results

When a game op times out, the plugin releases the pending record, sends the
idempotent cancel and - when the game's debugger session is breaked -
broadcasts an engine continue, so a game stopped in the debugger at a script
error no longer stays frozen after the call returns. The continue is only
sent while a breaked session exists; a normally running game never receives
it.

A game answer that arrives after the wait ended can no longer be delivered to
the caller. Instead of only logging it, the plugin keeps the five most recent
late responses (FIFO) and attaches them to the next timeout error as a
late_results array: one object per entry with the request id, the op name,
age in milliseconds since the request was sent, and a summary holding the
error or result JSON text truncated to 200 characters. The field is omitted
while nothing was seen late, so a timeout error carrying it proves the
earlier calls did answer - read `get_plugin_log` when you need the full
payloads.

## Eval script compile errors return structured errors, not timeouts

`execute_game_script` with the script action compiles the source inside the
running game. When the game runs from the editor, a GDScript parse failure
used to send the engine into its error breakpoint: the debugger blocked the
game main thread waiting for a continue command, so the structured compile
error the game had already prepared never reached the plugin and the call
timed out. The plugin now disables engine error breaks for the duration of
the eval request and restores the previous behaviour when the request
finishes, times out or is cancelled, so a malformed script comes back
quickly with the compile error text and line instead of a timeout. Only the
script action is affected: `get_property`, `set_property` and `call_method`
do not compile source and never disable error breaks. While an eval script
request is in flight, unrelated error breaks in the game are ignored too;
normal error breaks resume as soon as the request ends.

Details: godot-autopilot-runtime.

## Size limits

These limits are enforced by the server. Truncation is always detectable -
responses carry fields such as truncated, scan_truncated or scan_limit rather
than silently dropping data.

| Limit | Value | Applies to |
|---|---|---|
| JSON response size | 4 MiB | every tool response |
| eval output / error text truncation | 8192 bytes | game eval output and captured error text |
| runtime error / output buffers | 200 / 500 entries | game runtime channel |
| tilemap cells per call | no fixed server-side cap | `set_tilemap_cells` - oversized JSON arguments can hit client-side request limits and fail as a parse error before the tool runs; use script loops for bulk layouts; `source_id` -1 clears a cell |
| batch operations / input sequence steps | 256 | `batch_execute` / input sequences |
| scene tree export | depth 64, 2000 nodes | scene tree reads (max depth reported in the response) |
| viewport capture | 4096 px per side, 8 MiB PNG | `capture_game_viewport`, `capture_editor_viewport`, `capture_display_screen` |
| Variant single string | 64 KiB | serialized Variant values |
| Variant array elements | 10000 | arrays and packed arrays |
| file scan | 10000 files, 2 MiB per file, 32 MiB total, depth 64 | resource/file scans |
| default / max timeout | 5000 / 30000 ms | editor-only operations taking `timeout_ms` |
| default / max timeout, game ops | 5000 / 25000 ms | game tools taking `timeout_ms` (host wait = `timeout_ms` + 2000 ms, below the 30 s transport) |

For bulk work near any of these limits, prefer `code_execute` loops over
giant JSON payloads.

## Prompt-vs-implementation conflicts: the implementation wins

The plugin ships built-in prompt text that lags the implementation in a few
spots. Where they disagree, trust the implementation and `get_tool_detail`:

- The tool-usage prompt claims to describe 17 tools; the actual usage prompt
  body covers 19.
- KEY_ENTER: one built-in prompt says 4194310, the keycode reference table
  says 4194312 (4194310 is actually KEY_META). Prefer KEY_* name strings over
  hard-coded numbers.
- Serialization descriptions claiming that RID serializes to a plain number
  and PackedByteArray to a base64 string are outdated. The implementation
  serializes RID as an object like {"id": 42} and PackedByteArray as a plain
  JSON number array.

## Scene refresh timing

Right after `create_editor_scene` or `open_editor_scene`, an immediately
following `get_scene_tree` can still return the PREVIOUS scene's tree - the
editor applies the switch asynchronously relative to tool calls. Retry the
read (or check `get_editor_edited_scene_root` first) before concluding that
your change did not apply.

Details: godot-autopilot-scene-system.

## Error recovery

Two editor states block otherwise valid calls. Both have a defined recovery:

- **A dirty scene tab blocks open/create.** `open_editor_scene` refuses while
  any open scene has unsaved changes ("current scene has unsaved changes: ...
  - save first (save_editor_scene)"), and `create_editor_scene` with
  `close_current=true` refuses the same way. Recover by saving
  (`save_editor_scene` for the current scene, `save_editor_scenes` for every
  tab) or by discarding the edits - call `reload_editor_scene` with the
  listed path (the scene must be open) to reload it from disk - then retry
  the original call.
- **Importing or scanning blocks affected writes.** While the editor scans or
  imports, affected calls (for example `reimport_resource_files`, or
  `write_file` landing on an imported asset) fail soft with
  `retryable: true` and `retry_after_ms` (500) instead of erroring hard. Poll
  `get_editor_file_system_status` (its `result` carries `scanning` and
  `progress`) until `scanning` is false, wait at least `retry_after_ms`, then
  retry the same call. Do not fire-and-forget scans: `scan_editor_file_system`
  requested while a scan is already running is a no-op.

## code_execute traps

Before the traps: both `code_execute` and `execute_script` are denied by
default behind the `code_execute` authorization gate, and the game runtime
tools (`execute_game_script`, `queue_game_input`, `wait_game_input`,
`sequence_game_inputs`, `reload_game_scripts`) behind `game_runtime`. A denied
call returns an error carrying authorization_required plus an `enable` field
(and writes a warning to the plugin log); enable a capability by setting
`GODOT_AUTOPILOT_ALLOW` to it (or to `all`) and restarting the engine, or -
for `code_execute` and `game_runtime` - by ticking "Allow code_execute" or
"Allow game_runtime" in the plugin's MCP Config dock, which takes effect on
the next call without a restart. The process gate keeps no dock toggle: it
needs the environment variable and a restart. When the environment variable
is set it wins over the config.

`code_execute` wraps your source in a generated @tool Node script. Four traps:

- Single-function mode is the default: source is inlined inside a generated
  function and top-level func definitions are rejected with an error. Inline
  statements only, or define named functions and call one via `function_name`
  (multi-function mode).
- Calling close_scene on the editor interface is refused outright with an
  error telling you to use the `close_editor_scene` tool instead - the call
  would destroy the executing node and crash the editor.
- Mixing tabs and spaces in the source is rejected with a mixed indentation
  error; reindent with one style throughout.
- The exposed SceneRoot variable IS the edited scene root node: node paths
  under it carry no root-name prefix (SceneRoot.get_node("Player"), not
  SceneRoot.get_node("Root/Player")).

Details: godot-autopilot-scripting.

## The GDA_FORCE_HEADLESS environment variable

The name is inverted. Setting GDA_FORCE_HEADLESS=1 does not force headless
operation - it DISABLES the plugin's cmdline (headless) mode detection, so an
editor launched without a window still builds its UI and starts the MCP server
as usual. If the server is unexpectedly absent in a headless CI editor, set
GDA_FORCE_HEADLESS=1; if you intended the lightweight cmdline mode, leave it
unset.

## Side effect classes

`get_tool_detail` reports a `side_effect` marker for every tool that can act
beyond reading editor state. Read it before calling anything you have not
used before. The classes, in increasing order of risk:

- `writes_file` - writes files under res:// or user://. Confirm the path
  stays inside the project before calling.
- `writes_config` - persists project or editor settings. Treat as a durable
  change the user will find on disk.
- `shows_alert` - opens a native dialog that blocks the editor until
  dismissed. Never fire one in a batch or without an explicit user request.
- `modifies_window` - moves, resizes, focuses or flashes windows; also
  clipboard, mouse cursor and screen state.
- `process` - spawns or kills OS processes, opens paths with the default
  application and edits the environment. The highest-risk class: commands
  run outside the project's scope, so double-check every argument before
  calling.

Two cases deserve the same caution as `process`: the `code_execute` meta tool
runs arbitrary GDScript in the editor process, and tools marked `game_runtime`
change the running game's state. Treat both at the highest risk level.

## See also

- godot-autopilot - usage overview, discovery protocol and the error watermark
- godot-autopilot-scripting - the script execution channels behind the code_execute traps
- godot-autopilot-scene-system - property JSON value shapes and the memory:// rule
