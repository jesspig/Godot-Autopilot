# Script Execution Gotchas

Engine-level facts about how and when GDScript actually executes, each verified against the Godot engine source. Every section here describes a silent failure mode or a surprising ordering - not something that reports an error. Tool-level behavior (the four channels, code_execute details) lives in the main godot-autopilot-scripting skill; runtime behavior (pause, input, logs) lives in godot-autopilot-runtime.

## @tool gating and placeholder instances

- A GDScript is instantiable only when `can_instantiate` holds: `valid && !abstract && (tool || scripting_enabled)`. A script that failed to compile, is declared abstract, or is not marked `@tool` cannot be instantiated in the editor process.
- When a non-`@tool` script is attached to a node in the edited scene, the node does not hold a real script instance. It holds a `PlaceHolderScriptInstance`:
  - it exposes only the exported property **defaults**;
  - zero code executes - no `_init`, no `_ready`, no `_process`, no tool-time callbacks;
  - nothing errors and nothing warns - the inspector shows the script attached and healthy.
- Practical consequences:
  - `call_script_node` on a non-`@tool` script reports the restriction and fails; for those scripts use the game channel instead.
  - `get_script_property` with `script_path` returns the declared default - indistinguishable from a placeholder that never computed anything. The signature of this trap is "reads fine, never changes".
  - Editor-side mutations you expected the script to perform appear only after adding `@tool` and calling `reload_script` - or after running the game.
- Once a script is `@tool`, its callbacks run inside the **editor process**, which makes the process-semantics sections below apply to it: `is_editor_hint()` is true, tree callbacks fire on editor-side scene changes, and editor-only classes are available but game-only state is not.

## _static_init and static variables

- `_static_init` runs after **every successful reload**, not once per session. Any side effect you place there (registering helpers, building caches) re-executes on each `reload_script` call.
- `self` is unavailable inside `_static_init`: it executes in a static context, so touching instance state or assuming a node context fails.
- Static variables survive a hot reload only when `reload_script` runs with `keep_state=true`. A plain reload re-initializes statics from their field initializers, and `_static_init` runs again on top of that.

For C# there is no equivalent in-editor assembly hot-reload (see the `godot-autopilot-csharp` skill), so static-state semantics differences do not arise there.

## is_editor_hint and editor-only classes

- `is_editor_hint()` distinguishes **processes**, not contexts: it returns `true` for the whole editor process - including every `@tool` script and editor plugin - and `false` for the entire game subprocess launched with F5. There is no state in which a game-process script sees `true`, or an editor-side `@tool` script sees `false`.
- Knock-on effects verified in the engine source:
  - `Input.set_custom_mouse_cursor()` is a no-op when called inside the editor process.
  - Classes registered as editor-only (`API_EDITOR` registration) **error when instantiated in the running game**. Code shared between `@tool` and runtime paths must not construct such classes unconditionally.
- Because the flag is process-wide, `@tool` code that must behave differently per process should branch on `is_editor_hint()` at the top of its callbacks rather than relying on where a call "comes from".

## Tree enter/exit order

For any subtree attached to or removed from the scene tree, callbacks fire in a fixed three-phase order:

1. ENTER_TREE: parents before children.
2. READY: children before parents - a parent's `_ready` runs only after all descendants are ready, so it can rely on them being initialized.
3. EXIT_TREE: children before parents.

- `tree_exited` is emitted once **after the whole branch has been cut from the tree**, not per node during removal - a handler connected to it sees a fully detached subtree.
- The order is engine-level and applies identically in the editor process: in a `@tool` script the same phases run when scene operations (reparenting, scene open, undo/redo) add or remove nodes.

Assembly recipe: add a subtree's required children to their parent BEFORE the parent enters the tree (or before the surrounding scene is opened or instantiated), so no `_enter_tree` handler observes a half-built branch and every parent `_ready` sees fully initialized descendants. When the order cannot be staged up front, resolve lazily with get_node_or_null(...) plus a null guard instead of `get_node(...)` - a missing node then reads as null instead of erroring.

## Frame order: signals, callbacks and deferred deletion

Within one frame of the running game (verified in `SceneTree`):

- The `physics_frame` signal fires **before** node `_physics_process` callbacks in the same physics tick.
- The `process_frame` signal fires **before** node `_process` callbacks, and right after the signal the engine flushes the MessageQueue once - so calls queued with `call_deferred` inside a `process_frame` handler run before any `_process` callback that frame.
- Deferred deletion (`_flush_delete_queue`) runs at **end of frame**: after a node is queued for deletion but within the same frame, its pointer is still queryable - `is_instance_valid()` returns true and properties can still be read. The object is truly gone only after the frame-end flush.

Practical readings:

- "Still queryable after queueing for deletion" is a one-frame grace window, not a license to keep references: by the next frame the pointer is dangling. Use `is_instance_valid()` for cross-frame guards.
- A `process_frame` handler that calls `call_deferred` produces code that runs "before `_process` this frame", not "next frame" - ordering assumptions built on the opposite belief break subtly.

## call_deferred and MessageQueue flushing

`call_deferred`, deferred signal emissions and `queue_free` post messages to a per-frame MessageQueue flushed at defined points (for example right after the `process_frame` signal).

- **Released targets are dropped silently**: if the target object has been freed by flush time, the queued call is discarded without any error. A deferred "refresh the UI" call aimed at a node that died the same frame simply never happens - no crash, no log line.
- **Flush is not re-entrant**: entering the flush path while a flush is already in progress returns `ERR_BUSY` instead of recursing.
- **Messages posted during a flush are processed in the same flush**: a deferred call whose code itself calls `call_deferred` still runs within that same flush cycle - the loop keeps going until the queue is empty.
- Deferred calls therefore run "at the next flush point", which is usually still inside the same frame - not necessarily next frame. Treat them as same-frame-later ordering, and never as a liveness guarantee: if the receiver might die first, check validity at the call site instead of relying on the deferred round-trip.

## The editor and the game are two OS processes

- The Godot editor and the game you launch are **two separate OS processes running the same binary**. There is no shared memory, no shared Input state, no shared SceneTree, no shared autoload singletons.
- The game process connects back to the editor over a debug channel - by default `--remote-debug tcp://127.0.0.1:6007`.
- Consequences:
  - Script executed in the editor process (`execute_script`, `code_execute`) can never touch game state; script executed in the game process (`execute_game_script`) can never touch editor state. "It works in code_execute but not in the game" (or the reverse) is almost always a process-boundary issue, often combined with `is_editor_hint()` or placeholder-instance gating above.
  - To affect the running game you must go through the game-channel tools; to inspect it, the debugger channel. Cross-process calls from editor-side code have no effect.
- This is also why a non-`@tool` script does nothing in the editor yet runs normally in the game: the editor process never instantiated it (placeholder instance, first section above), while the game process does.

## See also

- `godot-autopilot-scripting` - script lifecycle, the four execution channels and code_execute details
- `godot-autopilot-runtime` - game channel, pause semantics, debug log paths and runtime inspection
- `godot-autopilot-scene-system` - node CRUD, reparenting and the undo history model behind editor-side tree changes
