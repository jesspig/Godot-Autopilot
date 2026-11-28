#include "tools/domains/editor/editor_ops.hpp"
#include "tools/domains/editor/editor_safety.hpp"
#include "tools/domains/editor/editor_ui_ops.hpp"
#include "tools/domains/editor/save_write_receipt.hpp"
#include "tools/domains/scene/scene_safety.hpp"
#include "runtime/gda_protocol.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include "util/scene_path.hpp"
#include "util/scene_verify.hpp"
#include "core/config.hpp"
#include "core/editor_readiness.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/sanitize_policy.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/base/dispatch.hpp"
#include "util/csharp_project_setup.hpp"
#include "tools/base/runtime_ops.hpp"
#include "util/dotnet_env.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_inspector.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/editor_paths.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_toaster.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_system_dock.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/script_editor_base.hpp>
#include <godot_cpp/classes/text_edit.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/undo_redo.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include "util/console_encoding.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace editor_ops_detail {

FileStamp file_stamp(const std::string &path) {
  FileStamp stamp;
  if (path.empty()) return stamp;
  const godot::String gs(path.c_str());
  if (!godot::FileAccess::file_exists(gs)) return stamp;
  godot::Ref<godot::FileAccess> file = godot::FileAccess::open(gs, godot::FileAccess::READ);
  if (file.is_null()) return stamp;
  stamp.exists = true;
  stamp.modified_ns = static_cast<uint64_t>(godot::FileAccess::get_modified_time(gs));
  const int64_t len = file->get_length();
  stamp.size_bytes = len > 0 ? len : 0;
  return stamp;
}

} // namespace editor_ops_detail

