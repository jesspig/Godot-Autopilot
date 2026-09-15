#include "tilemap_ops.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/scene_path.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/tile_map.hpp>
#include <godot_cpp/classes/tile_map_layer.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <limits>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace tilemap_ops {

using JV = mcp::JsonValue;

namespace {

godot::Node *find_node(const std::string &path_str) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  return util::resolve_scene_node(path_str, editor->get_edited_scene_root());
}

} // namespace

RectRange normalize_rect_range(int64_t from_x, int64_t from_y, int64_t to_x,
                               int64_t to_y) {
  RectRange range;
  range.min_x = from_x < to_x ? from_x : to_x;
  range.min_y = from_y < to_y ? from_y : to_y;
  range.max_x = from_x > to_x ? from_x : to_x;
  range.max_y = from_y > to_y ? from_y : to_y;
  range.width = range.max_x - range.min_x + 1;
  range.height = range.max_y - range.min_y + 1;
  return range;
}

JV handle_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_tilemap called");

  std::string name = "TileMap";
  auto *n = args.Find("name");
  if (n && n->IsString())
    name = n->GetString();

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return util::error_json("ClassDB singleton not available");

  godot::Variant obj_var = cdbs->instantiate(godot::StringName("TileMap"));
  if (obj_var.get_type() == godot::Variant::NIL)
    return util::error_json("failed to instantiate TileMap");

  auto *tile_map = godot::Object::cast_to<godot::TileMap>(obj_var);
  if (!tile_map)
    return util::error_json("instantiated object is not a TileMap");

  tile_map->set_name(godot::StringName(name.c_str()));

  int tile_size = 16;
  auto *ts = args.Find("tile_size");
  if (ts && ts->IsInt())
    tile_size = static_cast<int>(ts->GetInt());

  {
    godot::Variant ts_var = cdbs->instantiate(godot::StringName("TileSet"));
    if (ts_var.get_type() != godot::Variant::NIL) {
      godot::Ref<godot::TileSet> tile_set = ts_var;
      if (tile_set.is_valid()) {
        tile_set->set("tile_size", godot::Vector2i(tile_size, tile_size));
        tile_map->set_tileset(tile_set);
      }
    }
  }

  int quadrant_size = 16;
  auto *qs = args.Find("format");
  if (qs && qs->IsInt())
    quadrant_size = static_cast<int>(qs->GetInt());
  tile_map->set("cell_quadrant_size", quadrant_size);

  auto *editor = godot::EditorInterface::get_singleton();
  auto *pp = args.Find("parent_path");
  bool has_parent = pp && pp->IsString() && !pp->GetString().empty();

  if (has_parent) {
    auto *parent = find_node(pp->GetString());
    if (!parent)
      return util::error_json("parent node not found: " + pp->GetString());
    parent->add_child(tile_map);
    if (editor) {
      auto *scene_root = editor->get_edited_scene_root();
      if (scene_root)
        tile_map->set_owner(scene_root);
    }
  } else if (editor) {
    auto *existing_root = editor->get_edited_scene_root();
    if (existing_root) {
      existing_root->add_child(tile_map);
      tile_map->set_owner(existing_root);
    } else {
      editor->add_root_node(tile_map);
    }
  }

  std::string result_path = name;
  if (editor) {
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root) {
      std::string abs = util::to_std(tile_map->get_path());
      std::string root_pref = util::to_std(scene_root->get_path());
      if (abs == root_pref)
        result_path = name;
      else if (abs.find(root_pref + "/") == 0)
        result_path = abs.substr(root_pref.size() + 1);
      else
        result_path = abs;
    }
  }

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["class"] = JV("TileMap");
  info["name"] = JV(name);
  info["path"] = JV(result_path);
  info["object_id"] = JV(static_cast<int64_t>(tile_map->get_instance_id()));
  info["object_id_str"] =
      JV(std::to_string(static_cast<int64_t>(tile_map->get_instance_id())));
  r["result"] = std::move(info);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_tilemap completed");
  return r;
}

