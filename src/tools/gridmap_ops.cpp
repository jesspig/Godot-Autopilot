#include "gridmap_ops.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/scene_path.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/grid_map.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_library.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <limits>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace gridmap_ops {

using JV = mcp::JsonValue;

namespace {

godot::Node *find_node(const std::string &path_str) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  return util::resolve_scene_node(path_str, editor->get_edited_scene_root());
}

godot::GridMap *find_gridmap(const std::string &path_str, JV &out_error) {
  auto *node = find_node(path_str);
  if (!node) {
    out_error = util::error_json("node not found: " + path_str);
    return nullptr;
  }
  auto *grid = godot::Object::cast_to<godot::GridMap>(node);
  if (!grid) {
    out_error = util::error_json(
        "node is not a GridMap: " + path_str +
        " — create one with create_gridmap, or fix the path");
  }
  return grid;
}

godot::Ref<godot::MeshLibrary>
resolve_library(const std::string &name, JV &out_error) {
  godot::Ref<godot::Resource> res = resource_ops::resolve_memory_resource(name);
  if (res.is_null()) {
    out_error = util::error_json("memory resource not found: " + name +
                                 " (create it with create_resource type "
                                 "MeshLibrary first)");
    return godot::Ref<godot::MeshLibrary>();
  }
  godot::Ref<godot::MeshLibrary> library = res;
  if (library.is_null()) {
    out_error = util::error_json("resource " + name + " is not a MeshLibrary");
  }
  return library;
}

bool parse_cell_pos(const JV &obj, godot::Vector3i &out, std::string &error) {
  auto *px = obj.Find("x");
  auto *py = obj.Find("y");
  auto *pz = obj.Find("z");
  if (!px || !px->IsInt()) {
    error = "missing required parameter: position.x";
    return false;
  }
  if (!py || !py->IsInt()) {
    error = "missing required parameter: position.y";
    return false;
  }
  if (!pz || !pz->IsInt()) {
    error = "missing required parameter: position.z";
    return false;
  }
  out = godot::Vector3i(static_cast<int>(px->GetInt()),
                        static_cast<int>(py->GetInt()),
                        static_cast<int>(pz->GetInt()));
  return true;
}

bool read_corner(const JV &args, const char *name, int64_t &x, int64_t &y,
                 int64_t &z, std::string &error) {
  auto *corner = args.Find(name);
  if (!corner || !corner->IsObject()) {
    error = "missing required parameter: " + std::string(name);
    return false;
  }
  godot::Vector3i pos;
  if (!parse_cell_pos(*corner, pos, error)) {
    error = std::string(name) + "." + error.substr(error.find('.') + 1);
    return false;
  }
  x = pos.x;
  y = pos.y;
  z = pos.z;
  return true;
}

bool check_coords_in_range(int64_t x, int64_t y, int64_t z, JV &out_error) {
  constexpr int64_t kMin = std::numeric_limits<int32_t>::min();
  constexpr int64_t kMax = std::numeric_limits<int32_t>::max();
  if (x < kMin || x > kMax || y < kMin || y > kMax || z < kMin || z > kMax) {
    out_error = util::error_json(
        "cell coordinate out of range: x, y and z must fit in a 32-bit "
        "integer");
    return false;
  }
  return true;
}

bool read_orientation(const JV &args, bool clearing, int &out, JV &out_error) {
  out = 0;
  auto *o = args.Find("orientation");
  if (o && o->IsInt())
    out = static_cast<int>(o->GetInt());
  if (!clearing && (out < kMinOrientation || out > kMaxOrientation)) {
    out_error = util::error_json("invalid orientation: " +
                                 std::to_string(out) +
                                 " (must be 0-23; omit it when clearing a "
                                 "cell with item -1)");
    return false;
  }
  return true;
}

JV make_undo_info(const godot::String &action_name, bool undoable) {
  JV undo(JV::object_tag);
  undo["name"] = JV(util::to_std(action_name));
  undo["undoable"] = JV(undoable);
  if (!undoable)
    undo["undo_skip_reason"] = JV("EditorUndoRedoManager not available");
  return undo;
}

} // namespace

BoxRange normalize_box_range(int64_t from_x, int64_t from_y, int64_t from_z,
                             int64_t to_x, int64_t to_y, int64_t to_z) {
  BoxRange range;
  range.min_x = from_x < to_x ? from_x : to_x;
  range.min_y = from_y < to_y ? from_y : to_y;
  range.min_z = from_z < to_z ? from_z : to_z;
  range.max_x = from_x > to_x ? from_x : to_x;
  range.max_y = from_y > to_y ? from_y : to_y;
  range.max_z = from_z > to_z ? from_z : to_z;
  range.size_x = range.max_x - range.min_x + 1;
  range.size_y = range.max_y - range.min_y + 1;
  range.size_z = range.max_z - range.min_z + 1;
  return range;
}