namespace editor_ops {

namespace {

constexpr int MAX_TREE_DEPTH = 12;

std::string relative_path(godot::Node *node, godot::Node *root) {
  if (!node || !root)
    return "";
  std::string abs_path = util::to_std(node->get_path());
  std::string root_path = util::to_std(root->get_path());
  if (abs_path == root_path) {
    return util::to_std(node->get_name());
  }
  if (abs_path.size() > root_path.size() + 1 && abs_path.find(root_path) == 0 &&
      abs_path[root_path.size()] == '/') {
    return abs_path.substr(root_path.size() + 1);
  }
  return abs_path;
}

std::string editor_error_name(godot::Error err) {
  switch (err) {
  case godot::Error::OK:
    return "OK";
  case godot::Error::FAILED:
    return "FAILED";
  case godot::Error::ERR_UNAVAILABLE:
    return "ERR_UNAVAILABLE";
  case godot::Error::ERR_UNCONFIGURED:
    return "ERR_UNCONFIGURED";
  case godot::Error::ERR_UNAUTHORIZED:
    return "ERR_UNAUTHORIZED";
  case godot::Error::ERR_PARAMETER_RANGE_ERROR:
    return "ERR_PARAMETER_RANGE_ERROR";
  case godot::Error::ERR_OUT_OF_MEMORY:
    return "ERR_OUT_OF_MEMORY";
  case godot::Error::ERR_FILE_NOT_FOUND:
    return "ERR_FILE_NOT_FOUND";
  case godot::Error::ERR_FILE_BAD_DRIVE:
    return "ERR_FILE_BAD_DRIVE";
  case godot::Error::ERR_FILE_BAD_PATH:
    return "ERR_FILE_BAD_PATH";
  case godot::Error::ERR_FILE_NO_PERMISSION:
    return "ERR_FILE_NO_PERMISSION";
  case godot::Error::ERR_FILE_ALREADY_IN_USE:
    return "ERR_FILE_ALREADY_IN_USE";
  case godot::Error::ERR_FILE_CANT_OPEN:
    return "ERR_FILE_CANT_OPEN";
  case godot::Error::ERR_FILE_CANT_WRITE:
    return "ERR_FILE_CANT_WRITE";
  case godot::Error::ERR_FILE_CANT_READ:
    return "ERR_FILE_CANT_READ";
  case godot::Error::ERR_FILE_UNRECOGNIZED:
    return "ERR_FILE_UNRECOGNIZED";
  case godot::Error::ERR_FILE_CORRUPT:
    return "ERR_FILE_CORRUPT";
  case godot::Error::ERR_FILE_MISSING_DEPENDENCIES:
    return "ERR_FILE_MISSING_DEPENDENCIES";
  case godot::Error::ERR_FILE_EOF:
    return "ERR_FILE_EOF";
  case godot::Error::ERR_CANT_OPEN:
    return "ERR_CANT_OPEN";
  case godot::Error::ERR_CANT_CREATE:
    return "ERR_CANT_CREATE";
  case godot::Error::ERR_QUERY_FAILED:
    return "ERR_QUERY_FAILED";
  case godot::Error::ERR_ALREADY_IN_USE:
    return "ERR_ALREADY_IN_USE";
  case godot::Error::ERR_LOCKED:
    return "ERR_LOCKED";
  case godot::Error::ERR_TIMEOUT:
    return "ERR_TIMEOUT";
  case godot::Error::ERR_CANT_CONNECT:
    return "ERR_CANT_CONNECT";
  case godot::Error::ERR_CANT_RESOLVE:
    return "ERR_CANT_RESOLVE";
  case godot::Error::ERR_CONNECTION_ERROR:
    return "ERR_CONNECTION_ERROR";
  case godot::Error::ERR_CANT_ACQUIRE_RESOURCE:
    return "ERR_CANT_ACQUIRE_RESOURCE";
  case godot::Error::ERR_CANT_FORK:
    return "ERR_CANT_FORK";
  case godot::Error::ERR_INVALID_DATA:
    return "ERR_INVALID_DATA";
  case godot::Error::ERR_INVALID_PARAMETER:
    return "ERR_INVALID_PARAMETER";
  case godot::Error::ERR_ALREADY_EXISTS:
    return "ERR_ALREADY_EXISTS";
  case godot::Error::ERR_DOES_NOT_EXIST:
    return "ERR_DOES_NOT_EXIST";
  case godot::Error::ERR_DATABASE_CANT_READ:
    return "ERR_DATABASE_CANT_READ";
  case godot::Error::ERR_DATABASE_CANT_WRITE:
    return "ERR_DATABASE_CANT_WRITE";
  case godot::Error::ERR_COMPILATION_FAILED:
    return "ERR_COMPILATION_FAILED";
  case godot::Error::ERR_METHOD_NOT_FOUND:
    return "ERR_METHOD_NOT_FOUND";
  case godot::Error::ERR_LINK_FAILED:
    return "ERR_LINK_FAILED";
  case godot::Error::ERR_SCRIPT_FAILED:
    return "ERR_SCRIPT_FAILED";
  case godot::Error::ERR_CYCLIC_LINK:
    return "ERR_CYCLIC_LINK";
  case godot::Error::ERR_INVALID_DECLARATION:
    return "ERR_INVALID_DECLARATION";
  case godot::Error::ERR_DUPLICATE_SYMBOL:
    return "ERR_DUPLICATE_SYMBOL";
  case godot::Error::ERR_PARSE_ERROR:
    return "ERR_PARSE_ERROR";
  case godot::Error::ERR_BUSY:
    return "ERR_BUSY";
  case godot::Error::ERR_SKIP:
    return "ERR_SKIP";
  case godot::Error::ERR_HELP:
    return "ERR_HELP";
  case godot::Error::ERR_BUG:
    return "ERR_BUG";
  case godot::Error::ERR_PRINTER_ON_FIRE:
    return "ERR_PRINTER_ON_FIRE";
  default:
    return "ERR_UNKNOWN";
  }
}

std::string editor_error_hint(godot::Error err) {
  switch (err) {
  case godot::Error::ERR_DOES_NOT_EXIST:
    return "no scene is open (close_editor_scene only closes the current tab; "
           "repeat it until no scene is open)";
  case godot::Error::ERR_BUSY:
  case godot::Error::ERR_LOCKED:
  case godot::Error::ERR_ALREADY_IN_USE:
    return "editor is busy or the resource is locked — retry after the current "
           "operation finishes";
  case godot::Error::ERR_FILE_CANT_WRITE:
  case godot::Error::ERR_FILE_NO_PERMISSION:
  case godot::Error::ERR_UNAUTHORIZED:
    return "write was refused — check the target path, file permissions and "
           "that the directory exists";
  case godot::Error::ERR_FILE_NOT_FOUND:
  case godot::Error::ERR_FILE_BAD_PATH:
    return "path does not exist — verify the res:// path and scan the file "
           "system first";
  case godot::Error::ERR_TIMEOUT:
    return "operation timed out — retry, or retry with a larger timeout_ms";
  default:
    return "see the editor output log for details";
  }
}

std::string editor_error_text(const std::string &what, godot::Error err) {
  const int code = static_cast<int>(err);
  return what + " (error " + std::to_string(code) + ": " +
         editor_error_name(err) + ") — " + editor_error_hint(err);
}


void attach_save_receipt(godot_autopilot::util::JsonValue &r, const godot_autopilot::util::JsonValue &args,
                         godot::Node *root, const std::string &res_path) {
  if (!root || res_path.empty())
    return;
  bool disk_ok = false;
  const std::string text = scene_verify::read_text_file(res_path, disk_ok);
  if (!disk_ok) {
    r["verify_error"] = godot_autopilot::util::JsonValue(
        "saved, but the on-disk file could not be re-read for verification: " +
        res_path);
    return;
  }
  const bool want_hash = save_receipt_flag(args, "include_hash");
  const bool want_paths = save_receipt_flag(args, "include_paths");
  const scene_verify::VerifyResult vr =
      scene_verify::compare_tree_with_text(root, text, want_hash);
  r["memory_nodes"] = godot_autopilot::util::JsonValue(vr.memory_nodes);
  r["disk_nodes"] = godot_autopilot::util::JsonValue(vr.disk_nodes);
  r["nodes_match"] = godot_autopilot::util::JsonValue(vr.match);
  r["memory_connections"] = godot_autopilot::util::JsonValue(vr.memory_connections);
  r["disk_connections"] = godot_autopilot::util::JsonValue(vr.disk_connections);
  r["connections_match"] =
      godot_autopilot::util::JsonValue(vr.memory_connections == vr.disk_connections);
  if (vr.inherited_nodes > 0)
    r["inherited_nodes"] = godot_autopilot::util::JsonValue(vr.inherited_nodes);
  if (vr.inherited_connections > 0)
    r["inherited_connections"] = godot_autopilot::util::JsonValue(vr.inherited_connections);
  if (!vr.match || want_paths) {
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (const std::string &p : vr.missing_paths)
      arr.PushBack(godot_autopilot::util::JsonValue(p));
    r["missing_paths"] = std::move(arr);
    if (vr.missing_truncated)
      r["missing_truncated"] = godot_autopilot::util::JsonValue(true);
  }
  if (!vr.inherited_paths.empty() && (want_paths || !vr.match)) {
    godot_autopilot::util::JsonValue inherited_arr(godot_autopilot::util::JsonValue::array_tag);
    for (const std::string &p : vr.inherited_paths)
      inherited_arr.PushBack(godot_autopilot::util::JsonValue(p));
    r["inherited_paths"] = std::move(inherited_arr);
    if (vr.inherited_truncated)
      r["inherited_truncated"] = godot_autopilot::util::JsonValue(true);
  }
  if (want_hash && vr.hash_computed) {
    r["memory_hash"] = godot_autopilot::util::JsonValue(hash_hex(vr.memory_hash));
    r["disk_hash"] = godot_autopilot::util::JsonValue(hash_hex(vr.disk_hash));
  }
  if (!vr.match) {
    const std::string mismatch_text =
        "memory/disk mismatch after save (nodes memory " +
        std::to_string(vr.memory_nodes) + " vs disk " +
        std::to_string(vr.disk_nodes) + "; connections memory " +
        std::to_string(vr.memory_connections) + " vs disk " +
        std::to_string(vr.disk_connections) +
        "; inherited instance nodes and their connections are excluded from "
        "these counts) — inspect missing_paths, then run verify_scene_saved; "
        "if persisted nodes were moved with remove_child/add_child, use "
        "reparent_node so owners persist";
    auto *existing_warning = r.Find("warning");
    if (existing_warning != nullptr && existing_warning->IsString() &&
        !existing_warning->GetString().empty()) {
      r["warning"] =
          godot_autopilot::util::JsonValue(existing_warning->GetString() + "; " + mismatch_text);
    } else {
      r["warning"] = godot_autopilot::util::JsonValue(mismatch_text);
    }
  }
}

bool tree_entry_matches(const std::string &name, const std::string &path,
                        const std::string &filter_lower) {
  if (filter_lower.empty())
    return true;
  return util::ascii_lowercase(name).find(filter_lower) != std::string::npos ||
         util::ascii_lowercase(path).find(filter_lower) != std::string::npos;
}

bool dir_to_json(godot::EditorFileSystemDirectory *dir, godot_autopilot::util::JsonValue &j,
                 int depth, int max_depth, const std::string &filter_lower,
                 bool &out_filtered) {
  if (!dir)
    return false;
  j["name"] = godot_autopilot::util::JsonValue(util::to_std(dir->get_name()));
  j["path"] = godot_autopilot::util::JsonValue(util::to_std(dir->get_path()));
  j["type"] = godot_autopilot::util::JsonValue("directory");
  godot_autopilot::util::JsonValue children_arr(godot_autopilot::util::JsonValue::array_tag);
  bool truncated = false;
  if (depth < max_depth) {
    for (int i = 0; i < dir->get_subdir_count(); i++) {
      auto *sub = dir->get_subdir(i);
      if (!sub)
        continue;
      godot_autopilot::util::JsonValue child(godot_autopilot::util::JsonValue::object_tag);
      bool child_filtered = false;
      if (dir_to_json(sub, child, depth + 1, max_depth, filter_lower,
                      child_filtered))
        truncated = true;
      if (child_filtered)
        out_filtered = true;
      auto *kids = child.Find("children");
      const bool has_kept_children =
          kids && kids->IsArray() && !kids->GetArray().empty();
      if (!filter_lower.empty() &&
          !tree_entry_matches(util::to_std(sub->get_name()),
                              util::to_std(sub->get_path()), filter_lower) &&
          !has_kept_children) {
        out_filtered = true;
        continue;
      }
      children_arr.PushBack(std::move(child));
    }
    for (int i = 0; i < dir->get_file_count(); i++) {
      const std::string fname = util::to_std(dir->get_file(i));
      const std::string fpath = util::to_std(dir->get_file_path(i));
      if (!tree_entry_matches(fname, fpath, filter_lower)) {
        out_filtered = true;
        continue;
      }
      godot_autopilot::util::JsonValue file(godot_autopilot::util::JsonValue::object_tag);
      file["name"] = godot_autopilot::util::JsonValue(fname);
      file["path"] = godot_autopilot::util::JsonValue(fpath);
      godot::StringName sn = dir->get_file_type(i);
      file["type"] = godot_autopilot::util::JsonValue(util::to_std(godot::String(sn)));
      file["children"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
      children_arr.PushBack(std::move(file));
    }
  } else if (dir->get_subdir_count() > 0 || dir->get_file_count() > 0) {
    truncated = true;
  }
  j["children"] = std::move(children_arr);
  return truncated;
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
    list += util::to_std(scenes[i]);
  }
  return list;
}

constexpr int MIN_NEW_SCENE_TIMEOUT_MS = 50;
constexpr int MAX_NEW_SCENE_TIMEOUT_MS = 30000;

bool same_scene_path(const godot::String &a, const godot::String &b) {
  return a.simplify_path() == b.simplify_path();
}

bool release_unclaimed_node(godot::Node *node, godot::EditorInterface *editor) {
  if (!node)
    return false;
  if (node->get_parent() != nullptr || editor->get_edited_scene_root() == node)
    return false;
  godot::memdelete(node);
  return true;
}

godot_autopilot::util::JsonValue editor_scene_state_json(godot::EditorInterface *editor) {
  godot_autopilot::util::JsonValue state(godot_autopilot::util::JsonValue::object_tag);
  auto *root = editor->get_edited_scene_root();
  if (root) {
    godot_autopilot::util::JsonValue root_info(godot_autopilot::util::JsonValue::object_tag);
    root_info["name"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
    std::string root_path = util::to_std(root->get_scene_file_path());
    if (!root_path.empty())
      root_info["scene_file_path"] = godot_autopilot::util::JsonValue(root_path);
    state["edited_root"] = std::move(root_info);
  } else {
    state["edited_root"] = godot_autopilot::util::JsonValue(nullptr);
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  godot_autopilot::util::JsonValue unsaved_arr(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < unsaved.size(); i++)
    unsaved_arr.PushBack(godot_autopilot::util::JsonValue(util::to_std(unsaved[i])));
  state["unsaved_scenes"] = std::move(unsaved_arr);
  auto *efs = editor->get_resource_filesystem();
  if (efs) {
    godot_autopilot::util::JsonValue fs(godot_autopilot::util::JsonValue::object_tag);
    fs["scanning"] = godot_autopilot::util::JsonValue(efs->is_scanning());
    fs["progress"] = godot_autopilot::util::JsonValue(efs->get_scanning_progress());
    state["file_system"] = std::move(fs);
  } else {
    state["file_system"] = godot_autopilot::util::JsonValue(nullptr);
  }
  return state;
}

void focus_main_screen_for_class(godot::EditorInterface *editor,
                                 const godot::StringName &class_name) {
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return;
  if (cdbs->is_parent_class(class_name, godot::StringName("Node2D"))) {
    editor->set_main_screen_editor(godot::String("2D"));
  } else if (cdbs->is_parent_class(class_name, godot::StringName("Node3D"))) {
    editor->set_main_screen_editor(godot::String("3D"));
  }
}

int64_t steady_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct SceneSwitchWatch {
  godot::Node *node = nullptr;
  std::string name;
  std::string type;
  bool closed_previous = false;
  int64_t started_ms = 0;
  int64_t deadline_ms = 0;
};

std::mutex g_scene_watch_mutex;
std::map<int64_t, std::shared_ptr<SceneSwitchWatch>> g_scene_watches;
std::atomic<int64_t> g_scene_watch_seq{1};

godot_autopilot::util::JsonValue new_scene_success_result(godot::EditorInterface *editor,
                                         const std::string &name,
                                         const std::string &type,
                                         bool closed_previous) {
  scene_dirty_tracker::clear_scene_modified();
  focus_main_screen_for_class(editor, godot::StringName(type.c_str()));
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["path"] = godot_autopilot::util::JsonValue(name);
  inner["type"] = godot_autopilot::util::JsonValue(type);
  inner["closed_previous"] = godot_autopilot::util::JsonValue(closed_previous);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_scene completed");
  return r;
}

godot_autopilot::util::JsonValue new_scene_timeout_result(godot::EditorInterface *editor,
                                         godot::Node *node, int64_t waited_ms,
                                         int64_t timeout_ms) {
  const bool released = release_unclaimed_node(node, editor);
  std::string message =
      "scene switch did not take effect within " + std::to_string(waited_ms) +
      " ms (create_editor_scene) — the new scene root was not observed after "
      "add_root_node (neighbor-tab occupation is rejected before add and "
      "never reaches this timeout; this timeout means the post-add switch "
      "itself was not observed)";
  if (released) {
    message += "; the unclaimed node was released";
  } else {
    message +=
        "; the node is still attached to the editor and was not released";
  }
  godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
  e["error"] = godot_autopilot::util::JsonValue(message);
  e["waited_ms"] = godot_autopilot::util::JsonValue(waited_ms);
  e["timeout_ms"] = godot_autopilot::util::JsonValue(timeout_ms);
  e["node_released"] = godot_autopilot::util::JsonValue(released);
  e["editor_state"] = editor_scene_state_json(editor);
  LogSystem::instance().log(
      LogLevel::Error, LogCategory::Tools,
      "create_editor_scene timed out after " + std::to_string(waited_ms) +
          " ms");
  return e;
}

} // namespace

godot_autopilot::util::JsonValue handle_get_selection(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_selection called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  auto *sel = editor->get_selection();
  if (!sel) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorSelection not available");
    return e;
  }
  auto nodes = sel->get_selected_nodes();
  godot_autopilot::util::JsonValue result_arr(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < nodes.size(); i++) {
    auto *node = godot::Object::cast_to<godot::Node>(nodes[i]);
    if (!node)
      continue;
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    item["name"] = godot_autopilot::util::JsonValue(util::to_std(node->get_name()));
    item["class"] = godot_autopilot::util::JsonValue(util::to_std(node->get_class()));
    item["path"] = godot_autopilot::util::JsonValue(root ? relative_path(node, root)
                                       : util::to_std(node->get_name()));
    result_arr.PushBack(std::move(item));
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_selection completed");
  return r;
}

godot_autopilot::util::JsonValue handle_set_selection(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_selection called");
  auto *paths_p = args.Find("paths");
  if (!paths_p || !paths_p->IsArray()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: paths (array)");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *sel = editor->get_selection();
  if (!sel) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorSelection not available");
    return e;
  }
  sel->clear();
  auto *root = editor->get_edited_scene_root();
  const auto &paths_arr = paths_p->GetArray();
  for (const auto &p : paths_arr) {
    if (p.IsString()) {
      std::string hint;
      auto *node = godot_autopilot::util::resolve_scene_node(p.GetString(),
                                                                root, &hint);
      if (node) {
        sel->add_node(node);
      }
    }
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_selection completed");
  return r;
}

godot_autopilot::util::JsonValue handle_get_edited_scene_root(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_edited_scene_root called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = godot_autopilot::util::JsonValue(nullptr);
    return r;
  }
  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  j["name"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
  j["type"] = godot_autopilot::util::JsonValue(util::to_std(root->get_class()));
  j["path"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_edited_scene_root completed");
  return r;
}

godot_autopilot::util::JsonValue handle_save_scene(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("no scene open");
    return e;
  }
  godot::Error err = editor->save_scene();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  if (err != godot::Error::OK) {
    godot::String existing_path = root->get_scene_file_path();
    if (existing_path.is_empty()) {
      std::string root_name = util::to_std(root->get_name());
      std::string generated_path = "res://" + root_name + ".tscn";
      LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                "scene has no file path, saving as " +
                                    generated_path);
      const editor_ops_detail::FileStamp before =
          editor_ops_detail::file_stamp(generated_path);
      editor->save_scene_as(godot::String(generated_path.c_str()));
      const editor_ops_detail::FileStamp after =
          editor_ops_detail::file_stamp(generated_path);
      if (editor_ops_detail::decide_write_outcome(before, after) ==
          editor_ops_detail::WriteOutcome::kUnverifiable) {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] =
            godot_autopilot::util::JsonValue("failed to save scene to \"" + generated_path +
                           "\": the write could not be confirmed — use "
                           "save_editor_scene_as with an explicit path");
        LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                                  "save_editor_scene failed to save as " +
                                      generated_path);
        return e;
      }
      r["path"] = godot_autopilot::util::JsonValue(generated_path);
      r["result"] = godot_autopilot::util::JsonValue("saved");
      r["note"] =
          godot_autopilot::util::JsonValue("scene had no file path — saved to " + generated_path +
                         "; use save_editor_scene_as to choose a directory");
      attach_save_receipt(r, args, root, generated_path);
      scene_dirty_tracker::clear_scene_modified();
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "save_editor_scene completed (saved as new scene)");
      return r;
    }
    std::string failed_path = util::to_std(existing_path);
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(editor_error_text("save failed", err) + ": " +
                                failed_path);
    e["path"] = godot_autopilot::util::JsonValue(failed_path);
    e["error_code"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(err));
    e["error_name"] = godot_autopilot::util::JsonValue(editor_error_name(err));
    e["hint"] = godot_autopilot::util::JsonValue(editor_error_hint(err));
    LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                              "save_editor_scene failed " +
                                  editor_error_text("", err) + ": " + failed_path);
    return e;
  } else {
    godot::String file_path = root->get_scene_file_path();
    const std::string saved_path = util::to_std(file_path);
    r["path"] = godot_autopilot::util::JsonValue(saved_path);
    r["result"] = godot_autopilot::util::JsonValue("saved");
    r["disk_persisted"] = godot_autopilot::util::JsonValue(true);
    attach_save_receipt(r, args, root, saved_path);
    scene_dirty_tracker::clear_scene_modified();
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_save_all_scenes(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scenes called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  editor->save_all_scenes();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scenes completed");
  return r;
}

godot_autopilot::util::JsonValue handle_reload_scene(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_editor_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *sp = args.Find("scene_path");
  std::string scene_path;
  if (sp && sp->IsString()) {
    scene_path = sp->GetString();
  } else {
    auto *root = editor->get_edited_scene_root();
    if (!root) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("no scene open, provide scene_path");
      return e;
    }
    scene_path = util::to_std(root->get_scene_file_path());
    if (scene_path.empty()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] =
          godot_autopilot::util::JsonValue("current scene has no file path, provide scene_path");
      return e;
    }
  }
  godot::String target_path(scene_path.c_str());
  godot::PackedStringArray open_scenes = editor->get_open_scenes();
  bool is_open = false;
  for (int i = 0; i < open_scenes.size(); i++) {
    if (same_scene_path(open_scenes[i], target_path)) {
      is_open = true;
      break;
    }
  }
  if (!is_open) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("scene is not open: " + scene_path + "; reload did not run");
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "reload_editor_scene skipped, scene is not open: " +
                                  scene_path);
    return e;
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  bool target_unsaved = false;
  for (int i = 0; i < unsaved.size(); i++) {
    if (same_scene_path(unsaved[i], target_path)) {
      target_unsaved = true;
      break;
    }
  }
  const godot_autopilot::util::JsonValue *discard_p = args.Find("discard_unsaved");
  const bool discard_unsaved = discard_p != nullptr && discard_p->IsBool() &&
                               discard_p->GetBool();
  const godot_autopilot::util::JsonValue *confirm_p = args.Find("confirm");
  const bool confirm = confirm_p != nullptr && confirm_p->IsBool() &&
                       confirm_p->GetBool();
  if (editor_safety::check_reload_discard(target_unsaved, discard_unsaved,
                                          confirm) !=
      editor_safety::ReloadDiscardFailure::kNone) {
    godot_autopilot::util::JsonValue e = util::error_json(
        "scene has unsaved changes: " + unsaved_list_str(unsaved) +
        " — pass discard_unsaved=true and confirm=true to reload and discard them");
    e["code"] = godot_autopilot::util::JsonValue("confirmation_required");
    e["confirm_required"] = godot_autopilot::util::JsonValue(true);
    e["discard_required"] = godot_autopilot::util::JsonValue(true);
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (int i = 0; i < unsaved.size(); i++)
      arr.PushBack(godot_autopilot::util::JsonValue(util::to_std(unsaved[i])));
    e["unsaved_scenes"] = std::move(arr);
    return e;
  }
  std::string snapshot_path;
  std::string snapshot_error;
  if (target_unsaved) {
    auto *root = editor->get_edited_scene_root();
    if (!root) {
      snapshot_error = "no edited scene root available for snapshot";
    } else {
      godot::Ref<godot::PackedScene> snapshot;
      snapshot.instantiate();
      const godot::Error pack_error = snapshot->pack(root);
      if (pack_error != godot::OK) {
        snapshot_error = "failed to pack in-memory scene (error " +
                         std::to_string(static_cast<int>(pack_error)) + ")";
      } else {
        const std::string name = util::to_std(root->get_name());
        std::string safe_name;
        for (char c : name) {
          if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')
            safe_name.push_back(c);
          else
            safe_name.push_back('_');
        }
        if (safe_name.empty())
          safe_name = "scene";
        const uint64_t serial = resource_fs::next_file_transaction_serial();
        snapshot_path = "user://gda_reload_snapshots/" + safe_name + "-" +
                        std::to_string(serial) + ".tscn";
        std::string dir_created;
        std::string dir_error;
        auto *saver = godot::ResourceSaver::get_singleton();
        if (!resource_fs::ensure_parent_directory(snapshot_path, dir_created,
                                                  dir_error)) {
          snapshot_error = dir_error;
          snapshot_path.clear();
        } else if (!saver) {
          snapshot_error = "ResourceSaver not available";
          snapshot_path.clear();
        } else {
          const godot::Error save_error = saver->save(
              snapshot, godot::String::utf8(snapshot_path.c_str()));
          if (save_error != godot::OK) {
            snapshot_error = "failed to save snapshot (error " +
                             std::to_string(static_cast<int>(save_error)) + ")";
            snapshot_path.clear();
          }
        }
      }
    }
  }
  editor->reload_scene_from_path(target_path);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("reloaded");
  r["scene_path"] = godot_autopilot::util::JsonValue(scene_path);
  r["discarded_unsaved"] = godot_autopilot::util::JsonValue(target_unsaved);
  if (!snapshot_path.empty())
    r["snapshot_path"] = godot_autopilot::util::JsonValue(snapshot_path);
  else if (!snapshot_error.empty())
    r["snapshot_error"] = godot_autopilot::util::JsonValue(snapshot_error);
  auto *reloaded_root = editor->get_edited_scene_root();
  if (reloaded_root &&
      same_scene_path(reloaded_root->get_scene_file_path(), target_path)) {
    r["observed"] = godot_autopilot::util::JsonValue(true);
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_editor_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_undo_redo_start(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_undo_redo_action called");
  auto *an = args.Find("name");
  if (!an || !an->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: name");
    return e;
  }
  std::string action_name = an->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  undo_redo->create_action(godot::String(action_name.c_str()),
                           godot::UndoRedo::MERGE_DISABLE, editor);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_undo_redo_action completed");
  return r;
}

godot_autopilot::util::JsonValue handle_undo_redo_commit(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "commit_editor_undo_redo called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  undo_redo->commit_action();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "commit_editor_undo_redo completed");
  return r;
}

namespace {

enum class UndoRedoBranch {
  Do,
  Undo,
};

godot_autopilot::util::JsonValue handle_undo_redo_add_method(const godot_autopilot::util::JsonValue &args,
                                           UndoRedoBranch branch) {
  const bool is_do = branch == UndoRedoBranch::Do;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            is_do ? "add_editor_undo_redo_do called"
                                  : "add_editor_undo_redo_undo called");
  auto *np = args.Find("node_path");
  auto *mt = args.Find("method");
  if (!np || !np->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!mt || !mt->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: method");
    return e;
  }
  std::string node_path = np->GetString();
  std::string method = mt->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  std::string hint;
  auto *node = godot_autopilot::util::resolve_scene_node(
      node_path, editor ? editor->get_edited_scene_root() : nullptr, &hint);
  if (!node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("node not found: " + node_path + " — " + hint);
    return e;
  }
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  if (!undo_redo) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorUndoRedoManager not available");
    return e;
  }
  godot::UndoRedo *history = undo_redo->get_history_undo_redo(
      godot::EditorUndoRedoManager::GLOBAL_HISTORY);
  if (!history) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "global undo history is not available — call editor_undo create first");
    return e;
  }
  auto *val = args.Find("value");
  const godot::Callable callable(node, godot::StringName(method.c_str()));
  godot::Variant bound;
  if (val && !val->IsNull()) {
    std::string hint;
    godot::ClassDBSingleton *cdb = godot::ClassDBSingleton::get_singleton();
    if (cdb) {
      const godot::TypedArray<godot::Dictionary> methods =
          cdb->class_get_method_list(node->get_class());
      for (int i = 0; i < methods.size(); ++i) {
        const godot::Dictionary entry = methods[i];
        if (!entry.has("name") ||
            util::to_std(static_cast<godot::String>(entry["name"])) != method) {
          continue;
        }
        if (!entry.has("args")) {
          break;
        }
        const godot::Array params = entry["args"];
        if (params.size() == 0) {
          break;
        }
        const godot::Dictionary param = params[0];
        if (!param.has("type")) {
          break;
        }
        const auto type =
            static_cast<godot::Variant::Type>(static_cast<int>(param["type"]));
        if (type != godot::Variant::NIL && type != godot::Variant::OBJECT &&
            type != godot::Variant::DICTIONARY && type != godot::Variant::CALLABLE &&
            type != godot::Variant::SIGNAL && type != godot::Variant::RID) {
          hint = util::ascii_lowercase(
              util::to_std(godot::Variant::get_type_name(type)));
        }
        break;
      }
    }
    bound = hint.empty() ? VariantJson::deserialize(*val)
                         : VariantJson::deserialize(*val, hint);
  }
  if (is_do) {
    if (val && !val->IsNull()) {
      history->add_do_method(callable.bind(bound));
    } else {
      history->add_do_method(callable);
    }
  } else {
    if (val && !val->IsNull()) {
      history->add_undo_method(callable.bind(bound));
    } else {
      history->add_undo_method(callable);
    }
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            is_do ? "add_editor_undo_redo_do completed"
                                  : "add_editor_undo_redo_undo completed");
  return r;
}

} // namespace

