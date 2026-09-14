# Runtime and Debugging with godot-autopilot

This book covers the game process life cycle: launching and stopping the
game, the `game_*` runtime channel, input injection, viewport capture and the
visual verification loop, UI reconnaissance, the four log and error paths,
the error watermark confirmation loop and inline GDScript tests. Deep dives
live in `references/input-injection.md`, `references/debug-paths.md` and
`references/runtime-inspection.md`.

## Start and stop the game

- `play_editor_current_scene` launches the currently edited scene as a game.
  Save the scene first (`save_editor_scene`) or playback may fail.
- It returns `result` set to `ok` or `already_playing` plus an `is_playing`
  flag. While a game is already running you get `already_playing` — the tool
  does not restart the game. Call `stop_editor_playing` first if you want a
  fresh run.
- `stop_editor_playing` stops the running game and returns the editor to
  editing mode. It returns `ok` even when no game is running, so it is always
  safe to call at the end of a play-testing session or before modifying the
  scene again.

## Prerequisites of the game_* runtime channel

All `game_*` tools (and `capture_editor_viewport` with `target` set to `game`)
send requests to the running game process through the editor debug session.
Two prerequisites must hold:

1. The game was launched from the editor with `play_editor_current_scene`.
2. The game project loads the godot-autopilot extension (plugin enabled in
   that project).

Until the game reports ready, tools fail with:

```text
game not ready: the game process has not reported gda ready yet — wait a moment
after play, or verify the game project loads the godot-autopilot extension
```

The bridge reports ready asynchronously right after launch, so one early
failure immediately after `play_editor_current_scene` is normal: wait a
moment and retry. If the error persists, the game project does not load the
extension.

## Check liveness with get_game_status

`get_game_status` takes no parameters and reports the engine version, current
scene, node count, FPS, `paused` and `physics_frame`, plus derived liveness
fields:

- `healthy` — the game reported activity within the last 3000 ms (see
  `last_activity_ms`); read it together with `physics_stalled`.
- `physics_stalled` — `physics_frame` has not advanced while FPS is positive,
  i.e. a frozen physics loop.

A failed scene reload shows up as an empty scene with a sharp `node_count`
drop. Do not debug a half-dead process: stop with `stop_editor_playing` and
rerun `play_editor_current_scene`. Full scenario readings live in
`references/runtime-inspection.md`.

## The editor and the game are two processes

A running game is a separate OS process from the editor. Anything you do to
the editor process does not reach the game:

- The editor-side input tools — `press_input_action`,
  `release_input_action`, `is_input_action_pressed`,
  `is_input_action_just_pressed`, `press_input_key`, `release_input_key`,
  `move_input_mouse`, `press_input_mouse_button`,
  `release_input_mouse_button`, `start_input_gamepad_vibration`,
  `stop_input_gamepad_vibration` — inject events into the **editor process**;
  a running game never receives them.
- Script code executed in the editor process likewise cannot touch the
  game's `Input` or `SceneTree` singletons.

To drive the running game use `queue_game_input`, `wait_game_input`,
`sequence_game_inputs` or `execute_game_script` (all of which travel over the
debug channel into the game process).

## Inject inputs into the running game

- `queue_game_input` simulates one input: `type` is `key`, `mouse_button` or
  `action`; `mode` is `event` (default, parsed input event), `api`
  (immediate `Input.action_press`/`action_release`) or `hold` (event-style,
  held for `duration_ms`). Parameters outside the whitelist are rejected
  with an error — a typo fails the call.
- `wait_game_input` waits until an action reaches a transient state
  (`just_pressed` default, `just_released` or `pressed`) in a physics frame,
  or `timeout_ms` elapses (default 2000, max 30000). Optional `inject` sends
  the input first in the same call; `just_pressed`/`just_released` require
  `inject` — without it the one-frame transient window has already expired.
- `sequence_game_inputs` fires a timeline of inputs on exact physics frame
  offsets (max 256 items, each with `kind` and `at_frame`) — the tool for
  playtest automation.
