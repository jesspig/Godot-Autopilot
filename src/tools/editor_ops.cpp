#include "editor_ops.hpp"
#include "../runtime/gda_protocol.hpp"
#include "../util/scene_path.hpp"
#include "../util/scene_verify.hpp"
#include "core/editor_readiness.hpp"
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
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace godot_autopilot {
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

// Error 枚举→名映射（仅 editor_ops 内静态函数，不碰 util/error_util 以免冲突）。
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

// 数字码配套的可操作提示（保留数字码，仅改善文本，不改行为）。
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

bool save_receipt_flag(const mcp::JsonValue &args, const char *name) {
  auto *p = args.Find(name);
  return p && p->IsBool() && p->GetBool();
}

std::string hash_hex(uint64_t value) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(value));
  return std::string(buf);
}

// 保存成功回执：默认轻量计数（memory_nodes/disk_nodes/nodes_match），
// 失配或 include_paths 时附 missing_paths，include_hash 时附哈希。
// 只增字段，不改旧字段；磁盘重读失败只附 verify_error，不推翻 saved。
void attach_save_receipt(mcp::JsonValue &r, const mcp::JsonValue &args,
                         godot::Node *root, const std::string &res_path) {
  if (!root || res_path.empty())
    return;
  bool disk_ok = false;
  const std::string text = scene_verify::read_text_file(res_path, disk_ok);
  if (!disk_ok) {
    r["verify_error"] = mcp::JsonValue(
        "saved, but the on-disk file could not be re-read for verification: " +
        res_path);
    return;
  }
  const bool want_hash = save_receipt_flag(args, "include_hash");
  const bool want_paths = save_receipt_flag(args, "include_paths");
  const scene_verify::VerifyResult vr =
      scene_verify::compare_tree_with_text(root, text, want_hash);
  r["memory_nodes"] = mcp::JsonValue(vr.memory_nodes);
  r["disk_nodes"] = mcp::JsonValue(vr.disk_nodes);
  r["nodes_match"] = mcp::JsonValue(vr.match);
  if (!vr.match || want_paths) {
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (const std::string &p : vr.missing_paths)
      arr.PushBack(mcp::JsonValue(p));
    r["missing_paths"] = std::move(arr);
    if (vr.missing_truncated)
      r["missing_truncated"] = mcp::JsonValue(true);
  }
  if (want_hash && vr.hash_computed) {
    r["memory_hash"] = mcp::JsonValue(hash_hex(vr.memory_hash));
    r["disk_hash"] = mcp::JsonValue(hash_hex(vr.disk_hash));
  }
  if (!vr.match && r.Find("warning") == nullptr) {
    r["warning"] = mcp::JsonValue(
        "memory/disk node mismatch after save (memory " +
        std::to_string(vr.memory_nodes) + " vs disk " +
        std::to_string(vr.disk_nodes) +
        ") — inspect missing_paths, then run verify_scene_saved; if nodes were "
        "moved with remove_child/add_child, use reparent_node so owners persist");
  }
}