godot_autopilot::util::JsonValue handle_undo_redo_add_do(const godot_autopilot::util::JsonValue &args) {
  return handle_undo_redo_add_method(args, UndoRedoBranch::Do);
}

godot_autopilot::util::JsonValue handle_undo_redo_add_undo(const godot_autopilot::util::JsonValue &args) {
  return handle_undo_redo_add_method(args, UndoRedoBranch::Undo);
}

godot_autopilot::util::JsonValue handle_file_system_get_resources(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_tree called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorFileSystem not available");
    return e;
  }
  godot::EditorFileSystemDirectory *dir = nullptr;
  std::string root_path;
  auto *prefix_p = args.Find("prefix");
  auto *pp = args.Find("path");
  if (prefix_p && prefix_p->IsString() && !prefix_p->GetString().empty()) {
    root_path = prefix_p->GetString();
  } else if (pp && pp->IsString()) {
    root_path = pp->GetString();
  }
  if (!root_path.empty()) {
    dir = efs->get_filesystem_path(godot::String(root_path.c_str()));
  } else {
    dir = efs->get_filesystem();
  }
  if (!dir) {
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = godot_autopilot::util::JsonValue(nullptr);
    return r;
  }
  int max_depth = MAX_TREE_DEPTH;
  auto *depth_p = args.Find("depth");
  if (depth_p && depth_p->IsInt()) {
    const int64_t requested = depth_p->GetInt();
    if (requested < 0) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("invalid parameter: depth must be >= 0");
      return e;
    }
    max_depth = requested > MAX_TREE_DEPTH
                    ? MAX_TREE_DEPTH
                    : static_cast<int>(requested);
  }
  std::string filter_lower;
  auto *filter_p = args.Find("filter");
  if (filter_p && filter_p->IsString())
    filter_lower = util::ascii_lowercase(filter_p->GetString());
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  bool filtered = false;
  bool truncated = dir_to_json(dir, result, 0, max_depth, filter_lower, filtered);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(result);
  if (truncated) {
    r["truncated"] = godot_autopilot::util::JsonValue(true);
    r["max_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(max_depth));
  }
  if (filtered) {
    r["filtered"] = godot_autopilot::util::JsonValue(true);
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_tree completed");
  return r;
}

godot_autopilot::util::JsonValue handle_file_system_scan(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scan_editor_file_system called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorFileSystem not available");
    return e;
  }
  if (efs->is_scanning()) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "scan_editor_file_system skipped: already scanning");
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["skipped"] = godot_autopilot::util::JsonValue(true);
    r["reason"] = godot_autopilot::util::JsonValue("already scanning");
    return r;
  }
  efs->scan();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scan_editor_file_system completed");
  return r;
}

