# Runtime and Debugging

Launching the game, the game runtime channel, input injection, screenshots, UI
reconnaissance, logs, the error watermark, and inline tests.

## Start and stop

- `editor_scene_control` action=play launches the edited scene. Save it first or
  playback may fail. It returns `ok` or `already_playing` plus `is_playing`; it
  never restarts a running game. Call action=stop first for a fresh run.
- action=stop returns to editing mode and is safe even when no game is running.

## Game channel prerequisites

All game-domain tools travel to the running game through the editor debug
session. The game must be launched from the editor, and the game project must
load the GDA extension. Without an attached game they fail with
`game not running`. Mutating game tools are GameRuntime and require
`confirm: true`; a confirmation error is a stop-and-review signal, not a
channel failure.

When attached but not ready, the default `wait_ready: true` queues the request
and replays it once the game reports `gda:ready` with `scene_ready` (current
scene in the tree); `wait_ready: false` fails immediately. A queued call times
out (`game_ready_timeout`) if readiness never arrives. The editor sends
`gda:hello` to each attached session until the game replies with a real
`scene_ready`; `game_ready_timeout` and `get_game_state(session)`
now carry `ready_diagnosis` (`extension_not_loaded` / `scene_not_ready` /
`ready_lost` / `ready`) plus `hello_attempts`. After a force restart,
wait for scene-ready before the first call. If `wait_ready: false` keeps failing
while a game is attached, the game project does not load the extension.

Game calls wait `timeout_ms` plus 2000 ms, capped at 25000; budget long work as
shorter calls and poll `get_game_state` action=status.

## Liveness

`get_game_state` action=status reports engine version, current scene, node
count, FPS, `paused`, `physics_frame`, `healthy`, and `physics_stalled`.

- `healthy` means activity within the last 3000 ms; `last_activity_ms` is the
  age.
- `physics_stalled` means `physics_frame` did not advance while FPS is positive.
- `physics_frame` is engine-global and keeps advancing while paused, so combine
  `paused` with a gameplay-owned value (node position, gameplay tick, or probe).
- A failed scene reload appears as an empty scene with a sharp `node_count`
  drop; stop and rerun instead of debugging a half-dead process.

## Editor and game are two processes

Editor input tools inject into the editor process; a running game never
receives them. Editor scripts also cannot touch the game's `Input` or
`SceneTree`. Drive the game with `game_input` action=queue, action=wait,
action=sequence, or `game_script` action=execute.

## Input injection

- `game_input` action=queue sends one input. `type` is `key`, `mouse_button`,
  `wheel`, `mouse_motion`, or `action`; `mode` is `event` (parsed event), `api`
  (direct action state), or `hold` (re-pressed every physics frame,
  auto-released when `duration_ms` expires). Unknown parameters fail; `api`
  creates no `InputEvent`, so input callbacks never see it, and `event` is
  required for UI. `event` with `duration_ms=0` presses without a release.
  `click` pairs a press with a release two physics frames later; `double_click`
  adds a second pair.
- action=wait waits for `just_pressed` (default), `just_released`, or `pressed`
  in a physics frame, up to `timeout_ms` (default 5000, max 25000). Optional
  `inject` sends the input first; transient states require it.
- action=sequence fires a timeline on exact physics frame offsets (max 256
  items, each with its `at_frame` offset). Use it for playtest automation.

Timing facts:

- `just_pressed` has separate process and physics counters. Poll transient
  states in `_physics_process`, not `_process`; the earliest physics-context
  agreement is the next tick.
- Parsed events are buffered and flushed at the end of the display server pass,
  so they dispatch at the start of the next frame. Never assume
  inject-then-read lands on the same frame.
- Read transient key state back inside the hold window (inject-to-read gap
  shorter than `duration_ms`); after auto-release `pressed` is false again,
  while persistent effects such as accumulated movement remain. Pair the
  readback with a gameplay-owned counter.
- Injection goes through `Input.parse_input_event`, which never reaches
  `_unhandled_input`; poll `Input.is_action_pressed` in `_process` instead.

Frame-accuracy recipe: one `game_input` action=sequence timeline with explicit
physics-frame offsets, then a conditioned capture.

## Screenshots

- `capture_editor_viewport` captures the editor viewport by default; pass
  `target: "game"` to capture the running game. The game target writes a PNG in
  the game process and returns its path; `timeout_ms` defaults to 5000 (max
  25000) for that target.
- The editor 2D viewport falls back to 3D when degenerate (rendered image ≤8 px
  on a side, or zero visible rect); both unusable fails with `no_scene_open`
  (no scene open) or `viewport_degenerate` plus size diagnostics — no silent
  placeholder image.
- The editor target rejects `after_frames` and `when`.
- `save=true` or `save_dir` writes a PNG into the GDA capture folder or a
  `res://` directory inside the project; other paths are rejected. Only the 20
  most recent captures are kept.
- `scale` (1-8) upscales nearest-neighbour to read small HUD text; crop, then
  scale, then `max_dimension`.
- `after_frames` waits N rendered frames. `when` is a GDScript expression
  evaluated each frame; capture fires on the first true frame.
  `when_satisfied_frame` precedes `captured_frame`.
  `when` runs with the current scene as base; malformed expressions fail, runtime
  errors attach to `when_timeout`, and text conditions must also check visibility.
  Captures are capped at 4096 pixels per side and an 8 MiB PNG; a base64 payload
  over the 4 MiB JSON response cap is rejected.