std::string tree_to_lower(const std::string &s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool tree_entry_matches(const std::string &name, const std::string &path,
                        const std::string &filter_lower) {
  if (filter_lower.empty())
    return true;
  return tree_to_lower(name).find(filter_lower) != std::string::npos ||
         tree_to_lower(path).find(filter_lower) != std::string::npos;
}

bool dir_to_json(godot::EditorFileSystemDirectory *dir, mcp::JsonValue &j,
                 int depth, int max_depth, const std::string &filter_lower,
                 bool &out_filtered) {
  if (!dir)
    return false;
  j["name"] = mcp::JsonValue(util::to_std(dir->get_name()));
  j["path"] = mcp::JsonValue(util::to_std(dir->get_path()));
  j["type"] = mcp::JsonValue("directory");
  mcp::JsonValue children_arr(mcp::JsonValue::array_tag);
  bool truncated = false;
  if (depth < max_depth) {
    for (int i = 0; i < dir->get_subdir_count(); i++) {
      auto *sub = dir->get_subdir(i);
      if (!sub)
        continue;
      mcp::JsonValue child(mcp::JsonValue::object_tag);
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
      mcp::JsonValue file(mcp::JsonValue::object_tag);
      file["name"] = mcp::JsonValue(fname);
      file["path"] = mcp::JsonValue(fpath);
      godot::StringName sn = dir->get_file_type(i);
      file["type"] = mcp::JsonValue(util::to_std(godot::String(sn)));
      file["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
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

mcp::JsonValue editor_scene_state_json(godot::EditorInterface *editor) {
  mcp::JsonValue state(mcp::JsonValue::object_tag);
  auto *root = editor->get_edited_scene_root();
  if (root) {
    mcp::JsonValue root_info(mcp::JsonValue::object_tag);
    root_info["name"] = mcp::JsonValue(util::to_std(root->get_name()));
    std::string root_path = util::to_std(root->get_scene_file_path());
    if (!root_path.empty())
      root_info["scene_file_path"] = mcp::JsonValue(root_path);
    state["edited_root"] = std::move(root_info);
  } else {
    state["edited_root"] = mcp::JsonValue(nullptr);
  }
  godot::PackedStringArray unsaved = editor_unsaved_scenes(editor);
  mcp::JsonValue unsaved_arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < unsaved.size(); i++)
    unsaved_arr.PushBack(mcp::JsonValue(util::to_std(unsaved[i])));
  state["unsaved_scenes"] = std::move(unsaved_arr);
  auto *efs = editor->get_resource_filesystem();
  if (efs) {
    mcp::JsonValue fs(mcp::JsonValue::object_tag);
    fs["scanning"] = mcp::JsonValue(efs->is_scanning());
    fs["progress"] = mcp::JsonValue(efs->get_scanning_progress());
    state["file_system"] = std::move(fs);
  } else {
    state["file_system"] = mcp::JsonValue(nullptr);
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

} // namespace

mcp::JsonValue handle_get_selection(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_selection called");
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
    item["name"] = mcp::JsonValue(util::to_std(node->get_name()));
    item["class"] = mcp::JsonValue(util::to_std(node->get_class()));
    item["path"] = mcp::JsonValue(root ? relative_path(node, root)
                                       : util::to_std(node->get_name()));
    result_arr.PushBack(std::move(item));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_selection completed");
  return r;
}

mcp::JsonValue handle_set_selection(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_selection called");
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
      auto *node = godot_autopilot::util::resolve_scene_node(p.GetString(),
                                                                root, &hint);
      if (node) {
        sel->add_node(node);
      }
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_selection completed");
  return r;
}

mcp::JsonValue handle_get_edited_scene_root(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_edited_scene_root called");
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
  j["name"] = mcp::JsonValue(util::to_std(root->get_name()));
  j["type"] = mcp::JsonValue(util::to_std(root->get_class()));
  j["path"] = mcp::JsonValue(util::to_std(root->get_name()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_edited_scene_root completed");
  return r;
}

mcp::JsonValue handle_save_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene called");
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
      std::string root_name = util::to_std(root->get_name());
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
                           "save_editor_scene_as with an explicit path");
        LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                                  "save_editor_scene failed to save as " +
                                      generated_path);
        return e;
      }
      r["path"] = mcp::JsonValue(generated_path);
      r["result"] = mcp::JsonValue("saved");
      r["note"] =
          mcp::JsonValue("scene had no file path — saved to " + generated_path +
                         "; use save_editor_scene_as to choose a directory");
      attach_save_receipt(r, args, root, generated_path);
      scene_dirty_tracker::clear_scene_modified();
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "save_editor_scene completed (saved as new scene)");
      return r;
    }
    std::string failed_path = util::to_std(existing_path);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(editor_error_text("save failed", err) + ": " +
                                failed_path);
    e["path"] = mcp::JsonValue(failed_path);
    e["error_code"] = mcp::JsonValue(static_cast<int64_t>(err));
    e["error_name"] = mcp::JsonValue(editor_error_name(err));
    e["hint"] = mcp::JsonValue(editor_error_hint(err));
    LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                              "save_editor_scene failed " +
                                  editor_error_text("", err) + ": " + failed_path);
    return e;
  } else {
    godot::String file_path = root->get_scene_file_path();
    const std::string saved_path = util::to_std(file_path);
    r["path"] = mcp::JsonValue(saved_path);
    r["result"] = mcp::JsonValue("saved");
    attach_save_receipt(r, args, root, saved_path);
    scene_dirty_tracker::clear_scene_modified();
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene completed");
  return r;
}