godot_autopilot::util::JsonValue handle_set_main_scene(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_main_scene called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = pp->GetString();
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("ProjectSettings not available");
    return e;
  }
  ps->set_setting("application/run/main_scene",
                  godot::Variant(godot::String(path.c_str())));
  ps->save();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_main_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_play_current_scene(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_editor_current_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  if (editor->is_playing_scene()) {
    godot_autopilot::util::JsonValue a(godot_autopilot::util::JsonValue::object_tag);
    a["result"] = godot_autopilot::util::JsonValue("already_playing");
    a["is_playing"] = godot_autopilot::util::JsonValue(true);
    a["autosaved"] = godot_autopilot::util::JsonValue(false);
    a["disk_changed"] = godot_autopilot::util::JsonValue(false);
    return a;
  }
  bool autosave_setting = true;
  godot::Ref<godot::EditorSettings> settings = editor->get_editor_settings();
  if (settings.is_valid()) {
    const godot::Variant value =
        settings->get_setting("run/auto_save/save_before_running");
    if (value.get_type() == godot::Variant::BOOL)
      autosave_setting = value.operator bool();
  }
  const bool had_unsaved = editor->get_unsaved_scenes().size() > 0;
  editor->play_current_scene();
  if (!editor->is_playing_scene()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "failed to start scene playback: no game process started — ensure the "
        "current scene is saved and runnable");
    return startup_gated_error(editor_in_startup_window(), e);
  }
  const bool autosaved = autosave_setting && had_unsaved;
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  r["is_playing"] = godot_autopilot::util::JsonValue(true);
  r["autosaved"] = godot_autopilot::util::JsonValue(autosaved);
  r["disk_changed"] = godot_autopilot::util::JsonValue(autosaved);
  r["autosave_setting"] =
      godot_autopilot::util::JsonValue(std::string("run/auto_save/save_before_running"));
  if (autosaved) {
    r["hint"] = godot_autopilot::util::JsonValue(
        "Godot saved dirty scenes before running; remove temporary nodes and "
        "call save_editor_scene to restore the disk state");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_editor_current_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_stop_playing(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_editor_playing called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  editor->stop_playing_scene();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_editor_playing completed");
  return r;
}

godot_autopilot::util::JsonValue handle_get_resource_filesystem(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_status called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *efs = editor->get_resource_filesystem();
  if (!efs) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorFileSystem not available");
    return e;
  }
  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  j["scanning"] = godot_autopilot::util::JsonValue(efs->is_scanning());
  j["progress"] = godot_autopilot::util::JsonValue(efs->get_scanning_progress());
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_status completed");
  return r;
}

godot_autopilot::util::JsonValue handle_set_plugin_enabled(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_plugin_enabled called");
  auto *pp = args.Find("plugin");
  auto *ep = args.Find("enabled");
  if (!pp || !pp->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: plugin");
    return e;
  }
  if (!ep || !ep->IsBool()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: enabled (bool)");
    return e;
  }
  std::string plugin = pp->GetString();
  bool enabled = ep->GetBool();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  editor->set_plugin_enabled(godot::String(plugin.c_str()), enabled);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_plugin_enabled completed");
  return r;
}

godot_autopilot::util::JsonValue handle_new_scene(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
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

  int timeout_ms = GDA_NEW_SCENE_SWITCH_WAIT_MS;
  auto *tmp = args.Find("timeout_ms");
  if (tmp) {
    if (!tmp->IsInt()) {
      return util::error_json(
          "invalid parameter: timeout_ms must be an integer between 50 and 30000");
    }
    int64_t requested_ms = tmp->GetInt();
    if (requested_ms < MIN_NEW_SCENE_TIMEOUT_MS ||
        requested_ms > MAX_NEW_SCENE_TIMEOUT_MS) {
      return util::error_json(
          "invalid parameter: timeout_ms out of range (50-30000): " +
          std::to_string(requested_ms));
    }
    timeout_ms = static_cast<int>(requested_ms);
  }

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    return util::error_json("ClassDB singleton not available");
  }

  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Node"))) {
    return util::error_json(type + " is not a Node subclass");
  }

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    return util::error_json("failed to instantiate node type: " + type);
  }

  auto *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    return util::error_json("instantiated object is not a Node: " + type);
  }

  node->set_name(godot::StringName(name.c_str()));

  bool closed_previous = false;
  auto *existing_root = editor->get_edited_scene_root();
  if (existing_root) {
    if (!close_current) {
      release_unclaimed_node(node, editor);
      return util::error_json(
          "scene already has a root node — call create_editor_scene with "
          "close_current=true, or call close_editor_scene first");
    }
    if (existing_root->get_scene_file_path().is_empty()) {
      release_unclaimed_node(node, editor);
      return util::error_detail("current scene is unsaved",
                                util::to_std(existing_root->get_name()),
                                "scene saved before close",
                                "call save_editor_scene first, then "
                                "create_editor_scene with close_current=true");
    }
    godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
    if (!unsaved.is_empty()) {
      release_unclaimed_node(node, editor);
      return util::error_json(
          "scene has unsaved changes: " + unsaved_list_str(unsaved) +
          " — save first (save_editor_scene)");
    }
    godot::Error err = editor->close_scene();
    if (err != godot::Error::OK) {
      release_unclaimed_node(node, editor);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(editor_error_text("failed to close scene", err));
      e["error_code"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(err));
      e["error_name"] = godot_autopilot::util::JsonValue(editor_error_name(err));
      e["hint"] = godot_autopilot::util::JsonValue(editor_error_hint(err));
      return e;
    }
    closed_previous = true;
    scene_dirty_tracker::clear_scene_modified();
    auto *post_close_root = editor->get_edited_scene_root();
    if (post_close_root) {
      release_unclaimed_node(node, editor);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "close_scene only closed the current tab — another open scene tab "
          "still occupies the editor; repeat close_editor_scene until no "
          "scene is open, then retry create_editor_scene with "
          "close_current=true");
      e["editor_state"] = editor_scene_state_json(editor);
      LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
          "create_editor_scene aborted: neighbor tab still occupies editor");
      return e;
    }
  }

  editor->add_root_node(node);

  if (editor->get_edited_scene_root() == node) {
    return new_scene_success_result(editor, name, type, closed_previous);
  }

  auto watch = std::make_shared<SceneSwitchWatch>();
  watch->node = node;
  watch->name = name;
  watch->type = type;
  watch->closed_previous = closed_previous;
  watch->started_ms = steady_now_ms();
  watch->deadline_ms = watch->started_ms + timeout_ms;
  const int64_t watch_id = g_scene_watch_seq.fetch_add(1);
  {
    std::lock_guard<std::mutex> lock(g_scene_watch_mutex);
    g_scene_watches[watch_id] = watch;
  }
  godot_autopilot::util::JsonValue wait_obj(godot_autopilot::util::JsonValue::object_tag);
  wait_obj["kind"] = godot_autopilot::util::JsonValue("editor_scene_switch");
  wait_obj["id"] = godot_autopilot::util::JsonValue(watch_id);
  wait_obj["timeout_ms"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(timeout_ms));
  godot_autopilot::util::JsonValue marker(godot_autopilot::util::JsonValue::object_tag);
  marker["__gda_wait"] = std::move(wait_obj);
  LogSystem::instance().log_detailed(
      LogLevel::Info, LogCategory::Tools,
      "create_editor_scene handed the scene switch to the transport thread",
      "watch_id=" + std::to_string(watch_id) + " timeout_ms=" +
          std::to_string(timeout_ms));
  return marker;
}

