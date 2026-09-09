# Godot Scene System

godot-autopilot tools for creating, restructuring, saving and inspecting scenes
in the Godot editor, plus the property read/write and signal wiring that turn a
node tree into a working scene. Node paths resolve against the edited scene
root; the accepted path forms and their error text are listed under Gotchas.

Deep dives live in this skill's references: `references/property-json-shapes.md`
(JSON value shapes, Godot 3 to 4 renames, serialization rules),
`references/undo-history.md` (the editor undo history model) and
`references/scene-format-tokens.md` (the .tscn file format).

## Scene lifecycle

The editor works on one edited scene at a time; other scenes can stay open in
tabs. A typical lifecycle:

1. `create_editor_scene` — start a new empty scene with a typed root
2. build content — nodes (below), properties and signals (below)
3. `save_editor_scene` or `save_editor_scene_as` — persist to disk
4. `open_editor_scene` / `close_editor_scene` — switch scenes
5. optionally `set_editor_main_scene` — make the scene the project's launch scene

### Creating a scene

`create_editor_scene` creates a new empty scene with a root node of the given
`type` (any Node subclass, default `Node`) and `name` (default `NewRoot`).
`close_current` (default false) also closes the previous scene, but only when
it has no unsaved changes.

Replacing a scene with unsaved changes is refused:

`current scene is unsaved — position: Root — expected: scene saved before close — action: call save_editor_scene first, then create_editor_scene with close_current=true`

Save first, then retry. A `type` that is not a Node subclass fails with
`<type> is not a Node subclass`.

### Opening, saving, closing and reloading

- `open_editor_scene` opens a scene file such as `res://game.tscn` and replaces the edited scene; it errors while the current scene has unsaved changes — call `save_editor_scene` first.
- `save_editor_scene` saves the edited scene. A scene that was never saved is written automatically to `res://<root node name>.tscn` and the response note explains the fallback; use `save_editor_scene_as` to choose an explicit path (missing parent directories are created).
- `save_editor_scenes` saves every open scene tab at once; scenes without a file path cannot be written to disk.
- `close_editor_scene` refuses to close a scene with unsaved changes.
- `reload_editor_scene` restores the scene from disk, dropping all unsaved changes without confirmation (the current scene, or `scene_path` if given). Call `save_editor_scene` first when the changes matter. Reloading also clears the scene's undo history.
- `set_editor_main_scene` takes a `path` such as `res://game.tscn` and persists the main scene setting to project.godot.

## Creating nodes

`create_scene_node` parameters:

- `type` — any Node subclass (default `Node`)
- `name` — unique among siblings (default `NewNode`)
- `parent_path` — omit it only while the scene has no root; once a root exists, pass the parent path
- `properties` — optional object of property values applied immediately after creation

Creating a node while the scene already has a root and no `parent_path` is
given fails with `scene already has a root, use parent_path to add children`.

The `properties` object assigns values right after creation through the same
conversion chain as `property_set`: `NodePath` strings become node references
and `{"path": "res://..."}` values resolve to resources. Any failing property
frees the new node again and reports the property name, so a failed create
leaves no half-configured node behind. Creation is one editor undo step; the
response carries the new node's `path`, the applied properties, and an undo
hint describing removal via `delete_scene_node`.

```json
{
  "name": "create_scene_node",
  "arguments": {
    "parent_path": "Root",
    "name": "Player",
    "type": "CharacterBody2D",
    "properties": {"position": {"x": 0, "y": -10}}
  }
}
```

## Deleting nodes

`delete_scene_node` takes `path` and is managed by the editor undo/redo
history (action "Delete Node <name>"): the do step removes the child and frees
it at the end of the frame, undo re-adds it under the old parent with owner
and sibling index restored — effective while the deferred free has not
destroyed the node yet. The response returns `deleted` with `undoable` true
and an undo info object (name, type, `parent_path`).

The scene root refuses deletion: `cannot delete the scene root node — use
close_editor_scene to close the scene, then create_editor_scene`.

## Renaming nodes

`rename_scene_node` takes `path` and `new_name`. `/` and `:` are rejected in
`new_name`, and a name already used by a sibling is refused, mirroring Godot's
uniqueness rule. The rename is one editor undo/redo step ("Rename Node <old>")
restoring the previous name on undo; the response returns `renamed` with
`undoable` true and the new relative path.

## Reparenting nodes

`reparent_node` moves a node: pass `path` and `new_parent_path`. It refuses
the scene root, the node itself, its own descendants and its current parent.
It is one editor undo/redo step ("Reparent Node <name>") restoring the old
parent, original sibling index and owner on undo.

`keep_world_position` (default false) preserves the world transform when the
node and the new parent are both `Node2D` or both `Node3D`; other type
combinations ignore it. If the previous owner was the scene root, the node
stays owned by the current scene root so it is still saved with the scene.

