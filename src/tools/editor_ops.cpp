#include "editor_ops.hpp"
#include "../runtime/gsd_protocol.hpp"
#include "../util/scene_path.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <string>

namespace godot_self_driving {
namespace editor_ops {

namespace {

constexpr int MAX_TREE_DEPTH = 12;

std::string to_std(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

std::string relative_path(godot::Node *node, godot::Node *root) {
  if (!node || !root)
    return "";
  std::string abs_path = to_std(node->get_path());
  std::string root_path = to_std(root->get_path());
  if (abs_path == root_path) {
    return to_std(node->get_name());
  }
  if (abs_path.size() > root_path.size() + 1 && abs_path.find(root_path) == 0 &&
      abs_path[root_path.size()] == '/') {
    return abs_path.substr(root_path.size() + 1);
  }
  return abs_path;
}

bool dir_to_json(godot::EditorFileSystemDirectory *dir, mcp::JsonValue &j,
                 int depth) {
  if (!dir)
    return false;
  j["name"] = mcp::JsonValue(to_std(dir->get_name()));
  j["path"] = mcp::JsonValue(to_std(dir->get_path()));
  j["type"] = mcp::JsonValue("directory");
  mcp::JsonValue children_arr(mcp::JsonValue::array_tag);
  bool truncated = false;
  if (depth < MAX_TREE_DEPTH) {
    for (int i = 0; i < dir->get_subdir_count(); i++) {
      auto *sub = dir->get_subdir(i);
      if (sub) {
        mcp::JsonValue child(mcp::JsonValue::object_tag);
        if (dir_to_json(sub, child, depth + 1))
          truncated = true;
        children_arr.PushBack(std::move(child));
      }
    }
    for (int i = 0; i < dir->get_file_count(); i++) {
      mcp::JsonValue file(mcp::JsonValue::object_tag);
      file["name"] = mcp::JsonValue(to_std(dir->get_file(i)));
      file["path"] = mcp::JsonValue(to_std(dir->get_file_path(i)));
      godot::StringName sn = dir->get_file_type(i);
      file["type"] = mcp::JsonValue(to_std(godot::String(sn)));
      file["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
      children_arr.PushBack(std::move(file));
    }
  } else if (dir->get_subdir_count() > 0 || dir->get_file_count() > 0) {
    truncated = true;
  }
  j["children"] = std::move(children_arr);
  return truncated;
}

mcp::JsonValue error_json(const std::string &msg) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(msg);
  return e;
}

godot::PackedStringArray editor_unsaved_scenes(godot::EditorInterface *editor) {
  godot::Variant v = editor->call(godot::StringName("get_unsaved_scenes"));
  if (v.get_type() != godot::Variant::PACKED_STRING_ARRAY) {
    return {};
  }
  return v;
}

std::string unsaved_list_str(const godot::PackedStringArray &scenes) {
  std::string list;
  for (int i = 0; i < scenes.size(); i++) {
    if (i > 0)
      list += ", ";
    list += to_std(scenes[i]);
  }
  return list;
}

} // namespace

mcp::JsonValue handle_get_selection(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_selection called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  auto *sel = editor->get_selection();
  if (!sel) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorSelection not available");
    return e;
  }
  auto nodes = sel->get_selected_nodes();
  mcp::JsonValue result_arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < nodes.size(); i++) {
    auto *node = godot::Object::cast_to<godot::Node>(nodes[i]);
    if (!node)
      continue;
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["name"] = mcp::JsonValue(to_std(node->get_name()));
    item["class"] = mcp::JsonValue(to_std(node->get_class()));
    item["path"] = mcp::JsonValue(root ? relative_path(node, root)
                                       : to_std(node->get_name()));
    result_arr.PushBack(std::move(item));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_selection completed");
  return r;
}

mcp::JsonValue handle_set_selection(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_selection called");
  auto *paths_p = args.Find("paths");
  if (!paths_p || !paths_p->IsArray()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: paths (array)");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *sel = editor->get_selection();
  if (!sel) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorSelection not available");
    return e;
  }
  sel->clear();
  auto *root = editor->get_edited_scene_root();
  const auto &paths_arr = paths_p->GetArray();
  for (const auto &p : paths_arr) {
    if (p.IsString()) {
      std::string hint;
      auto *node = godot_self_driving::util::resolve_scene_node(p.GetString(),
                                                                root, &hint);
      if (node) {
        sel->add_node(node);
      }
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_selection completed");
  return r;
}

mcp::JsonValue handle_get_edited_scene_root(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_edited_scene_root called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(nullptr);
    return r;
  }
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["name"] = mcp::JsonValue(to_std(root->get_name()));
  j["type"] = mcp::JsonValue(to_std(root->get_class()));
  j["path"] = mcp::JsonValue(to_std(root->get_name()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_edited_scene_root completed");
  return r;
}

mcp::JsonValue handle_save_scene(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no scene open");
    return e;
  }
  godot::Error err = editor->save_scene();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  if (err != godot::Error::OK) {
    godot::String existing_path = root->get_scene_file_path();
    if (existing_path.is_empty()) {
      std::string root_name = to_std(root->get_name());
      std::string generated_path = "res://" + root_name + ".tscn";
      LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                "scene has no file path, saving as " +
                                    generated_path);
      editor->save_scene_as(godot::String(generated_path.c_str()));
      if (!godot::FileAccess::file_exists(
              godot::String(generated_path.c_str()))) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] =
            mcp::JsonValue("failed to save scene to \"" + generated_path +
                           "\": no file was created after save_scene_as — use "
                           "editor_save_scene_as with an explicit path");
        LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                                  "editor_save_scene failed to save as " +
                                      generated_path);
        return e;
      }
      r["path"] = mcp::JsonValue(generated_path);
      r["result"] = mcp::JsonValue("saved");
      r["note"] =
          mcp::JsonValue("scene had no file path — saved to " + generated_path +
                         "; use editor_save_scene_as to choose a directory");
      scene_dirty_tracker::clear_scene_modified();
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "editor_save_scene completed (saved as new scene)");
      return r;
    }
    r["path"] = mcp::JsonValue(to_std(existing_path));
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  } else {
    godot::String file_path = root->get_scene_file_path();
    r["path"] = mcp::JsonValue(to_std(file_path));
    r["result"] = mcp::JsonValue("saved");
    scene_dirty_tracker::clear_scene_modified();
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_scene completed");
  return r;
}

