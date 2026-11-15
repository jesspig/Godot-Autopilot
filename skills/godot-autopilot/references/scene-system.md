# Scene System

The editor works on one edited scene at a time; other scenes stay open in tabs.
Node paths resolve against the edited scene root. Discover and describe a tool
before the first call; the calling protocol lives in `references/tools.md`.

## Scene lifecycle

1. `create_editor_scene` starts an empty scene with a typed root.
2. Build nodes, properties, and signals.
3. `save_editor_scene` or `save_editor_scene_as` persists to disk.
4. `open_editor_scene` / `close_editor_scene` switches scenes.
5. `set_editor_main_scene` makes a scene the project's launch scene.

- `create_editor_scene` accepts only native Node subclasses; an inherited scene
  cannot be created this way. Copy the file or open it and use
  `save_editor_scene_as`.
- Replacing a scene with unsaved changes is refused. Save first, then retry with
  `close_current`. A type that is not a Node subclass fails explicitly.
- `open_editor_scene` also refuses while the current scene is unsaved; the error
  lists the unsaved paths and suggests saving or `reload_editor_scene`.
- `save_editor_scene` on a never-saved scene writes to
  `res://<root node name>.tscn`; use `save_editor_scene_as` for an explicit path.
  `save_editor_scenes` saves every open tab, but scenes without a file path
  cannot be written.
- `close_editor_scene` refuses unsaved changes. `reload_editor_scene` restores
  from disk and discards edits without confirmation; it also clears the scene's
  undo history. Save first when the changes matter.
- `verify_scene_saved` compares the in-memory tree with the on-disk `.tscn` and
  reports missing paths. Run it after a save when every node must persist.

## Nodes

`create_scene_node` takes `type`, `name`, `parent_path`, and optional
`properties`. Omit `parent_path` only while the scene has no root; once a root
exists, omitting it fails with `scene already has a root, use parent_path to add
children`.

`properties` is applied immediately through the same conversion chain as
`property_set`: NodePath strings become node references, `{"path":
"res://..."}` resolves a resource, and
`{"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}`
creates an inline sub-resource. A failing property frees the new node and
reports the property name, so a failed create leaves no half-configured node.
Creation is one undo step.

`build_nodes_from_spec` builds a subtree from one declarative spec. `dry_run`
validates and converts without writing; a failed step rolls the whole build back.

- `delete_scene_node` takes `path`; the root refuses deletion. The node is freed
  at the end of the frame, so an immediate tree read may still list it. Undo
  re-adds it with owner and sibling index while the deferred free has not run.
- `rename_scene_node` rejects `/`, `:`, and duplicate sibling names. It is one
  undo step.
- `reparent_node` takes `path` and `new_parent_path`; it refuses the root, the
  node itself, its descendants, and its current parent. `keep_world_position`
  preserves the transform when both nodes share the same 2D or 3D type. Never
  emulate a move with manual remove/add: that records no undo step and skips
  owner and sibling fix-up.
- `instantiate_scene` instantiates a `.tscn` under the edited scene. `owner`
  defaults to true so the instance's children are saved; persistent connections
  from the source scene are inherited, so do not reconnect them.

Groups are strings; `add_group_node` and `remove_group_node` are undoable
and persist with the scene. `has_group_node` checks membership.

## Inspection

- `get_scene_tree` returns `name`, `type`, `path`, and `children` per node.
  `max_depth` defaults to 8 (`-1` unlimited); `include_properties` adds up to 20
  filtered properties per node.
- `get_editor_edited_scene_root` starts scene workflows; it is null when no
  scene is open.
- `property_get_list` returns every property with type, hint, hint_string, usage
  flags, and class_name. `only_script_variables` and `property_filter` narrow it.
- `get_editor_selection` and `set_editor_selection` read or focus the selection;
  `capture_editor_viewport` returns a PNG for visual verification.
- `validate_scene_file` dry-runs a scene file: it loads without the resource
  cache, checks dependencies, instantiates, and frees. Use it before committing
  bulk edits.

## Properties

- `property_get` (`path`, `property`) returns the exact serialized value.
  Unknown nodes or properties error with candidate suggestions.