- `get_game_input_status` reports an action's `pressed`, `just_pressed`,
  `just_released` and the current `physics_frame`. Engine errors are not
  attached to this response — when input appears ignored, read
  `get_debugger_errors` (running game) or `get_debugger_log` (editor
  process) separately.

Two engine facts govern the timing of everything above:

- **`just_pressed` has two counters**: a press records `physics_frame + 1`
  for the physics context and the current frame for the process context.
  Right after injection, checking from the same `_process` returns true via
  the process branch, while the same physics tick returns false — the
  earliest a physics-context check agrees is the next tick. Transient
  states are tracked against physics frames only: poll in
  `_physics_process`, not `_process`.
- **Injected events land on the next frame**: parsed input events go through
  a buffer flushed at the end of the display server's event pass, so an
  event injected during `_process` is dispatched at the start of the next
  frame. Use `sequence_game_inputs` offsets, not round-trip timing.

Full semantics — buffering and flush points, `api` mode forgery versus real
key events, action matching and per-device state — are in
`references/input-injection.md`.

## Screenshots and visual verification

Screenshots are the observation step of the change/observe/verify loop: look
at the rendered result instead of trusting the values you set. Capture after
UI or scene appearance changes (editor and in-game), at key moments of a
playtest, and before and after click automation.

Three tools, three targets:

- `capture_game_viewport` — the running game's viewport over the runtime
  channel (`data`, `format`, `width`, `height`). Optional `timeout_ms` bounds
  the wait (default 5000, max 30000).
- `capture_editor_viewport` — the default `target` `editor` grabs the editor
  2D viewport (falling back to the 3D viewport), so it works while you are
  still building a scene; `target` `game` captures the running game instead.
  Add save=true on the editor target to also write the PNG under
  user://godot_autopilot/captures/ and receive its path in the result; both
  targets keep only the 20 most recent capture files.
- `capture_display_screen` — a whole physical screen by `screen` index, for
  desktop-level checks such as window placement; no running game required.

Delivery: through `call_tool` the base64 PNG arrives as an MCP image content
block, so a multimodal model sees the picture directly. The text JSON keeps
`format`, `width` and `height`, and its `data` field reads
`"<attached-as-image-content>"` with image_attached: true. Inside
`batch_execute` and `code_execute` no image block is attached — the JSON
keeps the full base64.

Limits: captures are capped at 4096 pixels per side and an 8 MiB PNG, and a
capture whose base64 payload would exceed the 4 MiB JSON response cap is
rejected with a response_too_large-style error (base64 inflates by about
one third), so prefer moderate resolutions.

Pair screenshots with `get_game_ui_elements`: enumerate the controls first
(`path`, `global_rect`), inject the click with `sequence_game_inputs` or
`queue_game_input`, then capture again and re-enumerate — screens change
under fixed coordinates.

## Enumerate the running game's UI

`get_game_ui_elements` walks the running game's scene tree and returns every
Control-derived node with:

- `path` (absolute node path), `type` (class name), `visible` (including
  ancestors), `text` (when present) and `global_rect` (`position` and
  `size` in viewport coordinates).

`max_elements` defaults to 100 (max 1000); `truncated` set to true signals
more elements remain — raise the cap or target a subtree via
`execute_game_script`. This is the reconnaissance step before click
automation: locate buttons and panels here, then drive them with
`sequence_game_inputs` or `queue_game_input` using `global_rect`
coordinates, or with `execute_game_script` using `path`.

## Pause, reload and run code in the game

- `set_scene_tree_pause` (`paused` boolean) pauses or resumes the running
  scene tree; `is_scene_tree_paused` confirms the state. Pause semantics are
  a matrix, not a switch: nodes keep processing according to their process
  mode (`INHERIT` resolves up the tree, the root is fixed `PAUSABLE`), and
  pausing also deactivates the physics servers — the simulation itself stops,
  not just node callbacks. Transient input states are lost while paused.
  The full matrix is in `references/input-injection.md`.
- `reload_scene_tree_current_scene` reloads the currently running scene from
  disk, recreating the scene root — script state, connections and runtime
  modifications reset to the saved file. Without a running scene it returns
  `ERR_UNCONFIGURED` (3).
