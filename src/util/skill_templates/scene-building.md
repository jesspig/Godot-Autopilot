# Building and Editing Godot Scenes

godot-autopilot tools for creating, restructuring and saving scenes in the Godot editor. Node paths resolve against the edited scene root; the accepted path forms and their error text are listed under Gotchas.

## Scene file lifecycle

The editor edits one scene at a time. A typical lifecycle:

1. `create_editor_scene` — start a new empty scene with a typed root
2. build content — nodes (this skill), properties and signals (godot-autopilot-properties-signals)
3. `save_editor_scene` or `save_editor_scene_as` — persist to disk
4. `open_editor_scene` / `close_editor_scene` — switch scenes
5. optionally `set_editor_main_scene` — make the scene the project's launch scene

### Creating a scene

`create_editor_scene` creates a new empty scene with a root node of the given `type` (any Node subclass, default `Node`) and `name` (default `NewRoot`). `close_current` (default false) also closes the previous scene, but only when it has no unsaved changes.

Replacing a scene with unsaved changes is refused:

`current scene is unsaved — position: Root — expected: scene saved before close — action: call save_editor_scene first, then create_editor_scene with close_current=true`

Save first, then retry. A `type` that is not a Node subclass fails with `<type> is not a Node subclass`.

### Opening, saving, closing and reloading

- `open_editor_scene` opens a scene file such as `res://game.tscn` and replaces the edited scene; it errors while the current scene has unsaved changes — call `save_editor_scene` first.
- `save_editor_scene` saves the edited scene. A scene that was never saved is written automatically to `res://<root node name>.tscn` and the response note explains the fallback; use `save_editor_scene_as` to choose an explicit path (missing parent directories are created).
- `save_editor_scenes` saves every open scene tab at once; scenes without a file path cannot be written to disk.
- `close_editor_scene` refuses to close a scene with unsaved changes.
- `reload_editor_scene` restores the scene from disk, dropping all unsaved changes without confirmation (the current scene, or `scene_path` if given). Call `save_editor_scene` first when the changes matter.
- `set_editor_main_scene` takes a `path` such as `res://game.tscn` and persists the main scene setting to project.godot.

## Creating nodes

`create_scene_node` parameters:

- `type` — any Node subclass (default `Node`)
- `name` — unique among siblings (default `NewNode`)
- `parent_path` — omit it only while the scene has no root; once a root exists, pass the parent path
- `properties` — optional object of property values applied immediately after creation

Creating a node while the scene already has a root and no `parent_path` is given fails with `scene already has a root, use parent_path to add children`.

The `properties` object assigns values right after creation through the same conversion chain as `property_set`: `NodePath` strings become node references and `{"path": "res://..."}` values resolve to resources. Any failing property frees the new node again and reports the property name, so a failed create leaves no half-configured node behind. Creation is one editor undo step; the response carries the new node's `path`, the applied properties, and an undo hint describing removal via `delete_scene_node`.

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

`delete_scene_node` takes `path` and is managed by the editor undo/redo history (action "Delete Node <name>"): the do step removes the child and frees it at the end of the frame, undo re-adds it under the old parent with owner and sibling index restored — effective while the deferred free has not destroyed the node yet. The response returns `deleted` with `undoable` true and an undo info object (name, type, `parent_path`).

The scene root refuses deletion: `cannot delete the scene root node — use close_editor_scene to close the scene, then create_editor_scene`.

## Renaming nodes

`rename_scene_node` takes `path` and `new_name`. `/` and `:` are rejected in `new_name`, and a name already used by a sibling is refused, mirroring Godot's uniqueness rule. The rename is one editor undo/redo step ("Rename Node <old>") restoring the previous name on undo; the response returns `renamed` with `undoable` true and the new relative path.

## Reparenting nodes

`reparent_node` moves a node: pass `path` and `new_parent_path`. It refuses the scene root, the node itself, its own descendants and its current parent. It is one editor undo/redo step ("Reparent Node <name>") restoring the old parent, original sibling index and owner on undo.