## Instantiating packed scenes

`instantiate_scene` instantiates a PackedScene from `path` (a `.tscn` file)
into the edited scene. `parent_path` defaults to the scene root; `name` is
optional; `owner` defaults to true and sets scene ownership so the instance's
children are saved with the scene. Instances inherit the CONNECT_PERSIST
connections saved in the source scene — do not reconnect the same signal pairs.

## Group management

Group names are plain strings shared across the whole scene tree and need no
pre-registration. Membership added here is saved with the scene.

- `add_group_node` (`node_path`, `group_name`) — adds the node persistently, registered with the editor undo/redo. Idempotent: an existing membership is removed first, then re-added. Returns `added` with the node path, the group and a `persistent` flag.
- `remove_group_node` — removes the node from the group, also undoable; removing a node that is not in the group is a safe no-op. Returns `removed`.
- `has_group_node` — returns true or false; errors when the node is not found.

At runtime, `get_scene_tree_nodes_in_group` lists member paths and
`call_scene_tree_group` calls a method on every member of a group.

## Editing-side read-only checks

Check what exists and what state the editor is in before modifying it:

- `get_scene_tree` walks the currently edited scene, returning `name`, `type`, `path` and `children` per node. Optional `max_depth` (default 8, `-1` for unlimited) and `include_properties` (true adds up to 20 filtered properties per node). Errors when no scene is open — create or open one first.
- `get_editor_edited_scene_root` returns the edited scene root's `name`, `type` and `path`; null when no scene is open. Start scene workflows here.
- `property_get_list` (`path`) — every property of a node with full metadata; see the properties section below.
- `get_editor_selection` reports which nodes the user has selected (each with `name`, `class` and `path`; an empty array means nothing is selected). `set_editor_selection` focuses subsequent operations on specific nodes.
- `get_editor_file_system_tree` fetches the project file tree, optionally rooted at the given path; directory entries carry children, file entries carry `name`, `path` and `type`. Trees deeper than 12 levels are truncated and the response carries a `max_depth` field. After creating or modifying files outside the editor, call `scan_editor_file_system`, then poll `get_editor_file_system_status` until `scanning` is false.
- `capture_editor_viewport` returns a base64 PNG of the editor 2D viewport (3D fallback) — visually verify the scene after placing nodes or changing properties.
- `validate_scene_file` dry-runs a `.tscn`/`.scn` file on disk without touching the edited scene: loads it bypassing the resource cache, checks every dependency for existence, then instantiates and frees it. Returns `valid`, `problems` (each `kind` is `load_failed`, `missing_dependency` or `instantiate_failed`), `missing_dependencies` and `dependency_count`. Useful after bulk edits or before committing scene changes.

## Properties — reading and writing

- `property_get` (`path`, `property`) — returns the exact serialized value. Unknown nodes or properties error with candidate suggestions.
- `property_get_list` (`path`) — every property with its metadata: name, type, hint, hint_string, usage flags and class_name. Query it first to discover valid property names and enum ordering before `property_get` or `property_set`.
- `get_scene_tree` with `include_properties` — a convenience snapshot of up to 20 filtered properties per node (metadata/, underscore-prefixed and Object-typed properties are skipped). Use `property_get` for the exact full value.

`property_set` takes `path`, `property` and `value`. Omit `type_hint` to infer
the Variant type from the property metadata. After writing, the value is read
back: a mismatch is reported either as an error (the property may be
read-only, nonexistent, or need an explicit `type_hint`) or as a warning when
the engine converted the value.

- Every successful set registers with the editor undo stack so Ctrl-Z reverts it (the response reports undoable:true). Exception: when the old or new value is an Object reference, no action is recorded and the response reports undoable:false with a skip reason.
- Node-typed properties accept a node path string; it is converted to a node reference automatically and the response notes the conversion.
- Assigning a string to an int property silently converts to 0 and reports ok. After any set with an unusual value shape, read back with `property_get` to confirm what actually landed.

```json
{"name": "property_set", "arguments": {"path": "Player", "property": "position", "value": {"x": 12.5, "y": -3.25}}}
```

JSON value shapes for all Godot types, the Godot 3 to 4 renamed-property
table, default-value serialization rules and the memory:// restriction are in
`references/property-json-shapes.md`.

## Signals

