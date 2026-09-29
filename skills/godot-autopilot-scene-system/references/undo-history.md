# The Editor Undo History Model

How the Godot editor routes, merges, invalidates and tracks undo/redo
actions — the machinery behind every undoable godot-autopilot scene edit.

## Three histories and how actions are routed

The editor keeps one history per open scene, plus a global history and an
isolated remote (debugger) history. When an action records an operation on an
object, the object decides the history:

| Operation target | History |
|---|---|
| The edited scene root or any node inside it | that scene's history |
| A built-in resource (empty path, or embedded as `file.tscn::Sub`) | the owning scene's history |
| A resource belonging to another open scene (`file.tscn::Sub` of that file) | that file's scene history |
| Editor-debugger remote objects | the remote history |
| Everything else (external files, editor-level objects) | global history |

An action that names a custom context object pins its history up front;
recording an operation on an object from a different history fails with a
history mismatch error. Consequence for tools: edits on nodes of scene tab A
never interleave with tab B's undo stack, and Ctrl-Z in one scene never
reverts another scene's edit — unless the global history is involved.

## Cross-history redo invalidation

Committing an action clears that history's redo stack, as any undo system
does. It also clears redo across histories:

- A scene-history commit clears the global history's redo.
- A global-history commit clears the redo stacks of every scene history.

Redo therefore never crosses history boundaries, and interleaving edits
through different histories destroys redo branches you might have expected to
survive.

## Ctrl+Z picks the newest history by timestamp

A plain undo does not necessarily act on the currently focused scene. The
editor looks at the global, remote and current-scene histories and undoes
whichever has the greatest timestamp on its most recent undo action. When a
tool call touches two scenes back to back, the next user Ctrl-Z reverts the
later edit, wherever it landed.

## The 800 ms merge window

An action created with a merge mode merges into the previous action instead
of becoming a new undo step when all of these hold: the merge mode is not
disabled, the previous action has the same name and the same backward-undo
configuration, and it was committed less than 800 ms ago (a sliding window —
the merged action refreshes the timer). This is how editor slider drags and
repeated property tweaks collapse into a single Ctrl-Z step.

Merge modes differ in what they keep:

- MERGE_ENDS keeps only the ends of the merged actions: the previous
  action's do operations are discarded (except force-kept ones) and no new
  undo operations are recorded while merging. The undo that remains therefore
  cannot restore intermediate states — undo operations are lost by design.
  Undoing a merged action jumps straight past everything it absorbed.
- MERGE_ALL keeps the previous do operations recorded but skips replaying
  them on redo.

## Empty actions are discarded

Committing an action that never recorded any operation is a no-op: nothing
enters the history, no version bump, no unsaved marking. A tool wrapper that
opens an undo action and forgets to add steps leaves no trace.

## Freed targets are skipped silently

When an action is undone or redone, each operation resolves its target object
fresh. If the object was deleted since the action was recorded, the operation
is skipped without any warning ("may have been deleted and this is fine"). An
undo or redo over a partially deleted scene can therefore apply only some of
its operations — and report success.

## Undo/redo marks resources dirty

Executing any method or property operation on a Resource marks that resource
edited (dirty) — on do, on undo and on redo alike. Undoing a change does not
clear the dirty flag it set; the resource stays unsaved until saved or
reloaded. This is why an undo-then-save still rewrites the file.

## Saved versions and mark_unsaved

Each history tracks a saved version — the undo-system version at the time the
scene was last saved or marked saved. A history counts as unsaved while any
action between the saved version and the current position carries its
`mark_unsaved` flag, which editor-created actions set by default:

- The check walks forward through the undo stack (or backward through the
  redo stack after an undo) from the saved version. Practical effect: undoing
  an edit does not make the scene count as saved again — the flagged action
  still sits on the redo side and keeps the unsaved state.
- Committing an action whose version passes the saved version marks the
  history unsaved, as does clearing the history with the version-increase
  flag (reload paths do exactly that).

This saved-version bookkeeping is what makes `close_editor_scene` refuse and
save prompts appear even for changes that were already undone.

## What this means for tool calls

- Each godot-autopilot editing tool is one action; to make several tool calls
  revert as a single step, wrap them in `create_editor_undo_redo_action` /
  `add_editor_undo_redo_do` / `add_editor_undo_redo_undo` /
  `commit_editor_undo_redo`.
- `undoable:true` in a tool response describes the moment of the call. After
  a `reload_editor_scene` (or any other history clear) the action is gone.
- Long tool sequences against one scene keep the scene's history growing;
  undo depth is bounded by the editor's maximum-history setting, and the
  oldest actions fall off the front.

## See also

- godot-autopilot-resources — saving and reloading files (what clears these histories)
- godot-autopilot-scripting — `code_execute` bypasses the undo system entirely
- references/scene-format-tokens.md — what a save actually writes
