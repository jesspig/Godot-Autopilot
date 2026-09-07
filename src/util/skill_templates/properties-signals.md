# Node Properties and Signals

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
