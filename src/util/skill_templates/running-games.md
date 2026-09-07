# Running Games with godot-autopilot

This book covers launching the game from the editor, the `game_*` runtime
channel that talks to the running game process, input injection, viewport
capture and in-game UI reconnaissance. For logs, performance monitors and
inline tests see `godot-autopilot-debugging`.

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

```json
{"name": "play_editor_current_scene", "arguments": {}}
```

## Prerequisites of the game_* runtime channel

All `game_*` tools (and `capture_editor_viewport` with `target` set to `game`)
send requests to the running game process through the editor debug session.
Two prerequisites must hold:

1. The game was launched from the editor with `play_editor_current_scene`.
2. The game project loads the godot-autopilot extension (plugin enabled in
   that project).

Until the game reports ready, tools fail with:

```text
game not ready: the game process has not reported gda ready yet — wait a
moment after play, or verify the game project loads the godot-autopilot
extension
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
  `last_activity_ms`); use it together with `physics_stalled` to distinguish
  a live game from a fake run.
- `physics_stalled` — `physics_frame` has not advanced while FPS is positive,
  i.e. a frozen physics loop.

A failed scene reload shows up as an empty scene with a sharp `node_count`
drop. Do not debug a half-dead process: stop with `stop_editor_playing` and
rerun `play_editor_current_scene`.

## Editor-side input tools do NOT reach the game

**Warning:** the editor-side input tools — `press_input_action`,
`release_input_action`, `is_input_action_pressed`,
`is_input_action_just_pressed`, `press_input_key`, `release_input_key`,
`move_input_mouse`, `press_input_mouse_button`,
`release_input_mouse_button`, `start_input_gamepad_vibration`,
`stop_input_gamepad_vibration` — inject events into the **editor process**.
A running game is a separate process and never receives these events; the
game is not affected by them at all.

To drive the running game use `queue_game_input`, `wait_game_input`,
`sequence_game_inputs` (below) or `execute_game_script`.

## Inject single inputs with queue_game_input

`queue_game_input` simulates one input inside the running game:

- `type` is `key`, `mouse_button` or `action`; `mode` is `event` (default,
  parsed input event), `api` (immediate action press/release) or `hold`
  (event-style, kept pressed for `duration_ms`).
- Parameters outside the whitelist (`type`, `keycode`, `pressed`,
  `button_index`, `position`, `action`, `duration_ms`, `mode`, `timeout_ms`)
  are ignored and reported back in `ignored_params` with a warning — a typo
  does not fail the call, so read the warning.
- A just-pressed state is transient: it is visible only in the first physics
  frame after injection and is lost while the game is paused. Transient states
  are tracked against physics frames only — polling in `_process` (render
  frame) will not observe injected input; poll in `_physics_process`.

```json
{"name": "queue_game_input", "arguments": {"type": "key", "keycode": "X"}}
```

## Wait for transient states with wait_game_input

`wait_game_input` waits until an action reaches a transient state in a
physics frame, or `timeout_ms` elapses (default 2000, max 30000):

- `state` is `just_pressed` (default), `just_released` or `pressed`.
- Optional `inject` sends input first in the same call, avoiding frame skew
  between round trips. `just_pressed`/`just_released` require `inject` —
  without it the one-frame transient window has already expired and the wait
  always times out.

## Schedule timelines with sequence_game_inputs

`sequence_game_inputs` fires a timeline of input events on exact physics
frame offsets. Unlike repeated `queue_game_input` round trips it avoids
cross-call latency jitter, which makes it the tool for playtest automation.

- Each item of `inputs` needs `kind` (`key`, `mouse_button`, `mouse_motion`
  or `action`) and `at_frame` (non-negative integer physics frame offset,
  relative to sequence start; 0 = immediately; out-of-order entries fire when
  their frame arrives), plus the `queue_game_input` fields for that kind:
  `keycode` for `key`, `button_index` and optional `position` for
  `mouse_button`, `position` for `mouse_motion`, the action name for
  `action`. Optional per item: `pressed` (default true), `duration_ms`
  (auto-release) and `mode`.
- Max 256 items. `timeout_ms` defaults to `max(at_frame) * 33 + 2000` and is
  capped at 30000. The call resolves when every item has fired
  (`completed` true, `executed` n) or on timeout (`completed` false).

```json
{"name": "sequence_game_inputs", "arguments": {"inputs": [
  {"kind": "key", "keycode": "X", "at_frame": 0},
  {"kind": "mouse_button", "button_index": 1, "position": {"x": 120, "y": 80}, "at_frame": 5},
  {"kind": "action", "action": "jump", "at_frame": 15}
]}}
```

## Diagnose input with get_game_input_status

`get_game_input_status` reports an action's `pressed`, `just_pressed`,
`just_released` and the current `physics_frame`. Use it to check whether
injections land and whether the game is paused (transient states are lost
while paused). Successful responses also include `recent_engine_errors` (up
to 5 recent engine errors) when any exist — often the reason input appears
ignored.

## Capture the game viewport

- `capture_game_viewport` returns the running game's viewport as a
  base64-encoded PNG (`data`, `format`, `width`, `height`). Optional
  `timeout_ms` bounds the capture wait (default 5000, max 30000). Captures
  are limited to 4096 pixels per side and an 8 MiB PNG.
- `capture_editor_viewport` with `target` set to `game` captures the same
  running game from the editor side and returns the same result shape.

## Enumerate the running game's UI

`get_game_ui_elements` walks the running game's scene tree and returns every
Control-derived node with:

- `path` (absolute node path), `type` (class name), `visible` (visibility
  including ancestors), `text` (when the control has a text property) and
  `global_rect` (`position` and `size` in viewport coordinates).

`max_elements` defaults to 100 (max 1000); `truncated` set to true signals
more elements remain — raise the cap or target a subtree via
`execute_game_script`. This is the reconnaissance step before click
automation: locate buttons and panels here, then drive them with
`sequence_game_inputs` or `queue_game_input` using `global_rect`
coordinates, or with `execute_game_script` using `path`.

## Pause, reload and run code in the game

- `set_scene_tree_pause` (`paused` boolean) pauses or resumes the running
  scene tree; only nodes whose process mode permits WhenPaused keep
  processing. Confirm the state with `is_scene_tree_paused`. Transient input
  states are lost while paused.
- `reload_scene_tree_current_scene` reloads the currently running scene from
  disk, recreating the scene root — script state, connections and runtime
  modifications reset to the saved file. Without a running scene it returns
  `ERR_UNCONFIGURED` (3).
- `execute_game_script` runs code inside the game process: `action` `script`
  (the source must extend Node and define `_run()`), `get_property`,
  `set_property` or `call_method`. `persist` (default false) keeps the
  temporary node alive under `/root/__gda_runtime` and returns its path for
  later calls.
- `reload_game_scripts` reloads GDScript files in the running game without
  restarting it (pass a `paths` array of `res://` script paths; multiple
  files in one call). The request is applied during the game's next idle poll
  and no confirmation is returned — verify with `get_game_log_entries` or
  `get_game_status`.

## Breakpoints and auto-resume

When the game stops at a debugger breakpoint, `get_debugger_session_info`
reports the session as breaked. By default the plugin automatically continues
breaked sessions so `game_*` calls keep working:

- Auto-resume is controlled by the `GDA_AUTO_CONTINUE` environment variable;
  set it to `0` or `false` to disable.
- Each session is auto-continued at most 3 times; after that the game stays
  paused at the breakpoint.

TODO(verify): a plugin diagnostic suggests that setting `GDA_AUTO_CONTINUE`
to a larger value allows more auto-continues per session, but the
implementation verifiably only treats `0`/`false` as special and uses a
fixed cap of 3 — do not rely on a higher cap without confirming.

## See also

- `godot-autopilot-debugging` — logs, errors, monitors and inline tests for a
  running game.
- `godot-autopilot-inspection` — reading the running game's scene tree and
  UI.
- `godot-autopilot-scripting` — the three script execution channels compared.
- `godot-autopilot-usage` — tool discovery protocol and the error watermark.
