# Property JSON Shapes and Renames

The JSON value shapes godot-autopilot accepts for Godot types, silent
conversions to watch, the Godot 3 to 4 renamed-property table, and the
serialization rules that decide which property lines end up in the scene file.

## JSON value shapes

| Godot type | JSON shape |
|---|---|
| Vector2 | `{"x": 1, "y": 2}` |
| Vector3 / Vector3i | `{"x": 1, "y": 2, "z": 3}` — all three components required |
| Vector4 / Vector4i | `{"x": 1, "y": 2, "z": 3, "w": 4}` — all four components required |
| Quaternion | `{"x": 0, "y": 0, "z": 0, "w": 1}` — all four components required |
| Color | `{"r": 1, "g": 0.5, "b": 0.25, "a": 1}` — `a` optional, defaults to 1 |
| Rect2 | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| Rect2i | `{"position": {"x": 0, "y": 0}, "size": {"w": 64, "h": 32}}` |
| AABB | `{"position": {"x": 0, "y": 0, "z": 0}, "size": {"w": 64, "h": 32, "d": 16}}` |
| Plane | `{"normal": {"x": 0, "y": 1, "z": 0}, "d": 0}` or `{"x": 0, "y": 1, "z": 0, "d": 0}` |
| Transform2D | `{"columns": [[1, 0], [0, 1], [x, y]]}` |
| Transform3D | `{"basis": {"rows": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]}, "origin": {"x": 0, "y": 0, "z": 0}}` |
| Basis | `{"rows": [[1, 0, 0], [0, 1, 0], [0, 0, 1]]}` |
| Projection | `{"columns": [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]}` |
| RID | read back as `{"id": 42}`; JSON does not restore a RID, so RID parameters take the integer handles the tools return |
| PackedByteArray | `[72, 101, 108, 108, 111]` |
| Resource | `{"path": "res://icon.svg"}` |
| Resource (inline sub-resource) | `{"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}` |
| Node reference | the node path string, e.g. `Player`; inside an array, `{"__node_ref__": "Player"}` is also accepted |
| Typed array | a JSON array whose elements are converted by the array's declared element type — see "Array properties" below |
| Dictionary | a JSON object, e.g. `{"speed": 1.5, "tags": ["a"]}`; a non-object value errors instead of writing an empty dict |
| SpriteFrames animations | engine array `[{"name": "idle", "frames": [...], "speed": 5.0, "loop": true}]` or dict `{"idle": {"frames": [...], "speed": 5.0, "loop": true}}` — see "SpriteFrames animations" below |

Size and transform key rules:

- Rect2 and Rect2i require `size` with `w` and `h`; `x` and `y` are
  accepted aliases for the same axes. When both spellings are present with
  different values the call errors. `position` is optional and defaults to
  `(0, 0)`.
- AABB requires `size` with `w`, `h` and `d`; `x`, `y` and `z` are accepted
  aliases under the same conflict rule. `position` is optional and defaults
  to `(0, 0, 0)`.
- Transform2D requires `columns`: at least 3 arrays of at least 2 numbers,
  in the `[x column, y column, origin column]` order. The `origin`, `x`, `y`
  spelling is not read.
- Transform3D requires `basis.rows` (3 arrays of 3 numbers); `origin` is
  optional and defaults to `(0, 0, 0)`.
- Plane accepts either shape above but not both at once: `normal` and
  top-level `x`/`y`/`z` are mutually exclusive, and `d` is always required.
  A zero normal is rejected when `d` is non-zero (WorldBoundaryShape3D and
  Jolt reject such planes); zero normal with `d == 0` is accepted.
- Vector3/Vector3i, Vector4/Vector4i and Quaternion require every component
  listed in the table as a JSON number; a missing or non-numeric component
  errors instead of silently defaulting to 0.
- Basis requires `rows` (at least 3 arrays of at least 3 numbers); Projection
  requires `columns` (at least 4 arrays of at least 4 numbers).
- Color requires `r`, `g` and `b` numbers; `a` is optional and defaults to 1.
- A RID property reads back as an object with an `id` number, but the
  conversion chain does not turn JSON back into a RID. RID-typed parameters
  take the integer handles returned by the tools that create or expose RIDs.
- PackedByteArray takes and returns a plain JSON number array; it is not a
  base64 string.

Example calls:

