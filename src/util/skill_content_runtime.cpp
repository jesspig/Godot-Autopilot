#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kRunningGamesDescription =
    R"gda_skill(Running the game and simulating input via godot-autopilot: play and stop the current scene, the game_* runtime channel status, input injection into the running game (queue/wait/sequence), and viewport capture. Use when launching the project, simulating gameplay or taking game screenshots. Editor-side input tools do NOT reach the game.)gda_skill";

const char *kDebuggingDescription =
    R"gda_skill(Debugging Godot projects via godot-autopilot: three complementary log and error paths (debugger capture, in-game buffers, on-disk log), performance monitors, memory and object counters, call stacks, pause and reload, and inline GDScript tests. Use when the game misbehaves, prints errors, or you need runtime diagnostics.)gda_skill";

const char *kInspectionDescription =
    R"gda_skill(Reading scene, node, engine and project information via godot-autopilot: editor scene tree and properties, the running game's scene tree and UI elements, engine stats and performance, ClassDB reflection queries, and project health analysis. Use when you need to know what exists or what state something is in.)gda_skill";

const char *kRunningGamesBody =
    R"gda_skill(# Running Games with godot-autopilot

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
)gda_skill";

const char *kDebuggingBody =
    R"gda_skill(# Debugging Godot Projects with godot-autopilot

Three complementary paths cover what the engine is telling you: the editor
log buffer, the debugger session capture of the running game, and the game's
on-disk log file. Performance monitors, memory counters, visualization
toggles and inline GDScript tests complete the toolbox.

## Three log and error paths

| Path | Tools | Source | Needs a running game |
|---|---|---|---|
| Editor engine log | `get_debugger_log` | engine log buffer of the editor process: script errors and messages routed through the engine logger | no — always contains data |
| Debugger session capture | `get_debugger_errors`, `get_debugger_output`, `get_debugger_scene_tree` | with an active debug session: the running game over the runtime channel; without one: editor-process captured errors/output or the last editor-captured tree | live game data needs a session |
| On-disk game log | `get_game_log_entries` | tail window of the game process log file `user://logs/godot.log` | the log file must exist — start the game once with `play_editor_current_scene` |

Details that matter:

- `get_debugger_log` takes an optional `limit` (default 50) and never
  requires a running game — read it first for script errors.
- `get_debugger_errors` (optional `limit`, default 20) returns a formatted
  text dump with time, file, line, error text and stack per error.
- `get_debugger_output` reads stdout/stderr captured from the game process
  (optional `limit`, default 50).
- `get_debugger_scene_tree` returns the running game's scene tree as a
  formatted text tree; it takes no parameters.
- When a debugger capture tool has nothing to return (no session and nothing
  captured), it returns an empty result plus a `note` field that suggests
  starting the game with `play_editor_current_scene` or reading
  `get_game_log_entries` instead.
- `get_game_log_entries` reads the on-disk log tail (optional `limit`,
  default 50, max 500) and returns `path`, `entries` and `total_lines`. It
  works without a debug session. If the file cannot be opened it falls back
  to the archived `godot.log.1` and reports `from_archive` with a warning;
  if the file does not exist the error includes directory diagnostics and a
  hint to start the game with `play_editor_current_scene`.

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
4. Make any next tool call and read `new_errors_since_last_call`: 0 means no
   new errors were recorded since the previous call; 1 or more means new
   errors appeared — read them before moving on.

## Performance monitors

- `get_debug_monitor_catalog` lists every built-in monitor with `id`, `name`
  and `type` (time, memory or quantity). It is the authoritative source of
  monitor ids — look ids up here, never guess.
- `get_debug_monitor` reads one monitor by integer id; ids outside the
  catalog range return an error.
- `get_debug_monitors` returns a full snapshot in one call: `{name, type,
  value}` entries covering 58 monitors, captured at the moment of the call.
- Custom monitors registered by game code: `get_debug_custom_monitor_names`
  lists the registered ids, `get_debug_custom_monitor` reads one value
  (serialized as a JSON Variant whose shape depends on the registration),
  and `remove_debug_custom_monitor` removes one so it stops consuming
  per-frame overhead during game runs.

