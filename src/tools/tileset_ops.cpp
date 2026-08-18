#include "tileset_ops.hpp"
#include "../util/error_util.hpp"
#include "core/log_system.hpp"
#include "resource_ops.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tile_data.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/classes/tile_set_atlas_source.hpp>
#include <godot_cpp/classes/tile_set_source.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace tileset_ops {

using JV = mcp::JsonValue;

namespace {

godot::Ref<godot::TileSet> resolve_tileset(const std::string &name,
                                           JV &out_error) {
  godot::Ref<godot::Resource> res = resource_ops::resolve_memory_resource(name);
  if (res.is_null()) {
    out_error = util::error_json("memory resource not found: " + name +
                      " (create it with create_tilemap_tileset first)");
    return godot::Ref<godot::TileSet>();
  }
  godot::Ref<godot::TileSet> tileset = res;
  if (tileset.is_null()) {
    out_error = util::error_json("resource " + name + " is not a TileSet");
  }
  return tileset;
}

bool parse_vec2i(const JV &obj, const char *x_key, const char *y_key,
                 godot::Vector2i &out) {
  auto *vx = obj.Find(x_key);
  auto *vy = obj.Find(y_key);
  if (!vx || !vx->IsInt() || !vy || !vy->IsInt())
    return false;
  out = godot::Vector2i(static_cast<int>(vx->GetInt()),
                        static_cast<int>(vy->GetInt()));
  return true;
}

double as_double(const JV &j) {
  if (j.IsInt())
    return static_cast<double>(j.GetInt());
  return j.GetDouble();
}

int compute_grid(int tex_dim, int tile_dim, int margin, int spacing) {
  int valid = tex_dim - margin;
  if (valid < tile_dim || tile_dim + spacing <= 0)
    return 0;
  return 1 + (valid - tile_dim) / (tile_dim + spacing);
}

bool parse_point(const JV &item, godot::Vector2 &out) {
  auto *px = item.Find("x");
  auto *py = item.Find("y");
  if (!item.IsObject() || !px || !px->IsNumber() || !py || !py->IsNumber())
    return false;
  out = godot::Vector2(static_cast<float>(as_double(*px)),
                       static_cast<float>(as_double(*py)));
  return true;
}

bool parse_polygon(const JV &points_arr, godot::PackedVector2Array &out,
                   std::string &out_error) {
  const auto &arr = points_arr.GetArray();
  for (size_t i = 0; i < arr.size(); ++i) {
    godot::Vector2 pt;
    if (!parse_point(arr[i], pt)) {
      out_error = "invalid polygon point at index " + std::to_string(i) +
                  ": expected {x: number, y: number}";
      return false;
    }
    out.append(pt);
  }
  return true;
}

} // namespace

