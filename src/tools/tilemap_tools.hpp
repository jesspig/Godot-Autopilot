#ifndef GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP
#define GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/tilemap_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace tilemap_tools {

GDA_TOOL_CLASS(CreateTilemapTool, "create_tilemap",
               "Create a TileMap node in the edited scene and attach a default TileSet with the given tile size. Optional name (default TileMap), tile_size in pixels (default 16), format (cell quadrant size, default 16) and parent_path (omit to add at the scene root). Returns class, name, path and object_id.",
               "TileMap", std::vector<std::string>({"tilemap", "create"}), tilemap_ops::handle_create, true)

GDA_TOOL_CLASS(SetTilemapCellTool, "set_tilemap_cell",
               "Set a single cell on a TileMap or TileMapLayer node. Requires path, x, y; optional layer (default 0), source_id (default 0) and atlas_coords {x, y} place the tile. The source must exist in the node's TileSet (build it with create_tilemap_tileset + add_tilemap_atlas_source first). Passing source_id -1 clears the cell at x, y instead (atlas_coords is ignored then). Returns 'ok'; use set_tilemap_cells for batches.",
               "TileMap", std::vector<std::string>({"tilemap", "cell", "set"}), tilemap_ops::handle_set_cell, true)

GDA_TOOL_CLASS(SetTilemapCellsTool, "set_tilemap_cells",
               "Set multiple cells on a TileMap or TileMapLayer node in one call. Every entry must be an object with x, y and source_id (atlas_coords optional); an entry with source_id -1 clears the cell at x, y (atlas_coords is ignored for that entry). Invalid entries are skipped and reported via error plus warnings. Returns set_count and layer. There is no fixed entry cap, but keep batches reasonably sized because client-side request limits may apply; generate big layouts programmatically with execute_script or code_execute instead.",
               "TileMap", std::vector<std::string>({"tilemap", "cell", "set", "batch"}), tilemap_ops::handle_set_cells, true)

GDA_TOOL_CLASS(CreateTilemapTilesetTool, "create_tilemap_tileset",
               "Create a TileSet resource in memory and register it as memory://<name>; nothing is written to disk. Optional name (default TileSet) and tile_size in pixels (default 16). add_tilemap_atlas_source, add_tilemap_physics_layer and set_tilemap_tile_collision reference it by name. Returns class, name, path and object_id.",
               "TileMap", std::vector<std::string>({"tileset", "create"}), tilemap_ops::handle_tileset_create, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(std::make_unique<CreateTilemapTool>());
  v.push_back(std::make_unique<SetTilemapCellTool>());
  v.push_back(std::make_unique<SetTilemapCellsTool>());
  v.push_back(std::make_unique<CreateTilemapTilesetTool>());
  return v;
}

} // namespace tilemap_tools
} // namespace godot_autopilot

#endif