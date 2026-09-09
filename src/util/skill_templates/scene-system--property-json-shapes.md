# Property JSON Shapes and Renames

The JSON value shapes godot-autopilot accepts for Godot types, silent
conversions to watch, the Godot 3 to 4 renamed-property table, and the
serialization rules that decide which property lines end up in the scene file.

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

`property_set` and `create_scene_node` (`properties` object) accept these
shapes through the same conversion chain.

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

Read-only or private-setter properties on C# nodes are invisible to GDScript:
reading a property without a getter returns null. When validating C#-derived
values through `property_get`, check readable exported fields instead of
derived values to avoid false failures.

## memory:// resources must be saved first

`property_set` rejects memory:// resources (resources created in memory
without a file path) as node property values — writing them into the scene
file would corrupt it. Save the resource to disk with `save_resource` first,
then assign it via its `res://` path in the `{"path": "res://..."}` shape.

## See also

- godot-autopilot-resources — `save_resource` and resource lifecycles
- references/scene-format-tokens.md — which tokens the saved file actually contains
- references/undo-history.md — why every `property_set` is one undo step