mcp::JsonValue handle_save_all_scenes(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_all_scenes called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  editor->save_all_scenes();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_all_scenes completed");
  return r;
}

mcp::JsonValue handle_reload_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_reload_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *sp = args.Find("scene_path");
  std::string scene_path;
  if (sp && sp->IsString()) {
    scene_path = sp->GetString();
  } else {
    auto *root = editor->get_edited_scene_root();
    if (!root) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("no scene open, provide scene_path");
      return e;
    }
    scene_path = to_std(root->get_scene_file_path());
    if (scene_path.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("current scene has no file path, provide scene_path");
      return e;
    }
  }
  editor->reload_scene_from_path(godot::String(scene_path.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_reload_scene completed");
  return r;
}

mcp::JsonValue handle_inspect_object(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_inspect_object called");
  auto *rp = args.Find("resource_path");
  if (!rp || !rp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: resource_path");
    return e;
  }
  std::string path = rp->GetString();
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }
  auto res = loader->load(godot::String(path.c_str()));
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to load resource: " + path);
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  editor->inspect_object(res.ptr());
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_inspect_object completed");
  return r;
}

mcp::JsonValue handle_undo_redo_start(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_start called");
  auto *an = args.Find("name");
  if (!an || !an->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  std::string action_name = an->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  undo_redo->create_action(godot::String(action_name.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_start completed");
  return r;
}

mcp::JsonValue handle_undo_redo_commit(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_commit called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  undo_redo->commit_action();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_commit completed");
  return r;
}

mcp::JsonValue handle_undo_redo_add_do(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_add_do called");
  auto *np = args.Find("node_path");
  auto *mt = args.Find("method");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!mt || !mt->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }
  std::string node_path = np->GetString();
  std::string method = mt->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  std::string hint;
  auto *node = godot_self_driving::util::resolve_scene_node(
      node_path, editor ? editor->get_edited_scene_root() : nullptr, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path + " — " + hint);
    return e;
  }
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  auto *val = args.Find("value");
  if (val && !val->IsNull()) {
    godot::Variant var = VariantJson::deserialize(*val);
    undo_redo->add_do_method(node, godot::StringName(method.c_str()), var);
  } else {
    undo_redo->add_do_method(node, godot::StringName(method.c_str()));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_add_do completed");
  return r;
}

mcp::JsonValue handle_undo_redo_add_undo(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_add_undo called");
  auto *np = args.Find("node_path");
  auto *mt = args.Find("method");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!mt || !mt->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: method");
    return e;
  }
  std::string node_path = np->GetString();
  std::string method = mt->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  std::string hint;
  auto *node = godot_self_driving::util::resolve_scene_node(
      node_path, editor ? editor->get_edited_scene_root() : nullptr, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path + " — " + hint);
    return e;
  }
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  auto *val = args.Find("value");
  if (val && !val->IsNull()) {
    godot::Variant var = VariantJson::deserialize(*val);
    undo_redo->add_undo_method(node, godot::StringName(method.c_str()), var);
  } else {
    undo_redo->add_undo_method(node, godot::StringName(method.c_str()));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo_redo_add_undo completed");
  return r;
}

mcp::JsonValue handle_file_system_get_resources(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_file_system_get_resources called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorFileSystem not available");
    return e;
  }
  godot::EditorFileSystemDirectory *dir = nullptr;
  auto *pp = args.Find("path");
  if (pp && pp->IsString()) {
    std::string path = pp->GetString();
    dir = efs->get_filesystem_path(godot::String(path.c_str()));
  } else {
    dir = efs->get_filesystem();
  }
  if (!dir) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(nullptr);
    return r;
  }
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  bool truncated = dir_to_json(dir, result, 0);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  if (truncated) {
    r["max_depth"] = mcp::JsonValue(static_cast<int64_t>(MAX_TREE_DEPTH));
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_file_system_get_resources completed");
  return r;
}