JV handle_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_gridmap called");

  std::string name = "GridMap";
  auto *n = args.Find("name");
  if (n && n->IsString())
    name = n->GetString();

  godot::Vector3 cell_size(1.0f, 1.0f, 1.0f);
  auto *cs = args.Find("cell_size");
  if (cs && cs->IsObject()) {
    cell_size = util::json_to_vec3(*cs);
    if (cell_size.x <= 0.0f || cell_size.y <= 0.0f || cell_size.z <= 0.0f)
      return util::error_json(
          "invalid cell_size: x, y and z must all be > 0");
  }

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return util::error_json("ClassDB singleton not available");

  godot::Variant obj_var = cdbs->instantiate(godot::StringName("GridMap"));
  if (obj_var.get_type() == godot::Variant::NIL)
    return util::error_json("failed to instantiate GridMap");

  auto *grid = godot::Object::cast_to<godot::GridMap>(obj_var);
  if (!grid)
    return util::error_json("instantiated object is not a GridMap");

  grid->set_name(godot::StringName(name.c_str()));
  grid->set_cell_size(cell_size);

  auto *editor = godot::EditorInterface::get_singleton();
  auto *pp = args.Find("parent_path");
  bool has_parent = pp && pp->IsString() && !pp->GetString().empty();

  if (has_parent) {
    auto *parent = find_node(pp->GetString());
    if (!parent)
      return util::error_json("parent node not found: " + pp->GetString());
    parent->add_child(grid);
    if (editor) {
      auto *scene_root = editor->get_edited_scene_root();
      if (scene_root)
        grid->set_owner(scene_root);
    }
  } else if (editor) {
    auto *existing_root = editor->get_edited_scene_root();
    if (existing_root) {
      existing_root->add_child(grid);
      grid->set_owner(existing_root);
    } else {
      editor->add_root_node(grid);
    }
  }

  if (editor)
    editor->set_main_screen_editor(godot::String("3D"));

  std::string result_path = name;
  if (editor) {
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root) {
      std::string abs = util::to_std(grid->get_path());
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
  info["class"] = JV("GridMap");
  info["name"] = JV(name);
  info["path"] = JV(result_path);
  info["object_id"] = JV(static_cast<int64_t>(grid->get_instance_id()));
  info["object_id_str"] =
      JV(std::to_string(static_cast<int64_t>(grid->get_instance_id())));
  r["result"] = std::move(info);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_gridmap completed");
  return r;
}

JV handle_create_library_item(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_meshlibrary_item called");

  auto *lib_arg = args.Find("library");
  if (!lib_arg || !lib_arg->IsString())
    lib_arg = args.Find("name");
  if (!lib_arg || !lib_arg->IsString())
    return util::error_json("missing required parameter: library");
  std::string library_name = lib_arg->GetString();

  auto *iid = args.Find("item_id");
  if (!iid || !iid->IsInt())
    return util::error_json("missing required parameter: item_id");
  int item_id = static_cast<int>(iid->GetInt());
  if (item_id < 0)
    return util::error_json("invalid item_id: " + std::to_string(item_id) +
                            " (must be >= 0)");

  auto *mesh_arg = args.Find("mesh");
  if (!mesh_arg || mesh_arg->IsNull())
    return util::error_json("missing required parameter: mesh");

  godot::Variant mesh_value;
  std::string resource_error;
  if (!resource_ops::try_resolve_resource_value(*mesh_arg, mesh_value,
                                                resource_error))
    return util::error_json(
        "invalid mesh: pass a resource reference as a \"res://...\" or "
        "\"memory://...\" string, or {\"path\": \"res://...\"} / "
        "{\"resource\": \"memory://...\"}");
  if (!resource_error.empty())
    return util::error_json(resource_error);

  godot::Object *mesh_obj = mesh_value;
  auto *mesh = godot::Object::cast_to<godot::Mesh>(mesh_obj);
  if (!mesh) {
    std::string actual =
        mesh_obj ? util::to_std(mesh_obj->get_class()) : "null";
    return util::error_json("mesh resource is not a Mesh (got " + actual +
                            ") — pass a Mesh resource such as a BoxMesh or an "
                            "imported model");
  }
  godot::Ref<godot::Mesh> mesh_ref(mesh);

  std::string item_name;
  auto *inm = args.Find("item_name");
  if (inm && inm->IsString())
    item_name = inm->GetString();

  JV resolve_err;
  godot::Ref<godot::MeshLibrary> library =
      resolve_library(library_name, resolve_err);
  if (library.is_null())
    return resolve_err;

  godot::PackedInt32Array existing = library->get_item_list();
  for (int i = 0; i < existing.size(); ++i) {
    if (existing[i] == item_id)
      return util::error_json("item_id already exists: " +
                              std::to_string(item_id));
  }

  library->create_item(item_id);
  library->set_item_mesh(item_id, mesh_ref);
  if (!item_name.empty())
    library->set_item_name(item_id, godot::StringName(item_name.c_str()));

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["result"] = JV("ok");
  info["item_id"] = JV(static_cast<int64_t>(item_id));
  info["item_name"] = JV(item_name);
  info["mesh_class"] = JV(util::to_std(mesh->get_class()));
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_meshlibrary_item completed");
  return r;
}

