# Runtime and Debugging

Launching the game, the game runtime channel, input injection, screenshots, UI
reconnaissance, logs, the error watermark, and inline tests.

## Start and stop

- `play_editor_current_scene` launches the edited scene. Save it first or
  playback may fail. It returns `ok` or `already_playing` plus `is_playing`; it
  never restarts a running game. Call `stop_editor_playing` first for a fresh
  run.
- `stop_editor_playing` returns to editing mode and is safe even when no game is
  running.

## Game channel prerequisites

All `game_*` tools travel to the running game through the editor debug session.
The game must be launched from the editor, and the game project must load the
GDA extension. Without an attached game they fail with `game not running`.

When attached but not ready, the default `wait_ready: true` queues the request
and replays it after `gda:ready`; `wait_ready: false` fails immediately. A queued
call times out if `gda:ready` never arrives. If `wait_ready: false` keeps failing
while a game is attached, the game project does not load the extension.

Game calls wait `timeout_ms` plus 2000 ms, capped at 25000; budget long work as
shorter calls and poll `get_game_status`.

## Liveness

`get_game_status` reports engine version, current scene, node count, FPS,
`paused`, `physics_frame`, `healthy`, and `physics_stalled`.

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
`SceneTree`. Drive the game with `queue_game_input`, `wait_game_input`,
`sequence_game_inputs`, or `execute_game_script`.

## Input injection

- `queue_game_input` sends one input. `type` is `key`, `mouse_button`, or
  `action`; `mode` is `event` (parsed event), `api` (direct action state), or
  `hold` (event held for `duration_ms`). Unknown parameters fail; `api` creates
  no `InputEvent`, so input callbacks never see it, and `event` is required for
  UI.
- `wait_game_input` waits for `just_pressed` (default), `just_released`, or
  `pressed` in a physics frame, up to `timeout_ms` (default 2000, max 25000).
  Optional `inject` sends the input first; transient states require it.
- `sequence_game_inputs` fires a timeline on exact physics frame offsets (max
  256 items, each with `kind` and `at_frame`). Use it for playtest automation.

Timing facts:

- `just_pressed` has separate process and physics counters. Poll transient
  states in `_physics_process`, not `_process`; the earliest physics-context
  agreement is the next tick.
- Parsed events are buffered and flushed at the end of the display server pass,
  so they dispatch at the start of the next frame. Never assume
  inject-then-read lands on the same frame.

Frame-accuracy recipe: one `sequence_game_inputs` timeline with explicit
`at_frame` offsets, then a conditioned capture.

## Screenshots

- `capture_game_viewport` captures the running game; `timeout_ms` defaults to
  5000 (max 25000).
- `capture_editor_viewport` defaults to the editor viewport and accepts
  `target: game`. `save=true` or `save_dir` writes a PNG into the GDA capture
  folder or a `res://` directory inside the project; other paths are rejected.
  Only the 20 most recent captures are kept.
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

`get_game_ui_elements` returns every Control with `path`, `type`, `visible`,
`text`, `global_rect`, `mouse_filter`, and `z_index`. `mouse_filter` and
`z_index` are the hit-testing and draw-order facts; a full-screen `PASS` control
can swallow clicks, while `IGNORE` passes them through. `max_elements` defaults
to 100 (max 1000); `truncated` means more remain.

`validate_game_ui_layout` reports zero-size, off-viewport, or possibly occluded
controls. Validate first, click with `click_game_ui_element`, and check
`hit_path`; `hit_mismatch` fails only with `strict_hit: true`.

## Pause, reload, and game code

- `set_scene_tree_pause` pauses or resumes; `is_scene_tree_paused` confirms.
  Process mode still decides which nodes run, and pausing deactivates the
  physics servers. Transient input states are lost while paused.
- `reload_scene_tree_current_scene` reloads the running scene from disk and
  resets script state, connections, and runtime modifications. Without a scene
  it returns `ERR_UNCONFIGURED`.
