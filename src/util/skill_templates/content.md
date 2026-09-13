# Godot Content Pipeline

Four content domains via godot-autopilot: TileMap and TileSet levels,
AnimationPlayer and AnimationTree animation, audio buses and playback, and
Control theming and layout. Each section below is the condensed workflow plus
that domain's top gotchas; the `references/` files carry the full tool
payloads and the source-verified silent-failure details. Scene tools act on
the scene open in the Godot editor; audio bus tools act on the editor
process (a running game keeps its own device and layout).

## TileMap and TileSet

Build order matters: node, then TileSet, atlas source, physics layer, then
cells.

1. `create_tilemap` - the TileMap node with an attached default TileSet
   (`name`, `tile_size` pixels, `parent_path`; the scene must already have a
   root, otherwise the call errors).
2. `create_tilemap_tileset` - a TileSet resource in memory (memory://
   reference; nothing on disk yet).
3. `add_tilemap_atlas_source` - slice a `res://` texture into a tile grid
   (`source_id` 0-255 must be unused; optional `margin`/`spacing`). Returns
   the source id, created tile count and grid size.
4. `add_tilemap_physics_layer` - required before per-tile collision
   (optional `collision_layer`/`collision_mask` bitmasks, default 1).
5. `save_resource` the TileSet to disk, then `property_set` the node's
   `tile_set` to the saved path - a memory:// resource must never be assigned
   to a node property directly. Finish with `save_editor_scene`.
6. `set_tilemap_cell` / `set_tilemap_cells` - place tiles. `source_id` must
   already exist in the node's TileSet; cell tools accept TileMap or
   TileMapLayer nodes.
7. `set_tilemap_tile_collision` - polygon points are relative to the tile
   center (a full 16 px square is (-8,-8)..(8,8)); an array of arrays sets
   multiple polygons, an empty array clears.

Bulk example:

```json
{"name": "set_tilemap_cells", "arguments": {"node_path": "Ground", "cells": [{"x": 0, "y": 9, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}, {"x": 1, "y": 9, "source_id": 0, "atlas_coords": {"x": 1, "y": 0}}]}}
```

Top gotchas (details in the reference):

- `set_cell` with exactly one of source id / atlas coords / alternative
  INVALID silently clears the cell instead of erroring.
- `set_tilemap_cells` has no fixed server-side entry cap; oversized JSON
  arguments can hit client or transport request limits and fail as a parse
  error before the tool runs. For map-scale fills, loop over `set_cell` with
  `execute_script` or `code_execute`. An entry with `source_id` -1 clears the
  cell at those coordinates.
- Invalid entries in a bulk call are skipped with a warnings list and a set
  count - read both back.
- A texture that is missing on disk or not yet imported errors at atlas
  creation.
- 4.8: TileMap/TileMapLayer/TileSet moved into modules/; TileMap is now a
  thin wrapper over TileMapLayer - prefer TileMapLayer for new work.

## Animation

Three sub-workflows: AnimationPlayer clips, AnimationTree state machines, and
SpriteFrames for AnimatedSprite2D.

### AnimationPlayer

1. `create_scene_animation_player` - add the node (`parent_path` required,
   e.g. Root/Actors; `name` defaults to AnimationPlayer).
2. `create_animation` - a clip in the player's default library. `name` must
   not exist yet (check `get_animation_list` first); `loop_mode` 0 none, 1
   linear, 2 pingpong; `length_sec` default 1.0.
3. `create_animation_track` - `track_type` value or method. Value tracks need
   `property` (e.g. position:x or modulate:a) and become node:property paths;
   duplicate node+property tracks are rejected.
4. `insert_animation_keyframe` - `time` in seconds. Value tracks take the
   property value as JSON (number or object); method tracks take
   {"method": ..., "args": [...]}.
5. `get_animation_list` - verify clip lengths and loop modes after building.

```json
{"name": "create_animation", "arguments": {"player_path": "AnimationPlayer", "name": "walk", "length_sec": 1.0, "loop_mode": 1}}
```

`remove_animation_track` deletes a track by index; `remove_animation` deletes
a clip by name across all libraries. There is no single-key delete tool - use
`code_execute` for surgical key edits. Clips persist with the scene after
`save_editor_scene`.

### AnimationTree state machines

`create_scene_animation_tree` creates the tree with an empty
AnimationNodeStateMachine root and optionally wires `anim_player`. Add states
with `add_animation_machine_state` (unique `state_name`; `animation` assigns
the clip) and connect with `connect_animation_states` - `condition` names a
bool parameter and switches the transition to automatic advance; omit it for
a manual transition. Both states must exist first; duplicate transitions are
rejected.

### SpriteFrames

`create_spriteframes` creates an in-memory SpriteFrames and, as a flagged
side effect, removes the built-in default animation. Add named animations
with `add_spriteframes_animation` (fps, loop), then `add_spriteframes_frame` -
optional `hframes`/`vframes` split a sheet into one AtlasTexture frame per
cell, `duration` scales per-frame timing. Save with `save_resource`, then
`property_set` the node's `sprite_frames` (the Godot 3 `frames` name is gone).

Top gotchas (details in the reference):

- Inserting a key at an existing time replaces it but keeps the old key's
  transition, and the engine returns the PREVIOUS index - do not treat it as
  a fresh append.
- `travel` with no constructable transition path silently teleports (states
  reset per reset_on_teleport); a missing destination name errors; deleting
  the currently playing state silently stops playback.
