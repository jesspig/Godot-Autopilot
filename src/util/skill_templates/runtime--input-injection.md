# Input Injection and Pause Semantics

Everything an autopilot needs to know about driving the running game's input
and about what keeps running while the scene tree is paused. All engine facts
here are verified against engine source.

## Two processes, one lesson

The editor and the running game are two OS processes running the same
binary. `Input`, `SceneTree` and every singleton exist twice — once per
process — and there is no shared state between them:

- The editor-side input tools (`press_input_action`, `release_input_action`,
  `is_input_action_pressed`, `is_input_action_just_pressed`,
  `press_input_key`, `release_input_key`, `move_input_mouse`,
  `press_input_mouse_button`, `release_input_mouse_button`,
  `start_input_gamepad_vibration`, `stop_input_gamepad_vibration`) inject
  and query the **editor process** only. A running game never sees them.
- Script code executed in the editor process cannot mutate the game's
  `Input` or `SceneTree` either.
- To drive the game you must cross the debug channel:
  `queue_game_input`, `wait_game_input`, `sequence_game_inputs` or
  `execute_game_script`. To observe the game use `get_game_input_status`,
  `get_game_status` or `capture_game_viewport`.

## Where an injected event actually lands

`Input.parse_input_event` does not dispatch synchronously. By default events
are buffered (input accumulation and/or agile flushing), and the buffer is
flushed at the end of the display server's event pass — on Windows, at the
end of `DisplayServer.process_events`. The per-frame order is therefore:

1. Display server pumps OS events, then flushes the buffered events —
   anything queued since last frame is dispatched now, at the start of the
   frame.
2. Physics steps and `_process` run.

Consequence: an event injected from code running in `_process` (which is
what a mid-frame tool call amounts to) is dispatched at the start of the
**next** frame, not the current one. Never build timing on "I injected and
polled in the same frame" round trips — use `sequence_game_inputs` with
explicit `at_frame` physics offsets, or `wait_game_input` with `inject` to
remove the skew between injection and observation.

Related footgun: parsing the *same event object* twice within one frame is
rejected with a one-time warning (`WARN_PRINT_ONCE`); instantiate a new
event (or `duplicate()`) instead of re-sending the same object.

## just_pressed has two counters

For every action the engine records the press time twice, with different
semantics (4.8-verified; the same rule drives injected and real input):

- The **physics counter** stores `current_physics_frame + 1`. The engine
  comment says it outright: input may arrive part way through a physics
  tick, so the earliest a physics-context check can react to it is the next
  physics tick.
- The **process counter** stores the *current* process frame.

`is_action_just_pressed` compares whichever counter matches the calling
context: called from `_physics_process` it compares the physics counter,
called from `_process` it compares the process counter. Practical results
for an event injected mid-frame:

- Checked from `_process` in the same frame → **true** (process branch,
  current frame matches).