JV handle_add_atlas_source(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_tilemap_atlas_source called");

  auto *n = args.Find("name");
  if (!n || !n->IsString())
    return util::error_json("missing required parameter: name");
  auto *sid = args.Find("source_id");
  if (!sid || !sid->IsInt())
    return util::error_json("missing required parameter: source_id");
  auto *tex = args.Find("texture");
  if (!tex || !tex->IsString())
    return util::error_json("missing required parameter: texture");
  auto *tsz = args.Find("tile_size");
  if (!tsz || !tsz->IsObject())
    return util::error_json("missing required parameter: tile_size");
  godot::Vector2i tile_size;
  if (!parse_vec2i(*tsz, "x", "y", tile_size))
    return util::error_json("missing required parameter: tile_size.x");

  int margin = 0;
  auto *mg = args.Find("margin");
  if (mg && mg->IsInt())
    margin = static_cast<int>(mg->GetInt());

  int spacing = 0;
  auto *sp = args.Find("spacing");
  if (sp && sp->IsInt())
    spacing = static_cast<int>(sp->GetInt());

  JV resolve_err;
  godot::Ref<godot::TileSet> tileset =
      resolve_tileset(n->GetString(), resolve_err);
  if (tileset.is_null())
    return resolve_err;

  int source_id = static_cast<int>(sid->GetInt());
  if (tileset->has_source(source_id))
    return util::error_json("source_id already exists: " + std::to_string(source_id));

  godot::String tex_path = godot::String(tex->GetString().c_str());
  if (!godot::FileAccess::file_exists(tex_path)) {
    return util::error_detail("failed to load texture: file does not exist: " +
                                  tex->GetString(),
                              "source_id " + std::to_string(source_id),
                              "the texture file to exist on disk",
                              "provide a valid texture path and retry");
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader)
    return util::error_json("ResourceLoader not available");
  godot::Ref<godot::Resource> tex_res = loader->load(tex_path);
  if (tex_res.is_null())
    return util::error_json("failed to load texture: " + tex->GetString());
  godot::Ref<godot::Texture2D> texture = tex_res;
  if (texture.is_null())
    return util::error_json("resource is not a Texture2D: " + tex->GetString());

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return util::error_json("ClassDB singleton not available");
  godot::Variant var =
      cdbs->instantiate(godot::StringName("TileSetAtlasSource"));
  if (var.get_type() == godot::Variant::NIL)
    return util::error_json("failed to instantiate TileSetAtlasSource");
  godot::Ref<godot::TileSetAtlasSource> src = var;
  if (src.is_null())
    return util::error_json("instantiated object is not a TileSetAtlasSource");

  src->set_texture(texture);
  src->set_texture_region_size(tile_size);
  src->set_margins(godot::Vector2i(margin, margin));
  src->set_separation(godot::Vector2i(spacing, spacing));

  int32_t actual_id = tileset->add_source(src, source_id);

  int grid_x = compute_grid(texture->get_width(), tile_size.x, margin, spacing);
  int grid_y =
      compute_grid(texture->get_height(), tile_size.y, margin, spacing);
  int created_tiles = 0;
  for (int y = 0; y < grid_y; ++y) {
    for (int x = 0; x < grid_x; ++x) {
      src->create_tile(godot::Vector2i(x, y));
      ++created_tiles;
    }
  }

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["source_id"] = JV(static_cast<int64_t>(actual_id));
  r["created_tiles"] = JV(static_cast<int64_t>(created_tiles));
  JV grid_size(JV::object_tag);
  grid_size["x"] = JV(static_cast<int64_t>(grid_x));
  grid_size["y"] = JV(static_cast<int64_t>(grid_y));
  r["grid_size"] = std::move(grid_size);
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "add_tilemap_atlas_source completed: " + std::to_string(created_tiles) +
          " tiles created");
  return r;
}

JV handle_add_physics_layer(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_tilemap_physics_layer called");

  auto *n = args.Find("name");
  if (!n || !n->IsString())
    return util::error_json("missing required parameter: name");
  auto *lid = args.Find("layer_id");
  if (!lid || !lid->IsInt())
    return util::error_json("missing required parameter: layer_id");

  int collision_layer = 1;
  auto *cl = args.Find("collision_layer");
  if (cl && cl->IsInt())
    collision_layer = static_cast<int>(cl->GetInt());

  int collision_mask = 1;
  auto *cm = args.Find("collision_mask");
  if (cm && cm->IsInt())
    collision_mask = static_cast<int>(cm->GetInt());

  JV resolve_err;
  godot::Ref<godot::TileSet> tileset =
      resolve_tileset(n->GetString(), resolve_err);
  if (tileset.is_null())
    return resolve_err;

  tileset->add_physics_layer();
  int32_t actual_id = tileset->get_physics_layers_count() - 1;
  tileset->set_physics_layer_collision_layer(
      actual_id, static_cast<uint32_t>(collision_layer));
  tileset->set_physics_layer_collision_mask(
      actual_id, static_cast<uint32_t>(collision_mask));

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["layer_id"] = JV(static_cast<int64_t>(actual_id));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_tilemap_physics_layer completed");
  return r;
}

