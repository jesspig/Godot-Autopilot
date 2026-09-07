#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kTilemapDescription =
    R"gda_skill(TileMap and TileSet workflows via godot-autopilot: create tilemap nodes with a default TileSet, add atlas sources and physics layers, set single or bulk cells, and Godot 4.7 API notes. Use when building tile-based levels.)gda_skill";

const char *kAnimationDescription =
    R"gda_skill(Animation workflows via godot-autopilot: AnimationPlayer and AnimationTree state machines, value and method tracks, keyframes, and SpriteFrames resources for AnimatedSprite2D. Use when animating characters, objects or UI.)gda_skill";

const char *kUiThemingDescription =
    R"gda_skill(UI theming and layout via godot-autopilot: create .tres theme files, set color/constant/font-size/stylebox entries, apply themes to Control nodes, anchor presets, and enumerating the running game's Control tree. Use when styling controls or inspecting game UI.)gda_skill";

const char *kTilemapSkill = R"gda_skill(# TileMap and TileSet

TileMap and TileSet workflows via godot-autopilot: create a tilemap node, build
its TileSet from an atlas texture, write single or bulk cells, and add per-tile
collision. All tools below run against the scene currently open in the Godot
editor.

## Workflow at a glance

1. `create_tilemap` - create the TileMap node (a default TileSet is attached).
2. `create_tilemap_tileset` - create a TileSet resource in memory.
3. `add_tilemap_atlas_source` - slice a texture into tiles; optionally
   `add_tilemap_physics_layer` for collision.
4. Save the TileSet to disk and assign it to the node (`save_resource` +
   `property_set`).
5. `set_tilemap_cell` or `set_tilemap_cells` - place tiles.
6. `set_tilemap_tile_collision` - give individual tiles collision polygons.

The examples below show the payload you pass to `call_tool`: the tool name and
its arguments object.

## Create the TileMap node

```json
{"name": "create_tilemap", "arguments": {"name": "Ground", "tile_size": 16}}
```

- `name` - node name (default TileMap).
- `tile_size` - tile size in pixels (default 16); also sets the attached
  default TileSet tile size.
- `format` - cell quadrant size in pixels (default 16).
- `parent_path` - omit to add the node at the scene root; the scene must have a
  root (create one first, otherwise the call errors).

Cell-setting tools accept either a TileMap or a TileMapLayer node via `path`,
so you can also target TileMapLayer nodes created by other means.

## Build the TileSet

`create_tilemap_tileset` creates a TileSet in memory and registers it as a
memory:// reference; nothing is written to disk at this point.

```json
{"name": "create_tilemap_tileset", "arguments": {"name": "MainTileSet", "tile_size": 16}}
```

Then slice a texture into a tile grid:

```json
{"name": "add_tilemap_atlas_source", "arguments": {"name": "MainTileSet", "source_id": 0, "texture": "res://tiles/tileset.png", "tile_size": {"x": 16, "y": 16}}}
```

- `source_id` - 0-255, must be unused in this TileSet.
- `texture` - a res:// image path that exists on disk (and has been imported).
- `tile_size` - per-tile size in pixels; the texture is split into a grid and
  one tile is created per cell.
- `margin` and `spacing` - optional pixel values framing the atlas.

The call returns the assigned source id, the created tile count and the grid
size. To make tiles solid, add a physics layer before setting collision:

```json
{"name": "add_tilemap_physics_layer", "arguments": {"name": "MainTileSet", "layer_id": 0, "collision_layer": 1, "collision_mask": 1}}
```

`collision_layer` and `collision_mask` are optional bitmasks (default 1). A
TileSet can carry several physics layers; each adds another set of collision
polygons per tile.

## Persist the TileSet and assign it to the node

A memory:// resource must not be assigned directly to a node property - such an
assignment is rejected because it would corrupt the saved scene file. Save the
TileSet to disk first, then point the node at the file:

```json
{"name": "save_resource", "arguments": {"name": "MainTileSet", "path": "res://tiles/main_tileset.tres"}}
```

```json
{"name": "property_set", "arguments": {"path": "Ground", "property": "tile_set", "value": {"path": "res://tiles/main_tileset.tres"}}}
```

Finally call `save_editor_scene` to persist the scene with the assignment.

## Place cells

Single cell:

```json
{"name": "set_tilemap_cell", "arguments": {"path": "Ground", "x": 0, "y": 9, "source_id": 0, "atlas_coords": {"x": 3, "y": 0}}}
```

- `path` - the TileMap or TileMapLayer node.
- `x`, `y` - cell coordinates.
- `layer` - optional tile layer index (default 0); TileMapLayer nodes ignore it.
- `source_id` - the atlas source (default 0); it must exist in the node's
  TileSet, which is why the build order above matters.