mcp::JsonValue handle_save_all_scenes(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scenes called");
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
                            "save_editor_scenes completed");
  return r;
}

mcp::JsonValue handle_reload_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_editor_scene called");
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
    scene_path = util::to_std(root->get_scene_file_path());
    if (scene_path.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("current scene has no file path, provide scene_path");
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
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("scene is not open: " + scene_path + "; reload did not run");
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "reload_editor_scene skipped, scene is not open: " +
                                  scene_path);
    return e;
  }
  editor->reload_scene_from_path(target_path);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("reloaded");
  r["scene_path"] = mcp::JsonValue(scene_path);
  auto *reloaded_root = editor->get_edited_scene_root();
  if (reloaded_root &&
      same_scene_path(reloaded_root->get_scene_file_path(), target_path)) {
    r["observed"] = mcp::JsonValue(true);
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "reload_editor_scene completed");
  return r;
}

mcp::JsonValue handle_inspect_object(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "inspect_editor_resource called");
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
                            "inspect_editor_resource completed");
  return r;
}

mcp::JsonValue handle_undo_redo_start(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_undo_redo_action called");
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
                            "create_editor_undo_redo_action completed");
  return r;
}

mcp::JsonValue handle_undo_redo_commit(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "commit_editor_undo_redo called");
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
                            "commit_editor_undo_redo completed");
  return r;
}

namespace {

enum class UndoRedoBranch {
  Do,
  Undo,
};

mcp::JsonValue handle_undo_redo_add_method(const mcp::JsonValue &args,
                                           UndoRedoBranch branch) {
  const bool is_do = branch == UndoRedoBranch::Do;
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            is_do ? "add_editor_undo_redo_do called"
                                  : "add_editor_undo_redo_undo called");
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
  auto *node = godot_autopilot::util::resolve_scene_node(
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
  if (is_do) {
    if (val && !val->IsNull()) {
      godot::Variant var = VariantJson::deserialize(*val);
      undo_redo->add_do_method(node, godot::StringName(method.c_str()), var);
    } else {
      undo_redo->add_do_method(node, godot::StringName(method.c_str()));
    }
  } else {
    if (val && !val->IsNull()) {
      godot::Variant var = VariantJson::deserialize(*val);
      undo_redo->add_undo_method(node, godot::StringName(method.c_str()), var);
    } else {
      undo_redo->add_undo_method(node, godot::StringName(method.c_str()));
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            is_do ? "add_editor_undo_redo_do completed"
                                  : "add_editor_undo_redo_undo completed");
  return r;
}

} // namespace

mcp::JsonValue handle_undo_redo_add_do(const mcp::JsonValue &args) {
  return handle_undo_redo_add_method(args, UndoRedoBranch::Do);
}

mcp::JsonValue handle_undo_redo_add_undo(const mcp::JsonValue &args) {
  return handle_undo_redo_add_method(args, UndoRedoBranch::Undo);
}

mcp::JsonValue handle_file_system_get_resources(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_tree called");
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
  // prefix 作子树根过滤（缺省全量兼容）：提供时优先于 path。
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
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(nullptr);
    return r;
  }
  int max_depth = MAX_TREE_DEPTH;
  auto *depth_p = args.Find("depth");
  if (depth_p && depth_p->IsInt()) {
    const int64_t requested = depth_p->GetInt();
    if (requested < 0) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("invalid parameter: depth must be >= 0");
      return e;
    }
    max_depth = requested > MAX_TREE_DEPTH
                    ? MAX_TREE_DEPTH
                    : static_cast<int>(requested);
  }
  std::string filter_lower;
  auto *filter_p = args.Find("filter");
  if (filter_p && filter_p->IsString())
    filter_lower = tree_to_lower(filter_p->GetString());
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  bool filtered = false;
  bool truncated = dir_to_json(dir, result, 0, max_depth, filter_lower, filtered);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  if (truncated) {
    r["truncated"] = mcp::JsonValue(true);
    r["max_depth"] = mcp::JsonValue(static_cast<int64_t>(max_depth));
  }
  if (filtered) {
    r["filtered"] = mcp::JsonValue(true);
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_tree completed");
  return r;
}