JV handle_set_cell(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_cell called");

  auto *p = args.Find("path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: path");

  auto *node = find_node(p->GetString());
  if (!node)
    return util::error_json("node not found: " + p->GetString());

  auto *tile_map = godot::Object::cast_to<godot::TileMap>(node);
  auto *tile_map_layer =
      tile_map ? nullptr : godot::Object::cast_to<godot::TileMapLayer>(node);
  if (!tile_map && !tile_map_layer)
    return util::error_json(
        "node is not a TileMap or TileMapLayer: " + p->GetString() +
        " — create one with create_tilemap (TileMap) or create_scene_node + "
        "property_set (TileMapLayer), or fix the path");

  auto *x = args.Find("x");
  auto *y = args.Find("y");
  if (!x || !x->IsInt())
    return util::error_json("missing required parameter: x");
  if (!y || !y->IsInt())
    return util::error_json("missing required parameter: y");

  int layer = 0;
  auto *la = args.Find("layer");
  if (la && la->IsInt())
    layer = static_cast<int>(la->GetInt());

  int source_id = 0;
  auto *si = args.Find("source_id");
  if (si && si->IsInt())
    source_id = static_cast<int>(si->GetInt());

  godot::Vector2i atlas_coords(-1, -1);
  auto *ac = args.Find("atlas_coords");
  if (ac && ac->IsObject()) {
    auto *acx = ac->Find("x");
    auto *acy = ac->Find("y");
    if (acx && acx->IsInt() && acy && acy->IsInt())
      atlas_coords = godot::Vector2i(static_cast<int>(acx->GetInt()),
                                     static_cast<int>(acy->GetInt()));
  }

  godot::Vector2i coords(static_cast<int>(x->GetInt()),
                         static_cast<int>(y->GetInt()));
  if (tile_map)
    tile_map->set_cell(layer, coords, source_id, atlas_coords);
  else
    tile_map_layer->set_cell(coords, source_id, atlas_coords);

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_cell completed");
  return r;
}

