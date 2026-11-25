#ifndef GODOT_AUTOPILOT_GRIDMAP_TOOLS_HPP
#define GODOT_AUTOPILOT_GRIDMAP_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/gridmap/gridmap_ops.hpp"
#include <tools/base/tool_spec.hpp>

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

const std::vector<ParamSpec> kSetGridmapCellsParams = {
    {"node_path", "string", "Path to the GridMap node", true},
    {"from", "object", "Inclusive first corner cell {x, y, z}; the two corners are normalized internally, so from/to order does not matter. A single cell sets from and to to the same position", true},
    {"to", "object", "Inclusive opposite corner cell {x, y, z}", true},
    {"item", "integer", "MeshLibrary item id placed in every cell; required and must be >= 0 unless erase is true", false},
    {"orientation", "integer", "Cell orientation 0-23 placed in every cell (default: 0)", false},
    {"erase", "boolean", "Clear every cell in the box instead of placing items; item and orientation are ignored (default: false)", false},
};

const std::vector<ParamSpec> kGetGridmapUsedCellsParams = {
    {"node_path", "string", "Path to the GridMap node", true},
    {"by_item", "integer", "Only return cells holding this MeshLibrary item id (default: return every used cell)", false},
};

inline godot_autopilot::util::JsonValue gridmap_cell_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  const godot_autopilot::util::JsonValue *path = args.Find("node_path");
  if (path == nullptr) {
    path = args.Find("path");
  }
  if (path != nullptr) {
    out["node_path"] = *path;
  }
  const godot_autopilot::util::JsonValue *position = args.Find("position");
  if (position != nullptr) {
    out["from"] = *position;
    out["to"] = *position;
  }
  const godot_autopilot::util::JsonValue *item = args.Find("item");
  if (item != nullptr && item->IsInt() && item->GetInt() == -1) {
    out["erase"] = godot_autopilot::util::JsonValue(true);
  } else if (item != nullptr) {
    out["item"] = *item;
  }
  const godot_autopilot::util::JsonValue *orientation = args.Find("orientation");
  if (orientation != nullptr) {
    out["orientation"] = *orientation;
  }
  return out;
}

inline godot_autopilot::util::JsonValue validate_gridmap_cell_args(
    const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *path = args.Find("node_path");
  if (path == nullptr) {
    path = args.Find("path");
  }
  const godot_autopilot::util::JsonValue *position = args.Find("position");
  const godot_autopilot::util::JsonValue *item = args.Find("item");
  if (path == nullptr || !path->IsString()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: node_path");
    return error;
  }
  if (position == nullptr || !position->IsObject()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: position");
    return error;
  }
  if (item == nullptr || !item->IsInt()) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("missing required parameter: item");
    return error;
  }
  return godot_autopilot::util::JsonValue();
}

inline godot_autopilot::util::JsonValue project_gridmap_cell_result(
    const godot_autopilot::util::JsonValue &result) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["result"] = godot_autopilot::util::JsonValue("ok");
  const godot_autopilot::util::JsonValue *inner = result.Find("result");
  const godot_autopilot::util::JsonValue *undo =
      inner != nullptr && inner->IsObject() ? inner->Find("undo") : nullptr;
  if (undo != nullptr) {
    out["undo"] = *undo;
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(ToolSpec{
      "create_gridmap",
      "Create a GridMap node in the edited scene with the given cell size. Optional name (default GridMap), cell_size {x, y, z} in meters (default 1,1,1; every component must be > 0) and parent_path (omit to add at the scene root). Attach a MeshLibrary later with property_set, or build items with create_meshlibrary_item. Returns class, name, path and object_id.",
      "TileMap", {"gridmap", "create"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kCreateGridmapParams, gridmap_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_meshlibrary_item",
      "Add an item with a mesh to a MeshLibrary created by create_resource (type MeshLibrary). Requires library (the memory:// reference name), item_id (>= 0, must be unused) and mesh (a Mesh resource reference: res:// or memory:// string or {\"path\": \"res://...\"} / {\"resource\": \"memory://...\"}); optional item_name display name. The item id is then usable as item in set_gridmap_cells. Returns result, item_id, item_name and mesh_class.",
      "TileMap", {"meshlibrary", "mesh", "create"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kCreateMeshLibraryItemParams, gridmap_ops::handle_create_library_item}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_gridmap_cells",
      "Set GridMap cells in one call, in single-cell, batch-box or fill forms. Requires node_path plus from and to {x, y, z} corner cells (both inclusive; the corner order does not matter; a single cell uses the same position for both). Pass item and optional orientation 0-23 (default 0) to place one item in every cell, or erase=true to clear the box instead (item and orientation are then ignored). The box is capped at 100000 cells per call, so split bigger volumes into chunks. Implemented as a per-cell set_cell_item loop: it saves round trips and produces a single undo action, but does not make the fill itself faster. Returns set_count, from, to, size_x, size_y, size_z and undo. Example: {\"node_path\": \"GridMap\", \"from\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"to\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"item\": 0} or {\"node_path\": \"GridMap\", \"from\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"to\": {\"x\": 9, \"y\": 0, \"z\": 9}, \"item\": 0, \"orientation\": 0}.",
      "TileMap", {"gridmap", "cell", "set", "fill", "box", "batch", "single"},
      SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetGridmapCellsParams, gridmap_ops::handle_fill_box,
      {},
      {},
      {"{\"node_path\": \"GridMap\", \"from\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"to\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"item\": 0}",
       "{\"node_path\": \"GridMap\", \"from\": {\"x\": 0, \"y\": 0, \"z\": 0}, \"to\": {\"x\": 9, \"y\": 0, \"z\": 9}, \"item\": 0}"}},
      {
          ToolAlias{
              "set_gridmap_cell", "set_gridmap_cells",
              gridmap_cell_args, project_gridmap_cell_result, "0.4.0", "0.6.0",
              validate_gridmap_cell_args},
          ToolAlias{
              "fill_gridmap_box", "set_gridmap_cells",
              {}, {}, "0.4.0", "0.6.0",
              [](const godot_autopilot::util::JsonValue &args) {
                const godot_autopilot::util::JsonValue *path = args.Find("node_path");
                const godot_autopilot::util::JsonValue *from = args.Find("from");
                const godot_autopilot::util::JsonValue *to = args.Find("to");
                if (path == nullptr || !path->IsString()) {
                  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
                  error["error"] = godot_autopilot::util::JsonValue(
                      "missing required parameter: node_path");
                  return error;
                }
                if (from == nullptr || !from->IsObject()) {
                  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
                  error["error"] =
                      godot_autopilot::util::JsonValue("missing required parameter: from");
                  return error;
                }
                if (to == nullptr || !to->IsObject()) {
                  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
                  error["error"] =
                      godot_autopilot::util::JsonValue("missing required parameter: to");
                  return error;
                }
                return godot_autopilot::util::JsonValue();
              }},
      })));
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
