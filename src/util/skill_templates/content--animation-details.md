# Animation Details

Reference for the animation workflow in the content skill: track and
keyframe semantics, AnimationTree state machine rules, SpriteFrames details,
and the silent behaviors verified against engine source. The content skill
page has the condensed workflow.

## AnimationPlayer details

### Creating player and clips

```json
{"name": "create_scene_animation_player", "arguments": {"parent_path": "Root", "name": "AnimationPlayer"}}
```

`parent_path` is required (scene-relative, e.g. Root/Actors); `name` defaults
to AnimationPlayer.

```json
{"name": "create_animation", "arguments": {"player_path": "AnimationPlayer", "name": "walk", "length_sec": 1.0, "loop_mode": 1}}
```

- `name` must not already exist - check `get_animation_list` first,
  otherwise the call errors.
- `length_sec` - clip length in seconds (default 1.0).
- `loop_mode` - 0 none, 1 linear loop, 2 pingpong.

### Tracks

```json
{"name": "create_animation_track", "arguments": {"player_path": "AnimationPlayer", "anim_name": "walk", "track_type": "value", "node_path": "Sprite2D", "property": "position:x"}}
```

- `track_type` - value animates a property; method invokes a method per
  key.
- `node_path` - scene-relative path of the node being animated.
- `property` - required for value tracks (e.g. position:x or modulate:a);
  the track path becomes node:property and the update mode is continuous.
  Method tracks ignore it.
- Duplicate tracks for the same node and property are rejected. The
  response returns the track index to use when inserting keys.

#### UpdateMode values and track lookup

An Animation value track has exactly three update modes:
`UPDATE_CONTINUOUS` (interpolate between keys - what the tools set up),
`UPDATE_DISCRETE` (jump between keys; required for bool, int and String
properties, which cannot interpolate) and `UPDATE_CAPTURE` (capture the
current property value as the starting key).

`Animation.find_track` matches on BOTH the track path and the track type -
querying an existing path with the wrong type returns -1 as if the track did
not exist. When scripting lookups via `code_execute`, pass the exact type
the track was created with.

### Keyframes

```json
{"name": "insert_animation_keyframe", "arguments": {"player_path": "AnimationPlayer", "anim_name": "walk", "track_index": 0, "time": 0.5, "value": {"x": 0, "y": 1, "z": 2}}}
```

- `time` - seconds since clip start (0 or more).
- `value` for value tracks is the property value as JSON: a number like 1.5
  or an object like a Vector3. For method tracks pass an object such as
  {"method": "jump", "args": [42]}.

#### Inserting at an existing time

Inserting a key at a time where the track already has one REPLACES it, and
two things bite:

- the old key's transition value is kept (the replacement inherits it), and
- the engine's `track_insert_key` returns the PREVIOUS key's index in that
  case, not a freshly appended one.

Index-based follow-up code must not assume an append; verify key counts via
`code_execute` or by playing the clip.

### Removing things

- `remove_animation_track` deletes a whole track by `track_index`
  (out-of-range indexes error).
- `remove_animation` deletes a clip by name, searching all libraries of the
  player.
- There is no single-key delete tool; for surgical key edits use
  `code_execute` against the Animation resource.

`get_animation_list` returns every clip with its length in seconds and loop
mode - use it to verify your work after building tracks and keys. Clips live
inside the AnimationPlayer's default library, so once you call
`save_editor_scene` the animations persist with the scene.

## AnimationTree state machine details

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
{"name": "connect_animation_states", "arguments": {"animation_tree_path": "AnimationTree", "from_state": "idle", "to_state": "run", "condition": "is_running"}}
```

- `state_name` must be unique in the machine; `animation` assigns the clip
  played by that state.
- `condition` names a bool parameter; the transition is switched to
  automatic advance with that condition. Omit it for a manual transition.
- Duplicate transitions are rejected, and both states must already exist -
  build the tree and its states first.

### travel semantics

`AnimationNodeStateMachine.travel(target)`:

- When no transition path can be constructed from the current state to the
  target, travel does NOT error - it silently teleports: the machine jumps
  straight to the target and states reset according to their
  reset_on_teleport flags. A "travel ignored my route, the animation just
  jumped" symptom usually means a missing or one-way transition.
- A target state name that does not exist raises an error.
- If the currently playing state is removed, playback silently stops - no
  error, the tree just goes quiet.

### AUTO transitions: three silent no-triggers

An automatic (AUTO mode) transition fails to fire for three independent
reasons, all silent:

1. The transition mode is not AUTO - the condition/advance path is ignored
   entirely; manual transitions only advance via `travel`.
2. The condition parameter is not registered on the AnimationTree, or
   evaluates false - the transition stays closed.
3. The transition's advance expression errors - a failed expression counts
   as "condition not met", not as an error.

Symptom: the machine sits in a state forever. Check mode, parameter
registration and expression, in that order.

## SpriteFrames details

```json
{"name": "create_spriteframes", "arguments": {"name": "hero_frames"}}
```

`create_spriteframes` registers a SpriteFrames in memory (nothing on disk
yet) and, as a side effect, removes the built-in default animation - the
response flags this. Always add named animations explicitly before adding
frames.

```json
{"name": "add_spriteframes_animation", "arguments": {"name": "hero_frames", "animation": "walk", "fps": 8, "loop": true}}
```

```json
{"name": "add_spriteframes_frame", "arguments": {"name": "hero_frames", "animation": "walk", "texture": "res://sprites/hero_sheet.png", "hframes": 4, "vframes": 2}}
```

- `texture` must exist on disk and import as a Texture2D; failures come
  back with an error detail suggesting a reimport when appropriate.
- `duration` (default 1.0) scales per-frame timing.
- `hframes` and `vframes` (default 1 each) split a sprite sheet into a
  grid; one AtlasTexture frame is added per cell. Leave both at 1 to add
  the whole texture as a single frame.

To use the resource on a node, save it to disk first - a memory:// resource
cannot be assigned to a node property directly:

```json
{"name": "save_resource", "arguments": {"name": "hero_frames", "path": "res://sprites/hero_frames.tres"}}
```

```json
{"name": "property_set", "arguments": {"path": "Hero", "property": "sprite_frames", "value": {"path": "res://sprites/hero_frames.tres"}}}
```

## Godot 4.7+ notes

- AnimatedSprite2D uses the `sprite_frames` property - the Godot 3 name
  `frames` no longer exists. If a `property_set` call fails on an animated
  sprite, the scene-system skill's rename table lists the likely rename.
- UPDATE_DISCRETE is required for bool/int/String value tracks; a
  CONTINUOUS track on such a property is an authoring bug, not an engine
  error.

## See also

- The [content skill](../SKILL.md) overview for the condensed animation
  workflow.
- [TileMap details](tilemap-details.md) - tile-based levels for these
  characters to walk on.
- godot-autopilot-scene-system - creating the nodes these animations drive,
  and the property rename table.
- godot-autopilot-scripting - `code_execute` for surgical key edits,
  `find_track` lookups and travel debugging.
- godot-autopilot-runtime - play the scene and watch the animation live.
