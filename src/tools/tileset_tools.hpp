#ifndef GODOT_AUTOPILOT_TILESET_TOOLS_HPP
#define GODOT_AUTOPILOT_TILESET_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/tileset_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace tileset_tools {

namespace {

const std::vector<ParamSpec> kAddTilemapAtlasSourceParams = {
    {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
    {"source_id", "integer", "Source ID to assign, 0-255; must not already be used", true},
    {"texture", "string", "Texture file path (e.g. res://tiles.png); must exist on disk", true},
    {"tile_size", "object", "Tile size in pixels (e.g. {\"x\":16,\"y\":16})", true},
    {"margin", "integer", "Margin in pixels around the atlas texture (default: 0)", false},
    {"spacing", "integer", "Spacing between tiles in pixels (default: 0)", false},
};

const std::vector<ParamSpec> kAddTilemapPhysicsLayerParams = {
    {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
    {"layer_id", "integer", "Physics layer index to add (appended at the end of the layer list)", true},
    {"collision_layer", "integer", "Collision layer bitmask (default: 1)", false},
    {"collision_mask", "integer", "Collision mask bitmask (default: 1)", false},
};

const std::vector<ParamSpec> kSetTilemapTileCollisionParams = {
    {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
    {"source_id", "integer", "Source ID of the atlas source added by add_tilemap_atlas_source", true},
    {"atlas_coords", "object", "Atlas coordinates of the tile (e.g. {\"x\":0,\"y\":0})", true},
    {"physics_layer", "integer", "Physics layer index added by add_tilemap_physics_layer", true},
    {"polygon", "array", "Collision polygon: array of {x,y} points, or array of such arrays for multiple polygons; an empty array clears existing collision. Points are relative to the TILE CENTER (e.g. for tile_size=16 use (-8,-8)-(8,8) for full-tile collision; (0,0)-(16,16) starts at the center and overhangs)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "add_tilemap_atlas_source",
      "Add a TileSetAtlasSource with a texture to a TileSet created by create_tilemap_tileset. Requires name (the memory:// reference), source_id (0-255, must be unused), texture path and tile_size {x, y}; optional margin and spacing in pixels. Splits the texture into a grid and creates one tile per cell. Returns source_id, created_tiles and grid_size.",
      "TileMap", {"tileset", "atlas", "texture"}, SideEffect::None, tool_flags::kNone,
      kAddTilemapAtlasSourceParams, tileset_ops::handle_add_atlas_source}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_tilemap_physics_layer",
      "Add a physics layer to a TileSet created by create_tilemap_tileset. Requires name (the memory:// reference) and layer_id; optional collision_layer and collision_mask bitmasks (default 1). Call it before set_tilemap_tile_collision, which needs an existing layer. Multiple layers add multiple collision polygons. Returns 'ok' and the actual layer_id.",
      "TileMap", {"tileset", "physics", "layer"}, SideEffect::None, tool_flags::kNone,
      kAddTilemapPhysicsLayerParams, tileset_ops::handle_add_physics_layer}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_tilemap_tile_collision",
      "Set collision polygons for one tile of a TileSet. Requires name (memory:// TileSet reference), source_id, atlas_coords and physics_layer; the TileSet needs an atlas source and a physics layer first (create_tilemap_tileset → add_tilemap_atlas_source → add_tilemap_physics_layer). polygon is an array of {x, y} points or an array of such arrays for multiple polygons; an empty array clears existing collision. Points are relative to the tile center. Returns 'ok', atlas_coords and polygon_points.",
      "TileMap", {"tileset", "collision", "polygon"}, SideEffect::None, tool_flags::kNone,
      kSetTilemapTileCollisionParams, tileset_ops::handle_set_tile_collision}));
  return v;
}

} // namespace tileset_tools
} // namespace godot_autopilot

#endif