#ifndef GODOT_AUTOPILOT_TILESET_TOOLS_HPP
#define GODOT_AUTOPILOT_TILESET_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/tileset_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace tileset_tools {

GDA_TOOL_CLASS(AddTilemapAtlasSourceTool, "add_tilemap_atlas_source",
               "Add a TileSetAtlasSource with a texture to a TileSet created by create_tilemap_tileset. Requires name (the memory:// reference), source_id (0-255, must be unused), texture path and tile_size {x, y}; optional margin and spacing in pixels. Splits the texture into a grid and creates one tile per cell. Returns source_id, created_tiles and grid_size.",
               "TileMap", std::vector<std::string>({"tileset", "atlas", "texture"}), tileset_ops::handle_add_atlas_source, true)

GDA_TOOL_CLASS(AddTilemapPhysicsLayerTool, "add_tilemap_physics_layer",
               "Add a physics layer to a TileSet created by create_tilemap_tileset. Requires name (the memory:// reference) and layer_id; optional collision_layer and collision_mask bitmasks (default 1). Call it before set_tilemap_tile_collision, which needs an existing layer. Multiple layers add multiple collision polygons. Returns 'ok' and the actual layer_id.",
               "TileMap", std::vector<std::string>({"tileset", "physics", "layer"}), tileset_ops::handle_add_physics_layer, true)

GDA_TOOL_CLASS(SetTilemapTileCollisionTool, "set_tilemap_tile_collision",
               "Set collision polygons for one tile of a TileSet. Requires name (memory:// TileSet reference), source_id, atlas_coords and physics_layer; the TileSet needs an atlas source and a physics layer first (create_tilemap_tileset → add_tilemap_atlas_source → add_tilemap_physics_layer). polygon is an array of {x, y} points or an array of such arrays for multiple polygons; an empty array clears existing collision. Points are relative to the tile center. Returns 'ok', atlas_coords and polygon_points.",
               "TileMap", std::vector<std::string>({"tileset", "collision", "polygon"}), tileset_ops::handle_set_tile_collision, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(std::make_unique<AddTilemapAtlasSourceTool>());
  v.push_back(std::make_unique<AddTilemapPhysicsLayerTool>());
  v.push_back(std::make_unique<SetTilemapTileCollisionTool>());
  return v;
}

} // namespace tileset_tools
} // namespace godot_autopilot

#endif