- `execute_game_script` runs code inside the game process: `action` `script`
  (the source must extend Node and define `_run()`), `get_property`,
  `set_property` or `call_method`. `persist` (default false) keeps the
  temporary node alive under `/root/__gda_runtime`.
- `reload_game_scripts` reloads GDScript files in the running game without
  restarting it (pass a `paths` array of `res://` script paths). The request
  is applied during the game's next idle poll and no confirmation is
  returned — verify with `get_game_log_entries` or `get_game_status`.

## Four log and error paths

| Path | Tools | Source | Needs a running game |
|---|---|---|---|
| Editor engine log | `get_debugger_log` | engine log buffer of the editor process: script errors and messages routed through the engine logger | no — always contains data |
| Debugger session capture | `get_debugger_errors`, `get_debugger_output`, `get_debugger_scene_tree` | with an active debug session: the running game over the runtime channel; without one: an empty result plus a `note` — no editor-side fallback | live game data needs a session |
| On-disk game log | `get_game_log_entries` | tail window of the game process log file `user://logs/godot.log` | the log file must exist — start the game once with `play_editor_current_scene` |
| Plugin LogSystem | `get_plugin_log` | the plugin's own in-process diagnostic buffer (authorization denials, timeout bookkeeping, dropped late game responses) — not routed through any engine logger | no — always available |

- `get_debugger_log` (optional `limit`, default 50) reads the editor engine
  log — script errors and `print` output from the editor process. It never
  requires a running game; read it first after editing any `.gd`, `.tscn`,
  `.tres` or `.cs` file, before changing code again.
- `get_plugin_log` (optional `limit`, default 100, max 1000; `level`,
  `category`, `filter`, `since_index`) reads the plugin's own in-process
  diagnostics and returns the entries array with a count and next_index.
  Unlike `get_debugger_log` it does not read the engine log buffer; this is
  where authorization denials, timeout bookkeeping and dropped late game
  responses appear.
- `get_debugger_errors` (optional `limit`, default 20) returns, with an
  active session, a structured list under `result` with `time`, `file`,
  `func`, `line`, `error`, `descr`, is_warning and `stack` per entry.
- `get_debugger_output` reads stdout/stderr captured from the game process
  (optional `limit`, default 50); `get_debugger_scene_tree` takes no
  parameters and returns the game's tree as formatted text.
- Without an active debug session, `get_debugger_errors`,
  `get_debugger_output` and `get_debugger_scene_tree` return an empty result
  plus a `note` suggesting `play_editor_current_scene` or
  `get_game_log_entries` — there is no editor-process fallback.
- `get_game_log_entries` reads the on-disk log tail (optional `limit`,
  default 50, max 500; returns `path`, `entries`, `total_lines`). Pass an
  optional filter to keep only lines containing that case-sensitive
  substring: the scan then covers the last 2000 lines and the result adds
  matched_lines with the total matches found (an empty filter means no
  filtering). It falls back to the archived `godot.log.1` (reported as
  `from_archive`); if the file does not exist the error includes directory
  diagnostics and a hint to start the game.

The on-disk log matters more than it looks: the debugger transport silently
drops messages under load (see below), but the log file does not.

## Confirm a fix with the error watermark

Errors raised while the game runs are recorded by the plugin, and the next
response of any tool carries a top-level `new_errors_since_last_call` field.
The counter is one-shot: it is consumed when read, so each response reports
exactly the errors recorded since your previous call (a failed call counts
its own error too). The field is always present, including when it is 0.

The correct "I fixed it — confirm nothing new broke" loop:

1. Reproduce and read the errors with `get_debugger_errors` or
   `get_game_log_entries`.
2. Apply the fix.
3. Re-run the game and exercise the broken code path.
4. Make any next tool call and read `new_errors_since_last_call`: 0 means
   nothing new was recorded; more means new errors appeared — read them
   before moving on.

One caveat keeps this loop honest: **why you may read no new errors even
when errors happened**. The engine's debug transport has three drop gates —
messages sent before a debugger peer connects are lost entirely (early
startup errors never surface), error/warning floods are silently discarded
for up to a second, and a message-queue overflow drops messages wholesale.
A watermark of 0 therefore means "nothing recorded since last call", not
"nothing happened": for error-storm code paths, cross-check with
`get_game_log_entries`, which bypasses all three gates. Details in
`references/debug-paths.md`.

