#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kSceneBuildingDescription =
    R"gda_skill(Building and editing Godot scenes through godot-autopilot: create, delete, rename and reparent nodes, instantiate packed scenes, group nodes, editor undo/redo grouping, and saving scenes. Use when creating or restructuring scenes or nodes in the editor.)gda_skill";

const char *kPropertiesSignalsDescription =
    R"gda_skill(Reading and writing node properties and connecting signals in the edited scene via godot-autopilot: JSON value shapes for Vector2/Vector3/Color/Rect2, the Godot 3 to 4 renamed-properties table, undo behavior, and signal connect/disconnect. Use when setting or inspecting properties or wiring signals.)gda_skill";

const char *kSceneBuildingBody = R"gda_skill(# Building and Editing Godot Scenes

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
)gda_skill";

const char *kPropertiesSignalsBody = R"gda_skill(# Node Properties and Signals

Reading and writing properties of nodes in the edited scene, the JSON value shapes for Godot types, the Godot 3 to 4 renamed-property table, and signal wiring. Property writes go through the editor undo stack unless noted otherwise.

## Reading properties

- `property_get` (`path`, `property`) — returns the exact serialized value. Unknown nodes or properties error with candidate suggestions; use `property_get_list` to see valid names.
- `property_get_list` (`path`) — every property with its metadata: name, type (Variant type name and id), hint, hint_string, usage flags and class_name. Query it first to discover valid property names and enum ordering before `property_get` or `property_set`.
- `get_scene_tree` with `include_properties` — a convenience snapshot of up to 20 filtered properties per node (metadata/, underscore-prefixed and Object-typed properties are skipped). Use `property_get` when you need the exact full value.

A missing property reads as `property not found: <name> on node <node> — use property_get_list to see available properties`.

## Writing properties

`property_set` takes `path`, `property` and `value`. Omit `type_hint` to infer the Variant type automatically from the property metadata (query `property_get_list` first for enum ordering). After writing, the value is read back: a mismatch is reported either as an error (the property may be read-only, nonexistent, or need an explicit `type_hint`) or as a warning when the engine converted the value.

- Every successful set registers with the editor undo stack so Ctrl-Z reverts it (the response reports undoable:true). Exception: when the old or new value is an Object reference, no action is recorded and the response reports undoable:false with a skip reason.
- Node-typed properties accept a node path string; it is converted to a node reference automatically and the response notes the conversion. If the path does not resolve, the error explains the two options: pass a valid in-scene path, or assign the node reference via `code_execute`.
- Omitting `value` errors with `missing required parameter: value`.

### Silent conversion to watch

Assigning a string to an int property silently converts to 0 and reports ok: setting property z_index to "abc" succeeds and the property becomes 0. After any set with an unusual value shape, read the property back with `property_get` to confirm what actually landed.

## JSON value shapes

| Godot type | JSON shape |
|---|---|
| Vector2 | `{"x": 1, "y": 2}` |
| Vector3 | `{"x": 1, "y": 2, "z": 3}` |
| Color | `{"r": 1, "g": 0.5, "b": 0.25, "a": 1}` |
| Rect2 | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| Resource | `{"path": "res://icon.svg"}` |
| Node reference | the node path string, e.g. `Player` |

Example calls:

```json
{"name": "property_set", "arguments": {"path": "Player", "property": "position", "value": {"x": 12.5, "y": -3.25}}}
{"name": "property_set", "arguments": {"path": "Player", "property": "modulate", "value": {"r": 1, "g": 0.5, "b": 0.25, "a": 1}}}
{"name": "property_set", "arguments": {"path": "Sprite", "property": "texture", "value": {"path": "res://icon.svg"}}}
```

## Godot 3 to 4 renamed properties

When `property_set` or `property_get` fails, the error automatically attaches Levenshtein-based candidate suggestions plus this rename table. Check it before inventing property names:

| Godot 3 name | Godot 4 name |
|---|---|
| frames | sprite_frames |
| cast_to | target_position |
| rect_position | position |
| rect_global_position | global_position |
| rect_size | size |
| rect_min_size | custom_minimum_size |
| rect_rotation | rotation |
| rect_scale | scale |
| rect_pivot_offset | pivot_offset |
| translation | position |

## Special cases

- Camera2D `enabled` — true is the default and the default value is not serialized: setting it to true leaves nothing in the scene file. The response carries a serialization note explaining this.
- Camera2D `current` — has no setter. Make a camera current through `code_execute` calling make_current():

```json
{
  "name": "code_execute",
  "arguments": {
    "source_code": "SceneRoot.get_node(\"Camera\").make_current()\nreturn \"ok\"",
    "function_name": "_run",
    "timeout_ms": 5000
  }
}
```

- C# properties — read-only or private-setter properties on C# nodes are invisible to GDScript: reading a property without a getter returns null. When validating C#-derived values through `property_get`, check readable exported fields instead of derived values to avoid false failures.
- int properties accept strings silently (see the conversion note above).

## memory:// resources must be saved first

`property_set` rejects memory:// resources (resources created in memory without a file path) as node property values — writing them into the scene file would corrupt it. Save the resource to disk with `save_resource` first, then assign it via its `res://` path in the `{"path": "res://..."}` shape.

## Signals

- `signal_connect` — connects a signal on `source_path` to a method on `target_path`; requires `source_path`, `signal`, `target_path` and `method`. `persist` defaults to true, which uses CONNECT_PERSIST: the connection is saved with the scene and every `instantiate_scene` of it inherits the connection. Set `persist` to false for a runtime-only connection that is not saved. Returns `connected`, or "already_connected" when the pair exists already (remove it with `signal_disconnect`), plus a `persisted` flag.
- `signal_disconnect` — same four parameters. Idempotent: removing a pair that does not exist returns "not_connected" and is not an error. Removing a persistent connection stops it being saved with the scene.
- `trace_signal_flow` — inspects the wiring around a node: `path` plus optional `direction` ("outgoing", "incoming" or "both", default both) and `max_depth` (default 3, cycles are cut). Each edge is `{from, signal, to, method, persisted}` where persisted marks connections saved in the scene file.

Example:

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

## See also

- godot-autopilot-scene-building — creating nodes and structuring scenes
- godot-autopilot-resources-files — `save_resource` and resource lifecycles
- godot-autopilot-inspection — reading scene and engine state
- godot-autopilot-scripting — `code_execute` escape hatch
- godot-autopilot-tips-gotchas — cross-domain limits and silent failures
)gda_skill";

} // namespace

std::vector<SkillSpec> make_scene_skills() {
  return {
      {"godot-autopilot-scene-building", kSceneBuildingDescription,
       {{ "SKILL.md", kSceneBuildingBody }}},
      {"godot-autopilot-properties-signals", kPropertiesSignalsDescription,
       {{ "SKILL.md", kPropertiesSignalsBody }}},
  };
}

} // namespace godot_autopilot::skill_gen