namespace {

godot_autopilot::util::JsonValue advance_scene_switch(int64_t watch_id) {
  std::shared_ptr<SceneSwitchWatch> watch;
  {
    std::lock_guard<std::mutex> lock(g_scene_watch_mutex);
    auto it = g_scene_watches.find(watch_id);
    if (it != g_scene_watches.end())
      watch = it->second;
  }
  if (!watch) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("the scene switch wait state is gone");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  const int64_t now_ms = steady_now_ms();
  if (editor->get_edited_scene_root() == watch->node) {
    {
      std::lock_guard<std::mutex> lock(g_scene_watch_mutex);
      g_scene_watches.erase(watch_id);
    }
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["done"] = godot_autopilot::util::JsonValue(true);
    r["result"] = new_scene_success_result(editor, watch->name, watch->type,
                                           watch->closed_previous);
    return r;
  }
  if (now_ms >= watch->deadline_ms) {
    {
      std::lock_guard<std::mutex> lock(g_scene_watch_mutex);
      g_scene_watches.erase(watch_id);
    }
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["done"] = godot_autopilot::util::JsonValue(true);
    r["result"] = new_scene_timeout_result(editor, watch->node,
                                           now_ms - watch->started_ms,
                                           watch->deadline_ms -
                                               watch->started_ms);
    return r;
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["done"] = godot_autopilot::util::JsonValue(false);
  return r;
}

} // namespace

godot_autopilot::util::JsonValue wait_scene_switch(int64_t watch_id, int64_t timeout_ms) {
  if (!runtime_ops::has_editor_queue()) {
    return util::error_json(
        "the editor main thread queue is unavailable — the scene switch "
        "cannot be waited for");
  }
  CommandQueue &queue = get_editor_queue();
  if (queue.is_main_thread()) {
    std::shared_ptr<SceneSwitchWatch> watch;
    {
      std::lock_guard<std::mutex> lock(g_scene_watch_mutex);
      auto it = g_scene_watches.find(watch_id);
      if (it != g_scene_watches.end()) {
        watch = it->second;
        g_scene_watches.erase(it);
      }
    }
    if (watch) {
      auto *editor = godot::EditorInterface::get_singleton();
      if (editor)
        release_unclaimed_node(watch->node, editor);
    }
    return util::error_json(
        "create_editor_scene cannot wait for the scene switch on the editor "
        "main thread — call the tool through the native control plane so the wait "
        "runs on the transport thread");
  }
  const int64_t deadline_ms = steady_now_ms() + timeout_ms;
  for (;;) {
    const int64_t remaining_ms = deadline_ms - steady_now_ms();
    if (remaining_ms <= 0)
      break;
    const godot_autopilot::util::JsonValue slice = dispatch::run_main_thread_slice(
        queue, "create_editor_scene",
        [watch_id]() { return advance_scene_switch(watch_id); },
        remaining_ms);
    if (slice.Find("error") != nullptr)
      return slice;
    const auto *done_p = slice.Find("done");
    if (done_p && done_p->IsBool() && done_p->GetBool()) {
      const auto *result_p = slice.Find("result");
      if (result_p)
        return *result_p;
      return util::error_json(
          "the scene switch finished without a readable result");
    }
  }
  const godot_autopilot::util::JsonValue final_slice = dispatch::run_main_thread_slice(
      queue, "create_editor_scene finalize",
      [watch_id]() { return advance_scene_switch(watch_id); }, 2000);
  if (final_slice.Find("error") == nullptr) {
    const auto *done_p = final_slice.Find("done");
    if (done_p && done_p->IsBool() && done_p->GetBool()) {
      const auto *result_p = final_slice.Find("result");
      if (result_p)
        return *result_p;
    }
  }
  return util::error_json(
      "the editor main thread did not report the scene switch result in time");
}

godot_autopilot::util::JsonValue handle_open_scene(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_editor_scene called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = pp->GetString();
  bool reload_if_open = false;
  auto *reload_p = args.Find("reload_if_open");
  if (reload_p) {
    if (!reload_p->IsBool())
      return util::error_json("reload_if_open must be a boolean");
    reload_if_open = reload_p->GetBool();
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  if (!unsaved.is_empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "current scene has unsaved changes: " + unsaved_list_str(unsaved) +
        " — save first (save_editor_scene), or discard the changes by calling "
        "reload_editor_scene with the listed path, then retry open_editor_scene");
    return e;
  }
  auto *old_root = editor->get_edited_scene_root();
  godot::String requested_path = godot::String::utf8(path.c_str());
  if (old_root != nullptr && !path.empty()) {
    godot::String open_path = old_root->get_scene_file_path();
    if (!open_path.is_empty() && same_scene_path(requested_path, open_path)) {
      if (reload_if_open) {
        editor->reload_scene_from_path(requested_path);
        auto *reloaded_root = editor->get_edited_scene_root();
        const bool observed =
            reloaded_root != nullptr &&
            same_scene_path(reloaded_root->get_scene_file_path(),
                            requested_path);
        godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
        r["result"] = godot_autopilot::util::JsonValue("ok");
        r["path"] = godot_autopilot::util::JsonValue(path);
        r["already_open"] = godot_autopilot::util::JsonValue(true);
        r["reloaded"] = godot_autopilot::util::JsonValue(true);
        r["reload_observed"] = godot_autopilot::util::JsonValue(observed);
        if (observed) {
          r["scene_root"] = godot_autopilot::util::JsonValue(
              util::to_std(godot::String(reloaded_root->get_name())));
          r["scene_root_type"] = godot_autopilot::util::JsonValue(
              util::to_std(godot::String(reloaded_root->get_class())));
        }
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                  "open_editor_scene reloaded already open: " +
                                      path);
        return r;
      }
      godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
      r["result"] = godot_autopilot::util::JsonValue("ok");
      r["path"] = godot_autopilot::util::JsonValue(path);
      r["already_open"] = godot_autopilot::util::JsonValue(true);
      r["hint"] = godot_autopilot::util::JsonValue(
          "the requested scene is already the edited scene, so it was not "
          "reopened and edits made by scripts since it was opened are not "
          "refreshed");
      r["suggested_action"] = godot_autopilot::util::JsonValue(
          "call open_editor_scene with reload_if_open=true to reload this "
          "scene from disk (discards unsaved changes), or close_editor_scene "
          "then open_editor_scene");
      LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                "open_editor_scene already open: " + path);
      return r;
    }
  }
  editor->open_scene_from_path(requested_path);
  auto *new_root = editor->get_edited_scene_root();
  if (new_root == old_root) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "failed to open scene at " + path +
        " — edited scene did not change (check editor output log)");
    return e;
  }

  if (new_root) {
    focus_main_screen_for_class(editor,
                                godot::StringName(new_root->get_class()));
  }

  scene_dirty_tracker::clear_scene_modified();
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  r["path"] = godot_autopilot::util::JsonValue(path);
  r["already_open"] = godot_autopilot::util::JsonValue(false);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_editor_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_close_scene(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "close_editor_scene called");
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  if (!unsaved.is_empty()) {
    return util::error_json(
        "scene has unsaved changes: " + unsaved_list_str(unsaved) +
        " — save first (save_editor_scene)");
  }
  godot::Error err = editor->close_scene();
  if (err != godot::Error::OK) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(editor_error_text("failed to close scene", err));
    e["error_code"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(err));
    e["error_name"] = godot_autopilot::util::JsonValue(editor_error_name(err));
    e["hint"] = godot_autopilot::util::JsonValue(editor_error_hint(err));
    return e;
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("closed");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "close_editor_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_save_scene_as(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene_as called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    return util::error_json("missing required parameter: path");
  }
  std::string path = pp->GetString();
  const util::ProjectPath checked =
      util::normalize_project_path(path, false, false);
  if (!checked.valid())
    return util::error_detail("path rejected", "path", checked.error,
                              "use a res:// path below the project root");
  path = checked.value;
  if (!scene_safety::is_scene_file(path))
    return util::error_json("parameter 'path' must end with .tscn or .scn");
  const godot_autopilot::util::JsonValue *overwrite_p = args.Find("overwrite");
  const bool overwrite = overwrite_p != nullptr && overwrite_p->IsBool() &&
                         overwrite_p->GetBool();
  const godot_autopilot::util::JsonValue *confirm_p = args.Find("confirm");
  const bool confirm = confirm_p != nullptr && confirm_p->IsBool() &&
                       confirm_p->GetBool();
  const bool existed = godot::FileAccess::file_exists(
      godot::String::utf8(path.c_str()));
  const editor_safety::SaveAsTargetFailure target_failure =
      editor_safety::check_save_as_target(true, true, existed, overwrite,
                                          confirm);
  if (target_failure != editor_safety::SaveAsTargetFailure::kNone) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    if (target_failure == editor_safety::SaveAsTargetFailure::kTargetExists) {
      e = util::error_json("target scene already exists: " + path +
                           " — pass overwrite=true to replace it");
      e["code"] = godot_autopilot::util::JsonValue("file_exists");
    } else {
      e = util::error_json(
          "overwriting an existing scene requires overwrite=true and "
          "confirm=true: " +
          path);
      e["code"] = godot_autopilot::util::JsonValue("confirmation_required");
      e["confirm_required"] = godot_autopilot::util::JsonValue(true);
    }
    e["path"] = godot_autopilot::util::JsonValue(path);
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    return util::error_json("no scene open");
  }
  godot::String path_gs(path.c_str());
  godot::String dir = path_gs.get_base_dir();
  if (!godot::DirAccess::dir_exists_absolute(dir)) {
    godot::Error err = godot::DirAccess::make_dir_recursive_absolute(dir);
    if (err != godot::Error::OK) {
      return util::error_json("failed to create parent directory: " +
                        util::to_std(dir) + " (error " +
                        std::to_string(static_cast<int>(err)) + ")");
    }
  }
  const editor_ops_detail::FileStamp before = editor_ops_detail::file_stamp(path);
  editor->save_scene_as(path_gs);
  const editor_ops_detail::FileStamp after = editor_ops_detail::file_stamp(path);
  if (editor_ops_detail::decide_write_outcome(before, after) ==
      editor_ops_detail::WriteOutcome::kUnverifiable) {
    return util::error_json(
        "save could not be confirmed — the target file is unchanged or unavailable: " +
        path + "; the current tab and unsaved state were preserved");
  }
  bool tab_path_registered = false;
  godot::PackedStringArray open_scenes = editor->get_open_scenes();
  for (int i = 0; i < open_scenes.size(); i++) {
    if (same_scene_path(open_scenes[i], path_gs)) {
      tab_path_registered = true;
      break;
    }
  }
  bool tab_rebuilt = false;
  if (!tab_path_registered) {
    godot::Error close_err = editor->close_scene();
    if (close_err == godot::Error::OK) {
      editor->open_scene_from_path(path_gs);
      auto *reopened_root = editor->get_edited_scene_root();
      tab_rebuilt = reopened_root != nullptr &&
                    same_scene_path(reopened_root->get_scene_file_path(), path_gs);
    }
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("saved");
  r["path"] = godot_autopilot::util::JsonValue(path);
  r["tab_path_registered"] = godot_autopilot::util::JsonValue(tab_path_registered);
  if (!tab_path_registered) {
    r["tab_rebuilt"] = godot_autopilot::util::JsonValue(tab_rebuilt);
    if (!tab_rebuilt) {
      r["warning"] = godot_autopilot::util::JsonValue(
          "saved, but the editor tab path could not be synchronized; "
          "get_open_scenes/reload_editor_scene may not see " + path);
    }
  }
  auto *verify_root = editor->get_edited_scene_root();
  if (!verify_root)
    verify_root = root;
  attach_save_receipt(r, args, verify_root, path);
  scene_dirty_tracker::clear_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene_as completed");
  return r;
}


bool save_receipt_flag(const godot_autopilot::util::JsonValue &args, const char *name) {
  auto *p = args.Find(name);
  return p && p->IsBool() && p->GetBool();
}

std::string hash_hex(uint64_t value) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(value));
  return std::string(buf);
}