mcp::JsonValue handle_file_system_scan(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_file_system_scan called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorFileSystem not available");
    return e;
  }
  efs->scan();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_file_system_scan completed");
  return r;
}

mcp::JsonValue handle_import_resource(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_import_resource called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = pp->GetString();
  godot::String path_gs(path.c_str());
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }
  auto res = loader->load(path_gs);
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to load resource: " + path +
                                " (may need manual import)");
    return e;
  }
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(to_std(res->get_class()));
  j["path"] = mcp::JsonValue(path);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_import_resource completed");
  return r;
}

mcp::JsonValue handle_set_main_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_main_scene called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = pp->GetString();
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  ps->set_setting("application/run/main_scene",
                  godot::Variant(godot::String(path.c_str())));
  ps->save();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_main_scene completed");
  return r;
}

mcp::JsonValue handle_play_current_scene(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_play_current_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  if (editor->is_playing_scene()) {
    mcp::JsonValue a(mcp::JsonValue::object_tag);
    a["result"] = mcp::JsonValue("already_playing");
    a["is_playing"] = mcp::JsonValue(true);
    return a;
  }
  editor->play_current_scene();
  if (!editor->is_playing_scene()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to start scene playback: no game process started — ensure the "
        "current scene is saved and runnable");
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["is_playing"] = mcp::JsonValue(true);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_play_current_scene completed");
  return r;
}