- AUTO transitions fail silently when the mode is not AUTO, the condition
  parameter is unregistered or false, or the expression errors.
- Value track update modes are only CONTINUOUS/DISCRETE/CAPTURE; bool, int
  and String properties need DISCRETE.

## Audio

Audio bus tools act on the editor process. Master is always bus 0; bus
setters take a zero-based `bus_index` and apply immediately.

1. `get_audio_bus_layout` - snapshot the complete layout first; it doubles as
   your read-back for bus count, names, volumes and effect chains.
2. `set_audio_bus_volume_db` / `set_audio_bus_mute` / `set_audio_bus_solo` /
   `set_audio_bus_bypass_effects` - the four per-bus switches.
3. `add_audio_bus_effect` - `effect_type` is an instantiable AudioEffect
   subclass class name (e.g. AudioEffectReverb); `at_position` inserts, -1
   appends. `remove_audio_bus_effect` takes the chain index - read the chain
   first.
4. `set_audio_bus_layout` - replaces the ENTIRE layout (count, names,
   volumes, effects). Never write back a hand-made partial object: get ->
   edit the returned object -> write back.
5. Player tools for AudioStreamPlayer/2D/3D: `play_audio_player` (optional
   `stream_path`, `from_position`), `stop_audio_player`,
   `set_audio_player_volume_db`, `set_audio_player_pitch_scale`,
   `get_audio_player_playback_position`, `seek_audio_player`.
6. Devices: `get_audio_device_outputs` / `get_audio_device_inputs` list
   hardware names; `set_audio_device_output` / `set_audio_device_input`
   switch the editor device (`device` must match a listed name).

Minimal example - a reverb on Master, then a shot:

```json
{"name": "get_audio_bus_layout", "arguments": {}}
{"name": "add_audio_bus_effect", "arguments": {"bus_index": 0, "effect_type": "AudioEffectReverb"}}
{"name": "play_audio_player", "arguments": {"node_path": "Level1/Player", "stream_path": "res://audio/shot.wav"}}
```

Top gotchas (details in the reference):

- A player whose `bus` names a nonexistent bus silently plays on Master.
- Adding or renaming a bus onto an existing name silently appends " 2" /
  " 3".
- bypass skips the effect chain; mute multiplies by zero AFTER effects - a
  muted bus still spends effect CPU. Solo overrides mute.
- A bus `send` to a missing bus or a cycle silently reroutes to Master; the
  layout caps at 256 buses.

## UI Theming and Layout

### Theme files

1. `create_theme_resource` - create the .tres under res:// (missing
   directories are created; an existing file is never overwritten;
   `base_type` is reference metadata only).
2. `set_theme_color` / `set_theme_constant` / `set_theme_font_size` /
   `set_theme_stylebox_flat` - each takes `path`, `theme_type` (e.g. Button)
   and `varname`, then load-modify-saves the file. Colors take #rrggbb,
   #rrggbbaa or rgba float objects; the stylebox builder takes optional
   bg_color, border_color, border_width, corner_radius and content_margin
   (a single number applies to all sides). Run `get_theme_info` first to
   list valid type/item pairs.
3. `apply_theme_to_control` - assign the theme to a Control in the edited
   scene; only the in-memory scene changes, so finish with
   `save_editor_scene`.

### Anchor presets and runtime recon

`set_control_anchor_preset` applies one of the 16 Control layout presets
(`center` is 8, `full_rect` is 15) to a node in the edited scene. The tool's
`keep_offsets` defaults to false - offsets are recomputed for the new
anchors. Persist with `save_editor_scene`.

While the game runs, `get_game_ui_elements` walks the running Control tree:
node path, type, visibility, optional text and global rectangle per Control
(`max_elements` default 100 / hard cap 1000, truncated responses are flagged;
`timeout_ms` default 5000 / max 30000). Use the rectangles to aim
`queue_game_input` - the game channel details are in the runtime skill.

Top gotchas (details in the reference):

- The engine preset APIs disagree: `set_anchors_preset` alone keeps offsets
  by default, `set_anchors_and_offsets_preset` forces a recompute - know
  which one your GDScript calls.
- Anchored Controls do not persist `position`/`size` in .tscn; anchor_*,
  offset_* and grow_* are the persisted truth.
- Theme lookup exhausts each ancestor's whole type chain before moving
  outward - a near ancestor's generic item beats a far ancestor's exact
  variation, and variation chains do not cross Theme boundaries.

## See also

- [TileMap and TileSet details](references/tilemap-details.md) - full
  payloads, serialization bounds, alternative-tile bits, silent failures.
- [Animation details](references/animation-details.md) - track/keyframe
  semantics, travel and AUTO transition rules, SpriteFrames payloads.
- [Audio details](references/audio-details.md) - bus naming and send
  routing, bypass vs mute CPU costs, polyphony stealing.
- [UI theme details](references/ui-theme-details.md) - preset enums, the
  persistence model, the theme lookup chain.
- godot-autopilot-scene-system - node lifecycle, `property_set` JSON value
  shapes, the property rename table.
- godot-autopilot-resources - `save_resource`, the texture import pipeline
  and .tres path rules.
- godot-autopilot-scripting - `code_execute` for bulk cell fills and
  surgical animation edits.
- godot-autopilot-runtime - run the scene, inspect the live UI tree and
  inject input at styled controls.