- `signal_connect` — connects a signal on `source_path` to a method on `target_path`; requires `source_path`, `signal`, `target_path` and `method`. `persist` defaults to true, which uses CONNECT_PERSIST: the connection is saved with the scene and every `instantiate_scene` of it inherits the connection. Set `persist` to false for a runtime-only connection that is not saved. Returns `connected`, or "already_connected" when the pair exists already (remove it with `signal_disconnect`), plus a `persisted` flag.
- `signal_disconnect` — same four parameters. Idempotent: removing a pair that does not exist returns "not_connected" and is not an error. Removing a persistent connection stops it being saved with the scene.
- `trace_signal_flow` — inspects the wiring around a node: `path` plus optional `direction` ("outgoing", "incoming" or "both", default both) and `max_depth` (default 3, cycles are cut). Each edge is `{from, signal, to, method, persisted}` where persisted marks connections saved in the scene file.

```json
{
  "name": "signal_connect",
  "arguments": {
    "source_path": "UI/StartButton",
    "signal": "pressed",
    "target_path": "Game",
    "method": "_on_start_button_pressed"
  }
}
```

## The undo history model

Every editing tool above records exactly one action in the editor's undo/redo
history, so a single Ctrl-Z reverts it. The engine model behind this:

- Actions are routed by their target object: nodes of the edited scene and built-in sub-resources land in that scene's own history; everything else lands in the global history. Each open scene tab has a separate history.
- Committing an action clears the redo stacks of the other histories, and a plain Ctrl-Z picks whichever history holds the newest action by timestamp.
- An action that recorded no operations is discarded on commit. When an action is undone or redone, operations whose target object was freed meanwhile are skipped silently.
- Executing an operation on a Resource marks it dirty; undoing does not make it clean again. Per-history saved versions decide the unsaved state, and actions carry a `mark_unsaved` flag that keeps a history unsaved even after the action is undone.

Routing table, redo-invalidation rules, the 800 ms merge window and its
undo-loss caveat: `references/undo-history.md`.

## Workflow checklist

1. Confirm tool names via `search_tools` and `get_tool_detail` — never guess (godot-autopilot).
2. Make sure a scene is open: `create_editor_scene` for a fresh one, `open_editor_scene` for an existing file; save an unsaved scene first.
3. Create the root — omit `parent_path` only while no root exists.
4. Add children with `create_scene_node`, passing `properties` for creation-time values. For many same-shaped nodes, `batch_execute` runs tool calls sequentially (`stop_on_error` defaults to true).
5. Set remaining properties and wire signals (sections above).
6. Persist with `save_editor_scene` before playing (`play_editor_current_scene`) or closing.
7. Read back to verify: `get_scene_tree` (optionally with `include_properties`) and `property_get`.

## Gotchas

- Three accepted node path forms: `Root/Child` (root-name prefixed), `/root/Root/Child` (absolute), `Child` (relative to the root). An unresolvable path errors as `node not found: NoSuchNode — 当前场景根为 "Root"；合法路径写法：Root/子路径、/root/Root/子路径、子路径` — the message names the current scene root.
- `create_editor_scene` and `open_editor_scene` refuse to run while the current scene has unsaved changes; save first.
- Immediately after `create_editor_scene` or `open_editor_scene`, `get_scene_tree` may still reflect the previous scene — call it again or wait briefly for the editor to refresh.
- The root node cannot be deleted; close the scene with `close_editor_scene` instead.
- `delete_scene_node` frees the node at the end of the frame, so a tree read right after may still list it.
- Opening or reloading a scene while the editor is in the middle of a scene change is silently skipped (no error, no effect) — if a call reports ok but the scene did not change, retry after the change settles.
- Opening a scene very early during editor startup is deferred until initialization finishes; the scene appears shortly after instead of failing.
- Opening an already-open scene only switches to its tab and does not re-read the file from disk. To pick up external changes call `reload_editor_scene`, which also clears that scene's undo history.
- When a script a scene depends on is updated, the editor may silently re-pack and re-instantiate the open scene and clear its undo history — undoable-only changes vanish without an error message.
- Saving re-packs the scene and silently drops: nodes whose owner is not the saved scene root, connections without CONNECT_PERSIST, properties equal to the node's computed default value unless pinned, and instanced-subscene nodes with no local property or group changes (details in `references/scene-format-tokens.md`).
- Saving a scene cascades to every dirty resource edited alongside it; dirty flags are cleared before writing and PackedScene resources are skipped.
- Property lines equal to the default value are omitted from the saved file — a missing line means "matches the default", not data loss (`references/property-json-shapes.md`).
- 4.7+: tree order is parent before child on enter, `_ready` fires child before parent, and exit is child before parent — a node cannot rely on a sibling being ready inside `_enter_tree`.

## See also

- godot-autopilot-resources — the `.tscn`/`.tres` files behind scenes, rename/move with reference rewriting
- godot-autopilot-scripting — `code_execute` for programmatic scene edits
- godot-autopilot-runtime — running the game and inspecting it live
- references/property-json-shapes.md — JSON value shapes and property renames
- references/undo-history.md — histories, merges and unsaved tracking
- references/scene-format-tokens.md — .tscn sections, tokens and uid resolution