mcp::JsonValue handle_file_system_scan(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scan_editor_file_system called");
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
  if (efs->is_scanning()) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "scan_editor_file_system skipped: already scanning");
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["skipped"] = mcp::JsonValue(true);
    r["reason"] = mcp::JsonValue("already scanning");
    return r;
  }
  efs->scan();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "scan_editor_file_system completed");
  return r;
}

mcp::JsonValue handle_set_main_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_main_scene called");
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
                            "set_editor_main_scene completed");
  return r;
}

mcp::JsonValue handle_play_current_scene(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "play_editor_current_scene called");
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
                            "play_editor_current_scene completed");
  return r;
}

mcp::JsonValue handle_stop_playing(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_editor_playing called");
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
                            "stop_editor_playing completed");
  return r;
}

mcp::JsonValue handle_get_resource_filesystem(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_file_system_status called");
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
                            "get_editor_file_system_status completed");
  return r;
}

mcp::JsonValue handle_set_plugin_enabled(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_plugin_enabled called");
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
                            "set_editor_plugin_enabled completed");
  return r;
}

mcp::JsonValue handle_new_scene(const mcp::JsonValue &args) {
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
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(editor_error_text("failed to close scene", err));
      e["error_code"] = mcp::JsonValue(static_cast<int64_t>(err));
      e["error_name"] = mcp::JsonValue(editor_error_name(err));
      e["hint"] = mcp::JsonValue(editor_error_hint(err));
      return e;
    }
    closed_previous = true;
    scene_dirty_tracker::clear_scene_modified();
    // 多签：close_scene 仅关闭当前签；若仍有邻签占用，get_edited_scene_root()
    // 仍非空。此时不得进入 add_root_node + 轮询（会空转至超时），须秒级明确失败。
    auto *post_close_root = editor->get_edited_scene_root();
    if (post_close_root) {
      release_unclaimed_node(node, editor);
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
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

  int64_t waited_ms = 0;
  bool switched = false;
  while (waited_ms < timeout_ms) {
    if (editor->get_edited_scene_root() == node) {
      switched = true;
      break;
    }
    godot::OS::get_singleton()->delay_usec(GDA_NEW_SCENE_POLL_MS * 1000);
    waited_ms += GDA_NEW_SCENE_POLL_MS;
  }
  if (!switched) {
    bool released = release_unclaimed_node(node, editor);
    std::string message =
        "scene switch did not take effect within " + std::to_string(waited_ms) +
        " ms (create_editor_scene) — the new scene root was not observed after "
        "add_root_node (neighbor-tab occupation is rejected before add and "
        "never reaches this timeout; this timeout means the post-add switch "
        "itself was not observed)";
    if (released) {
      message += "; the unclaimed node was released";
    } else {
      message += "; the node is still attached to the editor and was not released";
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(message);
    e["waited_ms"] = mcp::JsonValue(waited_ms);
    e["timeout_ms"] = mcp::JsonValue(static_cast<int64_t>(timeout_ms));
    e["node_released"] = mcp::JsonValue(released);
    e["editor_state"] = editor_scene_state_json(editor);
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::Tools,
        "create_editor_scene timed out after " + std::to_string(waited_ms) + " ms");
    return e;
  }

  scene_dirty_tracker::clear_scene_modified();

  focus_main_screen_for_class(editor, godot::StringName(type.c_str()));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(name);
  inner["type"] = mcp::JsonValue(type);
  inner["closed_previous"] = mcp::JsonValue(closed_previous);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_editor_scene completed");
  return r;
}