## Close the loop with inline GDScript tests

`run_gdscript_tests` runs inline test cases with a built-in assertion
framework. Each item of `tests` is a `{name, source}` object; the source
must be plain sequential statements (top-level `func`/`static`/`class`
declarations are rejected), is wrapped into a temporary `@tool` Node
script, executed serially and removed afterwards so the tree stays clean.
The edited scene root is exposed as `SceneRoot`.

Assertions available inside a source:

- `check(condition, msg="")` — records a failure when false, keeps executing.
- `check_equal(actual, expected)` — Variant equality.
- `check_almost_equal(actual, expected, epsilon=0.001)` — numeric absolute
  difference; non-numeric values fall back to `check_equal` semantics.
- `fail(msg)` — records a failure and keeps executing.
- `fatal(msg)` — records a failure and immediately stops this case.

A failed assertion does not abort the case; every check appends one row to
`checks`. Optional `timeout_ms` (default 10000, hard maximum 30000) budgets
the whole suite: once exhausted, remaining cases get status `skip` with
reason `timeout budget exhausted`, and the running case is flagged
`running_over_budget` (synchronous GDScript cannot be interrupted).

`run_gdscript_test_files` runs the same framework over files in `directory`
(default `res://tests`, non-recursive) filtered by `pattern` (default
`*.gda_test.gd`); each file is one case named by its `res://` path.

```json
{"name": "run_gdscript_tests", "arguments": {"tests": [
  {"name": "math", "source": "check_equal(1 + 1, 2)\ncheck_almost_equal(sqrt(2.0), 1.414, 0.01)\ncheck(SceneRoot != null, \"scene must be open\")"}
]}}
```

The response contains a `summary` object (`total`, `passed`, `failed`,
`skipped`) and a `cases` array (`name`, `status`, `checks`, `error`,
`elapsed_ms`), status one of `pass`, `fail` or `skip`. Verification loop:
modify code, run `run_gdscript_tests`, re-run the game, then confirm via
`get_game_status`, `get_game_log_entries` and
`new_errors_since_last_call`.

## Debug helpers while the game runs

- Monitors: `get_debug_monitor_catalog` lists every built-in monitor with
  `id`, `name` and `type` — the authoritative source of ids, never guess;
  `get_debug_monitor` reads one by integer id; `get_debug_monitors` returns
  a full snapshot of all 58 monitors. Custom monitors via
  `get_debug_custom_monitor_names`, `get_debug_custom_monitor` (JSON
  Variant value) and `remove_debug_custom_monitor`.
- Leaks and stacks: `get_debug_object_count` and `get_debug_node_count` —
  compare before/after an operation; a rising node count suggests leaks.
  `get_debug_memory_usage` is static engine memory only. `get_debug_stack`
  returns editor-process backtraces without a debug session.
- Visualization: `set_debug_collision_visual`, `set_debug_navigation_visual`
  and `set_debug_performance_visual` toggle overlays and persist in
  project.godot; `set_scene_tree_debug_collisions_hint` affects the current
  run only; `set_debug_physics_fps` speeds or slows physics — restore 60.

## Breakpoints and auto-resume

When the game stops at a debugger breakpoint, `get_debugger_session_info`
reports the session as breaked. By default the plugin automatically continues
breaked sessions so `game_*` calls keep working: auto-resume is controlled by
the `GDA_AUTO_CONTINUE` environment variable (`0` or `false` disables it),
with at most 3 auto-continues per session. What a breakpoint does to the game
process is in `references/debug-paths.md`.

## See also

- `references/input-injection.md` — frame-accurate injection, action
  matching, per-device state and the full pause matrix.
- `references/debug-paths.md` — the four log paths, protocol drop gates and
  breakpoint semantics.
- `references/runtime-inspection.md` — reading liveness, UI and scene state
  of the running game.
- `godot-autopilot-scripting` — the four script execution channels,
  including `execute_game_script`.
- `godot-autopilot` — tool discovery protocol, watermark details and
  retryable soft errors.
