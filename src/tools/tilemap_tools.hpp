#ifndef GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP
#define GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/tilemap_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace tilemap_tools {

namespace {

const std::vector<ParamSpec> kCreateTilemapParams = {
    {"name", "string", "TileMap node name (default: TileMap)", false},
    {"tile_size", "integer", "Tile size in pixels, sets the default TileSet tile_size (default: 16)", false},
    {"format", "integer", "Cell quadrant size in pixels (default: 16)", false},
    {"parent_path", "string", "Parent node path; omit to add at the scene root (errors when the scene has no root)", false},
};

const std::vector<ParamSpec> kSetTilemapCellParams = {
    {"path", "string", "Path to the TileMap or TileMapLayer node", true},
    {"x", "integer", "Cell X coordinate", true},
    {"y", "integer", "Cell Y coordinate", true},
    {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
    {"source_id", "integer", "TileSet source ID (default: 0); must exist in the node's TileSet", false},
    {"atlas_coords", "object", "Atlas coordinates Vector2i (e.g. {\"x\":0,\"y\":0})", false},
};

const std::vector<ParamSpec> kSetTilemapCellsParams = {
    {"node_path", "string", "Path to the TileMap or TileMapLayer node", true},
    {"cells", "array", "Array of cells, each {\"x\":int,\"y\":int,\"source_id\":int,\"atlas_coords\":{\"x\":int,\"y\":int}} — source_id is required per entry. No fixed server-side entry cap, but keep batches reasonably sized because client or transport layers may limit a single request payload (bigger layouts: loop in execute_script or code_execute). An entry with source_id -1 clears the cell at x, y (atlas_coords is ignored and may be omitted). Invalid entries are skipped and reported in the error plus warnings", true},
    {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
};

const std::vector<ParamSpec> kFillTilemapRectParams = {
    {"node_path", "string", "Path to the TileMap or TileMapLayer node", true},
    {"from", "object", "Inclusive first corner cell {x, y}; the two corners are normalized internally, so from/to order does not matter", true},
    {"to", "object", "Inclusive opposite corner cell {x, y}", true},
    {"source_id", "integer", "TileSet source ID placed in every cell; required and must be >= 0 unless erase is true", false},
    {"atlas_coords", "object", "Atlas coords {x, y} placed in every cell; both must be >= 0 and is required unless erase is true", false},
    {"alternative", "integer", "Alternative tile id placed in every cell (default: 0; must be >= 0)", false},
    {"erase", "boolean", "Clear every cell in the rect instead of placing tiles; source_id and atlas_coords are ignored (default: false)", false},
    {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
};

const std::vector<ParamSpec> kCreateTilemapTilesetParams = {
    {"name", "string", "Resource name used as the memory:// reference for later tools (default: TileSet)", false},
    {"tile_size", "integer", "Base tile size in pixels (default: 16)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "create_tilemap",
      "Create a TileMap node in the edited scene and attach a default TileSet with the given tile size. Optional name (default TileMap), tile_size in pixels (default 16), format (cell quadrant size, default 16) and parent_path (omit to add at the scene root). Returns class, name, path and object_id.",
      "TileMap", {"tilemap", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateTilemapParams, tilemap_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_tilemap_cell",
      "Set a single cell on a TileMap or TileMapLayer node. Requires path, x, y; optional layer (default 0), source_id (default 0) and atlas_coords {x, y} place the tile. The source must exist in the node's TileSet (build it with create_tilemap_tileset + add_tilemap_atlas_source first). Passing source_id -1 clears the cell at x, y instead (atlas_coords is ignored then). Returns 'ok'; use set_tilemap_cells for batches.",
      "TileMap", {"tilemap", "cell", "set"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kSetTilemapCellParams, tilemap_ops::handle_set_cell}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_tilemap_cells",
      "Set multiple cells on a TileMap or TileMapLayer node in one call. Every entry must be an object with x, y and source_id (atlas_coords optional); an entry with source_id -1 clears the cell at x, y (atlas_coords is ignored for that entry). Invalid entries are skipped and reported via error plus warnings. Returns set_count and layer. There is no fixed entry cap, but keep batches reasonably sized because client-side request limits may apply; generate big layouts programmatically with execute_script or code_execute instead.",
      "TileMap", {"tilemap", "cell", "set", "batch"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kSetTilemapCellsParams, tilemap_ops::handle_set_cells}));
  v.push_back(make_spec_tool(ToolSpec{
      "fill_tilemap_rect",
      "Fill an axis-aligned tile rectangle on a TileMap or TileMapLayer node in one call. Requires node_path plus from and to {x, y} corner cells (both inclusive; the corner order does not matter). Pass source_id and atlas_coords to place one tile in every cell, or erase=true to clear the rect instead (source_id and atlas_coords are then ignored). Optional alternative (default 0) and layer (default 0, TileMapLayer ignores it). The rect is capped at 100000 cells per call, so split bigger areas into chunks. Implemented as a per-cell set_cell/erase_cell loop: it saves a round trip and produces a single undo action, but does not make the fill itself faster. Returns set_count, from, to, width, height and undo.",
      "TileMap", {"tilemap", "cell", "fill", "rect"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kFillTilemapRectParams, tilemap_ops::handle_fill_rect}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_tilemap_tileset",
      "Create a TileSet resource in memory and register it as memory://<name>; nothing is written to disk. Optional name (default TileSet) and tile_size in pixels (default 16). add_tilemap_atlas_source, add_tilemap_physics_layer and set_tilemap_tile_collision reference it by name. Returns class, name, path and object_id.",
      "TileMap", {"tileset", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateTilemapTilesetParams, tilemap_ops::handle_tileset_create}));
  return v;
}

} // namespace tilemap_tools
} // namespace godot_autopilot

#endif