## Memory, objects and call stacks

- `get_debug_object_count` and `get_debug_node_count` — compare counts
  before and after a scripted operation or scene change to detect leaks; a
  steadily rising node count suggests leaked nodes.
- `get_debug_memory_usage` — static engine memory in bytes only, not GPU or
  texture memory.
- `get_debug_stack` — script backtraces for all currently running scripts in
  the editor process; no debug session needed. `include_variables` adds
  global variables per backtrace plus local and member variables per frame.

## Debug visualization and physics FPS

- `set_debug_collision_visual`, `set_debug_navigation_visual` and
  `set_debug_performance_visual` toggle wireframe collision shapes,
  navigation geometry and the FPS/frame-time overlay while the game runs
  from the editor. All three are written to project metadata and persist in
  project.godot until changed back.
- `set_scene_tree_debug_collisions_hint` draws collision shapes for the
  current run only; it affects debug drawing, not physics behavior, and
  shapes render only while the game is running.
- `set_debug_physics_fps` slows down or speeds up physics simulation while
  debugging; the change is not persisted, so restore 60 afterwards.

## Pause, reload and session state

- `set_scene_tree_pause` and `is_scene_tree_paused` — pause or resume the
  running scene tree; only nodes whose process mode permits WhenPaused keep
  processing.
- `reload_scene_tree_current_scene` — reload the running scene from disk;
  runtime state resets to the saved file. Without a running scene it returns
  `ERR_UNCONFIGURED` (3).
- `get_debugger_session_info` — reports `active`, `breaked`, `running` and
  the session count; check it before using the other debugger tools.
  Breakpoint auto-resume behavior is described in
  `godot-autopilot-running-games`.

## Inline GDScript tests close the loop

`run_gdscript_tests` runs inline test cases with a built-in assertion
framework and returns a structured report. Each item of `tests` is a
`{name, source}` object; the source must be plain sequential statements
(top-level `func`/`static`/`class` declarations are rejected), is wrapped
into a temporary `@tool` Node script, executed serially in order and removed
afterwards so the scene tree stays clean. The edited scene root is exposed
as `SceneRoot`.

Assertions available inside a source:

- `check(condition, msg="")` — records a failure when the condition is false
  and keeps executing.
- `check_equal(actual, expected)` — uses Variant equality.
- `check_almost_equal(actual, expected, epsilon=0.001)` — numeric absolute
  difference; non-numeric values fall back to `check_equal` semantics.
- `fail(msg)` — records a failure and keeps executing.
- `fatal(msg)` — records a failure and immediately stops this case.

A failed assertion does not abort the case; every check appends one row to
`checks`. Optional `timeout_ms` (default 10000, hard maximum 30000) budgets
the whole suite: once exhausted, remaining cases get status `skip` with
reason `timeout budget exhausted`, and the currently running case is flagged
`running_over_budget` because synchronous GDScript execution cannot be
interrupted.

`run_gdscript_test_files` runs the same framework over files in `directory`
(default `res://tests`, non-recursive) filtered by `pattern` (default
`*.gda_test.gd`, wildcards supported); each file runs as one case named by
its full `res://` path and all cases share one optional `timeout_ms` budget.

```json
{"name": "run_gdscript_tests", "arguments": {"tests": [
  {"name": "math", "source": "check_equal(1 + 1, 2)\ncheck_almost_equal(sqrt(2.0), 1.414, 0.01)\ncheck(SceneRoot != null, \"scene must be open\")"}
]}}
```

The response contains a `summary` object (`total`, `passed`, `failed`,
`skipped`) and a `cases` array (`name`, `status`, `checks`, `error`,
`elapsed_ms`), with status one of `pass`, `fail` or `skip`. Verification
loop: modify code, run `run_gdscript_tests`, re-run the game with
`play_editor_current_scene`, then confirm via `get_game_status`,
`get_game_log_entries` and `new_errors_since_last_call`.