mcp::JsonValue handle_stop_playing(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_stop_playing called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  editor->stop_playing_scene();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_stop_playing completed");
  return r;
}

mcp::JsonValue handle_get_resource_filesystem(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_resource_filesystem called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorFileSystem not available");
    return e;
  }
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["scanning"] = mcp::JsonValue(efs->is_scanning());
  j["progress"] = mcp::JsonValue(efs->get_scanning_progress());
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_resource_filesystem completed");
  return r;
}

mcp::JsonValue handle_get_plugin_list(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_plugin_list called");
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["plugins"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  j["note"] = mcp::JsonValue("Plugin enumeration not available via godot-cpp "
                             "API; use editor_set_plugin_enabled directly");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_get_plugin_list completed");
  return r;
}

mcp::JsonValue handle_set_plugin_enabled(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_plugin_enabled called");
  auto *pp = args.Find("plugin");
  auto *ep = args.Find("enabled");
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: plugin");
    return e;
  }
  if (!ep || !ep->IsBool()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: enabled (bool)");
    return e;
  }
  std::string plugin = pp->GetString();
  bool enabled = ep->GetBool();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  editor->set_plugin_enabled(godot::String(plugin.c_str()), enabled);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_set_plugin_enabled completed");
  return r;
}

mcp::JsonValue handle_new_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_new_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return error_json("EditorInterface not available");
  }

  std::string type = "Node";
  auto *tp = args.Find("type");
  if (tp && tp->IsString())
    type = tp->GetString();

  std::string name = "NewRoot";
  auto *np = args.Find("name");
  if (np && np->IsString())
    name = np->GetString();

  bool close_current = false;
  auto *cp = args.Find("close_current");
  if (cp && cp->IsBool())
    close_current = cp->GetBool();

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    return error_json("ClassDB singleton not available");
  }

  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Node"))) {
    return error_json(type + " is not a Node subclass");
  }

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    return error_json("failed to instantiate node type: " + type);
  }

  auto *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    return error_json("instantiated object is not a Node: " + type);
  }

  node->set_name(godot::StringName(name.c_str()));

  bool closed_previous = false;
  auto *existing_root = editor->get_edited_scene_root();
  if (existing_root) {
    if (!close_current) {
      return error_json(
          "scene already has a root node — call editor_new_scene with "
          "close_current=true, or call editor_close_scene first");
    }
    if (existing_root->get_scene_file_path().is_empty()) {
      return util::error_detail("current scene is unsaved",
                                to_std(existing_root->get_name()),
                                "scene saved before close",
                                "call editor_save_scene first, then "
                                "editor_new_scene with close_current=true");
    }
    godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
    if (!unsaved.is_empty()) {
      return error_json(
          "scene has unsaved changes: " + unsaved_list_str(unsaved) +
          " — save first (editor_save_scene)");
    }
    godot::Error err = editor->close_scene();
    if (err != godot::Error::OK) {
      return error_json("failed to close scene (error " +
                        std::to_string(static_cast<int>(err)) + ")");
    }
    closed_previous = true;
    scene_dirty_tracker::clear_scene_modified();
  }

  editor->add_root_node(node);

  int64_t waited_ms = 0;
  bool switched = false;
  while (waited_ms < GSD_NEW_SCENE_SWITCH_WAIT_MS) {
    if (editor->get_edited_scene_root() == node) {
      switched = true;
      break;
    }
    godot::OS::get_singleton()->delay_usec(GSD_NEW_SCENE_POLL_MS * 1000);
    waited_ms += GSD_NEW_SCENE_POLL_MS;
  }
  if (!switched) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "scene switch did not take effect within 2000 ms (editor_new_scene) — "
        "the new scene root was not observed after add_root_node");
    return e;
  }

  scene_dirty_tracker::clear_scene_modified();

  {
    godot::StringName type_sn(type.c_str());
    if (cdbs->is_parent_class(type_sn, godot::StringName("Node2D"))) {
      editor->set_main_screen_editor(godot::String("2D"));
    } else if (cdbs->is_parent_class(type_sn, godot::StringName("Node3D"))) {
      editor->set_main_screen_editor(godot::String("3D"));
    }
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(name);
  inner["type"] = mcp::JsonValue(type);
  inner["closed_previous"] = mcp::JsonValue(closed_previous);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_new_scene completed");
  return r;
}