JV handle_set_tile_collision(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_tile_collision called");

  auto *n = args.Find("name");
  if (!n || !n->IsString())
    return util::error_json("missing required parameter: name");
  auto *sid = args.Find("source_id");
  if (!sid || !sid->IsInt())
    return util::error_json("missing required parameter: source_id");
  auto *ac = args.Find("atlas_coords");
  if (!ac || !ac->IsObject())
    return util::error_json("missing required parameter: atlas_coords");
  godot::Vector2i coords;
  if (!parse_vec2i(*ac, "x", "y", coords))
    return util::error_json("missing required parameter: atlas_coords.x");
  auto *pl = args.Find("physics_layer");
  if (!pl || !pl->IsInt())
    return util::error_json("missing required parameter: physics_layer");
  auto *poly = args.Find("polygon");
  if (!poly || !poly->IsArray())
    return util::error_json("missing required parameter: polygon");

  JV resolve_err;
  godot::Ref<godot::TileSet> tileset =
      resolve_tileset(n->GetString(), resolve_err);
  if (tileset.is_null())
    return resolve_err;

  int source_id = static_cast<int>(sid->GetInt());
  godot::Ref<godot::TileSetSource> src_res = tileset->get_source(source_id);
  if (src_res.is_null())
    return util::error_json("source not found: " + std::to_string(source_id));
  godot::Ref<godot::TileSetAtlasSource> src = src_res;
  if (src.is_null())
    return util::error_json("source " + std::to_string(source_id) +
                 " is not a TileSetAtlasSource");

  if (!src->has_tile(coords)) {
    src->create_tile(coords);
  }
  if (!src->has_tile(coords)) {
    std::string pos = std::to_string(coords.x) + "," + std::to_string(coords.y);
    return util::error_json("failed to create tile at " + pos);
  }

  godot::TileData *data = src->get_tile_data(coords, 0);
  if (!data) {
    std::string pos = std::to_string(coords.x) + "," + std::to_string(coords.y);
    return util::error_json("failed to get tile data at " + pos);
  }

  int physics_layer = static_cast<int>(pl->GetInt());
  if (physics_layer >= tileset->get_physics_layers_count()) {
    std::string atlas_pos = "atlas_coords " + std::to_string(coords.x) + "," +
                            std::to_string(coords.y) + " (source " +
                            std::to_string(source_id) + ")";
    return util::error_detail(
        "TileSet '" + n->GetString() + "' has no physics layer " +
            std::to_string(physics_layer),
        atlas_pos,
        "a physics layer must exist before setting collision polygons",
        "call add_tilemap_physics_layer first, then retry");
  }
  const auto &poly_arr = poly->GetArray();

  std::vector<godot::PackedVector2Array> polygons;
  if (!poly_arr.empty()) {
    if (poly_arr[0].IsArray()) {
      for (size_t i = 0; i < poly_arr.size(); ++i) {
        if (!poly_arr[i].IsArray())
          return util::error_json("invalid polygon at index " + std::to_string(i) +
                       ": expected array of {x: number, y: number}");
        godot::PackedVector2Array points;
        std::string parse_err;
        if (!parse_polygon(poly_arr[i], points, parse_err))
          return util::error_json(parse_err);
        polygons.push_back(std::move(points));
      }
    } else {
      godot::PackedVector2Array points;
      std::string parse_err;
      if (!parse_polygon(*poly, points, parse_err))
        return util::error_json(parse_err);
      polygons.push_back(std::move(points));
    }
  }

  int point_count = 0;
  if (polygons.empty()) {
    int32_t count = data->get_collision_polygons_count(physics_layer);
    for (int32_t i = 0; i < count; ++i) {
      data->remove_collision_polygon(physics_layer, 0);
    }
  } else {
    std::string atlas_pos = "atlas_coords " + std::to_string(coords.x) + "," +
                            std::to_string(coords.y) + " (source " +
                            std::to_string(source_id) + ")";

    data->set_collision_polygons_count(physics_layer,
                                       static_cast<int32_t>(polygons.size()));
    for (size_t i = 0; i < polygons.size(); ++i) {
      int32_t poly_idx = static_cast<int32_t>(i);
      data->set_collision_polygon_points(physics_layer, poly_idx, polygons[i]);
      godot::PackedVector2Array rb =
          data->get_collision_polygon_points(physics_layer, poly_idx);
      if (rb.size() == 0) {
        return util::error_detail(
            "collision polygon write failed (engine rejected it)", atlas_pos,
            "polygon points to be written",
            "check physics layer exists and polygon is valid; engine logs show "
            "root cause");
      }
      point_count += static_cast<int>(polygons[i].size());
    }
  }

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  JV coords_json(JV::object_tag);
  coords_json["x"] = JV(static_cast<int64_t>(coords.x));
  coords_json["y"] = JV(static_cast<int64_t>(coords.y));
  r["atlas_coords"] = std::move(coords_json);
  r["polygon_points"] = JV(point_count);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_tile_collision completed");
  return r;
}

} // namespace tileset_ops
} // namespace godot_autopilot