```json
{"name": "property_set", "arguments": {"path": "Player", "property": "position", "value": {"x": 12.5, "y": -3.25}}}
{"name": "property_set", "arguments": {"path": "Player", "property": "modulate", "value": {"r": 1, "g": 0.5, "b": 0.25, "a": 1}}}
{"name": "property_set", "arguments": {"path": "Sprite", "property": "texture", "value": {"path": "res://icon.svg"}}}
{"name": "property_set", "arguments": {"path": "Player", "properties": {"position": {"x": 0, "y": -10}, "visible": false}}}
{"name": "property_set", "arguments": {"path": "Hero", "property": "sprite_frames.animations", "value": {"idle": {"frames": [{"texture": "res://icon.svg"}], "speed": 5.0, "loop": true}}}}
```

`property_set` single form (`property` + `value`) and batch form (`properties`
object, max 32, object-typed entries first) plus `create_scene_node`
(`properties` object) accept these shapes through the same conversion chain;
set_resource_property uses the same strict conversion for resource fields.
Batch failures report `failed_property` and `applied_properties`; dot paths
(`a.b.c`) address sub-resources with full-path errors; dictionary exports that
happen to carry `type`/`properties` keys are still stored as plain dicts.

## Inline sub-resources

`property_set` and `create_scene_node` (`properties` object) accept a third
resource shape next to the file path and the memory reference: an inline
description that creates the resource and assigns it to the node property in
one step.

```json
{"name": "property_set", "arguments": {"path": "Player/CollisionShape2D", "property": "shape", "value": {"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}}}}
```

- `type` is a Resource class name — an engine class from ClassDB or a global
  script class, resolved exactly like `create_resource` resolves it. Abstract
  classes and unknown names error before anything is assigned.
- `properties` is optional; it maps property names of the new resource to
  values converted through the ordinary property chain (vectors, colors,
  arrays, nested resource references). A name the resource does not have is an
  error.
- The instance is pathless, so the editor writes it into the scene file as a
  `sub_resource` block when the scene is saved — unlike a memory:// resource,
  it never leaves a broken reference behind.
- Inline descriptions nest: a resource property may itself hold an inline
  description (for example an `AtlasTexture` inside an `AtlasTexture`), up to 4
  levels; deeper nesting errors before anything is created.
- Successful calls echo the created sub-resources in the result as
  inline_resources, an array of {property, type} entries.

The shape applies to node property assignment only. `set_resource_property`
does not accept it: build a standalone resource with `create_resource`, edit
it with `set_resource_property`, then save it with `save_resource`.

`{"path": "res://..."}` and `{"resource": "memory://..."}` keep their meaning;
when an object carries `path` or `resource` next to `type`, the file or memory
reference wins.

## Array properties

Typed arrays (`Array[Node]`, `Array[Resource]`, `[Export] Node[]`, ...) convert
element by element against the declared element type:

- Node elements: a node path string or `{"__node_ref__": "Path/To/Node"}`;
  the path resolves against the edited scene root like any node reference.
- Resource elements: a `res://...` or `memory://...` string, `{"path":
  "res://..."}` or `{"resource": "memory://..."}` (memory resources
  registered in the current session); inline descriptions are not accepted as
  array elements — assign them to a resource property instead.
- `null` passes through and leaves the slot empty.

Failure rules — these return an error instead of a silent write:

- A non-array value assigned to an array property (it used to clear the array
  silently; pass `[]` to clear explicitly).
- A JSON array assigned to a Vector2 or Vector2i property (arrays used to be
  written as (0, 0) silently; pass the object form {"x": 1, "y": 2} instead).
- An element of an array whose element type cannot be determined at all (an
  untyped `Array` holding object or array elements), or an element shape the
  tool cannot safely express for the declared element type.
- A node path that does not resolve in the edited scene.

## Dictionary properties

Dictionary-typed exports take a JSON object through inferred deserialization.
A non-object value (array, string, number) errors instead of silently writing
an empty dict. An object that happens to look like an inline resource
(`{"type": "enemy", "properties": {...}}`) is still stored as a plain dict —
inline descriptions only trigger for object-typed properties.

## SpriteFrames animations

`SpriteFrames.animations` is an engine array of
`{name, frames, speed, loop}` dicts. `property_set` accepts both shapes:

- engine array: `[{"name": "idle", "frames": [...], "speed": 5.0, "loop": true}]`
- dict: `{"idle": {"frames": [...], "speed": 5.0, "loop": true}}` or
  `{"idle": [...]}` (frames array directly, speed 5.0 and loop true by default)