- `execute_game_script` runs in the game. `script` accepts a complete GDScript
  defining `func _run()` or bare statements wrapped the same way; a value needs
  an explicit return. `get_property`, `set_property`, and `call_method` inspect
  or modify node state. `persist` keeps the temporary node under
  `/root/__gda_runtime`.

## Log and error paths

| Path | Tools | Source | Needs a game |
|---|---|---|---|
| Editor engine log | `get_debugger_log` | Editor engine log buffer | No |
| Debugger session | `get_debugger_errors`, `get_debugger_output`, `get_debugger_scene_tree` | Running game over the runtime channel | Yes for live data |
| On-disk game log | `get_game_log_entries` | `user://logs/godot.log` tail | The log file must exist |
| Plugin log | `get_plugin_log` | Plugin diagnostics: denials, timeouts, dropped responses | No |

- `get_debugger_log` (`limit` default 50) reads editor script errors and print
  output. Read it after editing any `.gd`, `.tscn`, `.tres`, or `.cs`.
- `get_plugin_log` (`limit` default 100, max 1000) reads plugin diagnostics, not
  the engine log.
- `get_debugger_errors` returns structured entries (`time`, `file`, `func`,
  `line`, `error`, `descr`, `stack`); `get_debugger_output` reads captured
  stdout/stderr; `get_debugger_scene_tree` returns the game tree as text.
  Without a session they return an empty result plus a note.
- `get_game_log_entries` reads the on-disk tail (`limit` default 50, max 500)
  and falls back to `godot.log.1`. It bypasses the debugger transport's drop
  gates, so it is the reliable path for error storms.

## Error watermark

Every tool response carries `new_errors_since_last_call`, counting errors since
the previous call, including the call's own error. It is one-shot.

1. Reproduce and read errors with `get_debugger_errors` or
   `get_game_log_entries`.
2. Fix, rerun, and exercise the broken path.
3. Make any next call and read the watermark: 0 means nothing new was recorded;
   more means read them before moving on.

Zero does not prove nothing happened. The debugger transport drops messages
before a peer connects, discards error floods for up to a second, and drops on
queue overflow. Cross-check error-storm paths with `get_game_log_entries`.

## Inline GDScript tests

`run_gdscript_tests` runs `{name, source}` cases with a built-in assertion
framework. Sources are sequential statements (no top-level `func`, `static`, or
`class`), wrapped into a temporary `@tool` node, run serially, and removed.
`SceneRoot` is the edited scene root.

Assertions: `check`, `check_equal`, `check_almost_equal`, `fail`, and `fatal`.
A failed assertion records a row and continues; `fatal` stops that case. A
`timeout_ms` budget (default 10000, max 30000) marks remaining cases `skip` when
exhausted. `run_gdscript_test_files` runs the framework over `directory` (default
`res://tests`, non-recursive) filtered by `pattern` (default `*.gda_test.gd`).
The response has a `summary` (`total`, `passed`, `failed`, `skipped`) and
`cases` with `status` (`pass`, `fail`, `skip`).

## Debug helpers

- `get_debug_monitor_catalog` is the authoritative monitor id list;
  `get_debug_monitor` reads one; `get_debug_monitors` snapshots all.
- `sample_game_property` samples a game property across frames.
  `get_debug_object_count` and `get_debug_node_count` expose leaks;
  `get_debug_memory_usage` is static engine memory.
- Breakpoints: `get_debugger_session_info` reports a breaked session. The plugin
  auto-continues up to 3 times per session unless `GDA_AUTO_CONTINUE` is `0` or
  `false`; blocked game calls time out rather than returning stale state.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| `game not running` | Game not launched from the editor | `play_editor_current_scene` |
| Game call times out after play | `gda:ready` never arrived | Check the game project loads the extension |
| No debugger errors | No session or transport drop | Read `get_game_log_entries` |