- `property_set` single form takes `path`, `property`, and `value`. Batch form
  takes `path` plus `properties` (mutually exclusive, at most 32 entries). Batch
  applies object-typed entries first, then the rest.
- Property names accept dot paths (`a.b.c`) into sub-resources: intermediates
  are fetched, the leaf is type-checked and set. There is no expression
  evaluation or method call.
- Omit `type_hint` to infer the Variant type. Every write is read back: a
  mismatch is an error or a warning when the engine converted the value. For
  Object/Array properties, a value the engine did not apply is an error prefixed
  `value not applied`.
- Every successful set is undoable, except when the old or new value is an
  Object reference; that response reports `undoable: false` with a reason.
- Node-typed properties accept a node path string, converted to a node
  reference. An unresolvable path is an error.
- Array properties convert elements individually: node elements take a path or
  `{"__node_ref__": "..."}`; resource elements take a `res://...` or
  `memory://...` string, `{"path": "res://..."}`, or null. A non-array value is
  an error rather than clearing the property.
- Dictionary properties take a JSON object. SpriteFrames `animations` accepts
  the engine array or a name-to-`{frames, speed, loop}` map.
- Resource properties accept inline descriptions; nesting is limited to four
  levels. The sub-resource is written as a `sub_resource` on save.
- Assigning a string to an int property silently converts to 0 and reports ok.
  After any unusual value shape, read back with `property_get`.

Value shapes:

| Godot type | JSON |
|---|---|
| Vector2 | `{"x": 1, "y": 2}` |
| Vector3 / Vector3i | `{"x": 1, "y": 2, "z": 3}` |
| Vector4 / Quaternion | `{"x": 0, "y": 0, "z": 0, "w": 1}` |
| Color | `{"r": 1, "g": 0.5, "b": 0.25, "a": 1}` |
| Rect2 / Rect2i | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| AABB | `{"position": {...}, "size": {"w": 64, "h": 32, "d": 16}}` |
| Transform2D | `{"columns": [[1, 0], [0, 1], [x, y]]}` |
| Transform3D | `{"basis": {"rows": [[...]]}, "origin": {...}}` |
| Resource | `{"path": "res://icon.svg"}` or `{"resource": "memory://name"}` |
| Inline sub-resource | `{"type": "RectangleShape2D", "properties": {...}}` |
| Node reference | path string; in an array `{"__node_ref__": "Path/To/Node"}` |

Rect2/AABB sizes require `w`/`h` (and `d` for AABB); conflicting aliases error.
Transform2D reads `columns` only. Vector3/Vector4/Quaternion require every
component. Default-valued properties are omitted from the saved file, so a
missing line means "matches the default", not data loss.

## Signals

- `signal_connect` requires `source_path`, `signal`, `target_path`, and
  `method`. `persist` defaults to true and uses CONNECT_PERSIST, so the
  connection is saved and inherited by every instantiation. An existing pair
  returns `already_connected`.
- `signal_disconnect` is idempotent; a missing pair returns `not_connected` and
  is not an error.
- `trace_signal_flow` inspects edges around a node; each edge reports `from`,
  `signal`, `to`, `method`, and `persisted`.

## Undo model

Each editing tool records exactly one action. Actions route by target object:
edited-scene nodes and built-in sub-resources land in that scene's history;
everything else lands in the global history. Each tab has its own history, and
committing an action clears the other redo stacks. An action with no operations
is discarded. Undoing a Resource operation does not make the resource clean
again; per-history saved versions decide the unsaved state.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| Property write has no effect | Read-only, wrong type, or engine conversion | `property_get`; inspect the metadata |
| Saved scene drops a node | Owner is not the scene root | Set owner or use `instantiate_scene` correctly |
| Node missing right after delete | Deferred free at end of frame | Read the tree again next frame |
| Scene save loses connections | Connection was not persistent | Reconnect with `persist: true` |
| Node path not found | Wrong path form | Use `Root/Child`, `/root/Root/Child`, or `Child` |
| Scene switch reported ok but did not happen | Editor was mid scene-change | Retry after the change settles |