Each `frames` entry is a texture ref (`res://...`, `{"path": "..."}`) or a
`{texture, duration}` object; `speed`/`fps` must be a number, `loop` a boolean,
`name` non-empty and matching the dict key when both are present. Failures name
the real resource (`SpriteFrames resource 'res://...'`), not the host node.
When the reflected type is dictionary rather than array, the same dict input is
stored directly — both engine shapes keep working.

## Slash names, dotted paths and property families

`property_set` (and inline `properties` maps) address properties in three
ways:

- **Exact full names, including names that contain `/`**: write the whole
  name as one string — "theme_override_colors/font_color",
  "theme_override_font_sizes/font_size", "metadata/my_key". The slash is part
  of the engine property name; do not split it or treat it as a path
  separator.
- **Dotted paths (`a.b.c`) into sub-resources**: each intermediate is fetched
  with `get` and must be a non-null **object**, so deep navigation works over
  object-typed intermediates such as "material.albedo_color" or
  `sprite_frames.animations`. A dot path whose intermediate is a value type
  errors with `segment '<name>' is not an object and cannot be traversed`
  (for example `position.x` on a Node2D — position is a Vector2, not an
  object). Only plain property names joined by dots — no method calls,
  indexing, or expression evaluation. Failures carry the full path plus the
  owner's candidates, and the owner is reported as the real resource path
  when the leaf lives on a resource.
- **Property family dict expansion**: when a single-segment name does not
  exist on the node but is a prefix of an engine property family, a non-empty
  object value is expanded item by item — this call writes
  theme_override_colors/font_color:

  ```json
  {"name": "property_set", "arguments": {"path": "Label", "property": "theme_override_colors", "value": {"font_color": {"r": 1, "g": 0, "b": 0, "a": 1}}}}
  ```

  The success response adds an expanded_from field; an unknown item errors
  and lists the available suffixes. `build_nodes_from_spec` applies the same
  recognition in `dry_run` and in a real write.

## Silent conversions to watch

Assigning a string to an int property silently converts to 0 and reports ok:
setting property `z_index` to `"abc"` succeeds and the property becomes 0.
After any set with an unusual value shape, read the property back with
`property_get` to confirm what actually landed.

## Godot 3 to 4 renamed properties

When `property_set` or `property_get` fails, the error automatically attaches
Levenshtein-based candidate suggestions plus this rename table. Check it
before inventing property names:

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

## Default values are never serialized

4.7+: when a resource or scene is written to disk, any property whose value
equals its computed default value is omitted from the file entirely. The
default takes property overrides and instantiated-scene state into account,
so "default" means "what a freshly built object would hold", not just the
class default. Practical consequences:

- A missing property line in a `.tscn` means "matches the default" — absence
  is not data loss. Do not "fix" a scene by re-adding default-valued lines.
- Reading a property always works even when nothing is serialized: the value
  comes from the object, not from the file.
- An Object-typed property holding null is also omitted unless the property
  explicitly allows storing null.
- In the editor, pinning a property (the pin next to the value) forces the
  value to be kept even when it equals the default.

Camera2D `enabled` is the canonical example: true is the default, so setting
it to true leaves nothing in the scene file. The `property_set` response
carries a serialization note explaining this.

Camera2D `current` has no setter at all. Make a camera current through
`code_execute` calling make_current():

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

## C# properties

C# `[Export]` node fields carry the same NodeType hint, so node path strings
convert to node references through the standard chain. An object-typed
assignment the engine does not actually apply (C# clears it back to null) is
now an error ("value not applied") and the tool attempts to restore the old
value; read-only or private-setter properties error the same way. For
validation, still prefer readable exported fields over derived values: C#
properties without a getter are invisible to GDScript, so reading one returns
null, which is easy to misread as a missing property.

## memory:// resources must be saved or created inline

`property_set` rejects memory:// resources (resources created in memory
without a file path) as node property values — writing them into the scene
file would corrupt it. Two supported routes instead:

- Create the resource inline with `{"type": "RectangleShape2D",
  "properties": {...}}` (see "Inline sub-resources" above): a pathless
  instance is written as a `sub_resource` when the scene is saved, in one call.
- Save the resource to disk with `save_resource` first, then assign it via its
  `res://` path in the `{"path": "res://..."}` shape.

## See also

- godot-autopilot-resources — `save_resource` and resource lifecycles
- references/scene-format-tokens.md — which tokens the saved file actually contains
- references/undo-history.md — why every `property_set` is one undo step
