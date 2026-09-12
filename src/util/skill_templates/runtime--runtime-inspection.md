# Runtime Inspection of the Running Game

Read-only probes for the running game: its scene tree, its UI controls, and
the liveness signals that tell a live game from a frozen or half-dead one.
Read these before you inject, modify or reload — and after, to verify.

## Scene tree: get_debugger_scene_tree

`get_debugger_scene_tree` returns the running game's scene tree (node names
and types) as a formatted text tree. It takes no parameters.

- With an active debug session it reads the live game over the runtime
  channel.
- Without a session it returns the **last editor-captured tree** — data may
  be stale without any error saying so. Check `get_debugger_session_info`
  first when freshness matters.

Use it to answer "what is instantiated right now": which scene root is up,
what spawned at runtime, whether a node you are about to address actually
exists in the game process. For property-level detail, fall back to
`execute_game_script` with `get_property` inside the game.

## UI controls: get_game_ui_elements

`get_game_ui_elements` walks the game's scene tree and returns every
Control-derived node with:

- `path` — absolute node path,
- `type` — class name,
- `visible` — visibility including ancestors,
- `text` — when the control has a text property,
- `global_rect` — `position` and `size` in viewport coordinates.

`max_elements` defaults to 100 (max 1000); `truncated: true` signals more
elements remain — raise the cap or target a subtree via
`execute_game_script`.

This is the reconnaissance step of every UI automation loop: enumerate
here, pick targets, then drive them with `sequence_game_inputs` /
`queue_game_input` using `global_rect` coordinates, or with
`execute_game_script` using `path`. Re-enumerate after navigation — screens
change under fixed coordinates.

## Liveness: get_game_status

`get_game_status` takes no parameters and reports:

- `engine version`, `current scene`, `node count`, `fps`, `paused`,
  `physics_frame`
- `healthy` — the game reported activity within the last 3000 ms (see
  `last_activity_ms`)
- `physics_stalled` — `physics_frame` has not advanced while FPS is
  positive

`physics_frame` is the single most useful field for injection timing: every
transient input state and every `sequence_game_inputs` offset is expressed
in physics frames.

## Diagnosis scenarios

Read the fields as combinations, not in isolation:

| Reading | Meaning | Action |
|---|---|---|
| `healthy` true, `physics_stalled` false | live game, physics ticking | safe to inject, capture, reload |
| `healthy` true, `physics_stalled` true | render loop runs but physics is frozen — a deadlocked or disabled physics step, or a stall in a physics callback | inspect with `get_debug_monitors`; do not trust input timing; `reload_scene_tree_current_scene` or restart |
| `healthy` false shortly after play | startup still in progress | wait a moment and retry — one early `game not ready` failure is normal |
| `healthy` false for good | the process is gone or wedged | `stop_editor_playing`, then `play_editor_current_scene` again |
| empty scene with a sharp `node_count` drop | a failed reload left a half-dead process | do not debug it — stop and rerun |
| `paused` true | tree paused; physics simulation stopped server-side; transient input states are lost | unpause with `set_scene_tree_pause` before input automation |

Rule of thumb: never build automation on top of a half-dead process.
Stopping and replaying is cheaper than diagnosing zombie state.

## Reload without a running scene

`reload_scene_tree_current_scene` reloads the currently running scene from
disk, recreating the scene root — script state, connections and runtime
modifications reset to the saved file. Called when no scene is running it
returns `ERR_UNCONFIGURED` (3): the "no current scene configured" error, not
a transport failure. Gate reload calls behind a `get_game_status` check
(a non-empty `current scene` plus `healthy`) when the game's state is
uncertain.

Note what reload does **not** do: it does not reload changed scripts
(there is `reload_game_scripts` for that, applied during the game's next
idle poll with no confirmation — verify with `get_game_log_entries` or
`get_game_status`), and it does not restart the process
(`stop_editor_playing` + `play_editor_current_scene` does).

## Cross-checks from the editor process

These read engine state of the editor process, not the game — useful as
sanity checks around a session, not as game probes:

- `get_engine_fps` — live measured frames per second; `get_engine_frames_drawn`
  — monotonic total-frames counter (stall detection);
  `get_engine_time_scale` — global time scale.
- `get_debugger_session_info` — `active`, `breaked`, `running` and the
  session count; the gate for every session-capture tool and the reason a
  "live" probe may silently return stale data.
- `get_debugger_scene_tree` without a session returns the last captured
  tree, as noted above — the one runtime-inspection tool with a stale-data
  mode.

## See also

- `../SKILL.md` — game lifecycle, `game_*` channel prerequisites and the
  verification loops.
- `input-injection.md` — pause matrix and why transient states vanish while
  paused.
- `debug-paths.md` — where the game's errors and logs actually surface.
- `godot-autopilot-scene-system` — inspecting and restructuring the edited
  scene (editor side).