JV handle_set_cells(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_cells called");

  auto *p = args.Find("path");
  if (!p || !p->IsString())
    p = args.Find("node_path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: node_path");

  auto *node = find_node(p->GetString());
  if (!node)
    return util::error_json("node not found: " + p->GetString());

  auto *tile_map = godot::Object::cast_to<godot::TileMap>(node);
  auto *tile_map_layer =
      tile_map ? nullptr : godot::Object::cast_to<godot::TileMapLayer>(node);
  if (!tile_map && !tile_map_layer)
    return util::error_json(
        "node is not a TileMap or TileMapLayer: " + p->GetString() +
        " — create one with create_tilemap (TileMap) or create_scene_node + "
        "property_set (TileMapLayer), or fix the path");

  auto *cells = args.Find("cells");
  if (!cells || !cells->IsArray())
    return util::error_json("missing required parameter: cells");

  int layer = 0;
  auto *la = args.Find("layer");
  if (la && la->IsInt())
    layer = static_cast<int>(la->GetInt());

  const auto &arr = cells->GetArray();
  int set_count = 0;
  int failed_count = 0;
  std::string failure_detail;
  for (size_t i = 0; i < arr.size(); ++i) {
    const auto &cell = arr[i];
    auto *cx = cell.Find("x");
    auto *cy = cell.Find("y");
    auto *si = cell.Find("source_id");
    std::string failure;
    if (!cell.IsObject()) {
      failure = "expected an object";
    } else if (!cx || !cx->IsInt()) {
      failure = "missing required parameter: x";
    } else if (!cy || !cy->IsInt()) {
      failure = "missing required parameter: y";
    } else if (!si || !si->IsInt()) {
      failure = "missing required parameter: source_id";
    }
    if (!failure.empty()) {
      if (failed_count > 0)
        failure_detail += "; ";
      failure_detail += "cells[" + std::to_string(i) + "]: " + failure;
      ++failed_count;
      continue;
    }

    godot::Vector2i atlas_coords(0, 0);
    auto *ac = cell.Find("atlas_coords");
    if (ac && ac->IsObject()) {
      auto *acx = ac->Find("x");
      auto *acy = ac->Find("y");
      if (acx && acx->IsInt() && acy && acy->IsInt())
        atlas_coords = godot::Vector2i(static_cast<int>(acx->GetInt()),
                                       static_cast<int>(acy->GetInt()));
    }

    godot::Vector2i coords(static_cast<int>(cx->GetInt()),
                           static_cast<int>(cy->GetInt()));
    if (tile_map)
      tile_map->set_cell(layer, coords, static_cast<int>(si->GetInt()),
                         atlas_coords);
    else
      tile_map_layer->set_cell(coords, static_cast<int>(si->GetInt()),
                               atlas_coords);
    ++set_count;
  }

  if (failed_count > 0) {
    JV e(JV::object_tag);
    e["error"] = JV("invalid cells entries: " + failure_detail);
    e["warnings"] = JV(std::to_string(failed_count) + " cells skipped");
    return e;
  }

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["set_count"] = JV(set_count);
  info["layer"] = JV(layer);
  r["result"] = std::move(info);
  if (set_count > 0) {
    scene_dirty_tracker::mark_scene_modified();
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_tilemap_cells completed");
  return r;
}

JV handle_fill_rect(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "fill_tilemap_rect called");

  auto *p = args.Find("node_path");
  if (!p || !p->IsString())
    p = args.Find("path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: node_path");

  auto *node = find_node(p->GetString());
  if (!node)
    return util::error_json("node not found: " + p->GetString());

  auto *tile_map = godot::Object::cast_to<godot::TileMap>(node);
  auto *tile_map_layer =
      tile_map ? nullptr : godot::Object::cast_to<godot::TileMapLayer>(node);
  if (!tile_map && !tile_map_layer)
    return util::error_json(
        "node is not a TileMap or TileMapLayer: " + p->GetString() +
        " — create one with create_tilemap (TileMap) or create_scene_node + "
        "property_set (TileMapLayer), or fix the path");

  auto read_corner = [&args](const char *name, int64_t &x, int64_t &y,
                             std::string &error) {
    auto *corner = args.Find(name);
    if (!corner || !corner->IsObject()) {
      error = "missing required parameter: " + std::string(name);
      return false;
    }
    auto *cx = corner->Find("x");
    auto *cy = corner->Find("y");
    if (!cx || !cx->IsInt()) {
      error = "missing required parameter: " + std::string(name) + ".x";
      return false;
    }
    if (!cy || !cy->IsInt()) {
      error = "missing required parameter: " + std::string(name) + ".y";
      return false;
    }
    x = cx->GetInt();
    y = cy->GetInt();
    return true;
  };

  int64_t from_x = 0;
  int64_t from_y = 0;
  int64_t to_x = 0;
  int64_t to_y = 0;
  std::string corner_error;
  if (!read_corner("from", from_x, from_y, corner_error) ||
      !read_corner("to", to_x, to_y, corner_error))
    return util::error_json(corner_error);

  constexpr int64_t kCoordMin = std::numeric_limits<int32_t>::min();
  constexpr int64_t kCoordMax = std::numeric_limits<int32_t>::max();
  if (from_x < kCoordMin || from_x > kCoordMax || from_y < kCoordMin ||
      from_y > kCoordMax || to_x < kCoordMin || to_x > kCoordMax ||
      to_y < kCoordMin || to_y > kCoordMax)
    return util::error_json(
        "tile coordinate out of range: from/to x and y must fit in a 32-bit "
        "integer");

  const RectRange range = normalize_rect_range(from_x, from_y, to_x, to_y);
  if (rect_exceeds_cell_limit(range, kMaxFillCells))
    return util::error_detail(
        "rect requires more than " + std::to_string(kMaxFillCells) + " cells",
        "from (" + std::to_string(range.min_x) + ", " +
            std::to_string(range.min_y) + ") to (" + std::to_string(range.max_x) +
            ", " + std::to_string(range.max_y) + ")",
        "at most " + std::to_string(kMaxFillCells) + " cells per call",
        "split the rect into chunks of at most " + std::to_string(kMaxFillCells) +
            " cells and call fill_tilemap_rect per chunk");

  int layer = 0;
  auto *la = args.Find("layer");
  if (la && la->IsInt())
    layer = static_cast<int>(la->GetInt());

  bool erase = false;
  auto *er = args.Find("erase");
  if (er && er->IsBool())
    erase = er->GetBool();

  int source_id = 0;
  int alternative = 0;
  godot::Vector2i atlas_coords(0, 0);
  if (!erase) {
    auto *si = args.Find("source_id");
    if (!si || !si->IsInt())
      return util::error_json("missing required parameter: source_id");
    source_id = static_cast<int>(si->GetInt());
    if (source_id < 0)
      return util::error_json(
          "invalid source_id: " + std::to_string(source_id) +
          " — pass a non-negative source_id, or set erase=true to clear "
          "cells");

    auto *ac = args.Find("atlas_coords");
    auto *acx = ac && ac->IsObject() ? ac->Find("x") : nullptr;
    auto *acy = ac && ac->IsObject() ? ac->Find("y") : nullptr;
    if (!acx || !acx->IsInt())
      return util::error_json("missing required parameter: atlas_coords.x");
    if (!acy || !acy->IsInt())
      return util::error_json("missing required parameter: atlas_coords.y");
    if (acx->GetInt() < 0 || acy->GetInt() < 0)
      return util::error_json(
          "invalid atlas_coords: x and y must be >= 0 because an INVALID "
          "atlas coordinate clears the cell instead of placing a tile — set "
          "erase=true to clear cells");
    atlas_coords = godot::Vector2i(static_cast<int>(acx->GetInt()),
                                   static_cast<int>(acy->GetInt()));

    auto *al = args.Find("alternative");
    if (al && al->IsInt())
      alternative = static_cast<int>(al->GetInt());
    if (alternative < 0)
      return util::error_json("invalid alternative: " +
                              std::to_string(alternative) + " (must be >= 0)");
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  const bool undoable = undo_redo != nullptr;
  const godot::String action_name("Fill TileMap Rect");
  const godot::StringName set_cell_method("set_cell");
  const godot::StringName erase_cell_method("erase_cell");
  if (undoable)
    undo_redo->create_action(action_name);

  int64_t set_count = 0;
  for (int64_t y = range.min_y; y <= range.max_y; ++y) {
    for (int64_t x = range.min_x; x <= range.max_x; ++x) {
      const godot::Vector2i coords(static_cast<int>(x), static_cast<int>(y));
      if (tile_map) {
        const int32_t old_source = tile_map->get_cell_source_id(layer, coords);
        if (undoable) {
          if (erase)
            undo_redo->add_do_method(node, erase_cell_method, layer, coords);
          else
            undo_redo->add_do_method(node, set_cell_method, layer, coords,
                                     source_id, atlas_coords, alternative);
          if (old_source >= 0)
            undo_redo->add_undo_method(
                node, set_cell_method, layer, coords, old_source,
                tile_map->get_cell_atlas_coords(layer, coords),
                tile_map->get_cell_alternative_tile(layer, coords));
          else
            undo_redo->add_undo_method(node, erase_cell_method, layer, coords);
        }
        if (erase)
          tile_map->erase_cell(layer, coords);
        else
          tile_map->set_cell(layer, coords, source_id, atlas_coords,
                             alternative);
      } else {
        const int32_t old_source = tile_map_layer->get_cell_source_id(coords);
        if (undoable) {
          if (erase)
            undo_redo->add_do_method(node, erase_cell_method, coords);
          else
            undo_redo->add_do_method(node, set_cell_method, coords, source_id,
                                     atlas_coords, alternative);
          if (old_source >= 0)
            undo_redo->add_undo_method(
                node, set_cell_method, coords, old_source,
                tile_map_layer->get_cell_atlas_coords(coords),
                tile_map_layer->get_cell_alternative_tile(coords));
          else
            undo_redo->add_undo_method(node, erase_cell_method, coords);
        }
        if (erase)
          tile_map_layer->erase_cell(coords);
        else
          tile_map_layer->set_cell(coords, source_id, atlas_coords,
                                   alternative);
      }
      ++set_count;
    }
  }
  if (undoable)
    undo_redo->commit_action(false);

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["set_count"] = JV(set_count);
  JV from_corner(JV::object_tag);
  from_corner["x"] = JV(range.min_x);
  from_corner["y"] = JV(range.min_y);
  info["from"] = std::move(from_corner);
  JV to_corner(JV::object_tag);
  to_corner["x"] = JV(range.max_x);
  to_corner["y"] = JV(range.max_y);
  info["to"] = std::move(to_corner);
  info["width"] = JV(range.width);
  info["height"] = JV(range.height);
  JV undo(JV::object_tag);
  undo["name"] = JV(util::to_std(action_name));
  undo["undoable"] = JV(undoable);
  if (!undoable)
    undo["undo_skip_reason"] = JV("EditorUndoRedoManager not available");
  info["undo"] = std::move(undo);
  r["result"] = std::move(info);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "fill_tilemap_rect completed");
  return r;
}