namespace {

std::string editor_action_text(const godot_autopilot::util::JsonValue &args) {
  auto *action = args.Find("action");
  return (action && action->IsString()) ? action->GetString() : std::string();
}

godot_autopilot::util::JsonValue editor_action_missing(const std::string &choices) {
  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
  error["error"] =
      godot_autopilot::util::JsonValue("missing required parameter: action (" + choices + ")");
  return error;
}

godot_autopilot::util::JsonValue editor_action_invalid(const std::string &choices) {
  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
  error["error"] = godot_autopilot::util::JsonValue("invalid action: expected " + choices);
  return error;
}

godot_autopilot::util::JsonValue editor_open_scenes_result(godot::EditorInterface *editor) {
  godot::PackedStringArray scenes = editor->get_open_scenes();
  godot_autopilot::util::JsonValue array(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < scenes.size(); i++) {
    array.PushBack(godot_autopilot::util::JsonValue(util::to_std(scenes[i])));
  }
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["result"] = std::move(array);
  return result;
}

godot_autopilot::util::JsonValue editor_info_result(godot::EditorInterface *editor) {
  godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
  auto *engine = godot::Engine::get_singleton();
  if (engine) {
    godot::Dictionary version = engine->get_version_info();
    if (version.has("string")) {
      godot::String text = version["string"];
      info["engine_version"] = godot_autopilot::util::JsonValue(util::to_std(text));
    }
  }
  auto *settings = godot::ProjectSettings::get_singleton();
  if (settings) {
    godot::Variant project_name =
        settings->get_setting("application/config/name");
    if (project_name.get_type() == godot::Variant::STRING) {
      info["project_name"] =
          godot_autopilot::util::JsonValue(util::to_std(godot::String(project_name)));
    }
    godot::Variant main_scene =
        settings->get_setting("application/run/main_scene");
    if (main_scene.get_type() == godot::Variant::STRING) {
      info["main_scene"] =
          godot_autopilot::util::JsonValue(util::to_std(godot::String(main_scene)));
    }
  }
  auto *root = editor->get_edited_scene_root();
  if (root) {
    godot_autopilot::util::JsonValue root_info(godot_autopilot::util::JsonValue::object_tag);
    root_info["name"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
    root_info["type"] = godot_autopilot::util::JsonValue(util::to_std(root->get_class()));
    const std::string scene_path = util::to_std(root->get_scene_file_path());
    if (!scene_path.empty()) {
      root_info["scene_file_path"] = godot_autopilot::util::JsonValue(scene_path);
    }
    info["edited_root"] = std::move(root_info);
  } else {
    info["edited_root"] = godot_autopilot::util::JsonValue(nullptr);
  }
  info["open_scene_count"] =
      godot_autopilot::util::JsonValue(static_cast<int64_t>(editor->get_open_scenes().size()));
  info["is_playing"] = godot_autopilot::util::JsonValue(editor->is_playing_scene());
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["result"] = std::move(info);
  return result;
}

godot_autopilot::util::JsonValue packed_string_array_json(const godot::PackedStringArray &values) {
  godot_autopilot::util::JsonValue array(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < values.size(); i++) {
    array.PushBack(godot_autopilot::util::JsonValue(util::to_std(values[i])));
  }
  return array;
}

bool path_list_contains(const godot::PackedStringArray &paths,
                        const godot::String &path) {
  if (path.is_empty())
    return false;
  for (int i = 0; i < paths.size(); i++) {
    if (same_scene_path(paths[i], path))
      return true;
  }
  return false;
}

void attach_undo_history(godot_autopilot::util::JsonValue &result,
                         godot::EditorUndoRedoManager *manager) {
  godot::UndoRedo *history =
      manager ? manager->get_history_undo_redo(
                    godot::EditorUndoRedoManager::GLOBAL_HISTORY)
              : nullptr;
  result["has_undo"] =
      godot_autopilot::util::JsonValue(history != nullptr && history->has_undo());
  result["has_redo"] =
      godot_autopilot::util::JsonValue(history != nullptr && history->has_redo());
  result["current_action_name"] =
      godot_autopilot::util::JsonValue(history != nullptr
                         ? util::to_std(history->get_current_action_name())
                         : std::string());
  result["history_count"] = godot_autopilot::util::JsonValue(
      static_cast<int64_t>(history != nullptr ? history->get_history_count()
                                              : 0));
}

godot::Ref<godot::Script> load_script_resource(const std::string &path) {
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader)
    return {};
  godot::Ref<godot::Resource> resource =
      loader->load(godot::String(path.c_str()));
  godot::Ref<godot::Script> script = resource;
  return script;
}

godot::TextEdit *current_script_text_edit(
    godot::ScriptEditor *script_editor) {
  if (!script_editor)
    return nullptr;
  godot::ScriptEditorBase *base = script_editor->get_current_editor();
  if (!base)
    return nullptr;
  return godot::Object::cast_to<godot::TextEdit>(base->get_base_editor());
}

std::string script_resource_path(const godot::Ref<godot::Script> &script) {
  return script.is_valid() ? util::to_std(script->get_path()) : std::string();
}

godot_autopilot::util::JsonValue scene_root_summary(godot::Node *root,
                                  const godot::PackedStringArray &unsaved) {
  godot_autopilot::util::JsonValue summary(godot_autopilot::util::JsonValue::object_tag);
  summary["name"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
  summary["type"] = godot_autopilot::util::JsonValue(util::to_std(root->get_class()));
  const godot::String scene_path = root->get_scene_file_path();
  summary["scene_file_path"] = godot_autopilot::util::JsonValue(util::to_std(scene_path));
  summary["unsaved"] = godot_autopilot::util::JsonValue(path_list_contains(unsaved, scene_path));
  return summary;
}

godot_autopilot::util::JsonValue playing_state_json(godot::EditorInterface *editor) {
  godot_autopilot::util::JsonValue state(godot_autopilot::util::JsonValue::object_tag);
  state["is_playing"] = godot_autopilot::util::JsonValue(editor->is_playing_scene());
  state["playing_scene"] =
      godot_autopilot::util::JsonValue(util::to_std(editor->get_playing_scene()));
  return state;
}

godot_autopilot::util::JsonValue edited_object_summary(godot::Object *object) {
  if (!object)
    return godot_autopilot::util::JsonValue(nullptr);
  godot_autopilot::util::JsonValue summary(godot_autopilot::util::JsonValue::object_tag);
  summary["class"] = godot_autopilot::util::JsonValue(util::to_std(object->get_class()));
  if (auto *node = godot::Object::cast_to<godot::Node>(object)) {
    summary["name"] = godot_autopilot::util::JsonValue(util::to_std(node->get_name()));
    const std::string scene_path = util::to_std(node->get_scene_file_path());
    if (!scene_path.empty())
      summary["scene_file_path"] = godot_autopilot::util::JsonValue(scene_path);
  } else if (auto *resource =
                 godot::Object::cast_to<godot::Resource>(object)) {
    const std::string resource_path = util::to_std(resource->get_path());
    if (!resource_path.empty())
      summary["resource_path"] = godot_autopilot::util::JsonValue(resource_path);
  }
  return summary;
}

} // namespace

godot_autopilot::util::JsonValue handle_editor_state(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_state called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing("selection|root|open|unsaved|playing|info");
  }
  if (action == "selection") {
    return handle_get_selection(args);
  }
  if (action == "root") {
    return handle_get_edited_scene_root(args);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
    error["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return error;
  }
  if (action == "open") {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_editor_state open completed");
    return editor_open_scenes_result(editor);
  }
  if (action == "unsaved") {
    godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
    godot_autopilot::util::JsonValue array(godot_autopilot::util::JsonValue::array_tag);
    for (int i = 0; i < unsaved.size(); i++) {
      array.PushBack(godot_autopilot::util::JsonValue(util::to_std(unsaved[i])));
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(array);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_editor_state unsaved completed");
    return result;
  }
  if (action == "playing") {
    godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
    inner["is_playing"] = godot_autopilot::util::JsonValue(editor->is_playing_scene());
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(inner);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_editor_state playing completed");
    return result;
  }
  if (action == "info") {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_editor_state info completed");
    return editor_info_result(editor);
  }
  return editor_action_invalid("selection|root|open|unsaved|playing|info");
}

godot_autopilot::util::JsonValue handle_editor_files(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_files called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing("tree|status|scan");
  }
  if (action == "tree") {
    return handle_file_system_get_resources(args);
  }
  if (action == "status") {
    return handle_get_resource_filesystem(args);
  }
  if (action == "scan") {
    return handle_file_system_scan(args);
  }
  return editor_action_invalid("tree|status|scan");
}

godot_autopilot::util::JsonValue handle_editor_scene_control(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_scene_control called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing("create|open|close|reload|play|stop");
  }
  if (action == "create") {
    return handle_new_scene(args);
  }
  if (action == "open") {
    return handle_open_scene(args);
  }
  if (action == "close") {
    return handle_close_scene(args);
  }
  if (action == "reload") {
    return handle_reload_scene(args);
  }
  if (action == "play") {
    return handle_play_current_scene(args);
  }
  if (action == "stop") {
    return handle_stop_playing(args);
  }
  return editor_action_invalid("create|open|close|reload|play|stop");
}

godot_autopilot::util::JsonValue handle_editor_save(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene called");
  std::string action = editor_action_text(args);
  if (action.empty()) {
    action = "save";
  }
  if (action == "save") {
    return handle_save_scene(args);
  }
  if (action == "save_as") {
    return handle_save_scene_as(args);
  }
  if (action == "save_all") {
    return handle_save_all_scenes(args);
  }
  return editor_action_invalid("save|save_as|save_all");
}

godot_autopilot::util::JsonValue handle_editor_selection_set(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_selection_set called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing("set|select_tree");
  }
  if (action == "set") {
    return handle_set_selection(args);
  }
  if (action == "select_tree") {
    return editor_ui_ops::handle_select_scene_tree_node(args);
  }
  return editor_action_invalid("set|select_tree");
}

godot_autopilot::util::JsonValue handle_editor_undo(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_undo called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing(
        "create|add_do|add_undo|commit|undo|redo|history|clear");
  }
  if (action == "create") {
    return handle_undo_redo_start(args);
  }
  if (action == "commit") {
    return handle_undo_redo_commit(args);
  }
  if (action == "add_do" || action == "add_undo") {
    if (auto *side = args.Find("side")) {
      if (!side->IsString()) {
        godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
        error["error"] =
            godot_autopilot::util::JsonValue("invalid parameter: side must be 'do' or 'undo'");
        return error;
      }
      const std::string expected = action == "add_do" ? "do" : "undo";
      if (side->GetString() != expected) {
        godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
        error["error"] = godot_autopilot::util::JsonValue(
            "side '" + side->GetString() + "' does not match action '" +
            action + "'");
        return error;
      }
    }
    if (action == "add_do") {
      return handle_undo_redo_add_do(args);
    }
    return handle_undo_redo_add_undo(args);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  auto *manager = editor->get_editor_undo_redo();
  if (!manager) {
    return util::error_json("EditorUndoRedoManager not available");
  }
  if (action == "history") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    attach_undo_history(result, manager);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_undo history completed");
    return result;
  }
  godot::UndoRedo *history = manager->get_history_undo_redo(
      godot::EditorUndoRedoManager::GLOBAL_HISTORY);
  if (!history) {
    return util::error_json(
        "global undo history is not available — create an undo action first");
  }
  if (action == "undo" || action == "redo") {
    const bool is_undo = action == "undo";
    const bool performed = is_undo ? history->undo() : history->redo();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue(
        performed ? (is_undo ? "undone" : "redone")
                  : (is_undo ? "nothing_to_undo" : "nothing_to_redo"));
    result["performed"] = godot_autopilot::util::JsonValue(performed);
    attach_undo_history(result, manager);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              is_undo ? "editor_undo undo completed"
                                      : "editor_undo redo completed");
    return result;
  }
  if (action == "clear") {
    bool increase_version = true;
    if (auto *iv = args.Find("increase_version")) {
      if (!iv->IsBool()) {
        return util::error_json(
            "invalid parameter: increase_version must be a boolean");
      }
      increase_version = iv->GetBool();
    }
    manager->clear_history(godot::EditorUndoRedoManager::GLOBAL_HISTORY,
                           increase_version);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("cleared");
    result["increase_version"] = godot_autopilot::util::JsonValue(increase_version);
    attach_undo_history(result, manager);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_undo clear completed");
    return result;
  }
  return editor_action_invalid(
      "create|add_do|add_undo|commit|undo|redo|history|clear");
}

