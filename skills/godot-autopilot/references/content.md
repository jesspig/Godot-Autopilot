# Content Pipeline

TileMap levels, animation, audio, and UI theming. Scene tools act on the edited
scene; audio bus tools act on the editor process.

## TileMap and TileSet

Build in order: node, TileSet, atlas source, physics layer, cells.

1. `create_tilemap` adds the node with a default TileSet; the scene needs a root.
2. `create_tilemap_tileset` creates an in-memory TileSet.
3. `add_tilemap_atlas_source` slices a `res://` texture. `source_id` (0-255)
   must be unused; `margin` and `spacing` are optional.
4. `add_tilemap_physics_layer` is required before per-tile collision.
5. `save_resource` the TileSet, then `property_set` the node's `tile_set` to the
   saved path. Never assign `memory://` directly. Finish with
   `save_editor_scene`.
6. `set_tilemap_cell` / `set_tilemap_cells` place tiles; both TileMap and
   TileMapLayer are accepted.
7. `set_tilemap_tile_collision` takes polygon points relative to the tile center;
   an array of arrays sets multiple polygons, an empty array clears.

Gotchas:

- `set_cell` with only one of source id, atlas coords, or alternative `INVALID`
  silently clears the cell.
- Oversized bulk JSON can fail as a parse error before the tool runs. For
  map-scale fills, loop over `set_cell` with `code_execute`. `source_id: -1`
  clears a cell.
- Invalid bulk entries are skipped with a warnings list and a set count; read
  both.
- A missing or unimported texture errors at atlas creation.
- In 4.8, TileMap is a thin wrapper over TileMapLayer; prefer TileMapLayer.

## Animation

`AnimationPlayer`: `create_scene_animation_player` adds the node
(`parent_path` required); `create_animation` creates a clip (`loop_mode` 0 none,
1 linear, 2 pingpong); `create_animation_track` adds a value or method track
(value tracks need `property`, for example `position:x`);
`insert_animation_keyframe` takes `time` and the JSON value or
`{"method": ..., "args": [...]}`. `get_animation_list` verifies lengths and loop
modes. `remove_animation_track` deletes by index; `remove_animation` deletes a
clip. There is no single-key delete tool; use `code_execute`.

`AnimationTree`: `create_scene_animation_tree` creates an empty state machine.
Add states with `add_animation_machine_state` and connect with
`connect_animation_states`; `condition` names a bool parameter for automatic
advance. Both states must exist and duplicate transitions are rejected.

`SpriteFrames`: `create_spriteframes` creates an in-memory resource and removes
the default animation. `add_spriteframes_animation` sets fps/loop;
`add_spriteframes_frame` adds frames, with `hframes`/`vframes` splitting a sheet
and `duration` scaling timing. Save, then `property_set` `sprite_frames`.

Gotchas:

- A key inserted at an existing time replaces it but keeps the old transition;
  the engine returns the previous index.
- `travel` with no constructable path teleports silently; a missing destination
  errors; deleting the playing state stops playback.
- AUTO transitions fail silently when the mode is not AUTO, the condition
  parameter is unregistered or false, or the expression errors.
- Value track update modes are CONTINUOUS, DISCRETE, or CAPTURE; bool, int, and
  String properties need DISCRETE.

## Audio

Master is bus 0; setters take a zero-based `bus_index` and apply immediately.
`get_audio_bus_layout` snapshots the layout and is the read-back for names,
volumes, and effect chains. `set_audio_bus_volume_db`, `set_audio_bus_mute`,
`set_audio_bus_solo`, and `set_audio_bus_bypass_effects` are the per-bus
switches. `add_audio_bus_effect` takes an AudioEffect class name; `at_position`
inserts, -1 appends. `set_audio_bus_layout` replaces the entire layout: get the
object, edit it, write it back.

Player tools: `play_audio_player`, `stop_audio_player`,
`set_audio_player_volume_db`, `set_audio_player_pitch_scale`,
`get_audio_player_playback_position`, and `seek_audio_player`.
`set_audio_device_output` / `set_audio_device_input` switch the editor device.

Gotchas:

- A player whose `bus` names a missing bus silently plays on Master.
- Adding or renaming a bus onto an existing name silently appends " 2".
- Bypass skips the chain; mute multiplies by zero after effects, so a muted bus
  still spends CPU. Solo overrides mute.

## UI theming and layout

1. `create_theme_resource` creates a `.tres`; missing directories are created and
   an existing file is never overwritten.
2. `set_theme_color`, `set_theme_constant`, `set_theme_font_size`, and
   `set_theme_stylebox_flat` take `path`, `theme_type`, and `varname`, then
   load-modify-save. Run `get_theme_info` first for valid type/item pairs.
3. `apply_theme_to_control` assigns the theme in the edited scene; finish with
   `save_editor_scene`.

`set_control_anchor_preset` applies one of the 16 presets (`center` is 8,
`full_rect` is 15). `keep_offsets` defaults to false, so offsets are recomputed.

For runtime UI, `get_game_ui_elements` returns Control rectangles; for
short-lived HUD feedback use `capture_game_viewport` with `when`.

Gotchas:

- `set_anchors_preset` keeps offsets by default, while
  `set_anchors_and_offsets_preset` forces a recompute.
- Anchored Controls do not persist `position`/`size`; `anchor_*`, `offset_*`,
  and `grow_*` are the saved truth.
- Theme lookup exhausts each ancestor's whole type chain before moving outward;
  a near generic item beats a far exact variation.

## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| A placed tile disappears | `set_cell` got one coordinate field | Pass all cell fields together |
| Tile collision missing | No physics layer or polygon | Add the layer, then set collision |
| AUTO transition does nothing | Wrong mode or unregistered condition | Check mode and the bool parameter |
| A sound plays on Master | Player bus name does not exist | Read the bus layout and fix the name |
| Theme item not applied | Wrong type/item pair or variation chain | Run `get_theme_info`; check ancestors |
