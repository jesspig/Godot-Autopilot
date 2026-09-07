# Animation

Animation workflows via godot-autopilot: AnimationPlayer clips with value and
method tracks, AnimationTree state machines, and SpriteFrames resources for
AnimatedSprite2D.

## AnimationPlayer workflow

1. `create_scene_animation_player` - add an AnimationPlayer node.
2. `create_animation` - create a clip in its default library.
3. `create_animation_track` - add a value or method track.
4. `insert_animation_keyframe` - add keys to the track.
5. `get_animation_list` - list clips and metadata.

```json
{"name": "create_scene_animation_player", "arguments": {"parent_path": "Root", "name": "AnimationPlayer"}}
```

`parent_path` is required (scene-relative, e.g. Root/Actors); `name` defaults
to AnimationPlayer.

```json
{"name": "create_animation", "arguments": {"player_path": "AnimationPlayer", "name": "walk", "length_sec": 1.0, "loop_mode": 1}}
```

- `name` must not already exist - check `get_animation_list` first, otherwise
  the call errors.
- `length_sec` - clip length in seconds (default 1.0).
- `loop_mode` - 0 none, 1 linear loop, 2 pingpong.

### Tracks

```json
{"name": "create_animation_track", "arguments": {"player_path": "AnimationPlayer", "anim_name": "walk", "track_type": "value", "node_path": "Sprite2D", "property": "position:x"}}
```

- `track_type` - value animates a property; method invokes a method per key.
- `node_path` - scene-relative path of the node being animated.
- `property` - required for value tracks (e.g. position:x or modulate:a); the
  track path becomes node:property and the update mode is continuous. Method
  tracks ignore it.
- Duplicate tracks for the same node and property are rejected. The response
  returns the track index to use when inserting keys.

### Keyframes

```json
{"name": "insert_animation_keyframe", "arguments": {"player_path": "AnimationPlayer", "anim_name": "walk", "track_index": 0, "time": 0.5, "value": {"x": 0, "y": 1, "z": 2}}}
```

- `time` - seconds since clip start (0 or more).
- `value` for value tracks is the property value as JSON: a number like 1.5 or
  an object like a Vector3. For method tracks pass an object such as
  {"method": "jump", "args": [42]}.

### Removing things

- `remove_animation_track` deletes a whole track by `track_index`
  (out-of-range indexes error).
- `remove_animation` deletes a clip by name, searching all libraries of the
  player.
- There is no single-key delete tool; for surgical key edits use `code_execute`
  against the Animation resource.

`get_animation_list` returns every clip with its length in seconds and loop
mode - use it to verify your work after building tracks and keys.

Clips live inside the AnimationPlayer's default library, so once you call
`save_editor_scene` the animations persist with the scene.

## AnimationTree state machines

```json
{"name": "create_scene_animation_tree", "arguments": {"parent_path": "Root", "anim_player": "AnimationPlayer"}}
```

The new AnimationTree gets an empty AnimationNodeStateMachine as its root.
`anim_player` (optional) wires an AnimationPlayer into the tree's
animation_player property; `name` defaults to AnimationTree.

```json
{"name": "add_animation_machine_state", "arguments": {"animation_tree_path": "AnimationTree", "state_name": "idle", "animation": "idle"}}
```

```json
{"name": "add_animation_machine_state", "arguments": {"animation_tree_path": "AnimationTree", "state_name": "run", "animation": "walk"}}
```

```json
{"name": "connect_animation_states", "arguments": {"animation_tree_path": "AnimationTree", "from_state": "idle", "to_state": "run", "condition": "is_running"}}
```

- `state_name` must be unique in the machine; `animation` assigns the clip
  played by that state.
- `condition` names a bool parameter; the transition is switched to automatic
  advance with that condition. Omit it for a manual transition.
- Duplicate transitions are rejected, and both states must already exist -
  build the tree and its states first.

## SpriteFrames for AnimatedSprite2D

```json
{"name": "create_spriteframes", "arguments": {"name": "hero_frames"}}
```

`create_spriteframes` registers a SpriteFrames in memory (nothing on disk yet)
and, as a side effect, removes the built-in default animation - the response
flags this. Always add named animations explicitly before adding frames.

```json
{"name": "add_spriteframes_animation", "arguments": {"name": "hero_frames", "animation": "walk", "fps": 8, "loop": true}}
```

```json
{"name": "add_spriteframes_frame", "arguments": {"name": "hero_frames", "animation": "walk", "texture": "res://sprites/hero_sheet.png", "hframes": 4, "vframes": 2}}
```

- `texture` must exist on disk and import as a Texture2D; failures come back
  with an error detail suggesting a reimport when appropriate.
- `duration` (default 1.0) scales per-frame timing.
- `hframes` and `vframes` (default 1 each) split a sprite sheet into a grid;
  one AtlasTexture frame is added per cell. Leave both at 1 to add the whole
  texture as a single frame.

To use the resource on a node, save it to disk first - a memory:// resource
cannot be assigned to a node property directly:

```json
{"name": "save_resource", "arguments": {"name": "hero_frames", "path": "res://sprites/hero_frames.tres"}}
```

```json
{"name": "property_set", "arguments": {"path": "Hero", "property": "sprite_frames", "value": {"path": "res://sprites/hero_frames.tres"}}}
```

## Godot 4.7 note

AnimatedSprite2D uses the sprite_frames property - the Godot 3 name frames no
longer exists. The full Godot 3 to 4 property rename table is in the
properties-signals skill; if a property_set call fails, that skill lists the
likely rename.

## See also

- godot-autopilot-tilemap - tile-based levels for these characters to walk on.
- godot-autopilot-ui-theming - styling and laying out UI controls.
- godot-autopilot-properties-signals - property JSON shapes and renames.
- godot-autopilot-scene-building - creating the nodes these animations drive.