## UI reconnaissance

`get_game_ui` action=elements returns every Control with `path`, `type`,
`visible`, `text`, `global_rect`, `mouse_filter`, and `z_index`. `mouse_filter`
and `z_index` are the hit-testing and draw-order facts; a full-screen `PASS`
control can swallow clicks, while `IGNORE` passes them through. `max_elements`
defaults to 100 (max 1000); `truncated` means more remain.

`get_game_evidence` action=validate reports zero-size, off-viewport, or possibly
occluded controls. Validate first, click with `click_game_ui_element`, and check
`hit_path`; `hit_mismatch` fails only with `strict_hit: true`.

## Pause, reload, and game code

- `game_scene_tree` action=set_pause pauses or resumes; action=is_paused
  confirms. Process mode still decides which nodes run, and pausing deactivates
  the physics servers. Transient input states are lost while paused.
- action=reload_current_scene reloads the running scene from disk and resets
  script state, connections, and runtime modifications. Without a scene it
  returns `ERR_UNCONFIGURED`.
- `game_script` action=execute runs in the game. `script_action` is `script`
  (with `source_code`), `get_property`, `set_property`, or `call_method`.
  A script needs an explicit return to produce a value. Bare statements wrap
  into `@tool extends Node` + `func _run()` on a temporary node under `/root`,
  outside the current scene — reach scene nodes with absolute paths
  (`/root/...`) or `../`; the node is freed after the call unless `persist`
  keeps it under `/root/__gda_runtime`.

## Log and error paths

| Path | Tools | Source | Needs a game |
|---|---|---|---|
| Editor engine log | `get_debugger_stream` action=log (source=editor) | Editor engine log buffer | No |
| Debugger session | `get_debugger_stream` action=errors / action=output, `get_debugger_scene_tree` | Running game over the runtime channel | Yes for live data |
| On-disk game log | `get_game_log_entries` | `user://logs/godot.log` tail | The log file must exist |
| Plugin log | `get_plugin_log`, or `get_debugger_stream` action=log (source=plugin) | Plugin diagnostics: denials, timeouts, dropped responses | No |

- action=log (`limit` default 50) reads editor script errors and print output.
  Read it after editing any `.gd`, `.tscn`, `.tres`, or `.cs`.
- action=log with source=plugin (`limit` default 100, max 1000) reads plugin
  diagnostics, not the engine log.
- action=errors returns structured entries (`time`, `file`, `func`, `line`,
  `error`, `descr`, `stack`); action=output reads captured stdout/stderr;
  `get_debugger_scene_tree` returns the game tree as text. Without a session
  they return an empty result plus a note.
- `get_game_log_entries` reads the on-disk tail (`limit` default 50, max 500)
  and falls back to `godot.log.1`. It bypasses the debugger transport's drop
  gates, so it is the reliable path for error storms.

## Error watermark

Every tool response carries `new_errors_since_last_call`, counting errors since
the previous call, including the call's own error. It is one-shot.

1. Reproduce and read errors with `get_debugger_stream` action=errors or
   `get_game_log_entries`.
2. Fix, rerun, and exercise the broken path.
3. Make any next call and read the watermark: 0 means nothing new was recorded;
   more means read them before moving on.

Zero does not prove nothing happened. The debugger transport drops messages
before a peer connects, discards error floods for up to a second, and drops on
queue overflow. Cross-check error-storm paths with `get_game_log_entries`.

## Inline GDScript tests

`run_gdscript_tests` runs inline `tests` cases (`{name, source}`) or files under
`directory` (default `res://tests`, non-recursive) filtered by `pattern`
(default `*.gda_test.gd`). Sources are sequential statements (no top-level
`func`, `static`, or `class`), wrapped into a temporary `@tool` node, run
serially, and removed. `SceneRoot` is the edited scene root.

Assertions: `check`, `check_equal`, `check_almost_equal`, `fail`, and `fatal`.
A failed assertion records a row and continues; `fatal` stops that case. A
`timeout_ms` budget (default 10000, max 30000) marks remaining cases `skip` when
exhausted. The response has a `summary` (`total`, `passed`, `failed`, `skipped`)
and `cases` with `status` (`pass`, `fail`, `skip`); file mode also returns the
executed `files`.

## Debug helpers

- `get_debug_info` action=catalog is the authoritative monitor id list;
  action=monitors reads values; action=custom reads one custom monitor;
  action=object / action=node / action=memory expose counts and static memory.
- `get_game_evidence` action=sample samples a game node property across frames.
- Breakpoints: `debugger_breakpoints` action=list / set / clear;
  `get_debugger_stream` action=session reports a breaked session. The plugin
  auto-continues up to 3 times per session unless `GDA_AUTO_CONTINUE` is `0` or
  `false`; blocked game calls time out rather than returning stale state.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| `game not running` | Game not launched from the editor | `editor_scene_control` action=play |
| Game call times out after play | `gda:ready`/scene-ready never arrived | Check the game project loads the extension; wait for scene-ready |
| No debugger errors | No session or transport drop | Read `get_game_log_entries` |