## Rename hints for Godot 3 to 4 errors

When `get_debugger_log` captures an "Invalid access to property or key"
error, the response automatically appends Godot 3 to 4 renamed-property
hints (10 in total, e.g. `frames` to `sprite_frames`, `cast_to` to
`target_position`, `translation` to `position`). Check the hints before
assuming a real bug — a stale pre-migration property name is a common cause.

## Standard error shapes

- A missing required parameter returns
  `{"error": "missing required parameter: <param>"}`.
- Calling an unknown domain tool through `call_tool` returns
  `{"error": "domain tool '<name>' not found — use search_tools to discover
  available tools"}`.

## See also

- `godot-autopilot-running-games` — launching the game, the game_* channel
  and input injection.
- `godot-autopilot-inspection` — read-only state queries for the editor,
  game and engine.
- `godot-autopilot-scripting` — script execution channels and hot reload.
- `godot-autopilot-tips-gotchas` — size limits and silent-failure traps.
)gda_skill";

const char *kInspectionBody =
    R"gda_skill(# Inspecting Godot Projects with godot-autopilot

Read-only queries for the edited scene, the running game, the engine and the
project layout — check what exists and what state something is in before you
modify it.

## Editor-side inspection

- `get_scene_tree` walks the currently edited scene, returning `name`,
  `type`, `path` and `children` for each node. Optional `max_depth` (default
  8, `-1` for unlimited) and `include_properties` (true adds up to 20
  properties per node, skipping `metadata/` keys, names starting with an
  underscore and Object-typed properties). Errors when no scene is open —
  create or open one first.
- `get_editor_edited_scene_root` returns the edited scene root's `name`,
  `type` and `path`; null when no scene is open. Start scene workflows here
  and combine with `get_scene_tree` for the full hierarchy.
- `property_get_list` lists every property of a node with its metadata
  (`name`, `type`, `hint`, `hint_string`, usage flags, `class_name`) —
  discover valid property names and enum ordering before `property_get` or
  `property_set`.
- `get_editor_selection` reports which nodes the user has selected (each
  with `name`, `class` and `path`; an empty array means nothing is
  selected). Use `set_editor_selection` to focus subsequent operations on
  specific nodes.
- `get_editor_file_system_tree` fetches the project file tree, optionally
  rooted at the given path; directory entries carry children, file entries
  carry `name`, `path` and `type`. Trees deeper than 12 levels are truncated
  and the response carries a `max_depth` field. After creating or modifying
  files outside the editor, call `scan_editor_file_system`, then poll
  `get_editor_file_system_status` until `scanning` is false.

## Running-game inspection

- `get_debugger_scene_tree` — the running game's scene tree (node names and
  types) as a formatted text tree. With an active debug session it reads the
  game over the runtime channel; without one it returns the last
  editor-captured tree. Takes no parameters.
- `get_game_ui_elements` — every Control-derived node in the running game
  with `path`, `type`, `visible`, `text` and `global_rect`; `max_elements`
  defaults to 100 (max 1000) and `truncated` signals more remain. See
  `godot-autopilot-running-games` for driving these elements.
- `get_game_status` — engine version, current scene, node count, FPS,
  `paused`, `physics_frame`, plus the `healthy` and `physics_stalled`
  liveness fields.

## Engine and system facts

All read-only:

- `get_engine_version` — `major`, `minor` and `patch` integers plus
  `string`, the full version label; use it to branch behavior on engine
  capabilities.
- `get_engine_fps` — the live measured frames per second;
  `get_engine_frames_drawn` — a monotonic total-frames counter (stall
  detection); `get_engine_time_scale` — the current global time scale.
- OS facts: `get_os_system_info`, `get_os_datetime`, `get_os_environment`,
  `get_os_locale`, `get_os_system_fonts`, `get_os_unique_id`,
  `get_os_unix_time`, `get_os_user_data_dir`.
- Display facts: `get_display_screen_count`, `get_display_screen_size`,
  `get_display_screen_dpi`, `get_display_screen_refresh_rate`,
  `get_display_screen_position`, `get_display_mouse_position`,
  `get_display_clipboard`, `get_display_tts_voices`.