JV handle_set_cell(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_gridmap_cell called");

  auto *p = args.Find("node_path");
  if (!p || !p->IsString())
    p = args.Find("path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: node_path");

  JV node_err;
  auto *grid = find_gridmap(p->GetString(), node_err);
  if (!grid)
    return node_err;

  auto *pos_arg = args.Find("position");
  if (!pos_arg || !pos_arg->IsObject())
    return util::error_json("missing required parameter: position");
  godot::Vector3i pos;
  std::string pos_err;
  if (!parse_cell_pos(*pos_arg, pos, pos_err))
    return util::error_json(pos_err);
  JV range_err;
  if (!check_coords_in_range(pos.x, pos.y, pos.z, range_err))
    return range_err;

  auto *item_arg = args.Find("item");
  if (!item_arg || !item_arg->IsInt())
    return util::error_json("missing required parameter: item");
  int item = static_cast<int>(item_arg->GetInt());
  if (item < kInvalidCellItem)
    return util::error_json(
        "invalid item: " + std::to_string(item) +
        " — pass a non-negative MeshLibrary item id, or -1 to clear the cell");
  bool clearing = item == kInvalidCellItem;

  int orientation = 0;
  JV ori_err;
  if (!read_orientation(args, clearing, orientation, ori_err))
    return ori_err;

  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  const bool undoable = undo_redo != nullptr;
  const godot::String action_name("Set GridMap Cell");
  const godot::StringName set_cell_method("set_cell_item");
  int old_item = grid->get_cell_item(pos);
  int old_orientation = grid->get_cell_item_orientation(pos);
  if (undoable) {
    undo_redo->create_action(action_name);
    undo_redo->add_do_method(grid, set_cell_method, pos, item,
                             clearing ? 0 : orientation);
    undo_redo->add_undo_method(grid, set_cell_method, pos, old_item,
                               old_orientation);
  }
  grid->set_cell_item(pos, item, clearing ? 0 : orientation);
  if (undoable)
    undo_redo->commit_action(false);

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["undo"] = make_undo_info(action_name, undoable);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_gridmap_cell completed");
  return r;
}

