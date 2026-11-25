#ifndef GODOT_AUTOPILOT_TILESET_TOOLS_HPP
#define GODOT_AUTOPILOT_TILESET_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/tileset/tileset_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace tileset_tools {

namespace {

const std::vector<ParamSpec> kUpdateTilesetParams = {
    {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
    {"source_id", "integer", "Only with action=add_atlas_source or action=set_tile_collision: source ID to assign (0-255, must not already be used) or the existing atlas source ID", false},
    {"texture", "string", "Only with action=add_atlas_source: texture file path (e.g. res://tiles.png); must exist on disk", false},
    {"tile_size", "object", "Only with action=add_atlas_source: tile size in pixels (e.g. {\"x\":16,\"y\":16})", false},
    {"margin", "integer", "Only with action=add_atlas_source: margin in pixels around the atlas texture (default: 0)", false},
    {"spacing", "integer", "Only with action=add_atlas_source: spacing between tiles in pixels (default: 0)", false},
    {"layer_id", "integer", "Only with action=add_physics_layer: physics layer index to add (appended at the end of the layer list)", false},
    {"collision_layer", "integer", "Only with action=add_physics_layer: collision layer bitmask (default: 1)", false},
    {"collision_mask", "integer", "Only with action=add_physics_layer: collision mask bitmask (default: 1)", false},
    {"atlas_coords", "object", "Only with action=set_tile_collision: atlas coordinates of the tile (e.g. {\"x\":0,\"y\":0})", false},
    {"physics_layer", "integer", "Only with action=set_tile_collision: physics layer index added by action=add_physics_layer", false},
    {"polygon", "array", "Only with action=set_tile_collision: collision polygon as an array of {x,y} points, or an array of such arrays for multiple polygons; an empty array clears existing collision. Points are relative to the TILE CENTER (e.g. for tile_size=16 use (-8,-8)-(8,8) for full-tile collision; (0,0)-(16,16) starts at the center and overhangs)", false},
};

const std::vector<std::string> kUpdateTilesetActions = {
    "add_atlas_source", "add_physics_layer", "set_tile_collision"};

inline godot_autopilot::util::JsonValue tileset_action_args(const godot_autopilot::util::JsonValue &args,
                                          const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  out["action"] = godot_autopilot::util::JsonValue(action);
  if (!args.IsObject()) {
    return out;
  }
  for (const auto &entry : args.GetObject()) {
    if (entry.first != "action" && entry.first != "confirm") {
      out[entry.first] = entry.second;
    }
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "update_tileset",
      "Modify a TileSet resource created by create_tilemap_tileset (memory:// reference, nothing is written to disk). action selects the operation: 'add_atlas_source' adds a TileSetAtlasSource with a texture (requires source_id 0-255 that is unused, texture path and tile_size {x, y}; optional margin and spacing) and splits the texture into one tile per grid cell; 'add_physics_layer' appends a physics layer (requires layer_id; optional collision_layer and collision_mask bitmasks, default 1) and must run before set_tile_collision; 'set_tile_collision' sets collision polygons for one tile (requires source_id, atlas_coords and physics_layer; polygon is an array of {x, y} points or an array of such arrays, an empty array clears collision, points are relative to the tile center). Typical order: add_atlas_source, add_physics_layer, set_tile_collision. Example: {\"action\": \"add_atlas_source\", \"name\": \"TileSet\", \"source_id\": 0, \"texture\": \"res://tiles.png\", \"tile_size\": {\"x\": 16, \"y\": 16}} or {\"action\": \"set_tile_collision\", \"name\": \"TileSet\", \"source_id\": 0, \"atlas_coords\": {\"x\": 0, \"y\": 0}, \"physics_layer\": 0, \"polygon\": [{\"x\": -8, \"y\": -8}, {\"x\": 8, \"y\": -8}, {\"x\": 8, \"y\": 8}, {\"x\": -8, \"y\": 8}]}.",
      "TileMap", {"tileset", "atlas", "texture", "physics", "layer", "collision", "polygon"},
      SideEffect::None, tool_flags::kNone,
      kUpdateTilesetParams, tileset_ops::handle_update_tileset,
      schema::build_action_schema(
          kUpdateTilesetActions, kUpdateTilesetParams,
          "add_atlas_source: add a textured atlas source; add_physics_layer: append a physics layer; set_tile_collision: set collision polygons for one tile"),
      {},
      {"update_tileset action=add_atlas_source name=TileSet source_id=0 texture=res://tiles.png tile_size={\"x\":16,\"y\":16}",
       "update_tileset action=add_physics_layer name=TileSet layer_id=0"}},
      {
          ToolAlias{
              "add_tilemap_atlas_source", "update_tileset",
              [](const godot_autopilot::util::JsonValue &args) {
                return tileset_action_args(args, "add_atlas_source");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "add_tilemap_physics_layer", "update_tileset",
              [](const godot_autopilot::util::JsonValue &args) {
                return tileset_action_args(args, "add_physics_layer");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_tilemap_tile_collision", "update_tileset",
              [](const godot_autopilot::util::JsonValue &args) {
                return tileset_action_args(args, "set_tile_collision");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  return v;
}

} // namespace tileset_tools
} // namespace godot_autopilot

#endif