- Project and editor settings: `get_project_settings` and
  `has_project_settings`; `get_editor_settings` and `has_editor_settings`.

## Query the Godot API by reflection, not from memory

Before calling an engine method or setting a property from memory, resolve
the real signature with the ClassDB reflection tools (signatures only — no
docstrings):

1. `find_docs_class` — case-insensitive substring search over all registered
   classes, up to 50 matches with `name`, parent and `api_type`. Confirm the
   exact class name or discover related classes first.
2. `get_docs_class` — the full reflected signature of one class:
   `parent_class`, `api_type`, `can_instantiate`, `methods`, `properties`,
   `signals`, `enums` and `constants`. Errors when the class does not exist.
3. `get_docs_method` — one method's signature (`name`, arguments, return
   type, flags); errors when the class or method does not exist.
4. `get_docs_property` — one property's info (`name`, type, hint, usage,
   `class_name`); errors when the class or property does not exist.

```json
{"name": "find_docs_class", "arguments": {"query": "AnimationPlayer"}}
{"name": "get_docs_class", "arguments": {"class": "AnimationPlayer"}}
```

This is cheaper and more reliable than guessing: an invented method or
property name only produces a confusing runtime error later.

## Project health checks

- `validate_scene_file` — dry-run validation of a `.tscn`/`.scn` file on
  disk without touching the currently edited scene: loads it bypassing the
  resource cache, checks every dependency for existence on disk, then
  instantiates and immediately frees the scene. Returns `valid`, `problems`
  (each `kind` is `load_failed`, `missing_dependency` or
  `instantiate_failed`), `missing_dependencies` and `dependency_count`. Use
  it after bulk edits or before committing scene changes.

```json
{"name": "validate_scene_file", "arguments": {"path": "res://levels/level_01.tscn"}}
```

- `find_unused_resources` — project files never referenced by any other
  file's dependencies under `directory` (default `res://`). Exempts
  `icon.svg`, the main scene and autoload paths read from project settings,
  `*.import` sidecars and project.godot; unresolvable `uid://` entries are
  listed in `unresolved_uids` instead of being misreported as unused.
  Read-only; large projects may take a few seconds.
- `trace_signal_flow` — signal wiring around a node of the edited scene:
  optional `direction` (`outgoing`, `incoming` or `both`, default both) and
  `max_depth` (default 3, cycles cut by a visited set). Each edge carries
  `from`, `signal`, `to`, `method` and `persisted` (true when the connection
  is saved in the scene file); non-node targets such as autoloads are shown
  as `Class#instance_id`.

## Screenshots and object lookup

- `capture_editor_viewport` — default `target` `editor` grabs the editor 2D
  viewport (falling back to the 3D viewport) and returns a base64 PNG with
  `data`, `format`, `width` and `height` — visually verify the scene while
  editing, e.g. after placing nodes or changing properties.
- `get_debug_object_info` — resolve an object instance ID to its identity:
  `class`, `name` and `node_path` for Nodes, `resource_path` for Resources.
  Use it on object IDs obtained from scene or physics tools; it errors on
  stale or invalid ids.

## See also

- `godot-autopilot-running-games` — launching the game and reading its UI.
- `godot-autopilot-debugging` — logs, monitors and tests once something
  looks wrong.
- `godot-autopilot-tool-map` — task-to-tool routing across all categories.
- `godot-autopilot-resources-files` — resource and file introspection
  details.
)gda_skill";

} // namespace

std::vector<SkillSpec> make_runtime_skills() {
  return {
      {"godot-autopilot-running-games", kRunningGamesDescription,
       {{"SKILL.md", kRunningGamesBody}}},
      {"godot-autopilot-debugging", kDebuggingDescription,
       {{"SKILL.md", kDebuggingBody}}},
      {"godot-autopilot-inspection", kInspectionDescription,
       {{"SKILL.md", kInspectionBody}}},
  };
}

} // namespace godot_autopilot::skill_gen