godot_autopilot::util::JsonValue handle_editor_config(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_config called");
  const std::string action = editor_action_text(args);
  if (action.empty()) {
    return editor_action_missing("main_scene|plugin_enable");
  }
  if (action == "main_scene") {
    return handle_set_main_scene(args);
  }
  if (action == "plugin_enable") {
    return handle_set_plugin_enabled(args);
  }
  return editor_action_invalid("main_scene|plugin_enable");
}

godot_autopilot::util::JsonValue handle_script_editor(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "script_editor called");
  const std::string action = editor_action_text(args);
  const char *choices =
      "open|goto|list_open|current|unsaved|save_all|reload_open|close";
  if (action.empty()) {
    return editor_action_missing(choices);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  godot::ScriptEditor *script_editor = editor->get_script_editor();
  if (!script_editor) {
    return util::error_json("ScriptEditor not available");
  }
  if (action == "open") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    int64_t line = -1;
    if (auto *lp = args.Find("line")) {
      if (!lp->IsInt()) {
        return util::error_json(
            "invalid parameter: line must be an integer (1-based; -1 keeps the "
            "current caret)");
      }
      line = lp->GetInt();
    }
    int64_t column = 0;
    if (auto *cp = args.Find("column")) {
      if (!cp->IsInt()) {
        return util::error_json(
            "invalid parameter: column must be a non-negative integer");
      }
      column = cp->GetInt();
    }
    bool grab_focus = true;
    if (auto *gp = args.Find("grab_focus")) {
      if (!gp->IsBool()) {
        return util::error_json(
            "invalid parameter: grab_focus must be a boolean");
      }
      grab_focus = gp->GetBool();
    }
    godot::Ref<godot::Script> script = load_script_resource(path);
    if (script.is_null()) {
      return util::error_json(
          "failed to load script: " + path +
          " — check the res:// path and that the file is a Script resource");
    }
    editor->edit_script(script, static_cast<int32_t>(line),
                        static_cast<int32_t>(column), grab_focus);
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(path);
    info["line"] = godot_autopilot::util::JsonValue(line);
    info["column"] = godot_autopilot::util::JsonValue(column);
    info["grab_focus"] = godot_autopilot::util::JsonValue(grab_focus);
    if (auto *text_edit = current_script_text_edit(script_editor)) {
      info["caret_line"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(text_edit->get_caret_line()) + 1);
      info["caret_column"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(text_edit->get_caret_column()) + 1);
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "script_editor open completed: " + path);
    return result;
  }
  if (action == "goto") {
    auto *lp = args.Find("line");
    if (!lp || !lp->IsInt() || lp->GetInt() < 1) {
      return util::error_json(
          "missing required parameter: line (1-based integer)");
    }
    const int64_t line = lp->GetInt();
    std::string path;
    if (auto *pp = args.Find("path")) {
      if (!pp->IsString()) {
        return util::error_json("invalid parameter: path must be a string");
      }
      path = pp->GetString();
    }
    if (!path.empty()) {
      godot::Ref<godot::Script> script = load_script_resource(path);
      if (script.is_null()) {
        return util::error_json("failed to load script: " + path);
      }
      editor->edit_script(script, static_cast<int32_t>(line), 0, true);
    } else {
      godot::Ref<godot::Script> current = script_editor->get_current_script();
      if (current.is_null()) {
        return util::error_json(
            "no script is open in the script editor — pass path to open one");
      }
      script_editor->goto_line(static_cast<int32_t>(line - 1));
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(
        path.empty()
            ? script_resource_path(script_editor->get_current_script())
            : path);
    info["line"] = godot_autopilot::util::JsonValue(line);
    info["via"] = godot_autopilot::util::JsonValue(path.empty() ? "goto_line" : "edit_script");
    if (auto *text_edit = current_script_text_edit(script_editor)) {
      info["caret_line"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(text_edit->get_caret_line()) + 1);
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "script_editor goto completed");
    return result;
  }
  if (action == "list_open") {
    godot::TypedArray<godot::Script> scripts =
        script_editor->get_open_scripts();
    godot::PackedStringArray unsaved = script_editor->get_unsaved_files();
    const std::string current_path =
        script_resource_path(script_editor->get_current_script());
    godot_autopilot::util::JsonValue array(godot_autopilot::util::JsonValue::array_tag);
    for (int i = 0; i < scripts.size(); i++) {
      auto *script = godot::Object::cast_to<godot::Script>(scripts[i]);
      if (!script)
        continue;
      godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
      const std::string path = util::to_std(script->get_path());
      item["path"] = godot_autopilot::util::JsonValue(path);
      item["unsaved"] = godot_autopilot::util::JsonValue(
          path_list_contains(unsaved, script->get_path()));
      item["current"] = godot_autopilot::util::JsonValue(!path.empty() &&
                                      path == current_path);
      array.PushBack(std::move(item));
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(array);
    result["count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(scripts.size()));
    return result;
  }
  if (action == "current") {
    godot::Ref<godot::Script> current = script_editor->get_current_script();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    if (current.is_null()) {
      result["result"] = godot_autopilot::util::JsonValue(nullptr);
      return result;
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(util::to_std(current->get_path()));
    info["unsaved"] = godot_autopilot::util::JsonValue(path_list_contains(
        script_editor->get_unsaved_files(), current->get_path()));
    info["is_tool"] = godot_autopilot::util::JsonValue(current->is_tool());
    if (auto *text_edit = current_script_text_edit(script_editor)) {
      info["caret_line"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(text_edit->get_caret_line()) + 1);
      info["caret_column"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(text_edit->get_caret_column()) + 1);
    }
    result["result"] = std::move(info);
    return result;
  }
  if (action == "unsaved") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    godot::PackedStringArray unsaved = script_editor->get_unsaved_files();
    result["result"] = packed_string_array_json(unsaved);
    result["count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(unsaved.size()));
    return result;
  }
  if (action == "save_all") {
    godot::PackedStringArray unsaved_before =
        script_editor->get_unsaved_files();
    script_editor->save_all_scripts();
    godot::PackedStringArray unsaved_after =
        script_editor->get_unsaved_files();
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["saved_count"] =
        godot_autopilot::util::JsonValue(static_cast<int64_t>(unsaved_before.size()));
    info["saved_paths"] = packed_string_array_json(unsaved_before);
    info["unsaved_after"] = packed_string_array_json(unsaved_after);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "script_editor save_all completed");
    return result;
  }
  if (action == "reload_open") {
    script_editor->reload_open_files();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "script_editor reload_open completed");
    return result;
  }
  if (action == "close") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    godot::Error err = script_editor->close_file(godot::String(path.c_str()));
    if (err != godot::Error::OK) {
      godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
      error["error"] = godot_autopilot::util::JsonValue(
          editor_error_text("failed to close script", err) + ": " + path);
      error["error_code"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(err));
      error["error_name"] = godot_autopilot::util::JsonValue(editor_error_name(err));
      error["hint"] = godot_autopilot::util::JsonValue(editor_error_hint(err));
      return error;
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(path);
    info["closed"] = godot_autopilot::util::JsonValue(true);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "script_editor close completed: " + path);
    return result;
  }
  return editor_action_invalid(choices);
}