mcp::JsonValue handle_open_scene(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_editor_scene called");
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
        " — save first (save_editor_scene), or discard the changes by calling "
        "reload_editor_scene with the listed path, then retry open_editor_scene");
    return e;
  }
  auto *old_root = editor->get_edited_scene_root();
  // 引擎对「已打开」场景只切标签（editor/editor_node.cpp:4966-4974），请求目标恰为
  // 当前场景时 _set_current_scene 直接 return（:4701-4704）→ root 指针不变；
  // 因此以 root 指针是否变化判定成败会把幂等调用误判为失败，须先比对路径。
  godot::String requested_path = godot::String::utf8(path.c_str());
  if (old_root != nullptr && !path.empty()) {
    godot::String open_path = old_root->get_scene_file_path();
    if (!open_path.is_empty() && same_scene_path(requested_path, open_path)) {
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = mcp::JsonValue("ok");
      r["path"] = mcp::JsonValue(path);
      r["already_open"] = mcp::JsonValue(true);
      LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                "open_editor_scene already open: " + path);
      return r;
    }
  }
  editor->open_scene_from_path(requested_path);
  auto *new_root = editor->get_edited_scene_root();
  if (new_root == old_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to open scene at " + path +
        " — edited scene did not change (check editor output log)");
    return e;
  }

  if (new_root) {
    focus_main_screen_for_class(editor,
                                godot::StringName(new_root->get_class()));
  }

  scene_dirty_tracker::clear_scene_modified();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["path"] = mcp::JsonValue(path);
  r["already_open"] = mcp::JsonValue(false);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "open_editor_scene completed");
  return r;
}

mcp::JsonValue handle_close_scene(const mcp::JsonValue &) {
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
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(editor_error_text("failed to close scene", err));
    e["error_code"] = mcp::JsonValue(static_cast<int64_t>(err));
    e["error_name"] = mcp::JsonValue(editor_error_name(err));
    e["hint"] = mcp::JsonValue(editor_error_hint(err));
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("closed");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "close_editor_scene completed");
  return r;
}

mcp::JsonValue handle_save_scene_as(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene_as called");
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString()) {
    return util::error_json("missing required parameter: path");
  }
  std::string path = pp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  auto *root = editor->get_edited_scene_root();
  if (!root) {
    return util::error_json("no scene open");
  }
  godot::String path_gs(path.c_str());
  if (path_gs.begins_with("res://")) {
    godot::String dir = path_gs.get_base_dir();
    if (!godot::DirAccess::dir_exists_absolute(dir)) {
      godot::Error err = godot::DirAccess::make_dir_recursive_absolute(dir);
      if (err != godot::Error::OK) {
        return util::error_json("failed to create parent directory: " + util::to_std(dir) +
                          " (error " + std::to_string(static_cast<int>(err)) +
                          ")");
      }
    }
  }
  editor->save_scene_as(path_gs);
  if (!godot::FileAccess::file_exists(path_gs)) {
    return util::error_json("save failed — file not created at " + path +
                      " (check editor output log)");
  }
  bool tab_path_registered = false;
  godot::PackedStringArray open_scenes = editor->get_open_scenes();
  for (int i = 0; i < open_scenes.size(); i++) {
    if (same_scene_path(open_scenes[i], path_gs)) {
      tab_path_registered = true;
      break;
    }
  }
  // 存盘后页签仍登记旧路径时必须 close+reopen 重建标签：GDExtension 无页签
  // 路径登记 API（EditorInterface 仅公开 open/close/save/get_open_scenes，无
  // EditorData::set_scene_path 等价接口；Node::set_scene_file_path 只改节点
  // 属性、不更新编辑器页签表，不得冒用），close+reopen 是唯一的同步手段。
  // 代价是页签焦点可能变化（关闭再打开会切换焦点），为正确性必要代价。
  // L2 07_scene_tabs / 13_inline_subresource 依赖此语义：无 reopen 时
  // get_unsaved_scenes 报空路径、reload_editor_scene 报 scene is not open。
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
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("saved");
  r["path"] = mcp::JsonValue(path);
  r["tab_path_registered"] = mcp::JsonValue(tab_path_registered);
  if (!tab_path_registered) {
    r["tab_rebuilt"] = mcp::JsonValue(tab_rebuilt);
    if (!tab_rebuilt) {
      r["warning"] = mcp::JsonValue(
          "saved, but the editor tab path could not be synchronized; "
          "get_open_scenes/reload_editor_scene may not see " + path);
    }
  }
  // 页签重建后编辑器根可能已替换，用最新根做保存回执比对。
  auto *verify_root = editor->get_edited_scene_root();
  if (!verify_root)
    verify_root = root;
  attach_save_receipt(r, args, verify_root, path);
  scene_dirty_tracker::clear_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_editor_scene_as completed");
  return r;
}