- Checked from `_physics_process` in the same physics tick → **false**
  (the stored value is next tick's number). It turns true on the next tick.

So the same injection is "just pressed" and "not yet pressed" depending only
on where you ask. Transient states (`just_pressed`/`just_released`) are
tracked against physics frames only — polling them from `_process` (render
frames) does not observe injected input reliably; poll in
`_physics_process`. While the tree is paused transient states are lost
entirely (see the pause matrix below), which is why `wait_game_input` times
out on a paused game.

## action_press is an API forgery, not a key

`Input.action_press("jump")` — the `api` mode of `queue_game_input` — does
not synthesize an input event. It directly writes the action's state:

- It requires the action to be registered in `InputMap` (otherwise it errors
  and does nothing).
- It marks the state `exact` and clears the stored event id
  (`pressed_event_id` becomes empty), because no event object exists.
- The frame counters follow the same `physics_frame + 1` / current-frame
  rule as above.
- `Input.action_release` additionally clears **all** per-device state for
  the action, not just the API-written part.

Consequences:

- Any query that matches by event — `is_action_just_pressed_by_event`
  (4.8), or game code comparing the originating event — will never match an
  API-forged press, since there is no event id to match.
- Game code that inspects `Input.is_action_pressed` sees the forgery
  normally.
- To simulate a real key (so that `_input`, `_gui_input`, event-based action
  matching and shortcut handling all work), inject an `InputEventKey`
  instead — `queue_game_input` with `type: "key"` does exactly that.

## How actions match events

When an event is matched against an action's registered events
(`InputMap`), a key event matches along three independent branches — the
action's event compares `key_label`, `keycode` or `physical_keycode`,
whichever the registered event specifies (a registered event with only
`key_label` compares labels; one with `keycode` compares keycodes; one with
`physical_keycode` compares physical keycodes and, when it has an explicit
key location, that too).

Modifiers use **subset matching** by default: an event triggers an action
when the event's modifier set *contains* the action's modifier set. A
plain `X` action therefore fires for Shift+X, Ctrl+X and so on — exactly
what vanilla players expect, and a classic reason an action "fires too
often" under automation. Only `exact_match = true` requires the modifier
sets to be identical.

The deadzone is applied only to `JoypadMotion` events; key and mouse
matching ignore it. Device filtering also applies: a registered event with
a specific device id only matches events from that device, `ALL_DEVICES`
matches everything.

## Per-device state and by-event queries (4.8)

Action state is tracked globally per action *and* per input device: each
action keeps a state array indexed by device id and by the matching event
index (pressed, strength, raw strength, event type), with a hard cap on how
many events may be assigned to one action. This is what makes
`Input.action_release` reset "all devices" meaningful.

Alongside it, 4.8 adds `is_action_just_pressed_by_event(action, event,
exact)`: instead of comparing frame counters alone it first compares the
stored `pressed_event_id` with the given event's instance id. This answers
"did *this event* just press the action" — and it is the query that an API
forgery can never satisfy (no event id is stored).

## The pause matrix

`set_scene_tree_pause(true)` flips `SceneTree.paused`. Whether a node keeps
processing is decided per node by its effective process mode:

| Effective mode | Unpaused | Paused |
|---|---|---|
| `PAUSABLE` | processes | stops |
| `WHEN_PAUSED` | stops | processes |
| `ALWAYS` | processes | processes |
| `DISABLED` | never | never |
| `INHERIT` | inherits the nearest non-INHERIT ancestor | same |

Rules verified in engine source:

- `INHERIT` resolves through the node's `process_owner` — the nearest
  ancestor with a non-INHERIT mode. A node whose chain resolves to nothing
  (the tree root) defaults to `PAUSABLE`; the root itself is fixed
  `PAUSABLE`.
- `can_process()` additionally requires the node to be inside the tree and
  the tree not suspended — a node removed from the tree always reports
  false, and a suspended tree stops everything regardless of mode.
- Pausing also calls `PhysicsServer.set_active(false)` on both 2D and 3D
  servers. The physics **simulation** stops — bodies, areas and queries do
  not step — not merely node `_physics_process` callbacks. An autopilot that
  pauses the game to inspect state should not expect physics queries to
  advance.
- Suspension (`SceneTree.set_suspend`, used by e.g. the editor when the
  game loses debug focus in some flows) is a stronger, separate state and
  also deactivates the physics servers.

## What keeps running while paused

- `NOTIFICATION_PAUSED`/`NOTIFICATION_UNPAUSED` are sent only to nodes whose
  ability to process actually flips. An `ALWAYS` node keeps processing
  through the pause and never receives the notification; a `PAUSABLE` node
  receives both on pause and on unpause.
- The tree signals `physics_frame` and `process_frame` are emitted
  regardless of pause — code hooked to the signals still runs every tick;
  it is per-node callbacks that are filtered by the matrix above.
- `SceneTreeTimer` (from `create_timer`): the timer's `process_always` flag
  decides. It defaults to **true** — the default timer keeps counting while
  the tree is paused (it escapes the pause). Pass `process_always = false`
  to get a timer that stops while paused.
- `Tween`: obeys its pause mode, which defaults to `TWEEN_PAUSE_BOUND` —
  the tween follows the `can_process` result of the node it is bound to.
  Bind it to an `ALWAYS` node and it animates through a pause.
- Input state: transient states (`just_pressed`/`just_released`) are
  effectively lost across a pause — they are physics-frame-relative and the
  tick they were valid in is gone by the time the game resumes. Inject
  again after unpausing instead of expecting the old state to be observed.

## Game-side injection tools

- `queue_game_input` — one input. `type`: `key`, `mouse_button` or
  `action`; `mode`: `event` (default), `api`, `hold` (event-style, held for
  `duration_ms`). Whitelist: `type`, `keycode`, `pressed`, `button_index`,
  `position`, `action`, `duration_ms`, `mode`, `timeout_ms`; anything else
  is reported in `ignored_params` with a warning.
- `wait_game_input` — wait for `just_pressed` (default), `just_released` or
  `pressed` within a physics frame, `timeout_ms` default 2000 max 30000.
  `just_pressed`/`just_released` require `inject` — the transient window is
  one physics frame and always expired by the time a separate call could
  poll.
- `sequence_game_inputs` — timeline on exact physics frame offsets. Each
  item: `kind` (`key`, `mouse_button`, `mouse_motion`, `action`), `at_frame`
  (relative to sequence start, 0 = immediately; entries fire when their
  frame arrives even if listed out of order) plus the `queue_game_input`
  fields for that kind; optional `pressed` (default true), `duration_ms`,
  `mode`. Max 256 items; `timeout_ms` defaults to
  `max(at_frame) * 33 + 2000`, capped at 30000. Resolves with `completed`
  and `executed` counts; `completed: false` means timeout.
- `get_game_input_status` — `pressed`, `just_pressed`, `just_released`,
  `physics_frame`; includes `recent_engine_errors` (up to 5) when any
  exist, often the reason input looks ignored.

```json
{"name": "sequence_game_inputs", "arguments": {"inputs": [
  {"kind": "key", "keycode": "X", "at_frame": 0},
  {"kind": "mouse_button", "button_index": 1, "position": {"x": 120, "y": 80}, "at_frame": 5},
  {"kind": "action", "action": "jump", "at_frame": 15}
]}}
```

## Editor-side input tools

The `press_input_*` / `release_input_*` / `move_input_mouse` /
`is_input_action_*` / `*_input_gamepad_vibration` tools operate on the
editor process. They are for editor-UI automation (driving editor
shortcuts, viewport navigation) — never for driving the game.

## See also

- `../SKILL.md` — game lifecycle, the `game_*` channel and the confirmation
  loops.
- `debug-paths.md` — log and error paths, protocol drop gates, breakpoints.
- `runtime-inspection.md` — reading liveness and UI state of the game.
- `godot-autopilot-scripting` — `execute_game_script` and the other
  execution channels.