mcp::JsonValue handle_open_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_open_scene called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = pp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  if (!unsaved.is_empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "current scene has unsaved changes: " + unsaved_list_str(unsaved) +
        " — save first (editor_save_scene)");
    return e;
  }
  auto *old_root = editor->get_edited_scene_root();
  editor->open_scene_from_path(godot::String(path.c_str()));
  auto *new_root = editor->get_edited_scene_root();
  if (new_root == old_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to open scene at " + path +
        " — edited scene did not change (check editor output log)");
    return e;
  }

  {
    auto *root = new_root;
    if (root) {
      godot::StringName root_class = root->get_class();
      auto *cdbs = godot::ClassDBSingleton::get_singleton();
      if (cdbs &&
          cdbs->is_parent_class(root_class, godot::StringName("Node2D"))) {
        editor->set_main_screen_editor(godot::String("2D"));
      } else if (cdbs && cdbs->is_parent_class(root_class,
                                               godot::StringName("Node3D"))) {
        editor->set_main_screen_editor(godot::String("3D"));
      }
    }
  }

  scene_dirty_tracker::clear_scene_modified();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_open_scene completed");
  return r;
}

mcp::JsonValue handle_close_scene(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_close_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return error_json("EditorInterface not available");
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  if (!unsaved.is_empty()) {
    return error_json(
        "scene has unsaved changes: " + unsaved_list_str(unsaved) +
        " — save first (editor_save_scene)");
  }
  godot::Error err = editor->close_scene();
  if (err != godot::Error::OK) {
    return error_json("failed to close scene (error " +
                      std::to_string(static_cast<int>(err)) + ")");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("closed");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_close_scene completed");
  return r;
}

mcp::JsonValue handle_save_scene_as(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_scene_as called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    return error_json("missing required parameter: path");
  }
  std::string path = pp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return error_json("EditorInterface not available");
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    return error_json("no scene open");
  }
  godot::String path_gs(path.c_str());
  if (path_gs.begins_with("res://")) {
    godot::String dir = path_gs.get_base_dir();
    if (!godot::DirAccess::dir_exists_absolute(dir)) {
      godot::Error err = godot::DirAccess::make_dir_recursive_absolute(dir);
      if (err != godot::Error::OK) {
        return error_json("failed to create parent directory: " + to_std(dir) +
                          " (error " + std::to_string(static_cast<int>(err)) +
                          ")");
      }
    }
  }
  editor->save_scene_as(path_gs);
  if (!godot::FileAccess::file_exists(path_gs)) {
    return error_json("save failed — file not created at " + path +
                      " (check editor output log)");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("saved");
  scene_dirty_tracker::clear_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_save_scene_as completed");
  return r;
}

mcp::JsonValue handle_new_text_resource(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_new_text_resource called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    return error_json("missing required parameter: path");
  }
  auto *sp = args.Find("source_code");
  if (!sp || !sp->IsString()) {
    return error_json("missing required parameter: source_code");
  }
  std::string path = pp->GetString();
  auto file = godot::FileAccess::open(godot::String(path.c_str()),
                                      godot::FileAccess::WRITE);
  if (file.is_null()) {
    return error_json("failed to open file for writing: " + path);
  }
  file->store_string(godot::String(sp->GetString().c_str()));
  file->close();
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(path);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_new_text_resource completed");
  return r;
}

} // namespace editor_ops
} // namespace godot_self_driving
