#ifndef GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP
#define GODOT_AUTOPILOT_TILEMAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/tilemap/tilemap_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace tilemap_tools {

namespace {

const std::vector<ParamSpec> kCreateTilemapParams = {
    {"name", "string", "TileMap node name (default: TileMap)", false},
    {"tile_size", "integer", "Tile size in pixels, sets the default TileSet tile_size (default: 16)", false},
    {"format", "integer", "Cell quadrant size in pixels (default: 16)", false},
    {"parent_path", "string", "Parent node path; omit to add at the scene root (errors when the scene has no root)", false},
};

const std::vector<ParamSpec> kSetTilemapCellsParams = {
    {"node_path", "string", "Path to the TileMap or TileMapLayer node", true},
    {"cells", "array", "Array of cells, each {\"x\":int,\"y\":int,\"source_id\":int,\"atlas_coords\":{\"x\":int,\"y\":int}} — source_id is required per entry. An entry with source_id -1 clears the cell. Use either cells or the from/to rect form", false},
    {"from", "object", "Inclusive first corner cell {x, y} for the rect form; corners are normalized internally", false},
    {"to", "object", "Inclusive opposite corner cell {x, y} for the rect form", false},
    {"source_id", "integer", "TileSet source ID for the rect form; required and >= 0 unless erase is true", false},
    {"atlas_coords", "object", "Atlas coords {x, y} for the rect form; required unless erase is true", false},
    {"alternative", "integer", "Alternative tile id for the rect form (default: 0)", false},
    {"erase", "boolean", "Clear every cell in the rect instead of placing tiles (default: false)", false},
    {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
};

const std::vector<ParamSpec> kCreateTilemapTilesetParams = {
    {"name", "string", "Resource name used as the memory:// reference for later tools (default: TileSet)", false},
    {"tile_size", "integer", "Base tile size in pixels (default: 16)", false},
};

inline mcp::JsonValue tilemap_cell_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *path = args.Find("path");
  if (path != nullptr) {
    out["node_path"] = *path;
  }
  const mcp::JsonValue *layer = args.Find("layer");
  if (layer != nullptr) {
    out["layer"] = *layer;
  }
  mcp::JsonValue cell(mcp::JsonValue::object_tag);
  const mcp::JsonValue *x = args.Find("x");
  const mcp::JsonValue *y = args.Find("y");
  if (x != nullptr) cell["x"] = *x;
  if (y != nullptr) cell["y"] = *y;
  const mcp::JsonValue *source_id = args.Find("source_id");
  cell["source_id"] =
      source_id != nullptr ? *source_id : mcp::JsonValue(static_cast<int64_t>(0));
  const mcp::JsonValue *atlas = args.Find("atlas_coords");
  if (atlas != nullptr) {
    cell["atlas_coords"] = *atlas;
  } else {
    mcp::JsonValue default_atlas(mcp::JsonValue::object_tag);
    default_atlas["x"] = mcp::JsonValue(static_cast<int64_t>(-1));
    default_atlas["y"] = mcp::JsonValue(static_cast<int64_t>(-1));
    cell["atlas_coords"] = std::move(default_atlas);
  }
  mcp::JsonValue cells(mcp::JsonValue::array_tag);
  cells.PushBack(std::move(cell));
  out["cells"] = std::move(cells);
  return out;
}

inline mcp::JsonValue validate_tilemap_cell_args(const mcp::JsonValue &args) {
  const mcp::JsonValue *path = args.Find("path");
  const mcp::JsonValue *x = args.Find("x");
  const mcp::JsonValue *y = args.Find("y");
  if (path == nullptr || !path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: path");
    return error;
  }
  if (x == nullptr || !x->IsInt()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: x");
    return error;
  }
  if (y == nullptr || !y->IsInt()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: y");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_tilemap_cell_result(const mcp::JsonValue &) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "create_tilemap",
      "Create a TileMap node in the edited scene and attach a default TileSet with the given tile size. Optional name (default TileMap), tile_size in pixels (default 16), format (cell quadrant size, default 16) and parent_path (omit to add at the scene root). Returns class, name, path and object_id.",
      "TileMap", {"tilemap", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateTilemapParams, tilemap_ops::handle_create}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_tilemap_cells",
      "Set cells on a TileMap or TileMapLayer node in one call, in three forms. Single or batch: pass 'cells', an array of explicit cells. Rect: pass 'from' and 'to' inclusive corner cells (order does not matter) plus source_id/atlas_coords/alternative, or erase=true to clear the rect; the rect is capped at 100000 cells per call, so split bigger areas into chunks. Single cell: a cells array with one entry. An entry or rect with source_id -1 / erase clears cells. Invalid cell entries are skipped and reported via error plus warnings. Keep batches reasonably sized; generate big layouts programmatically with execute_script or code_execute. Returns set_count and layer for the cells form, or the rect metadata (set_count, from, to, width, height, undo) for the from/to form. Example: {\"node_path\": \"TileMap\", \"cells\": [{\"x\": 0, \"y\": 0, \"source_id\": 0, \"atlas_coords\": {\"x\": 0, \"y\": 0}}]} or {\"node_path\": \"TileMap\", \"from\": {\"x\": 0, \"y\": 0}, \"to\": {\"x\": 9, \"y\": 9}, \"source_id\": 0, \"atlas_coords\": {\"x\": 0, \"y\": 0}}.",
      "TileMap", {"tilemap", "cell", "set", "batch", "fill", "rect", "single"},
      SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kSetTilemapCellsParams, tilemap_ops::handle_set_cells,
      {},
      {},
      {"{\"node_path\": \"TileMap\", \"cells\": [{\"x\": 0, \"y\": 0, \"source_id\": 0}]}",
       "{\"node_path\": \"TileMap\", \"from\": {\"x\": 0, \"y\": 0}, \"to\": {\"x\": 9, \"y\": 9}, \"source_id\": 0, \"atlas_coords\": {\"x\": 0, \"y\": 0}}"}},
      {
          ToolAlias{
              "set_tilemap_cell", "set_tilemap_cells",
              tilemap_cell_args, project_tilemap_cell_result, "0.4.0", "0.6.0",
              validate_tilemap_cell_args},
          ToolAlias{
              "fill_tilemap_rect", "set_tilemap_cells",
              {}, {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                const mcp::JsonValue *path = args.Find("node_path");
                const mcp::JsonValue *from = args.Find("from");
                const mcp::JsonValue *to = args.Find("to");
                if (path == nullptr || !path->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] = mcp::JsonValue(
                      "missing required parameter: node_path");
                  return error;
                }
                if (from == nullptr || !from->IsObject()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: from");
                  return error;
                }
                if (to == nullptr || !to->IsObject()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: to");
                  return error;
                }
                return mcp::JsonValue();
              }},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "create_tilemap_tileset",
      "Create a TileSet resource in memory and register it as memory://<name>; nothing is written to disk. Optional name (default TileSet) and tile_size in pixels (default 16). update_tileset (actions add_atlas_source, add_physics_layer, set_tile_collision) references it by name. Returns class, name, path and object_id.",
      "TileMap", {"tileset", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateTilemapTilesetParams, tilemap_ops::handle_tileset_create}));
  return v;
}

} // namespace tilemap_tools
} // namespace godot_autopilot

#endif