- `atlas_coords` - which tile inside the atlas to place.

### Bulk writes and the 64-cell cap

`set_tilemap_cells` writes many cells in one call. Every entry needs `x`, `y`
and `source_id`; `atlas_coords` is optional per entry.

```json
{"name": "set_tilemap_cells", "arguments": {"node_path": "Ground", "cells": [{"x": 0, "y": 9, "source_id": 0, "atlas_coords": {"x": 0, "y": 0}}, {"x": 1, "y": 9, "source_id": 0, "atlas_coords": {"x": 1, "y": 0}}]}}
```

Keep batches at roughly 64 entries or fewer. Larger payloads can be truncated
by client-side parameter limits and then fail with a JSON parse error before
the tool ever runs - the symptom is a parse error, not a tool error. For row-,
floor- or map-scale fills, generate the layout programmatically with
`code_execute` (loop over TileMap.set_cell in GDScript) instead of hand-building
a huge JSON array. Invalid entries are skipped rather than failing the whole
call; the response reports how many cells were set plus a warnings list naming
the skipped ones.

## Per-tile collision

```json
{"name": "set_tilemap_tile_collision", "arguments": {"name": "MainTileSet", "source_id": 0, "atlas_coords": {"x": 0, "y": 0}, "physics_layer": 0, "polygon": [{"x": -8, "y": -8}, {"x": 8, "y": -8}, {"x": 8, "y": 8}, {"x": -8, "y": 8}]}}
```

- The TileSet needs an atlas source and a physics layer first (see the order
  above); a missing physics layer returns an error telling you to call
  `add_tilemap_physics_layer` first.
- Polygon points are relative to the tile center: for a 16 px tile a full-tile
  square is (-8,-8) to (8,8); a polygon starting at (0,0) hangs off the tile's
  lower-right quadrant.
- Pass an array of arrays to set multiple polygons, or an empty array to clear
  existing collision for that tile.
- If the engine rejects a polygon the tool reports an error detail after
  read-back verification.

## Godot 4.7 API notes

- TileSetAtlasSource.get_tile_data now takes (atlas_coords, alternative_tile) -
  the old Godot 3 argument order (source id first) is gone. The autopilot tools
  already use the new signature; if you write your own GDScript through
  `code_execute`, use the new signature too.
- CharacterBody2D.motion_mode: GROUNDED = 0, FLOATING = 1. For top-down tile
  levels set the character's motion mode to 1 explicitly; the default is 0
  (platformer gravity).

## Common errors

| Error | Cause and fix |
|---|---|
| scene already has a root | `create_tilemap` without `parent_path` on a non-empty scene; pass `parent_path`. |
| node not found / not a TileMap | `path` or `node_path` typo or wrong node type. |
| source not found | `source_id` was never added with `add_tilemap_atlas_source`. |
| no physics layer N | Call `add_tilemap_physics_layer` before `set_tilemap_tile_collision`. |
| texture errors | Texture path missing on disk or not imported; check the path. |
| JSON parse error on a large cells call | Payload exceeded the client limit; split into batches of about 64 or switch to `code_execute`. |

## See also

- godot-autopilot-animation - sprite sheets and SpriteFrames for the characters
  walking on your tiles.
- godot-autopilot-scene-building - scene and node lifecycle around the tilemap.
- godot-autopilot-properties-signals - JSON value shapes and property renames
  used by `property_set`.
- godot-autopilot-running-games - run the game to try the level and inject
  input.
)gda_skill";

const char *kAnimationSkill = R"gda_skill(# Animation

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
)gda_skill";

const char *kUiThemingSkill = R"gda_skill(# UI Theming and Layout

UI theming and layout via godot-autopilot: create .tres theme files, set
color/constant/font-size/stylebox entries, apply them to Control nodes, apply
anchor presets, and enumerate the running game's Control tree.

## Theme file lifecycle

1. `create_theme_resource` - create the .tres file on disk.
2. `set_theme_color` / `set_theme_constant` / `set_theme_font_size` /
   `set_theme_stylebox_flat` - add theme items.
3. `apply_theme_to_control` - assign the theme to a Control node.
4. `get_theme_info` - inspect the saved file.

```json
{"name": "create_theme_resource", "arguments": {"path": "res://themes/main.tres", "base_type": "Control"}}
```

- `path` must be inside res:// (a .tres path; user:// is rejected). Missing
  directories are created and the editor file system is refreshed after saving.
- An existing file is never overwritten - pick a new path or edit the file with
  the set tools below.