mcp::JsonValue handle_build_csharp_assembly(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_csharp_assembly called");
  static const char *kNote =
      "本工具仅等效触发 C# 项目编译，不等同于在编辑器中点击 Build 后的程序集热重载。"
      "GDExtension(C++) 无公共 API 触发编辑器内的 C# 程序集重载——该能力位于引擎 "
      "modules/mono 内部，非 GDExtension 可调用。适合 CI / 命令行式编译验证；若需编辑器内"
      "类或签名变更后校验渲染，仍须用户在编辑器中点 Build 或重启编辑器。";

  std::string project_file;
  godot::Ref<godot::DirAccess> da = godot::DirAccess::open("res://");
  if (!da.is_null()) {
    da->list_dir_begin();
    godot::String entry = da->get_next();
    while (entry != godot::String()) {
      if (entry != "." && entry != ".." && !da->current_is_dir() &&
          (entry.ends_with(".csproj") || entry.ends_with(".sln"))) {
        project_file = "res://" + util::to_std(entry);
        break;
      }
      entry = da->get_next();
    }
    da->list_dir_end();
  }
  if (project_file.empty())
    return util::error_json("当前项目不是 C# 工程，未发现 .csproj/.sln");

  auto *os = godot::OS::get_singleton();
  if (!os)
    return util::error_json("OS singleton not available");

  godot::PackedStringArray argv;
  argv.append("build");
  argv.append("--nologo");
  argv.append(godot::String(project_file.c_str()));
  int32_t pid = os->create_process("dotnet", argv);

  std::string command = "dotnet build --nologo " + project_file;
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["project_file"] = mcp::JsonValue(project_file);
  inner["command"] = mcp::JsonValue(command);
  inner["pid"] = mcp::JsonValue(static_cast<int64_t>(pid));
  inner["note"] = mcp::JsonValue(kNote);
  inner["started"] = mcp::JsonValue(pid > 0);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  if (pid <= 0) {
    r["error"] = mcp::JsonValue(
        "启动 dotnet 失败（SDK 缺失或不在 PATH？），请确认已安装 .NET SDK 且 "
        "dotnet 命令可用");
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_csharp_assembly completed");
  return r;
}

mcp::JsonValue handle_verify_scene_saved(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "verify_scene_saved called");
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
  std::string target_path;
  auto *sp = args.Find("scene_path");
  if (sp && sp->IsString() && !sp->GetString().empty()) {
    target_path = sp->GetString();
  } else {
    target_path = util::to_std(root->get_scene_file_path());
  }
  if (target_path.empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "current scene has no file path — save it first (save_editor_scene), "
        "or provide scene_path");
    return e;
  }
  if (!godot::FileAccess::file_exists(godot::String(target_path.c_str()))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("scene file not found on disk: " + target_path);
    e["path"] = mcp::JsonValue(target_path);
    return e;
  }
  bool disk_ok = false;
  const std::string text = scene_verify::read_text_file(target_path, disk_ok);
  if (!disk_ok) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("scene file could not be read: " + target_path);
    e["path"] = mcp::JsonValue(target_path);
    return e;
  }
  const bool want_hash = save_receipt_flag(args, "include_hash");
  const scene_verify::VerifyResult vr =
      scene_verify::compare_tree_with_text(root, text, want_hash);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(vr.match ? "match" : "mismatch");
  r["match"] = mcp::JsonValue(vr.match);
  r["path"] = mcp::JsonValue(target_path);
  r["memory_nodes"] = mcp::JsonValue(vr.memory_nodes);
  r["disk_nodes"] = mcp::JsonValue(vr.disk_nodes);
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (const std::string &p : vr.missing_paths)
    arr.PushBack(mcp::JsonValue(p));
  r["missing_paths"] = std::move(arr);
  if (vr.missing_truncated)
    r["missing_truncated"] = mcp::JsonValue(true);
  if (want_hash && vr.hash_computed) {
    r["memory_hash"] = mcp::JsonValue(hash_hex(vr.memory_hash));
    r["disk_hash"] = mcp::JsonValue(hash_hex(vr.disk_hash));
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "verify_scene_saved completed");
  return r;
}

} // namespace editor_ops
} // namespace godot_autopilot