`keep_world_position` (default false) preserves the world transform when the node and the new parent are both `Node2D` or both `Node3D`; other type combinations ignore it. If the previous owner was the scene root, the node stays owned by the current scene root so it is still saved with the scene.

## Instantiating packed scenes

`instantiate_scene` instantiates a PackedScene from `path` (a `.tscn` file) into the edited scene. `parent_path` defaults to the scene root; `name` is optional; `owner` defaults to true and sets scene ownership so the instance's children are saved with the scene. Instances inherit the CONNECT_PERSIST connections saved in the source scene — do not reconnect the same signal pairs.

## Group management

Group names are plain strings shared across the whole scene tree and need no pre-registration. Membership added here is saved with the scene.

- `add_group_node` (`node_path`, `group_name`) — adds the node persistently, registered with the editor undo/redo. Idempotent: an existing membership is removed first, then re-added. Returns `added` with the node path, the group and a `persistent` flag.
- `remove_group_node` — removes the node from the group, also undoable; removing a node that is not in the group is a safe no-op. Returns `removed`.
- `has_group_node` — returns true or false; errors when the node is not found.

At runtime, `get_scene_tree_nodes_in_group` lists member paths and `call_scene_tree_group` calls a method on every member of a group.

## One undo step for several operations

Every tool above is already its own undo step. To make several manual operations revert as one, wrap them:

1. `create_editor_undo_redo_action` with an action `name`
2. `add_editor_undo_redo_do` / `add_editor_undo_redo_undo` — each takes `node_path`, a `method` name and an optional `value`; do steps run when the action is applied, undo steps run on revert
3. `commit_editor_undo_redo` — finalizes the action; afterwards it can no longer be extended

```json
{"name": "create_editor_undo_redo_action", "arguments": {"name": "Move player to spawn"}}
{"name": "add_editor_undo_redo_do", "arguments": {"node_path": "Player", "method": "set_position", "value": {"x": 0, "y": 0}}}
{"name": "add_editor_undo_redo_undo", "arguments": {"node_path": "Player", "method": "set_position", "value": {"x": 10, "y": 20}}}
{"name": "commit_editor_undo_redo", "arguments": {}}
```

## Workflow checklist

1. Confirm tool names via `search_tools` and `get_tool_detail` — never guess (godot-autopilot-usage).
2. Make sure a scene is open: `create_editor_scene` for a fresh one, `open_editor_scene` for an existing file; save an unsaved scene first.
3. Create the root — omit `parent_path` only while no root exists.
4. Add children with `create_scene_node`, passing `properties` for creation-time values. For many same-shaped nodes, `batch_execute` runs tool calls sequentially (`stop_on_error` defaults to true).
5. Set remaining properties and wire signals — see godot-autopilot-properties-signals.
6. Persist with `save_editor_scene` before playing (`play_editor_current_scene`) or closing.
7. Read back to verify: `get_scene_tree` (optionally with `include_properties`) and `property_get`.

## Gotchas

- Three accepted node path forms: `Root/Child` (root-name prefixed), `/root/Root/Child` (absolute), `Child` (relative to the root). An unresolvable path errors as `node not found: NoSuchNode — 当前场景根为 "Root"；合法路径写法：Root/子路径、/root/Root/子路径、子路径` — the message names the current scene root.
- `create_editor_scene` and `open_editor_scene` refuse to run while the current scene has unsaved changes; save first.
- Immediately after `create_editor_scene` or `open_editor_scene`, `get_scene_tree` may still reflect the previous scene — call it again or wait briefly for the editor to refresh.
- The root node cannot be deleted; close the scene with `close_editor_scene` instead.
- `delete_scene_node` frees the node at the end of the frame, so a tree read right after may still list it.

## See also

- godot-autopilot-properties-signals — property values, JSON shapes, signals
- godot-autopilot-resources-files — the `.tscn`/`.tres` files behind scenes
- godot-autopilot-scripting — `code_execute` for programmatic scene edits
- godot-autopilot-tips-gotchas — cross-domain limits and silent failures
- godot-autopilot-usage — tool discovery protocol