- `base_type` is stored as reference metadata only; Godot themes have no global
  base type, so real items are added with the set tools.

### Set theme items

Every set tool takes `path` (the saved .tres file), `theme_type` (the widget
class the item belongs to, e.g. Button or Label) and `varname` (the item name,
e.g. font_color), then loads the file, modifies it and saves it back.

```json
{"name": "set_theme_color", "arguments": {"path": "res://themes/main.tres", "theme_type": "Button", "varname": "font_color", "color": "#e0e0e0"}}
```

- `color` accepts a hex string (#rrggbb or #rrggbbaa) or an object with
  r/g/b/a floats in 0..1.
- `set_theme_constant` takes an integer `constant` (e.g. separation or
  outline_size).
- `set_theme_font_size` takes an integer `font_size`.
- `set_theme_stylebox_flat` builds a StyleBoxFlat from the `stylebox` object:
  optional bg_color and border_color (hex string or rgba object), border_width
  {left, top, right, bottom}, corner_radius (four corners) and content_margin
  (four sides); border_width and content_margin also accept a single number
  applied to all sides. Every field is optional.

Not sure which type and item pairs are valid? Run `get_theme_info` on the file
before the set tools - it lists every theme type with its color, constant,
font size and stylebox item names.

### Apply to a control

```json
{"name": "apply_theme_to_control", "arguments": {"theme_path": "res://themes/main.tres", "control_path": "UI/Panel"}}
```

Only the in-memory scene changes - call `save_editor_scene` afterwards to
persist the assignment.

## Anchor presets

`set_control_anchor_preset` applies one of the 16 Control layout presets to a
node in the edited scene:

| Preset | Value | Preset | Value |
|---|---|---|---|
| top_left | 0 | left_wide | 9 |
| top_right | 1 | top_wide | 10 |
| bottom_left | 2 | right_wide | 11 |
| bottom_right | 3 | bottom_wide | 12 |
| center_left | 4 | vcenter_wide | 13 |
| center_top | 5 | hcenter_wide | 14 |
| center_right | 6 | full_rect | 15 |
| center_bottom | 7 | center | 8 |

```json
{"name": "set_control_anchor_preset", "arguments": {"control_path": "UI/Panel", "preset": 15, "keep_offsets": false}}
```

`keep_offsets` (default false) recomputes the node's offsets for the new
anchors; set it true to keep the current offsets instead. As with theme
application, only the in-memory scene changes - call `save_editor_scene` to
persist.

## Runtime UI reconnaissance

While the game runs, `get_game_ui_elements` walks the running Control tree and
returns one entry per Control: node path, type, visibility, optional text and
its global rectangle (position and size).

```json
{"name": "get_game_ui_elements", "arguments": {"max_elements": 200}}
```

- `max_elements` defaults to 100, hard cap 1000; when the cap is hit the
  response is flagged as truncated - raise the cap or narrow the search.
- `timeout_ms` (default 5000, max 30000) bounds the round trip to the game.

Use the returned rectangles to aim input injection - for example pressing a
button by its on-screen coordinates with `queue_game_input`. This requires the
game to be running via `play_editor_current_scene`; the game channel
prerequisites and input injection details are covered in the running-games
skill. Editor-side input tools do not reach the running game - only the game
channel tools do.

## Side effects

Five of the eight theme tools write to disk (`create_theme_resource`,
`set_theme_color`, `set_theme_constant`, `set_theme_font_size`,
`set_theme_stylebox_flat`): each loads the .tres file, modifies it and saves
it back. Confirm the `path` before calling - especially on shared theme files.
`create_theme_resource` refuses to overwrite an existing file, but the set
tools edit in place. `get_theme_info` is read-only; `apply_theme_to_control`
and `set_control_anchor_preset` only modify the in-memory edited scene
(persist with `save_editor_scene`).

## See also

- godot-autopilot-running-games - starting the game and injecting input at the
  UI you just inspected.
- godot-autopilot-animation - animating UI controls such as modulate or
  position.
- godot-autopilot-properties-signals - property JSON shapes and renames for
  Control properties.
- godot-autopilot-tilemap - the other content-domain skill in this family.
)gda_skill";

} // namespace

std::vector<SkillSpec> make_domain_skills() {
  return {
      {"godot-autopilot-tilemap", kTilemapDescription,
       {{"SKILL.md", kTilemapSkill}}},
      {"godot-autopilot-animation", kAnimationDescription,
       {{"SKILL.md", kAnimationSkill}}},
      {"godot-autopilot-ui-theming", kUiThemingDescription,
       {{"SKILL.md", kUiThemingSkill}}},
  };
}

} // namespace godot_autopilot::skill_gen