JV handle_tileset_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_tilemap_tileset called");

  std::string name = "TileSet";
  auto *n = args.Find("name");
  if (n && n->IsString())
    name = n->GetString();

  int tile_size = 16;
  auto *ts = args.Find("tile_size");
  if (ts && ts->IsInt())
    tile_size = static_cast<int>(ts->GetInt());

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return util::error_json("ClassDB singleton not available");

  godot::Variant ts_var = cdbs->instantiate(godot::StringName("TileSet"));
  if (ts_var.get_type() == godot::Variant::NIL)
    return util::error_json("failed to instantiate TileSet");

  godot::Ref<godot::TileSet> tile_set = ts_var;
  if (tile_set.is_null())
    return util::error_json("instantiated object is not a TileSet");

  tile_set->set("resource_name", godot::String(name.c_str()));
  tile_set->set("tile_size", godot::Vector2i(tile_size, tile_size));

  resource_ops::register_memory_resource(tile_set, name);
  tile_set->set_path(godot::String(("memory://" + name).c_str()));

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["class"] = JV("TileSet");
  info["name"] = JV(name);
  info["path"] = JV("memory://" + name);
  info["object_id"] = JV(static_cast<int64_t>(tile_set->get_instance_id()));
  info["object_id_str"] =
      JV(std::to_string(static_cast<int64_t>(tile_set->get_instance_id())));
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_tilemap_tileset completed");
  return r;
}

} // namespace tilemap_ops
} // namespace godot_autopilot