JV handle_fill_box(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "fill_gridmap_box called");

  auto *p = args.Find("node_path");
  if (!p || !p->IsString())
    p = args.Find("path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: node_path");

  JV node_err;
  auto *grid = find_gridmap(p->GetString(), node_err);
  if (!grid)
    return node_err;

  int64_t from_x = 0;
  int64_t from_y = 0;
  int64_t from_z = 0;
  int64_t to_x = 0;
  int64_t to_y = 0;
  int64_t to_z = 0;
  std::string corner_error;
  if (!read_corner(args, "from", from_x, from_y, from_z, corner_error) ||
      !read_corner(args, "to", to_x, to_y, to_z, corner_error))
    return util::error_json(corner_error);

  if (!check_coords_in_range(from_x, from_y, from_z, node_err) ||
      !check_coords_in_range(to_x, to_y, to_z, node_err))
    return node_err;

  const BoxRange range =
      normalize_box_range(from_x, from_y, from_z, to_x, to_y, to_z);
  if (box_exceeds_cell_limit(range, kMaxFillCells))
    return util::error_detail(
        "box requires more than " + std::to_string(kMaxFillCells) + " cells",
        "from (" + std::to_string(range.min_x) + ", " +
            std::to_string(range.min_y) + ", " + std::to_string(range.min_z) +
            ") to (" + std::to_string(range.max_x) + ", " +
            std::to_string(range.max_y) + ", " + std::to_string(range.max_z) +
            ")",
        "at most " + std::to_string(kMaxFillCells) + " cells per call",
        "split the box into chunks of at most " +
            std::to_string(kMaxFillCells) +
            " cells and call fill_gridmap_box per chunk");

  bool erase = false;
  auto *er = args.Find("erase");
  if (er && er->IsBool())
    erase = er->GetBool();

  int item = kInvalidCellItem;
  if (!erase) {
    auto *item_arg = args.Find("item");
    if (!item_arg || !item_arg->IsInt())
      return util::error_json("missing required parameter: item");
    item = static_cast<int>(item_arg->GetInt());
    if (item < 0)
      return util::error_json(
          "invalid item: " + std::to_string(item) +
          " — pass a non-negative MeshLibrary item id, or set erase=true to "
          "clear cells");
  }

  int orientation = 0;
  JV ori_err;
  if (!read_orientation(args, erase, orientation, ori_err))
    return ori_err;

  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  const bool undoable = undo_redo != nullptr;
  const godot::String action_name("Fill GridMap Box");
  const godot::StringName set_cell_method("set_cell_item");
  if (undoable)
    undo_redo->create_action(action_name);

  int64_t set_count = 0;
  for (int64_t z = range.min_z; z <= range.max_z; ++z) {
    for (int64_t y = range.min_y; y <= range.max_y; ++y) {
      for (int64_t x = range.min_x; x <= range.max_x; ++x) {
        const godot::Vector3i pos(static_cast<int>(x), static_cast<int>(y),
                                  static_cast<int>(z));
        const int old_item = grid->get_cell_item(pos);
        const int old_orientation = grid->get_cell_item_orientation(pos);
        if (undoable) {
          if (erase)
            undo_redo->add_do_method(grid, set_cell_method, pos,
                                     kInvalidCellItem, 0);
          else
            undo_redo->add_do_method(grid, set_cell_method, pos, item,
                                     orientation);
          undo_redo->add_undo_method(grid, set_cell_method, pos, old_item,
                                     old_orientation);
        }
        if (erase)
          grid->set_cell_item(pos, kInvalidCellItem, 0);
        else
          grid->set_cell_item(pos, item, orientation);
        ++set_count;
      }
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
  from_corner["z"] = JV(range.min_z);
  info["from"] = std::move(from_corner);
  JV to_corner(JV::object_tag);
  to_corner["x"] = JV(range.max_x);
  to_corner["y"] = JV(range.max_y);
  to_corner["z"] = JV(range.max_z);
  info["to"] = std::move(to_corner);
  info["size_x"] = JV(range.size_x);
  info["size_y"] = JV(range.size_y);
  info["size_z"] = JV(range.size_z);
  info["undo"] = make_undo_info(action_name, undoable);
  r["result"] = std::move(info);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "fill_gridmap_box completed");
  return r;
}

JV handle_get_used_cells(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_gridmap_used_cells called");

  auto *p = args.Find("node_path");
  if (!p || !p->IsString())
    p = args.Find("path");
  if (!p || !p->IsString())
    return util::error_json("missing required parameter: node_path");

  JV node_err;
  auto *grid = find_gridmap(p->GetString(), node_err);
  if (!grid)
    return node_err;

  bool by_item = false;
  int filter_item = 0;
  auto *bi = args.Find("by_item");
  if (bi && bi->IsInt()) {
    by_item = true;
    filter_item = static_cast<int>(bi->GetInt());
    if (filter_item < 0)
      return util::error_json("invalid by_item: " +
                              std::to_string(filter_item) + " (must be >= 0)");
  }

  godot::Array used = by_item ? grid->get_used_cells_by_item(filter_item)
                              : grid->get_used_cells();

  JV cells(JV::array_tag);
  for (int i = 0; i < used.size(); ++i) {
    godot::Vector3i pos = static_cast<godot::Vector3i>(used[i]);
    JV entry(JV::object_tag);
    JV position(JV::object_tag);
    position["x"] = JV(static_cast<int64_t>(pos.x));
    position["y"] = JV(static_cast<int64_t>(pos.y));
    position["z"] = JV(static_cast<int64_t>(pos.z));
    entry["position"] = std::move(position);
    entry["item"] = JV(static_cast<int64_t>(grid->get_cell_item(pos)));
    entry["orientation"] =
        JV(static_cast<int64_t>(grid->get_cell_item_orientation(pos)));
    cells.PushBack(std::move(entry));
  }

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["cells"] = std::move(cells);
  info["count"] = JV(static_cast<int64_t>(used.size()));
  r["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_gridmap_used_cells completed");
  return r;
}

} // namespace gridmap_ops
} // namespace godot_autopilot
