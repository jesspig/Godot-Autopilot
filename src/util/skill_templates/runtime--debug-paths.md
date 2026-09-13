# Log, Error and Debug Protocol Paths

The log and error paths available to the autopilot — editor log buffer,
debugger session transport, on-disk game log and the plugin's own in-process
LogSystem buffer — and the protocol facts that explain the strange cases:
missing early errors, silent message loss and breakpoint behavior. All engine
facts here are verified against engine source.

## Four log and error paths

| Path | Tools | Source | Needs a running game |
|---|---|---|---|
| Editor engine log | `get_debugger_log` | engine log buffer of the editor process: script errors and messages routed through the engine logger | no — always contains data |
| Debugger session capture | `get_debugger_errors`, `get_debugger_output`, `get_debugger_scene_tree` | with an active debug session: the running game over the runtime channel; without one: an empty result plus a `note` — no editor-side fallback | live game data needs a session |
| On-disk game log | `get_game_log_entries` | tail window of the game process log file `user://logs/godot.log` | the log file must exist — start the game once with `play_editor_current_scene` |
| Plugin LogSystem | `get_plugin_log` | the plugin's own in-process diagnostic buffer (authorization denials, timeout bookkeeping, dropped late game responses) — not routed through any engine logger | no — always available |

Details that matter:

- `get_debugger_log` takes an optional `limit` (default 50) and never
  requires a running game — read it first for script errors.
- `get_debugger_errors` (optional `limit`, default 20) returns, with an
  active session, a structured list under `result` (`time`, `file`, `func`,
  `line`, `error`, `descr`, is_warning, `stack` per entry).
- `get_debugger_output` reads stdout/stderr captured from the game process
  (optional `limit`, default 50).
- `get_debugger_scene_tree` returns the running game's scene tree as a
  formatted text tree; it takes no parameters. Without a session it returns
  an empty result plus a `note` like the other session-capture tools.
- When the session-capture tools have nothing to return (no active
  session), they return an empty result plus a `note` field that suggests
  starting the game with `play_editor_current_scene` or reading
  `get_game_log_entries` instead.
- `get_game_log_entries` reads the on-disk log tail (optional `limit`,
  default 50, max 500) and returns `path`, `entries` and `total_lines`. It
  works without a debug session. If the file cannot be opened it falls back
  to the archived `godot.log.1` and reports `from_archive` with a warning;
  if the file does not exist the error includes directory diagnostics and a
  hint to start the game with `play_editor_current_scene`.
- `get_plugin_log` reads the plugin's own in-process LogSystem buffer
  (optional `limit`, default 100, max 1000) and needs no running game. Filter
  with `level` (debug, info, warning or error; the default debug applies no
  filtering), `category` (system, transport, tools, resources or prompts) and
  `filter` (case-insensitive substring on the message). For an incremental
  read pass `since_index` (start with 0, then the returned next_index); the
  result carries the entries array with a count and next_index. This is a
  different source from `get_debugger_log`, which reads the editor-process
  engine log buffer.

Check `get_debugger_session_info` (`active`, `breaked`, `running`, session
count) before trusting the session-capture path.

## Why "no new errors" can lie: the three drop gates

The debugger transport between the game and the editor drops messages in
three distinct places. Each one produces the same symptom — an error you
know happened never shows up in the session capture:

1. **No peer connected: everything is lost.** Both the generic
   `send_message` and `send_error` paths check for a connected debugger
   peer first; with no peer, messages are not queued at all. Errors raised
   during early startup — before the editor's debug session attaches —
   never reach the editor. This is the gate that hides the "first screen"
   of errors after launch.
2. **Rate limiting: silent drop for up to a second.** Per rolling
   one-second window, once more than `network/limits/debugger/
   max_errors_per_second` errors (or `max_warnings_per_second` warnings)
   arrive, further ones of that kind are discarded. One overflow notice
   (`TOO_MANY_ERRORS` / `TOO_MANY_WARNINGS`) is injected into the error
   stream the first time it happens in a window. An error storm therefore
   shows you a tail plus one overflow notice, not the storm.
3. **Queue overflow: wholesale message drop.** When more than
   `network/limits/debugger/max_queued_messages` messages pile up, messages
   are dropped; the next flush reports the drop count in a
   `TOO_MANY_MESSAGES` overflow notice.

A recursive error raised while the error stream is being flushed is dropped
too (the flusher cannot handle reentrancy).

Practical protocol for the autopilot: treat the session capture as
best-effort, and cross-check anything suspicious with
`get_game_log_entries` — the on-disk log is written by the engine logger
directly and passes none of these gates. The `new_errors_since_last_call`
watermark counts what the plugin recorded, so a watermark of 0 means
"nothing recorded since last call", not "nothing happened".

## Protocol asymmetry and reserved prefixes