godot_autopilot::util::JsonValue handle_editor_session(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_session called");
  const std::string action = editor_action_text(args);
  const char *choices = "get|mark_unsaved|play_main|play_custom";
  if (action.empty()) {
    return editor_action_missing(choices);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  if (action == "get") {
    godot::TypedArray<godot::Node> roots = editor->get_open_scene_roots();
    godot::PackedStringArray unsaved = editor->get_unsaved_scenes();
    godot_autopilot::util::JsonValue root_array(godot_autopilot::util::JsonValue::array_tag);
    for (int i = 0; i < roots.size(); i++) {
      auto *root = godot::Object::cast_to<godot::Node>(roots[i]);
      if (!root)
        continue;
      root_array.PushBack(scene_root_summary(root, unsaved));
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["open_scene_roots"] = std::move(root_array);
    info["open_scene_count"] =
        godot_autopilot::util::JsonValue(static_cast<int64_t>(roots.size()));
    info["unsaved_scenes"] = packed_string_array_json(unsaved);
    auto *edited_root = editor->get_edited_scene_root();
    if (edited_root) {
      info["edited_root"] = scene_root_summary(edited_root, unsaved);
    } else {
      info["edited_root"] = godot_autopilot::util::JsonValue(nullptr);
    }
    info["playing"] = playing_state_json(editor);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_session get completed");
    return result;
  }
  if (action == "mark_unsaved") {
    editor->mark_scene_as_unsaved();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    result["unsaved_scenes"] =
        packed_string_array_json(editor->get_unsaved_scenes());
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_session mark_unsaved completed");
    return result;
  }
  if (action == "play_main") {
    if (editor->is_playing_scene()) {
      godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
      result["result"] = godot_autopilot::util::JsonValue("already_playing");
      result["playing"] = playing_state_json(editor);
      return result;
    }
    editor->play_main_scene();
    if (!editor->is_playing_scene()) {
      return util::error_json(
          "failed to start main scene playback — set "
          "application/run/main_scene and make sure the scene is runnable");
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    result["playing"] = playing_state_json(editor);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_session play_main completed");
    return result;
  }
  if (action == "play_custom") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    if (editor->is_playing_scene()) {
      godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
      result["result"] = godot_autopilot::util::JsonValue("already_playing");
      result["playing"] = playing_state_json(editor);
      return result;
    }
    editor->play_custom_scene(godot::String(path.c_str()));
    if (!editor->is_playing_scene()) {
      return util::error_json(
          "failed to start custom scene playback: " + path +
          " — verify the scene exists and is runnable");
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    result["path"] = godot_autopilot::util::JsonValue(path);
    result["playing"] = playing_state_json(editor);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_session play_custom completed");
    return result;
  }
  return editor_action_invalid(choices);
}

godot_autopilot::util::JsonValue handle_editor_inspect(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_inspect called");
  const std::string action = editor_action_text(args);
  const char *choices =
      "inspect_object|edit_resource|edit_node|selected_path|expand_all|"
      "collapse_all";
  if (action.empty()) {
    return editor_action_missing(choices);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  if (action == "inspect_object") {
    std::string for_property;
    if (auto *fp = args.Find("for_property")) {
      if (!fp->IsString()) {
        return util::error_json(
            "invalid parameter: for_property must be a string");
      }
      for_property = fp->GetString();
    }
    bool inspector_only = false;
    if (auto *ip = args.Find("inspector_only")) {
      if (!ip->IsBool()) {
        return util::error_json(
            "invalid parameter: inspector_only must be a boolean");
      }
      inspector_only = ip->GetBool();
    }
    godot::Object *object = nullptr;
    std::string kind;
    std::string target;
    if (auto *rp = args.Find("resource_path")) {
      if (!rp->IsString()) {
        return util::error_json(
            "invalid parameter: resource_path must be a string");
      }
      const std::string path = rp->GetString();
      auto *loader = godot::ResourceLoader::get_singleton();
      if (!loader) {
        return util::error_json("ResourceLoader not available");
      }
      godot::Ref<godot::Resource> resource =
          loader->load(godot::String(path.c_str()));
      if (resource.is_null()) {
        return util::error_json("failed to load resource: " + path);
      }
      object = resource.ptr();
      kind = "resource";
      target = path;
    } else if (auto *np = args.Find("node_path")) {
      if (!np->IsString()) {
        return util::error_json(
            "invalid parameter: node_path must be a string");
      }
      const std::string path = np->GetString();
      std::string hint;
      object = util::resolve_scene_node(path, editor->get_edited_scene_root(),
                                        &hint);
      if (!object) {
        return util::error_json("node not found: " + path + " — " + hint);
      }
      kind = "node";
      target = path;
    } else {
      return util::error_json(
          "missing required parameter: resource_path or node_path");
    }
    editor->inspect_object(object, godot::String(for_property.c_str()),
                           inspector_only);
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["kind"] = godot_autopilot::util::JsonValue(kind);
    info["target"] = godot_autopilot::util::JsonValue(target);
    info["for_property"] = godot_autopilot::util::JsonValue(for_property);
    info["inspector_only"] = godot_autopilot::util::JsonValue(inspector_only);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    return result;
  }
  if (action == "edit_resource") {
    auto *rp = args.Find("resource_path");
    if (!rp || !rp->IsString() || rp->GetString().empty()) {
      return util::error_json("missing required parameter: resource_path");
    }
    const std::string path = rp->GetString();
    auto *loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
      return util::error_json("ResourceLoader not available");
    }
    godot::Ref<godot::Resource> resource =
        loader->load(godot::String(path.c_str()));
    if (resource.is_null()) {
      return util::error_json("failed to load resource: " + path);
    }
    editor->edit_resource(resource);
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["resource_path"] = godot_autopilot::util::JsonValue(path);
    info["resource_type"] =
        godot_autopilot::util::JsonValue(util::to_std(resource->get_class()));
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_inspect edit_resource completed: " + path);
    return result;
  }
  if (action == "edit_node") {
    auto *np = args.Find("node_path");
    if (!np || !np->IsString() || np->GetString().empty()) {
      return util::error_json("missing required parameter: node_path");
    }
    const std::string path = np->GetString();
    std::string hint;
    auto *node = util::resolve_scene_node(path, editor->get_edited_scene_root(),
                                          &hint);
    if (!node) {
      return util::error_json("node not found: " + path + " — " + hint);
    }
    editor->edit_node(node);
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["node_path"] = godot_autopilot::util::JsonValue(path);
    info["name"] = godot_autopilot::util::JsonValue(util::to_std(node->get_name()));
    info["type"] = godot_autopilot::util::JsonValue(util::to_std(node->get_class()));
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_inspect edit_node completed: " + path);
    return result;
  }
  auto *inspector = editor->get_inspector();
  if (!inspector) {
    return util::error_json("EditorInspector not available");
  }
  if (action == "selected_path") {
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["selected_path"] =
        godot_autopilot::util::JsonValue(util::to_std(inspector->get_selected_path()));
    info["edited_object"] =
        edited_object_summary(inspector->get_edited_object());
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    return result;
  }
  if (action == "expand_all") {
    inspector->expand_all_folding();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    return result;
  }
  if (action == "collapse_all") {
    inspector->collapse_all_folding();
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue("ok");
    return result;
  }
  return editor_action_invalid(choices);
}

godot_autopilot::util::JsonValue handle_editor_files_panel(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_files called");
  const std::string action = editor_action_text(args);
  const char *choices =
      "select|selected_paths|current_path|current_dir|navigate|file_meta";
  if (action.empty()) {
    return editor_action_missing(choices);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  if (action == "select") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    editor->select_file(godot::String(path.c_str()));
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(path);
    info["selected_paths"] =
        packed_string_array_json(editor->get_selected_paths());
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_files select completed: " + path);
    return result;
  }
  if (action == "selected_paths") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] =
        packed_string_array_json(editor->get_selected_paths());
    return result;
  }
  if (action == "current_path") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] =
        godot_autopilot::util::JsonValue(util::to_std(editor->get_current_path()));
    return result;
  }
  if (action == "current_dir") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] =
        godot_autopilot::util::JsonValue(util::to_std(editor->get_current_directory()));
    return result;
  }
  if (action == "navigate") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    auto *dock = editor->get_file_system_dock();
    if (!dock) {
      return util::error_json("FileSystemDock not available");
    }
    dock->navigate_to_path(godot::String(path.c_str()));
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(path);
    info["current_dir"] =
        godot_autopilot::util::JsonValue(util::to_std(editor->get_current_directory()));
    info["current_path"] =
        godot_autopilot::util::JsonValue(util::to_std(editor->get_current_path()));
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_files navigate completed: " + path);
    return result;
  }
  if (action == "file_meta") {
    auto *pp = args.Find("path");
    if (!pp || !pp->IsString() || pp->GetString().empty()) {
      return util::error_json("missing required parameter: path");
    }
    const std::string path = pp->GetString();
    const godot::String res_path(path.c_str());
    if (res_path.get_file().is_empty()) {
      return util::error_json(
          "invalid parameter: path must point to a file, e.g. "
          "'res://scripts/player.gd'");
    }
    auto *efs = editor->get_resource_filesystem();
    if (!efs) {
      return util::error_json("EditorFileSystem not available");
    }
    auto *dir = efs->get_filesystem_path(res_path.get_base_dir());
    if (!dir) {
      return util::error_json(
          "directory not found in the editor file system: " +
          util::to_std(res_path.get_base_dir()) +
          " — run get_editor_files action=scan and retry");
    }
    const int index = dir->find_file_index(res_path.get_file());
    if (index < 0) {
      return util::error_json(
          "file not found in the editor file system: " + path +
          " — the file may be new or the scan may be stale; run "
          "get_editor_files action=scan and retry");
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["path"] = godot_autopilot::util::JsonValue(path);
    info["type"] = godot_autopilot::util::JsonValue(util::to_std(
        godot::String(dir->get_file_type(index))));
    info["script_class"] =
        godot_autopilot::util::JsonValue(util::to_std(dir->get_file_script_class_name(index)));
    info["extends"] =
        godot_autopilot::util::JsonValue(util::to_std(dir->get_file_script_class_extends(index)));
    info["import_valid"] = godot_autopilot::util::JsonValue(dir->get_file_import_is_valid(index));
    info["scanning"] = godot_autopilot::util::JsonValue(efs->is_scanning());
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    return result;
  }
  return editor_action_invalid(choices);
}

godot_autopilot::util::JsonValue handle_editor_info(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "editor_info called");
  const std::string action = editor_action_text(args);
  const char *choices =
      "theme|scale|language|paths|multi_window|distraction_free|toast";
  if (action.empty()) {
    return editor_action_missing(choices);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  if (action == "theme") {
    godot::Ref<godot::Theme> theme_ref = editor->get_editor_theme();
    godot::Theme *theme = theme_ref.ptr();
    if (!theme) {
      return util::error_json("editor theme is not available");
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["type"] = godot_autopilot::util::JsonValue(util::to_std(theme->get_class()));
    info["has_base_scale"] = godot_autopilot::util::JsonValue(theme->has_default_base_scale());
    if (theme->has_default_base_scale()) {
      info["base_scale"] = godot_autopilot::util::JsonValue(theme->get_default_base_scale());
    }
    info["has_default_font"] = godot_autopilot::util::JsonValue(theme->has_default_font());
    info["default_font_size"] =
        godot_autopilot::util::JsonValue(static_cast<int64_t>(theme->get_default_font_size()));
    info["type_count"] = godot_autopilot::util::JsonValue(
        static_cast<int64_t>(theme->get_type_list().size()));
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    return result;
  }
  if (action == "scale") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue(editor->get_editor_scale());
    return result;
  }
  if (action == "language") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue(util::to_std(editor->get_editor_language()));
    return result;
  }
  if (action == "paths") {
    godot::EditorPaths *paths = editor->get_editor_paths();
    if (!paths) {
      return util::error_json("EditorPaths not available");
    }
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["data_dir"] = godot_autopilot::util::JsonValue(util::to_std(paths->get_data_dir()));
    info["config_dir"] = godot_autopilot::util::JsonValue(util::to_std(paths->get_config_dir()));
    info["cache_dir"] = godot_autopilot::util::JsonValue(util::to_std(paths->get_cache_dir()));
    info["project_settings_dir"] =
        godot_autopilot::util::JsonValue(util::to_std(paths->get_project_settings_dir()));
    info["self_contained"] = godot_autopilot::util::JsonValue(paths->is_self_contained());
    if (paths->is_self_contained()) {
      info["self_contained_file"] =
          godot_autopilot::util::JsonValue(util::to_std(paths->get_self_contained_file()));
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    return result;
  }
  if (action == "multi_window") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = godot_autopilot::util::JsonValue(editor->is_multi_window_enabled());
    return result;
  }
  if (action == "distraction_free") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] =
        godot_autopilot::util::JsonValue(editor->is_distraction_free_mode_enabled());
    return result;
  }
  if (action == "toast") {
    auto *mp = args.Find("message");
    if (!mp || !mp->IsString() || mp->GetString().empty()) {
      return util::error_json("missing required parameter: message");
    }
    const std::string message = mp->GetString();
    std::string tooltip;
    if (auto *tp = args.Find("tooltip")) {
      if (!tp->IsString()) {
        return util::error_json("invalid parameter: tooltip must be a string");
      }
      tooltip = tp->GetString();
    }
    std::string severity_name = "info";
    if (auto *sp = args.Find("severity")) {
      if (!sp->IsString()) {
        return util::error_json("invalid parameter: severity must be a string");
      }
      severity_name = sp->GetString();
    }
    godot::EditorToaster::Severity severity =
        godot::EditorToaster::SEVERITY_INFO;
    if (severity_name == "info") {
      severity = godot::EditorToaster::SEVERITY_INFO;
    } else if (severity_name == "warning") {
      severity = godot::EditorToaster::SEVERITY_WARNING;
    } else if (severity_name == "error") {
      severity = static_cast<godot::EditorToaster::Severity>(2);
    } else {
      return util::error_json(
          "invalid severity '" + severity_name +
          "': expected info, warning or error");
    }
    auto *toaster = editor->get_editor_toaster();
    if (!toaster) {
      return util::error_json("EditorToaster not available");
    }
    toaster->push_toast(godot::String(message.c_str()), severity,
                        godot::String(tooltip.c_str()));
    godot_autopilot::util::JsonValue info(godot_autopilot::util::JsonValue::object_tag);
    info["message"] = godot_autopilot::util::JsonValue(message);
    info["severity"] = godot_autopilot::util::JsonValue(severity_name);
    info["tooltip"] = godot_autopilot::util::JsonValue(tooltip);
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "editor_info toast completed");
    return result;
  }
  return editor_action_invalid(choices);
}

} // namespace editor_ops
} // namespace godot_autopilot
