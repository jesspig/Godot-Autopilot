#ifndef GODOT_AUTOPILOT_GRIDMAP_TOOLS_HPP
#define GODOT_AUTOPILOT_GRIDMAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/gridmap_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace gridmap_tools {

namespace {

const std::vector<ParamSpec> kCreateGridmapParams = {
    {"name", "string", "GridMap node name (default: GridMap)", false},
    {"cell_size", "object", "Cell size in meters as {x, y, z} (default: {\"x\":1,\"y\":1,\"z\":1}); every component must be > 0", false},
    {"parent_path", "string", "Parent node path; omit to add at the scene root (errors when the scene has no root)", false},
};

const std::vector<ParamSpec> kCreateMeshLibraryItemParams = {
    {"library", "string", "MeshLibrary name from create_resource type MeshLibrary (memory:// reference)", true},
    {"item_id", "integer", "Item ID to assign (must be >= 0 and unused)", true},
    {"mesh", "object", "Mesh resource reference: a res:// or memory:// string, or {\"path\": \"res://...\"} / {\"resource\": \"memory://...\"}; must resolve to a Mesh", true},
    {"item_name", "string", "Display name stored on the item (default: empty)", false},
};

const std::vector<ParamSpec> kSetGridmapCellParams = {
    {"node_path", "string", "Path to the GridMap node", true},
    {"position", "object", "Cell coordinates {x, y, z} as integers", true},
    {"item", "integer", "MeshLibrary item id placed in the cell; -1 clears the cell (orientation is ignored then)", true},
    {"orientation", "integer", "Cell orientation 0-23 (default: 0)", false},
};

const std::vector<ParamSpec> kFillGridmapBoxParams = {
    {"node_path", "string", "Path to the GridMap node", true},
    {"from", "object", "Inclusive first corner cell {x, y, z}; the two corners are normalized internally, so from/to order does not matter", true},
    {"to", "object", "Inclusive opposite corner cell {x, y, z}", true},
    {"item", "integer", "MeshLibrary item id placed in every cell; required and must be >= 0 unless erase is true", false},
    {"orientation", "integer", "Cell orientation 0-23 placed in every cell (default: 0)", false},
    {"erase", "boolean", "Clear every cell in the box instead of placing items; item and orientation are ignored (default: false)", false},
};

const std::vector<ParamSpec> kGetGridmapUsedCellsParams = {
    {"node_path", "string", "Path to the GridMap node", true},
    {"by_item", "integer", "Only return cells holding this MeshLibrary item id (default: return every used cell)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "create_gridmap",
      "Create a GridMap node in the edited scene with the given cell size. Optional name (default GridMap), cell_size {x, y, z} in meters (default 1,1,1; every component must be > 0) and parent_path (omit to add at the scene root). Attach a MeshLibrary later with property_set, or build items with create_meshlibrary_item. Returns class, name, path and object_id.",
      "TileMap", {"gridmap", "create"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kCreateGridmapParams, gridmap_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_meshlibrary_item",
      "Add an item with a mesh to a MeshLibrary created by create_resource (type MeshLibrary). Requires library (the memory:// reference name), item_id (>= 0, must be unused) and mesh (a Mesh resource reference: res:// or memory:// string or {\"path\": \"res://...\"} / {\"resource\": \"memory://...\"}); optional item_name display name. The item id is then usable as item in set_gridmap_cell and fill_gridmap_box. Returns result, item_id, item_name and mesh_class.",
      "TileMap", {"meshlibrary", "mesh", "create"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kCreateMeshLibraryItemParams, gridmap_ops::handle_create_library_item}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_gridmap_cell",
      "Set a single cell on a GridMap node. Requires node_path, position {x, y, z} and item; optional orientation 0-23 (default 0). The item must exist in the node's MeshLibrary (build it with create_meshlibrary_item). Passing item -1 clears the cell at the position instead (orientation is ignored then). Returns 'ok' and undo; use fill_gridmap_box for volumes.",
      "TileMap", {"gridmap", "cell", "set"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetGridmapCellParams, gridmap_ops::handle_set_cell}));
  v.push_back(make_spec_tool(ToolSpec{
      "fill_gridmap_box",
      "Fill an axis-aligned cell box on a GridMap node in one call. Requires node_path plus from and to {x, y, z} corner cells (both inclusive; the corner order does not matter). Pass item and optional orientation 0-23 (default 0) to place one item in every cell, or erase=true to clear the box instead (item and orientation are then ignored). The box is capped at 100000 cells per call, so split bigger volumes into chunks. Implemented as a per-cell set_cell_item loop: it saves round trips and produces a single undo action, but does not make the fill itself faster. Returns set_count, from, to, size_x, size_y, size_z and undo.",
      "TileMap", {"gridmap", "cell", "fill", "box"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kFillGridmapBoxParams, gridmap_ops::handle_fill_box}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_gridmap_used_cells",
      "List the used cells of a GridMap node. Requires node_path; optional by_item limits the result to cells holding that MeshLibrary item id. Returns cells (each with position {x, y, z}, item and orientation) and count.",
      "TileMap", {"gridmap", "cell", "get", "used"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kGetGridmapUsedCellsParams, gridmap_ops::handle_get_used_cells}));
  return v;
}

} // namespace gridmap_tools
} // namespace godot_autopilot

#endif
