# TileMap and TileSet

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
