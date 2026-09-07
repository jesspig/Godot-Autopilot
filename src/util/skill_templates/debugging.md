# Debugging Godot Projects with godot-autopilot

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