The wire protocol routes messages by `prefix:name`. Behavior on unknown
input is asymmetric:

- On the game side, during the regular idle poll, a message whose prefix
  has no registered capture is **silently skipped**; a `core`-capture
  subcommand that is not recognized is swallowed without any output. Only
  inside the breakpoint loop does an unknown message produce a warning.
- The editor side logs a warning for messages it cannot interpret.

For anything that extends the channel (custom captures, plugin bridges,
in-game agents): the `live_*` and `runtime_node_select_*` message name
families are reserved by the engine's scene capture (live edit and runtime
node selection), alongside its screenshot and window-focus messages. Pick
prefixes outside those families or your messages will be silently routed
into engine handlers — or silently ignored.

## Breakpoints: blocking loop, evaluate channel, mouse focus

When the game hits a breakpoint, the engine's `debug()` call enters a
**blocking message loop**: the game thread stops inside the loop, polling
for debugger commands (`step`, `next`, `out`, `continue`, stack dumps,
`evaluate`) until a continue-like command arrives. While it spins on the
main thread it forces the display server to process and *drop* pending
events — so input injected while the game sits at a breakpoint is not
delivered to the game.

Two side effects worth knowing:

- **Mouse focus steal.** Entering a break saves the current mouse mode and
  forces it to visible; leaving restores the saved mode. A game running in
  captured/confined/hidden-mouse mode visibly flashes its cursor at every
  breakpoint, and anything relying on mouse mode during the break sees
  `VISIBLE`.
- **`evaluate` is an in-game expression channel.** The `evaluate` command
  parses an expression and executes it inside the game process, with the
  break stack frame's locals and globals, engine singletons, and user
  global classes as inputs, and the frame's instance as `self`. It returns
  the result to the editor. Anything reachable from those inputs can be
  probed mid-breakpoint.

## Auto-resume and GDA_AUTO_CONTINUE

When the game stops at a debugger breakpoint, `get_debugger_session_info`
reports the session as breaked. By default the plugin automatically
continues breaked sessions so `game_*` calls keep working:

- Auto-resume is controlled by the `GDA_AUTO_CONTINUE` environment variable;
  set it to `0` or `false` to disable.
- Each session is auto-continued at most 3 times; after that the game stays
  paused at the breakpoint.

TODO(verify): a plugin diagnostic suggests that setting `GDA_AUTO_CONTINUE`
to a larger value allows more auto-continues per session, but the
implementation verifiably only treats `0`/`false` as special and uses a
fixed cap of 3 — do not rely on a higher cap without confirming.

## Rename hints for Godot 3 to 4 errors

When `get_debugger_log` captures an "Invalid access to property or key"
error, the response automatically appends Godot 3 to 4 renamed-property
hints (10 in total, e.g. `frames` to `sprite_frames`, `cast_to` to
`target_position`, `translation` to `position`). Check the hints before
assuming a real bug — a stale pre-migration property name is a common
cause.

## Degradation paths when the runtime channel fails

When the game process stops answering, the tools that depend on the
runtime channel lose their only observation and control path. On a channel
failure reported directly by the tool (unreachable debug session, send
failure), `capture_game_viewport` and the input-injection tools add a
`hint` field to the `error` response with a one-line summary of the
fallbacks below — prefer these over OS-level screen capture or synthetic
keyboard input:

| Failing tool(s) | Degradation path |
|---|---|
| `capture_game_viewport` | `capture_display_screen` captures a whole physical screen and needs no running game; `capture_editor_viewport` with the default `editor` target grabs the editor viewport. Its `game` target needs the same runtime channel, so it fails the same way. |
| `get_debugger_errors`, `get_debugger_output` | `get_game_log_entries` reads the on-disk log tail (`user://logs/godot.log`); the log file must exist, so the game must have started at least once. |
| `queue_game_input`, `wait_game_input`, `get_game_input_status`, `sequence_game_inputs` | No GDA substitute: injected input can only travel over the runtime channel, and the editor process never sees the game's `Input` state. Drive the game from inside instead — an in-game test script created with `create_script` and attached to a node with `attach_script_to_node` simulates input and checks results in the game process itself. |
| plugin-side timeout details | `get_plugin_log` records the plugin's own timeout bookkeeping for the channel. |

## Standard error shapes

- A missing required parameter returns
  `{"error": "missing required parameter: <param>"}`.
- Calling an unknown domain tool through `call_tool` returns
  `{"error": "domain tool '<name>' not found — use search_tools to
  discover available tools"}`.

## See also

- `../SKILL.md` — the error watermark confirmation loop and runtime
  lifecycle.
- `input-injection.md` — why injected input does not land while a
  breakpoint or pause holds the game.
- `runtime-inspection.md` — read-only probes for the running game.
- `godot-autopilot` — watermark semantics and retryable soft errors in
  detail.
