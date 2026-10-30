# Runtime Inspection of the Running Game

Read-only probes for the running game: its scene tree, its UI controls, and
the liveness signals that tell a live game from a frozen or half-dead one.
Read these before you inject, modify or reload — and after, to verify.

## Scene tree: get_debugger_scene_tree

`get_debugger_scene_tree` returns the running game's scene tree (node names
and types) as a formatted text tree. It takes no parameters.

- With an active debug session it reads the live game over the runtime
  channel.
- Without a session it returns an empty result plus a `note` — there is no
  editor-side fallback, so check `get_debugger_session_info` to tell the two
  cases apart.

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
- `global_rect` — `position` and `size` in viewport coordinates,
- `mouse_filter` — `STOP`, `PASS` or `IGNORE`,
- `z_index` — draw order within its canvas.

`mouse_filter` decides whether a control consumes, passes or ignores mouse
events; a full-screen `PASS` control covers the controls below it for
hit-testing even if it draws nothing. `z_index` explains draw/layer order;
read it together with `mouse_filter` when interpreting an occlusion report.

Optional filters narrow the enumeration before `max_elements` counts:

- `filter` — case-insensitive substring matched against each element's
  `path`, `type` and `text` (omit or pass an empty string for no filter),
- `types` — array of 1-32 exact class names (e.g. `Button`, `Label`),
- `visible_only` — keep only elements visible in the tree (default false
  keeps hidden controls too).

`max_elements` defaults to 100 (max 1000) and caps the filtered matches;
`truncated: true` signals more filtered matches remain — raise the cap or
narrow the filters rather than filtering client-side.

This is the reconnaissance step of every UI automation loop: enumerate
here, pick targets, then drive them with `sequence_game_inputs` /
`queue_game_input` using `global_rect` coordinates, or with
`execute_game_script` using `path`. Re-enumerate after navigation — screens
change under fixed coordinates.

### UI change acceptance

After any UI change, validate before clicking:

1. Run `validate_game_ui_layout`. It reports zero-size, off-viewport and
   possibly-occluded controls as triage items, and a
   `possibly_occluded` item names the occluding control.
2. Click the target with `click_game_ui_element`, not raw coordinates.
3. Read `hit_path` (the control actually under the injected point) and
   `hit_mismatch` (true when it differs from the requested target). A
   mismatch is reported without failing the call by default; pass
   `strict_hit: true` to make it an error. Cross-check the
   `possibly_occluded` issue before retrying coordinates — a full-screen
   `PASS` container swallows the click instead of passing it through.

## Screenshots: frame phase and saving

`capture_game_viewport` (and `capture_editor_viewport` with
`target: game`) evaluates `when` once per rendered frame and waits out
`after_frames` first. The response distinguishes two frames:
`when_satisfied_frame` is the rendered frame where the condition first held,
and `captured_frame` is the following rendered frame whose image was read —
`captured_frame > when_satisfied_frame`, so the picture is the state after
the condition became true, not the frame that evaluated it. Use the pair to
prove the capture really landed after the trigger instead of trusting the
picture alone.

Saving works on both targets: pass `save: true` (default destination: the GDA
per-project captures folder) or `save_dir` (a `res://` directory inside the
project root; anything outside is rejected with `path rejected`). The result
reports `saved_path` and the destination keeps only the 20 most recent
`gda_capture_*` files. Saving needs the `write` capability; game-target saves
copy the PNG the editor already read back over the runtime channel, so they
also work when the game-side temporary file would otherwise be pruned.

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
| `healthy` false shortly after play | startup still in progress | wait a moment; game calls default to `wait_ready: true`, which queues the request and replays it once the game reports `gda:ready` instead of failing |
| `healthy` false for good | the process is gone or wedged | `stop_editor_playing`, then `play_editor_current_scene` again |
| empty scene with a sharp `node_count` drop | a failed reload left a half-dead process | do not debug it — stop and rerun |
| `paused` true | tree paused; physics simulation stopped server-side; transient input states are lost; `physics_frame` still advances because it is the engine-global counter, so it cannot prove the freeze on its own | unpause with `set_scene_tree_pause` before input automation; prove gameplay froze with a gameplay node's position or a gameplay-owned tick |

Rule of thumb: never build automation on top of a half-dead process.
Stopping and replaying is cheaper than diagnosing zombie state.

## Deterministic in-game verification

Timing-sensitive checks (wall jumps, stomps, pickups, patrol turnarounds)
must converge into a single `execute_game_script` call: teleport or set up
state, await physics frames inside the script, then read the values back.
Never split setup, wait and read across separate tool calls — the round
trip between calls lets the game advance (the player lands, slides or dies)
and the verdict becomes a misread of stale timing rather than of the logic.

```gdscript
extends Node
func _run():
    var player = get_node("/root/Main/Player")
    player.global_position = Vector2(100, 200)
    player.velocity = Vector2(120, -300)
    player.move_and_slide()
    for i in range(30):
        await physics_frame
    return {"pos": player.global_position, "on_wall": player.is_on_wall()}
```

`CharacterBody2D` refreshes `is_on_floor()` and `is_on_wall()` only inside
`move_and_slide()`. Teleporting a body leaves the previous floor/wall flags
in place, so call `move_and_slide()` once after the teleport (after setting
`velocity`) before awaiting physics frames — otherwise a stale floor flag
makes gravity and wall-slide checks read the old state.

Drive inputs with `sequence_game_inputs` on explicit `at_frame` offsets
instead of chained single presses, and watch a value evolve with
`sample_game_property` instead of a hand-rolled poll loop. Gate the call
behind `get_game_status` (`healthy`, non-empty current scene) and confirm
with `get_debugger_errors` plus the `new_errors_since_last_call` watermark,
cross-checking `get_game_log_entries` on error-storm paths.

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
- `get_debugger_scene_tree` without a session returns an empty result plus a
  `note`, as noted above — a live tree requires an active session.

## See also

- `../SKILL.md` — game lifecycle, `game_*` channel prerequisites and the
  verification loops.
- `input-injection.md` — pause matrix and why transient states vanish while
  paused.
- `debug-paths.md` — where the game's errors and logs actually surface.
- `godot-autopilot-scene-system` — inspecting and restructuring the edited
  scene (editor side).
