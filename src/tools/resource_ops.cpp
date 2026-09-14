#include "resource_ops.hpp"
#include "editor_ops.hpp"
#include "core/config.hpp"
#include "core/editor_readiness.hpp"
#include "core/log_system.hpp"
#include "core/resource_registry.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/project_path.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace resource_ops {

namespace {

bool normalize_resource_path(const std::string &raw, std::string &out,
                             std::string &error, bool allow_root = false) {
  const util::ProjectPath checked =
      util::normalize_project_path(raw, false, allow_root);
  if (!checked.valid()) {
    error = checked.error;
    return false;
  }
  out = checked.value;
  return true;
}

mcp::JsonValue serialize_ref(const godot::Ref<godot::Resource> &res) {
  if (res.is_null())
    return mcp::JsonValue(nullptr);
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
  j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  j["object_id_str"] = mcp::JsonValue(
      std::to_string(static_cast<int64_t>(res->get_instance_id())));
  j["name"] = mcp::JsonValue(util::to_std(res->get_name()));
  return j;
}

} // namespace

namespace {

godot::Ref<godot::Resource>
resolve_resource(const mcp::JsonValue &args, bool &out_has_oid,
                 int64_t &out_obj_id, std::string &out_error,
                 std::string *out_warning = nullptr) {
  out_has_oid = false;
  out_obj_id = 0;
  out_error.clear();
  if (out_warning)
    out_warning->clear();

  std::string name;
  auto *it_name = args.Find("name");
  if (it_name && it_name->IsString())
    name = it_name->GetString();

  std::string path;
  auto *it_path = args.Find("path");
  if (it_path && it_path->IsString())
    path = it_path->GetString();
  if (!path.empty()) {
    std::string normalized_path;
    if (!normalize_resource_path(path, normalized_path, out_error))
      return godot::Ref<godot::Resource>();
    path = normalized_path;
  }

  int64_t obj_id = 0;
  bool has_oid = false;
  auto *it_oid_str = args.Find("object_id_str");
  if (it_oid_str && it_oid_str->IsString()) {
    try {
      obj_id = std::stoll(it_oid_str->GetString());
      has_oid = true;
    } catch (...) {
      out_error = "invalid object_id_str: not a valid 64-bit integer";
      return godot::Ref<godot::Resource>();
    }
  } else {
    auto *it_oid = args.Find("object_id");
    if (it_oid && it_oid->IsInt()) {
      obj_id = it_oid->GetInt();
      has_oid = true;
    }
  }

  godot::Ref<godot::Resource> res;
  if (has_oid) {
    auto *obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(obj_id));
    if (obj) {
      auto *res_obj = godot::Object::cast_to<godot::Resource>(obj);
      if (res_obj) {
        res = godot::Ref<godot::Resource>(res_obj);
      }
    }
    if (res.is_null()) {
      std::string oid_key = std::to_string(obj_id);
      res = resource_registry::lookup_memory(oid_key);
      if (res.is_null() && !name.empty()) {
        res = resource_registry::lookup_memory(name);
      }
    }
  } else if (!name.empty()) {
    res = resource_registry::lookup_memory(name);
  }

  if (res.is_null() && !path.empty()) {
    auto *loader = godot::ResourceLoader::get_singleton();
    if (loader) {
      res = loader->load(godot::String(path.c_str()));
    }
    if (res.is_valid()) {
      resource_registry::register_resource(res, path);
    }
    if (res.is_valid() && out_warning) {
      *out_warning =
          "loaded existing file from disk (" + path +
          "); in-memory modifications to this resource (if any) are NOT "
          "included — pass name or object_id to save the live instance";
    }
  }

  out_has_oid = has_oid;
  out_obj_id = obj_id;
  return res;
}

std::string describe_target(const mcp::JsonValue &args) {
  auto *it_oid_str = args.Find("object_id_str");
  if (it_oid_str && it_oid_str->IsString()) {
    return "object_id_str=" + it_oid_str->GetString();
  }
  auto *it_oid = args.Find("object_id");
  if (it_oid && it_oid->IsInt()) {
    return "object_id=" + std::to_string(it_oid->GetInt());
  }
  auto *it_name = args.Find("name");
  if (it_name && it_name->IsString()) {
    return "name=" + it_name->GetString();
  }
  auto *it_path = args.Find("path");
  if (it_path && it_path->IsString()) {
    return "path=" + it_path->GetString();
  }
  return "no identifier provided";
}

const char kLoadFailSuffix[] =
    " (file exists but failed to load — not imported or wrong type)";

bool load_resource_or_error(const std::string &path,
                            godot::Ref<godot::Resource> &out_res,
                            std::string &out_error,
                            const std::string &type_hint = {},
                            const char *load_fail_suffix = kLoadFailSuffix) {
  std::string normalized_path;
  if (!normalize_resource_path(path, normalized_path, out_error))
    return false;
  if (!godot::FileAccess::file_exists(
          godot::String(normalized_path.c_str()))) {
    out_error = "file does not exist: " + normalized_path;
    return false;
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  out_res = loader ? loader->load(godot::String(normalized_path.c_str()),
                                  godot::String(type_hint.c_str()))
                   : godot::Ref<godot::Resource>();
  if (out_res.is_null()) {
    out_error =
        "failed to load resource: " + normalized_path +
        (load_fail_suffix != nullptr ? load_fail_suffix : "");
    return false;
  }
  return true;
}

bool find_global_class_path(const std::string &class_name,
                            std::string &out_path) {
  out_path.clear();
  auto *settings = godot::ProjectSettings::get_singleton();
  if (!settings)
    return false;
  godot::TypedArray<godot::Dictionary> classes =
      settings->get_global_class_list();
  for (int i = 0; i < classes.size(); i++) {
    const godot::Dictionary entry = classes[i];
    const std::string cls = util::to_std(godot::String(entry["class"]));
    if (cls != class_name)
      continue;
    const std::string path = util::to_std(godot::String(entry["path"]));
    if (path.empty())
      return false;
    out_path = path;
    return true;
  }
  return false;
}

std::string scan_directory_case_conflict(const std::string &save_dir) {
  std::string target;
  std::string parent;
  size_t slash2 = save_dir.find_last_of('/');
  if (slash2 == std::string::npos) {
    target = save_dir;
    parent = "res://";
  } else if (slash2 + 1 < save_dir.size()) {
    target = save_dir.substr(slash2 + 1);
    parent = save_dir.substr(0, slash2);
    if (parent == "res:" || parent == "res:/") {
      parent = "res://";
    } else if (parent == "user:" || parent == "user:/") {
      parent = "user://";
    }
  }
  if (target.empty()) {
    return {};
  }
  godot::String target_lower = godot::String(target.c_str()).to_lower();
  auto parent_dir = godot::DirAccess::open(godot::String(parent.c_str()));
  if (parent_dir.is_valid()) {
    parent_dir->list_dir_begin();
    godot::String entry = parent_dir->get_next();
    while (!entry.is_empty()) {
      if (parent_dir->current_is_dir() &&
          entry != godot::String(target.c_str()) &&
          entry.to_lower() == target_lower) {
        std::string conflict =
            "directory case mismatch: 保存目录 \"" + target +
            "\" 与已有目录 \"" + util::to_std(entry) +
            "\" 仅大小写不同（Windows "
            "大小写不敏感，可能引发资源加载警告）— 建议统一目录名大小写";
        parent_dir->list_dir_end();
        return conflict;
      }
      entry = parent_dir->get_next();
    }
    parent_dir->list_dir_end();
  }
  return {};
}

bool ensure_save_directory(const std::string &save_dir, bool &dirs_created,
                           std::string &out_error) {
  auto dir = godot::DirAccess::open(godot::String("res://"));
  if (!dir.is_valid()) {
    return true;
  }
  if (dir->dir_exists(godot::String(save_dir.c_str()))) {
    return true;
  }
  godot::Error mk_err =
      dir->make_dir_recursive(godot::String(save_dir.c_str()));
  if (mk_err != godot::Error::OK) {
    out_error =
        "failed to create directory: " + save_dir + " (error " +
        std::to_string(static_cast<int>(mk_err)) +
        ") — expected the directory to be creatable before saving the "
        "resource";
    return false;
  }
  dirs_created = true;
  return true;
}

void sync_editor_and_uid_after_save(const std::string &src_path,
                                    const std::string &dest_path) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    godot::String src_gs(src_path.c_str());
    godot::String dst_gs(dest_path.c_str());
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(dst_gs);
    }
    if (dest_path != src_path) {
      auto *root = editor->get_edited_scene_root();
      if (root && root->get_scene_file_path() == src_gs) {
        root->set_scene_file_path(dst_gs);
      }
    }
  }
  auto *ruid = godot::ResourceUID::get_singleton();
  if (ruid) {
    auto *loader = godot::ResourceLoader::get_singleton();
    int64_t uid_val =
        loader ? loader->get_resource_uid(godot::String(dest_path.c_str()))
               : godot::ResourceUID::INVALID_ID;
    if (uid_val != godot::ResourceUID::INVALID_ID) {
      ruid->add_id(uid_val, godot::String(dest_path.c_str()));
    }
  }
}

struct TabCleanupResult {
  std::vector<std::string> open_tabs;
  std::vector<std::string> closed_tabs;
  std::vector<std::string> warnings;
};

TabCleanupResult
close_open_scene_tabs(const std::vector<std::string> &targets) {
  TabCleanupResult out;
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor || targets.empty())
    return out;
  godot::PackedStringArray open_scenes = editor->get_open_scenes();
  auto *active_root = editor->get_edited_scene_root();
  const std::string active_path =
      active_root ? util::to_std(active_root->get_scene_file_path())
                  : std::string();
  for (int i = 0; i < open_scenes.size(); i++) {
    const std::string open_path = util::to_std(open_scenes[i]);
    if (open_path.empty())
      continue;
    for (const std::string &target : targets) {
      if (open_path == target) {
        out.open_tabs.push_back(open_path);
        break;
      }
    }
  }
  if (out.open_tabs.empty())
    return out;
  if (active_path.empty()) {
    out.warnings.push_back(
        "cannot close stale scene tab(s) while no edited scene is active — "
        "tabs left open; switch to a scene tab and close it manually");
    return out;
  }
  bool switched_any = false;
  for (const std::string &tab : out.open_tabs) {
    if (tab == active_path) {
      out.warnings.push_back(
          "scene tab " + tab +
          " is the current edited scene and was left open (its file was "
          "moved or removed — save it elsewhere or close it manually)");
      continue;
    }
    editor->open_scene_from_path(godot::String(tab.c_str()));
    auto *root_after = editor->get_edited_scene_root();
    if (!root_after || util::to_std(root_after->get_scene_file_path()) != tab) {
      out.warnings.push_back(
          "failed to activate scene tab " + tab +
          " before closing; tab left open (it may be marked as having "
          "unsaved changes)");
      continue;
    }
    switched_any = true;
    godot::Error close_err = editor->close_scene();
    if (close_err == godot::OK) {
      out.closed_tabs.push_back(tab);
    } else {
      out.warnings.push_back(
          "failed to close scene tab " + tab + " (error " +
          std::to_string(static_cast<int>(close_err)) + "); tab left open");
    }
  }
  if (switched_any) {
    editor->open_scene_from_path(godot::String(active_path.c_str()));
  }
  return out;
}

} // namespace

godot::Ref<godot::Resource> resolve_memory_resource(const std::string &name) {
  return resource_registry::lookup_memory(name);
}

void register_memory_resource(const godot::Ref<godot::Resource> &res,
                              const std::string &name) {
  resource_registry::register_resource(res, name);
}

bool try_resolve_resource_value(const mcp::JsonValue &val, godot::Variant &out,
                                std::string &out_error) {
  out_error.clear();
  if (val.IsString()) {
    const std::string str = val.GetString();
    const std::string mem_prefix = "memory://";
    if (str.compare(0, mem_prefix.size(), mem_prefix) == 0) {
      std::string name = str.substr(mem_prefix.size());
      godot::Ref<godot::Resource> res = resource_registry::lookup_memory(name);
      if (res.is_null()) {
        out_error =
            "memory resource not found: " + name +
            " (create it with create_resource/create_spriteframes first)";
        return true;
      }
      out = godot::Variant(res.ptr());
      return true;
    }
    const std::string res_prefix = "res://";
    if (str.compare(0, res_prefix.size(), res_prefix) == 0) {
      godot::Ref<godot::Resource> res;
      if (!load_resource_or_error(str, res, out_error, {}, "")) {
        return true;
      }
      out = godot::Variant(res.ptr());
      return true;
    }
    return false;
  }
  if (!val.IsObject()) {
    return false;
  }
  for (auto it = val.begin(); it != val.end(); ++it) {
    const std::string &key = it->first;
    if (!key.empty() && key[0] == '$') {
      out_error = "unsupported resource reference format: 使用了 \"" + key +
                  "\" 包装 — 支持格式: {\"path\": \"res://...\"}（磁盘资源）或 "
                  "{\"resource\": \"memory://名称\"}（内存资源）或字符串路径 + "
                  "type_hint";
      return true;
    }
  }
  auto *it_resource = val.Find("resource");
  if (it_resource && it_resource->IsString()) {
    std::string name = it_resource->GetString();
    const std::string mem_prefix = "memory://";
    const bool is_mem = name.compare(0, mem_prefix.size(), mem_prefix) == 0;
    if (is_mem) {
      name = name.substr(mem_prefix.size());
    }
    const std::string res_prefix = "res://";
    if (!is_mem && name.compare(0, res_prefix.size(), res_prefix) == 0) {
      godot::Ref<godot::Resource> res;
      if (!load_resource_or_error(name, res, out_error)) {
        return true;
      }
      out = godot::Variant(res.ptr());
      return true;
    }
    godot::Ref<godot::Resource> res = resolve_memory_resource(name);
    if (res.is_null()) {
      out_error =
          "memory resource not found: " + name +
          (is_mem ? " (create it with create_resource first)"
                  : " — 建议使用 {\"path\": \"res://...\"}（磁盘资源）或 "
                    "{\"resource\": \"memory://名称\"}（内存资源）");
      return true;
    }
    out = godot::Variant(res.ptr());
    return true;
  }
  auto *it_path = val.Find("path");
  if (it_path && it_path->IsString()) {
    std::string path = it_path->GetString();
    godot::Ref<godot::Resource> res;
    if (!load_resource_or_error(path, res, out_error)) {
      return true;
    }
    out = godot::Variant(res.ptr());
    return true;
  }
  return false;
}

mcp::JsonValue handle_load(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");

  std::string type_hint;
  auto *th = args.Find("type_hint");
  if (th && th->IsString())
    type_hint = th->GetString();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res;
  std::string load_err;
  if (!load_resource_or_error(normalized_path, res, load_err, type_hint)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(load_err);
    return e;
  }
  if (util::to_std(res->get_path()).empty()) {
    res->set_path(godot::String(normalized_path.c_str()));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = serialize_ref(res);
  return r;
}

mcp::JsonValue handle_load_threaded(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");

  std::string type_hint;
  auto *th = args.Find("type_hint");
  if (th && th->IsString())
    type_hint = th->GetString();

  bool use_sub_threads = false;
  auto *us = args.Find("use_sub_threads");
  if (us && us->IsBool())
    use_sub_threads = us->GetBool();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Error err = loader->load_threaded_request(
      godot::String(normalized_path.c_str()), godot::String(type_hint.c_str()),
      use_sub_threads);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  return r;
}

mcp::JsonValue handle_load_threaded_get_status(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::ResourceLoader::ThreadLoadStatus status =
      loader->load_threaded_get_status(godot::String(normalized_path.c_str()));
  static const char *names[] = {"invalid_resource", "in_progress", "failed",
                                "loaded"};
  int idx = static_cast<int>(status);
  std::string name = (idx >= 0 && idx < 4) ? names[idx] : "unknown";

  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["status"] = mcp::JsonValue(name);
  j["code"] = mcp::JsonValue(static_cast<int64_t>(idx));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  return r;
}

mcp::JsonValue handle_load_threaded_wait(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res =
      loader->load_threaded_get(godot::String(normalized_path.c_str()));
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("threaded load did not return a resource for: " + path);
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = serialize_ref(res);
  return r;
}

mcp::JsonValue handle_reload(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::String path_gs(normalized_path.c_str());
  godot::Ref<godot::Resource> previous;
  if (loader->has_cached(path_gs)) {
    previous = loader->get_cached_ref(path_gs);
  }
  godot::Ref<godot::Resource> reloaded =
      loader->load(path_gs, godot::String(),
                   godot::ResourceLoader::CACHE_MODE_REPLACE);
  if (reloaded.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to reload resource: " + normalized_path +
        " (missing, not imported or failed to load with CACHE_MODE_REPLACE)");
    return e;
  }

  const bool replaced = previous.is_null() || previous.ptr() != reloaded.ptr();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("reloaded");
  r["path"] = mcp::JsonValue(normalized_path);
  r["replaced"] = mcp::JsonValue(true);
  r["reused_cached_instance"] = mcp::JsonValue(!replaced);
  return r;
}

mcp::JsonValue handle_save(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  if (!path.empty()) {
    std::string normalized_path;
    std::string path_error;
    if (!normalize_resource_path(path, normalized_path, path_error))
      return util::error_detail("path rejected", "path", path_error,
                                "save only inside the res:// project namespace");
    path = normalized_path;
  }

  std::string dest_path = path;
  auto *dp = args.Find("dest_path");
  if (dp && dp->IsString())
    dest_path = dp->GetString();
  std::string normalized_dest_path;
  std::string dest_error;
  if (!normalize_resource_path(dest_path, normalized_dest_path, dest_error))
    return util::error_detail("path rejected", "dest_path", dest_error,
                              "save only inside the res:// project namespace");
  dest_path = normalized_dest_path;

  int flags = 0;
  auto *fl = args.Find("flags");
  if (fl && fl->IsInt())
    flags = static_cast<int>(fl->GetInt());

  std::string name;
  auto *nm = args.Find("name");
  if (nm && nm->IsString())
    name = nm->GetString();

  bool has_oid = false;
  int64_t obj_id = 0;
  std::string resolve_err;
  std::string resolve_warning;
  godot::Ref<godot::Resource> res =
      resolve_resource(args, has_oid, obj_id, resolve_err, &resolve_warning);

  if (res.is_null() && !resolve_err.empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(resolve_err);
    return e;
  }

  if (res.is_null()) {
    auto *ct = args.Find("class_type");
    if (ct && ct->IsString()) {
      std::string class_type = ct->GetString();
      auto *cdbs = godot::ClassDBSingleton::get_singleton();
      if (cdbs) {
        godot::Variant obj_var =
            cdbs->instantiate(godot::StringName(class_type.c_str()));
        if (obj_var.get_type() != godot::Variant::NIL) {
          auto *obj = godot::Object::cast_to<godot::Resource>(obj_var);
          if (obj) {
            res = godot::Ref<godot::Resource>(obj);
            if (resolve_memory_resource(name).is_null()) {
              register_memory_resource(res, name);
            }
          }
        }
      }
    }
  }

  if (!res.is_null() && res->get_path().is_empty() && !name.empty()) {
    res->set_path(godot::String(("memory://" + name).c_str()));
  }

  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    if (!path.empty()) {
      e["error"] = mcp::JsonValue(
          "failed to resolve resource: resource at path '" + path +
          "' could not be loaded from disk — provide object_id (from "
          "create_resource) or class_type + name to create a new resource in "
          "memory");
    } else {
      e["error"] =
          mcp::JsonValue("failed to resolve resource: provide object_id (from "
                         "create_resource) or class_type + name to create a "
                         "new resource in memory");
    }
    return e;
  }

  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceSaver not available");
    return e;
  }

  godot::Ref<godot::Resource> cow_source;
  bool copy_on_write = false;
  if (!path.empty() && dest_path != path && !resolve_warning.empty()) {
    cow_source = res;
    godot::Ref<godot::Resource> copy = res->duplicate(true);
    if (copy.is_null()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "failed to duplicate resource for copy-on-write save: " + path +
          " -> " + dest_path);
      return e;
    }
    res = copy;
    copy_on_write = true;
  }

  std::string save_dir = dest_path;
  bool dirs_created = false;
  std::string case_conflict;
  size_t last_slash = save_dir.find_last_of('/');
  if (last_slash != std::string::npos) {
    save_dir = save_dir.substr(0, last_slash);
    case_conflict = scan_directory_case_conflict(save_dir);
    std::string dir_error;
    if (!ensure_save_directory(save_dir, dirs_created, dir_error)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(dir_error);
      return e;
    }
  }

  godot::Error err = saver->save(
      res, godot::String(dest_path.c_str()),
      static_cast<godot::BitField<godot::ResourceSaver::SaverFlags>>(flags));
  if (err == godot::OK) {
    sync_editor_and_uid_after_save(path, dest_path);
  }
  bool verified = false;
  if (err == godot::OK) {
    auto *loader = godot::ResourceLoader::get_singleton();
    if (loader) {
      godot::Ref<godot::Resource> verify =
          loader->load(godot::String(dest_path.c_str()), godot::String(),
                       godot::ResourceLoader::CACHE_MODE_IGNORE);
      verified = verify.is_valid();
      if (copy_on_write && verify.is_valid() &&
          verify.ptr() == cow_source.ptr()) {
        verified = false;
      }
    }
  }
  if (err == godot::OK && has_oid) {
    resource_registry::erase_oid(obj_id);
    resource_registry::register_resource(res, name);
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  if (err == godot::OK && has_oid) {
    r["cache"] = mcp::JsonValue("kept");
  }
  if (copy_on_write) {
    r["copy_on_write"] = mcp::JsonValue(true);
    r["source_path"] = mcp::JsonValue(path);
  }
  if (err == godot::OK) {
    r["verified"] = mcp::JsonValue(verified);
  }
  if (dirs_created) {
    r["directories_created"] = mcp::JsonValue(true);
  }
  if (!case_conflict.empty()) {
    r["warning"] = mcp::JsonValue(case_conflict);
  } else if (!resolve_warning.empty()) {
    r["warning"] = mcp::JsonValue(resolve_warning);
  }
  return r;
}

mcp::JsonValue handle_create(const mcp::JsonValue &args) {
  auto *it_type = args.Find("type");
  if (!it_type || !it_type->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: type");
    return e;
  }
  std::string type = it_type->GetString();

  std::string name;
  auto *nm = args.Find("name");
  if (nm && nm->IsString())
    name = nm->GetString();

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDB not available");
    return e;
  }

  godot::Ref<godot::Resource> res;
  if (cdbs->is_parent_class(godot::StringName(type.c_str()),
                            godot::StringName("Resource"))) {
    godot::Variant obj_var =
        cdbs->instantiate(godot::StringName(type.c_str()));
    if (obj_var.get_type() == godot::Variant::NIL) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("failed to instantiate: " + type);
      return e;
    }

    auto *obj = godot::Object::cast_to<godot::Resource>(obj_var);
    if (!obj) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("instantiated object is not a Resource: " + type);
      return e;
    }
    res = godot::Ref<godot::Resource>(obj);
  } else {
    std::string script_path;
    if (!find_global_class_path(type, script_path)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      if (cdbs->class_exists(godot::StringName(type.c_str()))) {
        e["error"] = mcp::JsonValue(type + " is not a Resource subclass");
      } else {
        e["error"] = mcp::JsonValue(
            type +
            " is not a known class: no engine class or registered global "
            "script class with this name exists");
      }
      return e;
    }

    auto *loader = godot::ResourceLoader::get_singleton();
    godot::Ref<godot::Resource> loaded =
        loader ? loader->load(godot::String(script_path.c_str()))
               : godot::Ref<godot::Resource>();
    godot::Ref<godot::Script> script = loaded;
    if (script.is_null()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "failed to load global class script: " + script_path);
      return e;
    }

    godot::StringName base = script->get_instance_base_type();
    if (base == godot::StringName()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "failed to resolve the base type of global class script: " +
          script_path +
          " (script may be invalid or its language is unavailable)");
      return e;
    }
    if (!cdbs->is_parent_class(base, godot::StringName("Resource"))) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          type + " is not a Resource subclass (global class script " +
          script_path + " extends " + util::to_std(godot::String(base)) + ")");
      return e;
    }

    if (!script->can_instantiate()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          type +
          " is not instantiable (abstract or invalid global class script): " +
          script_path);
      return e;
    }

    godot::Variant obj_var = script->call(godot::StringName("new"));
    auto *obj = godot::Object::cast_to<godot::Resource>(obj_var);
    if (!obj) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("failed to instantiate global class: " + type);
      return e;
    }
    res = godot::Ref<godot::Resource>(obj);
  }

  if (!name.empty()) {
    res->set_name(godot::String(name.c_str()));
    res->set_path(godot::String(("memory://" + name).c_str()));
  }

  auto result = serialize_ref(res);

  resource_registry::register_resource(res, name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

mcp::JsonValue handle_duplicate(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  bool deep = false;
  auto *dp = args.Find("deep");
  if (dp && dp->IsBool())
    deep = dp->GetBool();

  std::string name;
  auto *nm = args.Find("name");
  if (nm && nm->IsString())
    name = nm->GetString();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res;
  std::string load_err;
  if (!load_resource_or_error(path, res, load_err)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(load_err);
    return e;
  }

  godot::Ref<godot::Resource> dup = res->duplicate(deep);
  if (dup.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("duplicate returned null");
    return e;
  }

  if (!name.empty()) {
    dup->set_name(godot::String(name.c_str()));
  }
  resource_registry::register_resource(dup, name);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = serialize_ref(dup);
  return r;
}

mcp::JsonValue handle_get_type(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res;
  std::string load_err;
  if (!load_resource_or_error(path, res, load_err)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(load_err);
    return e;
  }

  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
  j["path"] = mcp::JsonValue(path);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  return r;
}

mcp::JsonValue handle_exists(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error, true))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project directory path");
  path = normalized_path;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  bool exists = loader->exists(godot::String(path.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(exists);
  return r;
}

mcp::JsonValue handle_list_types(const mcp::JsonValue &) {
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDB not available");
    return e;
  }

  std::vector<std::string> names;
  godot::PackedStringArray all = cdbs->get_class_list();
  for (int i = 0; i < all.size(); i++) {
    std::string cls = util::to_std(all[i]);
    if (cdbs->is_parent_class(godot::StringName(cls.c_str()),
                              godot::StringName("Resource")) &&
        cdbs->can_instantiate(godot::StringName(cls.c_str()))) {
      names.push_back(cls);
    }
  }

  auto *settings = godot::ProjectSettings::get_singleton();
  auto *loader = godot::ResourceLoader::get_singleton();
  if (settings && loader) {
    std::vector<std::string> script_classes;
    godot::TypedArray<godot::Dictionary> classes =
        settings->get_global_class_list();
    for (int i = 0; i < classes.size(); i++) {
      const godot::Dictionary entry = classes[i];
      const std::string cls = util::to_std(godot::String(entry["class"]));
      const std::string script_path =
          util::to_std(godot::String(entry["path"]));
      if (cls.empty() || script_path.empty() ||
          std::find(names.begin(), names.end(), cls) != names.end()) {
        continue;
      }
      godot::Ref<godot::Resource> loaded =
          loader->load(godot::String(script_path.c_str()));
      godot::Ref<godot::Script> script = loaded;
      if (script.is_null() || !script->can_instantiate())
        continue;
      godot::StringName base = script->get_instance_base_type();
      if (base == godot::StringName() ||
          !cdbs->is_parent_class(base, godot::StringName("Resource"))) {
        continue;
      }
      script_classes.push_back(cls);
    }
    std::sort(script_classes.begin(), script_classes.end());
    names.insert(names.end(), script_classes.begin(), script_classes.end());
  }

  mcp::JsonValue types(mcp::JsonValue::array_tag);
  for (const std::string &cls : names) {
    types.PushBack(mcp::JsonValue(cls));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(types);
  return r;
}

mcp::JsonValue handle_get_extensions(const mcp::JsonValue &args) {
  std::string type;
  auto *tp = args.Find("type");
  if (tp && tp->IsString())
    type = tp->GetString();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::PackedStringArray exts =
      loader->get_recognized_extensions_for_type(godot::String(type.c_str()));
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < exts.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(exts[i])));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(arr);
  return r;
}

mcp::JsonValue handle_list_dir(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::PackedStringArray entries =
      loader->list_directory(godot::String(path.c_str()));
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < entries.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(entries[i])));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(arr);
  return r;
}

namespace {

struct RefHit {
  bool path;
  bool uid;
  bool klass;
};

godot::String make_token(const char *prefix, const std::string &value) {
  return godot::String(prefix) + godot::String(value.c_str()) + "\"";
}

std::string join_path(const std::string &dir, const std::string &name) {
  return dir + (dir.empty() || dir.back() == '/' ? std::string() : "/") + name;
}

bool has_binary_reference_extension(const std::string &path) {
  static const char *kBinaryExts[] = {".scn", ".res", ".csv", ".translation"};
  for (const char *ext : kBinaryExts) {
    const size_t len = std::char_traits<char>::length(ext);
    if (path.size() >= len &&
        path.compare(path.size() - len, len, ext) == 0) {
      return true;
    }
  }
  return false;
}

bool is_text_resource_entry(const godot::String &entry) {
  return entry.ends_with(".tscn") || entry.ends_with(".tres");
}

bool is_binary_candidate_entry(const godot::String &entry) {
  return has_binary_reference_extension(util::to_std(entry));
}

struct ResourceScanBudget {
  size_t files_listed = 0;
  size_t files_scanned = 0;
  size_t bytes_scanned = 0;
  std::string limit_reason;
  bool truncated = false;
  bool hit_limit(size_t file_bytes, size_t depth) {
    if (depth > GDA_SCAN_MAX_DEPTH) {
      limit_reason = "directory_depth";
      truncated = true;
      return true;
    }
    if (files_listed >= GDA_SCAN_MAX_FILES) {
      limit_reason = "file_count";
      truncated = true;
      return true;
    }
    if (file_bytes > GDA_SCAN_MAX_FILE_BYTES) {
      limit_reason = "single_file_bytes";
      truncated = true;
      return true;
    }
    if (bytes_scanned > GDA_SCAN_MAX_TOTAL_BYTES - file_bytes) {
      limit_reason = "total_bytes";
      truncated = true;
      return true;
    }
    return false;
  }
};

void collect_matching_files(const std::string &dir,
                            bool (*match)(const godot::String &),
                            std::vector<std::string> &out) {
  ResourceScanBudget tmp;
  bool truncated = false;
  // delegate to budget-aware version for backwards compatibility (no byte tracking)
  // simple path without bytes tracking
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) {
    return;
  }
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (truncated) break;
    if (entry != "." && entry != "..") {
      if (da->current_is_dir()) {
        if (tmp.files_listed >= GDA_SCAN_MAX_FILES || tmp.hit_limit(0, 1)) {
          truncated = true;
          break;
        }
        collect_matching_files(join_path(dir, util::to_std(entry)), match, out);
        if (out.size() >= GDA_SCAN_MAX_FILES) truncated = true;
      } else if (match(entry)) {
        if (tmp.files_listed >= GDA_SCAN_MAX_FILES) { truncated = true; break; }
        ++tmp.files_listed;
        out.push_back(join_path(dir, util::to_std(entry)));
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

void collect_matching_files_budget(const std::string &dir,
                                   bool (*match)(const godot::String &),
                                   std::vector<std::string> &out,
                                   ResourceScanBudget &budget,
                                   size_t depth,
                                   bool &truncated) {
  if (truncated) return;
  if (depth > GDA_SCAN_MAX_DEPTH) {
    budget.limit_reason = "directory_depth";
    budget.truncated = true;
    truncated = true;
    return;
  }
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) return;
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (truncated) break;
    if (entry != "." && entry != "..") {
      std::string name = util::to_std(entry);
      std::string fpath = join_path(dir, name);
      if (da->current_is_dir()) {
        collect_matching_files_budget(fpath, match, out, budget, depth + 1, truncated);
      } else if (match(entry)) {
        if (budget.files_listed >= GDA_SCAN_MAX_FILES) {
          budget.limit_reason = "file_count";
          budget.truncated = true;
          truncated = true;
          break;
        }
        ++budget.files_listed;
        out.push_back(fpath);
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

RefHit scan_reference_tokens(const godot::String &content,
                             const std::string &target_path,
                             const std::string &target_uid,
                             const std::string &target_class) {
  RefHit hit;
  hit.path = false;
  hit.uid = false;
  hit.klass = false;
  if (!target_path.empty()) {
    const godot::String tok = make_token("path=\"", target_path);
    hit.path = content.contains(tok);
  }
  if (!target_uid.empty()) {
    const godot::String tok = make_token("uid=\"", target_uid);
    hit.uid = content.contains(tok);
  }
  if (!target_class.empty()) {
    const godot::String tok = make_token("script_class=\"", target_class);
    hit.klass = content.contains(tok);
  }
  return hit;
}

std::vector<std::string> extract_script_class_tokens(const godot::String &content) {
  std::vector<std::string> out;
  const godot::String prefix("script_class=\"");
  int64_t pos = content.find(prefix, 0);
  while (pos >= 0) {
    const int64_t start = pos + prefix.length();
    const int64_t end = content.find(godot::String("\""), start);
    if (end < 0) {
      break;
    }
    const std::string tok =
        "script_class=\"" +
        util::to_std(content.substr(start, end - start)) + "\"";
    if (std::find(out.begin(), out.end(), tok) == out.end()) {
      out.push_back(tok);
    }
    pos = content.find(prefix, end + 1);
  }
  return out;
}

mcp::JsonValue collect_reference_hits(const std::string &target_path,
                                      const std::string &target_uid,
                                      const std::string &target_class) {
  ResourceScanBudget budget;
  bool truncated = false;
  std::vector<std::string> text_files;
  collect_matching_files_budget("res://", is_text_resource_entry, text_files, budget, 0, truncated);
  mcp::JsonValue result(mcp::JsonValue::array_tag);
  for (const std::string &fp : text_files) {
    if (truncated) break;
    godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (fa.is_null()) continue;
    int64_t len = fa->get_length();
    size_t file_bytes = len > 0 ? static_cast<size_t>(len) : 0;
    if (budget.hit_limit(file_bytes, 0)) { truncated = true; break; }
    budget.files_scanned++;
    budget.bytes_scanned += file_bytes;
    const godot::String content = fa->get_as_text();
    const RefHit hit =
        scan_reference_tokens(content, target_path, target_uid, target_class);
    if (hit.path || hit.uid || hit.klass) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["file"] = mcp::JsonValue(fp);
      mcp::JsonValue matched(mcp::JsonValue::array_tag);
      if (hit.path) matched.PushBack(mcp::JsonValue("path"));
      if (hit.uid) matched.PushBack(mcp::JsonValue("uid"));
      if (hit.klass) matched.PushBack(mcp::JsonValue("script_class"));
      item["matched"] = std::move(matched);
      result.PushBack(std::move(item));
    }
  }
  return result;
}

mcp::JsonValue collect_reference_hits_budget(const std::string &target_path,
                                             const std::string &target_uid,
                                             const std::string &target_class,
                                             ResourceScanBudget &out_budget,
                                             bool &out_truncated) {
  out_budget = ResourceScanBudget();
  out_truncated = false;
  std::vector<std::string> text_files;
  collect_matching_files_budget("res://", is_text_resource_entry, text_files, out_budget, 0, out_truncated);
  mcp::JsonValue result(mcp::JsonValue::array_tag);
  for (const std::string &fp : text_files) {
    if (out_truncated) break;
    godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (fa.is_null()) continue;
    int64_t len = fa->get_length();
    size_t file_bytes = len > 0 ? static_cast<size_t>(len) : 0;
    if (out_budget.hit_limit(file_bytes, 0)) { out_truncated = true; break; }
    out_budget.files_scanned++;
    out_budget.bytes_scanned += file_bytes;
    const godot::String content = fa->get_as_text();
    const RefHit hit =
        scan_reference_tokens(content, target_path, target_uid, target_class);
    if (hit.path || hit.uid || hit.klass) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["file"] = mcp::JsonValue(fp);
      mcp::JsonValue matched(mcp::JsonValue::array_tag);
      if (hit.path) matched.PushBack(mcp::JsonValue("path"));
      if (hit.uid) matched.PushBack(mcp::JsonValue("uid"));
      if (hit.klass) matched.PushBack(mcp::JsonValue("script_class"));
      item["matched"] = std::move(matched);
      result.PushBack(std::move(item));
    }
  }
  return result;
}

} // namespace

mcp::JsonValue handle_get_uid(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  int64_t uid = loader->get_resource_uid(godot::String(path.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(uid));
  return r;
}

mcp::JsonValue handle_set_uid(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  auto *uid_svc = godot::ResourceUID::get_singleton();
  if (!uid_svc) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceUID not available");
    return e;
  }

  int64_t uid;
  auto *it_uid = args.Find("uid");
  if (it_uid && it_uid->IsNumber()) {
    uid = it_uid->GetInt();
  } else {
    uid = uid_svc->create_id();
  }
  uid_svc->set_id(uid, godot::String(path.c_str()));
  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs && !is_import_in_progress()) {
      efs->reimport_files(godot::PackedStringArray());
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(uid));
  return r;
}

mcp::JsonValue handle_remove(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  bool force = false;
  auto *it_force = args.Find("force");
  if (it_force && it_force->IsBool())
    force = it_force->GetBool();

  if (!force) {
    if (!godot::FileAccess::file_exists(godot::String(path.c_str())) &&
        !godot::DirAccess::dir_exists_absolute(godot::String(path.c_str()))) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("file does not exist: " + path);
      return e;
    }
    std::string uid_str;
    auto *ruid = godot::ResourceUID::get_singleton();
    auto *loader = godot::ResourceLoader::get_singleton();
    if (ruid && loader) {
      const int64_t uid =
          loader->get_resource_uid(godot::String(path.c_str()));
      if (uid >= 0) {
        uid_str = util::to_std(ruid->id_to_text(uid));
      }
    }
    ResourceScanBudget dep_budget;
    bool dep_truncated = false;
    mcp::JsonValue dependents =
        collect_reference_hits_budget(path, uid_str, std::string(), dep_budget, dep_truncated);
    int64_t dep_count = 0;
    for (const auto &entry : dependents.GetArray()) {
      (void)entry;
      dep_count++;
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["would_delete"] = mcp::JsonValue(true);
    r["dependents"] = std::move(dependents);
    auto *editor_open = godot::EditorInterface::get_singleton();
    if (editor_open) {
      godot::PackedStringArray open_scenes = editor_open->get_open_scenes();
      for (int i = 0; i < open_scenes.size(); i++) {
        if (util::to_std(open_scenes[i]) == path) {
          mcp::JsonValue open_tabs_arr(mcp::JsonValue::array_tag);
          open_tabs_arr.PushBack(mcp::JsonValue(path));
          r["open_tabs"] = std::move(open_tabs_arr);
          break;
        }
      }
    }
    if (dep_truncated) {
      r["scan_truncated"] = mcp::JsonValue(true);
      mcp::JsonValue lim(mcp::JsonValue::object_tag);
      lim["files_listed"] = mcp::JsonValue(static_cast<int64_t>(dep_budget.files_listed));
      lim["files_scanned"] = mcp::JsonValue(static_cast<int64_t>(dep_budget.files_scanned));
      lim["bytes_scanned"] = mcp::JsonValue(static_cast<int64_t>(dep_budget.bytes_scanned));
      if (!dep_budget.limit_reason.empty()) lim["reason"] = mcp::JsonValue(dep_budget.limit_reason);
      r["scan_limit"] = std::move(lim);
    }
    r["hint"] = mcp::JsonValue(
        "dry-run only: nothing was deleted (" + std::to_string(dep_count) +
        " referencing file(s) found); call again with force=true to move the "
        "file to the OS trash and remove its .uid sidecar");
    return r;
  }

  TabCleanupResult tab_cleanup =
      close_open_scene_tabs(std::vector<std::string>{path});

  godot::String path_gs(path.c_str());

  auto *loader = godot::ResourceLoader::get_singleton();
  if (loader && loader->has_cached(path_gs)) {
    auto cached = loader->get_cached_ref(path_gs);
    if (cached.is_valid()) {
      cached->set_path("");
    }
  }

  const std::string uid_sidecar = path + ".uid";
  const bool has_sidecar =
      godot::FileAccess::file_exists(godot::String(uid_sidecar.c_str()));

  auto *os = godot::OS::get_singleton();
  bool trashed = false;
  godot::Error err = godot::OK;
  if (os) {
    err = os->move_to_trash(path_gs);
    trashed = err == godot::OK;
  }
  if (!trashed) {
    err = godot::DirAccess::remove_absolute(path_gs);
  }

  mcp::JsonValue sidecars_removed(mcp::JsonValue::array_tag);
  if (err == godot::OK && has_sidecar) {
    bool side_done = false;
    if (os) {
      side_done = os->move_to_trash(
                        godot::String(uid_sidecar.c_str())) == godot::OK;
    }
    if (!side_done) {
      side_done = godot::DirAccess::remove_absolute(
                        godot::String(uid_sidecar.c_str())) == godot::OK;
    }
    if (side_done) {
      sidecars_removed.PushBack(mcp::JsonValue(uid_sidecar));
    }
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(path_gs);
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  if (err == godot::OK) {
    r[trashed ? "trashed" : "permanent"] = mcp::JsonValue(true);
  }
  r["sidecars_removed"] = std::move(sidecars_removed);
  if (!tab_cleanup.open_tabs.empty()) {
    mcp::JsonValue open_tabs_arr(mcp::JsonValue::array_tag);
    for (const std::string &p : tab_cleanup.open_tabs) {
      open_tabs_arr.PushBack(mcp::JsonValue(p));
    }
    r["open_tabs"] = std::move(open_tabs_arr);
  }
  if (!tab_cleanup.closed_tabs.empty()) {
    mcp::JsonValue closed_tabs_arr(mcp::JsonValue::array_tag);
    for (const std::string &p : tab_cleanup.closed_tabs) {
      closed_tabs_arr.PushBack(mcp::JsonValue(p));
    }
    r["closed_tabs"] = std::move(closed_tabs_arr);
  }
  if (!tab_cleanup.warnings.empty()) {
    mcp::JsonValue warnings_arr(mcp::JsonValue::array_tag);
    for (const std::string &w : tab_cleanup.warnings) {
      warnings_arr.PushBack(mcp::JsonValue(w));
    }
    r["closed_tabs_warning"] = std::move(warnings_arr);
  }
  return r;
}

mcp::JsonValue apply_path_rewrite_transaction(const std::string &from_raw,
                                              const std::string &to_raw) {
  std::string normalized_from;
  std::string normalized_to;
  std::string path_error;
  if (!normalize_resource_path(from_raw, normalized_from, path_error))
    return util::error_detail("path rejected", "from", path_error,
                              "rename only inside res://");
  if (!normalize_resource_path(to_raw, normalized_to, path_error))
    return util::error_detail("path rejected", "to", path_error,
                              "rename only inside res://");
  const std::string &from_path = normalized_from;
  const std::string &to_path = normalized_to;
  if (from_path == "res://" || to_path == "res://")
    return util::error_detail("path rejected", "path", "namespace root is not a file",
                              "provide resource paths below res://");
  const std::string &from = from_path;
  const std::string &to = to_path;
  const std::string from_uid_path = from + ".uid";
  const std::string to_uid_path = to + ".uid";
  const std::string from_import_path = from + ".import";
  const std::string to_import_path = to + ".import";

  auto *loader = godot::ResourceLoader::get_singleton();
  auto *ruid = godot::ResourceUID::get_singleton();

  int64_t old_uid = -1;
  std::string old_uid_str;
  if (loader && ruid) {
    old_uid = loader->get_resource_uid(godot::String(from.c_str()));
    if (old_uid >= 0) {
      old_uid_str = util::to_std(ruid->id_to_text(old_uid));
    }
  }

  const bool uid_sidecar_absent =
      !godot::FileAccess::file_exists(godot::String(from_uid_path.c_str()));
  godot::Error uid_err = godot::OK;
  if (!uid_sidecar_absent) {
    uid_err = godot::DirAccess::rename_absolute(
        godot::String(from_uid_path.c_str()),
        godot::String(to_uid_path.c_str()));
  }

  const bool import_sidecar_present = godot::FileAccess::file_exists(
      godot::String(from_import_path.c_str()));
  godot::Error import_err = godot::OK;
  if (import_sidecar_present) {
    import_err = godot::DirAccess::rename_absolute(
        godot::String(from_import_path.c_str()),
        godot::String(to_import_path.c_str()));
  }

  ResourceScanBudget scan_budget;
  bool scan_truncated = false;
  std::vector<std::string> text_files;
  collect_matching_files_budget("res://", is_text_resource_entry, text_files, scan_budget, 0, scan_truncated);
  std::vector<std::pair<std::string, RefHit>> deps;
  for (const std::string &fp : text_files) {
    if (scan_truncated) break;
    godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (fa.is_null()) continue;
    int64_t len = fa->get_length();
    size_t file_bytes = len > 0 ? static_cast<size_t>(len) : 0;
    if (scan_budget.hit_limit(file_bytes, 0)) { scan_truncated = true; break; }
    scan_budget.files_scanned++;
    scan_budget.bytes_scanned += file_bytes;
    const godot::String content = fa->get_as_text();
    const RefHit hit =
        scan_reference_tokens(content, from, old_uid_str, std::string());
    if (hit.path || hit.uid || hit.klass) {
      deps.emplace_back(fp, hit);
    }
  }

  std::vector<std::string> unsupported_binary;
  std::vector<std::string> binary_candidates;
  collect_matching_files_budget("res://", is_binary_candidate_entry,
                         binary_candidates, scan_budget, 0, scan_truncated);
  for (const std::string &fp : binary_candidates) {
    if (scan_truncated) break;
    godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (fa.is_null()) continue;
    int64_t len = fa->get_length();
    size_t file_bytes = len > 0 ? static_cast<size_t>(len) : 0;
    if (scan_budget.hit_limit(file_bytes, 0)) { scan_truncated = true; break; }
    scan_budget.files_scanned++;
    scan_budget.bytes_scanned += file_bytes;
    const RefHit hit =
        scan_reference_tokens(fa->get_as_text(), from, old_uid_str,
                              std::string());
    if (hit.path || hit.uid || hit.klass) {
      unsupported_binary.push_back(fp);
    }
  }

  std::vector<std::string> affected_open_scenes;
  auto *editor_pre = godot::EditorInterface::get_singleton();
  if (editor_pre) {
    godot::PackedStringArray open_scenes = editor_pre->get_open_scenes();
    for (int i = 0; i < open_scenes.size(); i++) {
      const std::string open_path = util::to_std(open_scenes[i]);
      if (open_path.empty()) {
        continue;
      }
      bool affected = open_path == from;
      if (!affected) {
        for (const auto &dep : deps) {
          if (dep.first == open_path) {
            affected = true;
            break;
          }
        }
      }
      if (affected) {
        affected_open_scenes.push_back(open_path);
      }
    }
    if (!affected_open_scenes.empty()) {
      editor_pre->save_all_scenes();
    }
  }

  godot::Error err = godot::DirAccess::rename_absolute(
      godot::String(from.c_str()), godot::String(to.c_str()));
  if (err != godot::OK) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
  }

  std::vector<std::string> ghost_tabs;
  auto *editor_tabs = godot::EditorInterface::get_singleton();
  if (editor_tabs) {
    auto *active_root = editor_tabs->get_edited_scene_root();
    const std::string active_path =
        active_root ? util::to_std(active_root->get_scene_file_path())
                    : std::string();
    godot::PackedStringArray open_now = editor_tabs->get_open_scenes();
    for (int i = 0; i < open_now.size(); i++) {
      const std::string open_path = util::to_std(open_now[i]);
      if (open_path == from && open_path != active_path) {
        ghost_tabs.push_back(open_path);
      }
    }
  }
  TabCleanupResult tab_cleanup;
  if (!ghost_tabs.empty()) {
    tab_cleanup = close_open_scene_tabs(ghost_tabs);
  }

  struct UpdatedFileRec {
    std::string file;
    std::vector<std::string> changes;
  };
  struct StaleRefRec {
    std::string file;
    std::string token;
  };
  std::vector<UpdatedFileRec> updated;
  std::vector<StaleRefRec> stale;

  for (const auto &dep : deps) {
    const std::string &fp = dep.first;
    const RefHit &hit = dep.second;
    if (has_binary_reference_extension(fp)) {
      unsupported_binary.push_back(fp);
      continue;
    }
    godot::String content;
    {
      godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
          godot::String(fp.c_str()), godot::FileAccess::READ);
      if (fa.is_null()) {
        stale.push_back({fp, std::string()});
        continue;
      }
      content = fa->get_as_text();
    }
    std::vector<std::string> changes;
    bool needs_write = false;

    if (hit.path) {
      const godot::String old_tok = make_token("path=\"", from);
      const godot::String new_tok = make_token("path=\"", to);
      if (content.contains(old_tok)) {
        content = content.replace(old_tok, new_tok);
        needs_write = true;
        changes.emplace_back("path");
      }
    }

    if (hit.klass) {
      const std::vector<std::string> class_tokens =
          extract_script_class_tokens(content);
      if (class_tokens.empty()) {
        stale.push_back({fp, "script_class"});
      } else {
        for (const std::string &tok : class_tokens) {
          stale.push_back({fp, tok});
        }
      }
    }

    if (!needs_write) {
      continue;
    }
    godot::Ref<godot::FileAccess> writer = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::WRITE);
    if (writer.is_null() || !writer->store_string(content)) {
      stale.push_back({fp, "path=\"" + from + "\""});
      continue;
    }
    writer->flush();
    writer->close();
    godot::Ref<godot::FileAccess> checker = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (checker.is_null() ||
        !checker->get_as_text().contains(make_token("path=\"", to))) {
      stale.push_back({fp, "path=\"" + from + "\""});
      continue;
    }
    auto *editor_written = godot::EditorInterface::get_singleton();
    if (editor_written) {
      auto *efs_written = editor_written->get_resource_filesystem();
      if (efs_written) {
        efs_written->update_file(godot::String(fp.c_str()));
      }
    }
    updated.push_back({fp, std::move(changes)});
  }

  const bool uid_preserved = uid_sidecar_absent || uid_err == godot::OK;

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  mcp::JsonValue updated_arr(mcp::JsonValue::array_tag);
  for (auto &u : updated) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["file"] = mcp::JsonValue(u.file);
    mcp::JsonValue cs(mcp::JsonValue::array_tag);
    for (const std::string &c : u.changes) {
      cs.PushBack(mcp::JsonValue(c));
    }
    item["changes"] = std::move(cs);
    updated_arr.PushBack(std::move(item));
  }
  r["updated_files"] = std::move(updated_arr);
  mcp::JsonValue stale_arr(mcp::JsonValue::array_tag);
  for (const auto &s : stale) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["file"] = mcp::JsonValue(s.file);
    item["token"] = mcp::JsonValue(s.token);
    stale_arr.PushBack(std::move(item));
  }
  r["stale_references"] = std::move(stale_arr);
  mcp::JsonValue binary_arr(mcp::JsonValue::array_tag);
  for (const std::string &fp : unsupported_binary) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["file"] = mcp::JsonValue(fp);
    item["reason"] = mcp::JsonValue(
        "binary resource references cannot be rewritten in place");
    binary_arr.PushBack(std::move(item));
  }
  r["unsupported_binary_references"] = std::move(binary_arr);
  r["note"] = mcp::JsonValue(
      "stale_references entries are {file, token} objects where token is the "
      "literal reference left behind; script_class tokens are never rewritten "
      "automatically — after renaming a script's class_name run "
      "scan_editor_file_system so Godot re-registers global classes; binary "
      "resources cannot be rewritten in place — use the editor FileSystem "
      "dock for these; open scenes affected by the move are saved before the "
      "rewrite and reloaded afterwards (their undo history resets)");
  r["uid_preserved"] = mcp::JsonValue(uid_preserved);
  if (!tab_cleanup.closed_tabs.empty()) {
    mcp::JsonValue closed_tabs_arr(mcp::JsonValue::array_tag);
    for (const std::string &p : tab_cleanup.closed_tabs) {
      closed_tabs_arr.PushBack(mcp::JsonValue(p));
    }
    r["closed_tabs"] = std::move(closed_tabs_arr);
  }
  if (!tab_cleanup.warnings.empty()) {
    mcp::JsonValue closed_warn_arr(mcp::JsonValue::array_tag);
    for (const std::string &w : tab_cleanup.warnings) {
      closed_warn_arr.PushBack(mcp::JsonValue(w));
    }
    r["closed_tabs_warning"] = std::move(closed_warn_arr);
  }
  if (scan_truncated) {
    r["scan_truncated"] = mcp::JsonValue(true);
    mcp::JsonValue lim(mcp::JsonValue::object_tag);
    lim["files_listed"] = mcp::JsonValue(static_cast<int64_t>(scan_budget.files_listed));
    lim["files_scanned"] = mcp::JsonValue(static_cast<int64_t>(scan_budget.files_scanned));
    lim["bytes_scanned"] = mcp::JsonValue(static_cast<int64_t>(scan_budget.bytes_scanned));
    lim["max_files"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_FILES));
    lim["max_file_bytes"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_FILE_BYTES));
    lim["max_total_bytes"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_TOTAL_BYTES));
    lim["max_depth"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_DEPTH));
    if (!scan_budget.limit_reason.empty()) lim["reason"] = mcp::JsonValue(scan_budget.limit_reason);
    r["scan_limit"] = std::move(lim);
  }
  if (import_sidecar_present && import_err != godot::OK) {
    r["import_sidecar_warning"] = mcp::JsonValue(
        "failed to move .import sidecar " + from_import_path + " -> " +
        to_import_path + " (error " +
        std::to_string(static_cast<int>(import_err)) +
        "); the asset will be re-imported with default settings at the new "
        "location");
  }

  godot::String from_gs(from.c_str());
  godot::String to_gs(to.c_str());

  if (loader && loader->has_cached(from_gs)) {
    auto cached = loader->get_cached_ref(from_gs);
    if (cached.is_valid()) {
      cached->set_path(to_gs);
    }
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *root = editor->get_edited_scene_root();
    if (root && root->get_scene_file_path() == from_gs) {
      root->set_scene_file_path(to_gs);
    }
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(from_gs);
      efs->update_file(to_gs);
    }
  }

  struct RemapRec {
    std::string setting;
    std::string old;
    std::string updated;
  };
  std::vector<RemapRec> remaps;
  auto *ps = godot::ProjectSettings::get_singleton();
  if (ps) {
    const std::string kAutoloadPrefix = "autoload/";
    auto try_remap = [&](const std::string &setting_name) {
      if (!ps->has_setting(godot::String(setting_name.c_str()))) {
        return;
      }
      const godot::Variant v =
          ps->get_setting(godot::String(setting_name.c_str()));
      if (v.get_type() != godot::Variant::STRING) {
        return;
      }
      const std::string s = util::to_std(v.operator godot::String());
      const bool starred = !s.empty() && s[0] == '*';
      const std::string core = starred ? s.substr(1) : s;
      if (core != from) {
        return;
      }
      const std::string rewritten =
          (starred ? std::string("*") : std::string()) + to;
      ps->set_setting(
          godot::String(setting_name.c_str()),
          godot::Variant(godot::String(rewritten.c_str())));
      remaps.push_back({setting_name, s, rewritten});
    };
    try_remap("application/run/main_scene");
    godot::TypedArray<godot::Dictionary> props = ps->get_property_list();
    for (int64_t i = 0; i < props.size(); i++) {
      godot::Dictionary d = props[i];
      const std::string name =
          util::to_std(d["name"].operator godot::String());
      if (name.compare(0, kAutoloadPrefix.size(), kAutoloadPrefix) == 0) {
        try_remap(name);
      }
    }
    if (!remaps.empty()) {
      ps->save();
    }
  }

  mcp::JsonValue remap_arr(mcp::JsonValue::array_tag);
  for (const auto &rm : remaps) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["setting"] = mcp::JsonValue(rm.setting);
    item["old"] = mcp::JsonValue(rm.old);
    item["new"] = mcp::JsonValue(rm.updated);
    remap_arr.PushBack(std::move(item));
  }
  r["remapped_settings"] = std::move(remap_arr);

  mcp::JsonValue saved_arr(mcp::JsonValue::array_tag);
  for (const std::string &p : affected_open_scenes) {
    saved_arr.PushBack(mcp::JsonValue(p));
  }
  mcp::JsonValue reloaded_arr(mcp::JsonValue::array_tag);
  auto *editor_post = godot::EditorInterface::get_singleton();
  if (editor_post && !affected_open_scenes.empty()) {
    godot::String active_path;
    auto *root = editor_post->get_edited_scene_root();
    if (root) {
      active_path = root->get_scene_file_path();
    }
    godot::PackedStringArray open_post = editor_post->get_open_scenes();
    std::vector<std::string> ordered;
    std::string deferred_active;
    for (const std::string &p : affected_open_scenes) {
      const std::string cur = p == from ? to : p;
      bool still_open = false;
      for (int i = 0; i < open_post.size(); i++) {
        if (util::to_std(open_post[i]) == cur) {
          still_open = true;
          break;
        }
      }
      if (!still_open) {
        continue;
      }
      if (!active_path.is_empty() &&
          godot::String(cur.c_str()) == active_path) {
        deferred_active = cur;
        continue;
      }
      ordered.push_back(cur);
    }
    if (!deferred_active.empty()) {
      ordered.push_back(deferred_active);
    }
    for (const std::string &cur : ordered) {
      editor_post->reload_scene_from_path(godot::String(cur.c_str()));
      reloaded_arr.PushBack(mcp::JsonValue(cur));
    }
  }
  r["saved_scenes"] = std::move(saved_arr);
  r["reloaded_scenes"] = std::move(reloaded_arr);
  return r;
}

mcp::JsonValue handle_rename(const mcp::JsonValue &args) {
  auto *it_from = args.Find("path");
  if ((!it_from || !it_from->IsString()) && args.Contains("from")) {
    it_from = args.Find("from");
  }
  auto *it_to = args.Find("new_path");
  if ((!it_to || !it_to->IsString()) && args.Contains("to")) {
    it_to = args.Find("to");
  }
  if (!it_from || !it_from->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  if (!it_to || !it_to->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: new_path");
    return e;
  }
  std::string from = it_from->GetString();
  std::string to = it_to->GetString();
  std::string normalized_from;
  std::string path_error;
  if (normalize_resource_path(from, normalized_from, path_error, true) &&
      godot::DirAccess::dir_exists_absolute(
          godot::String(normalized_from.c_str()))) {
    return util::error_detail(
        "directory rename not supported", normalized_from,
        from + " is a directory — directory-level rename does not rewrite "
               "dependency references, so updated_files would be empty and "
               "referencing files would be left stale",
        "use move_resource_file with path and new_directory instead");
  }
  return apply_path_rewrite_transaction(from, to);
}

namespace {

bool ends_with_uid_sidecar(const std::string &p) {
  const std::string suffix = ".uid";
  return p.size() > suffix.size() &&
         p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool ends_with_import_sidecar(const std::string &p) {
  const std::string suffix = ".import";
  return p.size() > suffix.size() &&
         p.compare(p.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void collect_all_file_paths(const std::string &dir,
                            std::vector<std::string> &out) {
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) {
    return;
  }
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != "..") {
      const std::string full = join_path(dir, util::to_std(entry));
      if (da->current_is_dir()) {
        collect_all_file_paths(full, out);
      } else {
        out.push_back(full);
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

void collect_all_file_paths_budget(const std::string &dir,
                                   std::vector<std::string> &out,
                                   ResourceScanBudget &budget,
                                   size_t depth,
                                   bool &truncated) {
  if (truncated) return;
  if (depth > GDA_SCAN_MAX_DEPTH) {
    budget.limit_reason = "directory_depth";
    budget.truncated = true;
    truncated = true;
    return;
  }
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) return;
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (truncated) break;
    if (entry != "." && entry != "..") {
      const std::string full = join_path(dir, util::to_std(entry));
      if (da->current_is_dir()) {
        collect_all_file_paths_budget(full, out, budget, depth + 1, truncated);
      } else {
        if (budget.files_listed >= GDA_SCAN_MAX_FILES) {
          budget.limit_reason = "file_count";
          budget.truncated = true;
          truncated = true;
          break;
        }
        ++budget.files_listed;
        out.push_back(full);
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

} // namespace

mcp::JsonValue handle_move(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *it_dir = args.Find("new_directory");
  if (!it_dir || !it_dir->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: new_directory");
    return e;
  }
  const std::string raw_dir = it_dir->GetString();
  std::string target_dir;
  std::string path_error;
  if (!normalize_resource_path(raw_dir, target_dir, path_error, true))
    return util::error_detail("path rejected", "new_directory", path_error,
                              "move only inside res://");

  std::string path;
  if (!normalize_resource_path(it_path->GetString(), path, path_error, true))
    return util::error_detail("path rejected", "path", path_error,
                              "move only res:// files or directories");
  if (path == "res://")
    return util::error_detail("path rejected", "path", "namespace root is not movable",
                              "provide a path below res://");
  godot::String path_gs(path.c_str());
  const bool src_is_dir = godot::DirAccess::dir_exists_absolute(path_gs);
  const bool src_is_file = godot::FileAccess::file_exists(path_gs);
  if (!src_is_dir && !src_is_file) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("source does not exist: " + path);
    return e;
  }

  ResourceScanBudget move_budget;
  bool move_truncated = false;
  std::vector<std::string> sources;
  if (src_is_dir) {
    if (target_dir.size() > path.size() &&
        target_dir.compare(0, path.size(), path) == 0 &&
        target_dir[path.size()] == '/') {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("cannot move a directory inside itself: " +
                                  path + " -> " + target_dir);
      return e;
    }
    std::vector<std::string> all;
    collect_all_file_paths_budget(path, all, move_budget, 0, move_truncated);
    for (const std::string &f : all) {
      if (!ends_with_uid_sidecar(f) && !ends_with_import_sidecar(f)) {
        sources.push_back(f);
      }
    }
  } else {
    sources.push_back(path);
  }

  mcp::JsonValue moved_arr(mcp::JsonValue::array_tag);
  mcp::JsonValue failed_arr(mcp::JsonValue::array_tag);
  mcp::JsonValue stale_arr(mcp::JsonValue::array_tag);
  mcp::JsonValue binary_agg(mcp::JsonValue::array_tag);
  mcp::JsonValue saved_agg(mcp::JsonValue::array_tag);
  mcp::JsonValue reloaded_agg(mcp::JsonValue::array_tag);
  mcp::JsonValue warn_list(mcp::JsonValue::array_tag);
  mcp::JsonValue closed_agg(mcp::JsonValue::array_tag);
  mcp::JsonValue closed_warn_agg(mcp::JsonValue::array_tag);
  bool scanned = false;

  for (const std::string &src : sources) {
    std::string dest_dir = target_dir;
    if (src_is_dir) {
      const std::string rel = src.substr(path.size() + 1);
      const size_t rel_slash = rel.find_last_of('/');
      if (rel_slash != std::string::npos) {
        dest_dir = join_path(target_dir, rel.substr(0, rel_slash));
      }
    }
    const std::string src_base = src.substr(src.find_last_of('/') + 1);
    const std::string dest = join_path(dest_dir, src_base);
    if (dest == src) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["from"] = mcp::JsonValue(src);
      item["error"] = mcp::JsonValue("destination equals source");
      failed_arr.PushBack(std::move(item));
      continue;
    }
    bool dirs_created = false;
    std::string dir_error;
    if (!ensure_save_directory(dest_dir, dirs_created, dir_error)) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["from"] = mcp::JsonValue(src);
      item["error"] = mcp::JsonValue(dir_error);
      failed_arr.PushBack(std::move(item));
      continue;
    }
    mcp::JsonValue tx = apply_path_rewrite_transaction(src, dest);
    auto *it_err = tx.Find("error");
    if (it_err && it_err->IsString()) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["from"] = mcp::JsonValue(src);
      item["error"] = mcp::JsonValue(it_err->GetString());
      failed_arr.PushBack(std::move(item));
      continue;
    }
    int64_t rc = 0;
    auto *it_rc = tx.Find("result");
    if (it_rc && it_rc->IsInt()) {
      rc = it_rc->GetInt();
    }
    if (rc != 0) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["from"] = mcp::JsonValue(src);
      item["error"] = mcp::JsonValue("rename failed with code " +
                                     std::to_string(rc) + " for: " + dest);
      failed_arr.PushBack(std::move(item));
      continue;
    }
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["from"] = mcp::JsonValue(src);
    item["to"] = mcp::JsonValue(dest);
    auto *it_updated = tx.Find("updated_files");
    if (it_updated && it_updated->IsArray()) {
      item["updated_files"] = *it_updated;
    }
    auto *it_uid = tx.Find("uid_preserved");
    if (it_uid && it_uid->IsBool()) {
      item["uid_preserved"] = mcp::JsonValue(it_uid->GetBool());
    }
    auto *it_saved = tx.Find("saved_scenes");
    if (it_saved && it_saved->IsArray()) {
      for (const auto &s : it_saved->GetArray()) {
        saved_agg.PushBack(s);
      }
    }
    auto *it_reloaded = tx.Find("reloaded_scenes");
    if (it_reloaded && it_reloaded->IsArray()) {
      for (const auto &s : it_reloaded->GetArray()) {
        reloaded_agg.PushBack(s);
      }
    }
    auto *it_warn = tx.Find("import_sidecar_warning");
    if (it_warn && it_warn->IsString()) {
      warn_list.PushBack(*it_warn);
    }
    auto *it_closed = tx.Find("closed_tabs");
    if (it_closed && it_closed->IsArray()) {
      for (const auto &c : it_closed->GetArray()) {
        closed_agg.PushBack(c);
      }
    }
    auto *it_closed_warn = tx.Find("closed_tabs_warning");
    if (it_closed_warn && it_closed_warn->IsArray()) {
      for (const auto &w : it_closed_warn->GetArray()) {
        closed_warn_agg.PushBack(w);
      }
    }
    moved_arr.PushBack(std::move(item));
    auto *it_stale = tx.Find("stale_references");
    if (it_stale && it_stale->IsArray()) {
      for (const auto &s : it_stale->GetArray()) {
        stale_arr.PushBack(s);
      }
    }
    auto *it_binary = tx.Find("unsupported_binary_references");
    if (it_binary && it_binary->IsArray()) {
      for (const auto &s : it_binary->GetArray()) {
        binary_agg.PushBack(s);
      }
    }
  }

  if (src_is_dir) {
    editor_ops::handle_file_system_scan(mcp::JsonValue());
    scanned = true;
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["moved"] = std::move(moved_arr);
  r["failed"] = std::move(failed_arr);
  r["stale_references"] = std::move(stale_arr);
  r["unsupported_binary_references"] = std::move(binary_agg);
  r["saved_scenes"] = std::move(saved_agg);
  r["reloaded_scenes"] = std::move(reloaded_agg);
  if (warn_list.Size() > 0) {
    r["import_sidecar_warnings"] = std::move(warn_list);
  }
  if (closed_agg.Size() > 0) {
    r["closed_tabs"] = std::move(closed_agg);
  }
  if (closed_warn_agg.Size() > 0) {
    r["closed_tabs_warning"] = std::move(closed_warn_agg);
  }
  if (scanned) {
    r["filesystem_scanned"] = mcp::JsonValue(true);
  }
  if (move_truncated) {
    r["scan_truncated"] = mcp::JsonValue(true);
    mcp::JsonValue lim(mcp::JsonValue::object_tag);
    lim["files_listed"] = mcp::JsonValue(static_cast<int64_t>(move_budget.files_listed));
    lim["reason"] = mcp::JsonValue(move_budget.limit_reason);
    lim["max_files"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_FILES));
    lim["max_depth"] = mcp::JsonValue(static_cast<int64_t>(GDA_SCAN_MAX_DEPTH));
    r["scan_limit"] = std::move(lim);
  }
  return r;
}

mcp::JsonValue handle_copy(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *it_dest = args.Find("dest_path");
  if (!it_dest || !it_dest->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: dest_path");
    return e;
  }

  std::string path;
  std::string dest_path;
  std::string path_error;
  if (!normalize_resource_path(it_path->GetString(), path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "copy only res:// files");
  if (!normalize_resource_path(it_dest->GetString(), dest_path, path_error))
    return util::error_detail("path rejected", "dest_path", path_error,
                              "copy only inside res://");

  if (path == dest_path) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("source and destination are the same file: " + path);
    return e;
  }

  godot::String src_gs(path.c_str());
  if (!godot::FileAccess::file_exists(src_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("source does not exist: " + path);
    return e;
  }
  godot::String dest_gs(dest_path.c_str());
  if (godot::DirAccess::dir_exists_absolute(dest_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("destination is a directory: " + dest_path);
    return e;
  }

  const size_t last_slash = dest_path.find_last_of('/');
  if (last_slash != std::string::npos) {
    const std::string dest_dir = dest_path.substr(0, last_slash);
    bool dirs_created = false;
    std::string dir_error;
    if (!ensure_save_directory(dest_dir, dirs_created, dir_error)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(dir_error);
      return e;
    }
  }

  godot::Error err = godot::DirAccess::copy_absolute(src_gs, dest_gs);
  if (err != godot::OK) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to copy file: " + path + " -> " + dest_path + " (error " +
        std::to_string(static_cast<int>(err)) + ")");
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs)
      efs->update_file(dest_gs);
  }
  editor_ops::handle_file_system_scan(mcp::JsonValue());

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("copied");
  r["from"] = mcp::JsonValue(path);
  r["to"] = mcp::JsonValue(dest_path);
  return r;
}

mcp::JsonValue handle_create_directory(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  const std::string raw = it_path->GetString();
  std::string dir;
  std::string path_error;
  if (!normalize_resource_path(raw, dir, path_error, false))
    return util::error_detail("path rejected", "path", path_error,
                              "create directories only below res://");

  godot::String dir_gs(dir.c_str());
  const bool already_existed =
      godot::DirAccess::dir_exists_absolute(dir_gs);
  if (!already_existed) {
    godot::Error err = godot::DirAccess::make_dir_recursive_absolute(dir_gs);
    if (err != godot::OK) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "failed to create directory: " + dir +
          " (error " + std::to_string(static_cast<int>(err)) + ")");
      return e;
    }
    editor_ops::handle_file_system_scan(mcp::JsonValue());
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["created"] = mcp::JsonValue(dir);
  r["already_existed"] = mcp::JsonValue(already_existed);
  return r;
}

mcp::JsonValue handle_get_references(const mcp::JsonValue &args) {
  std::string target_path;
  auto *it_path = args.Find("path");
  if (it_path && it_path->IsString()) {
    target_path = it_path->GetString();
    std::string normalized_target;
    std::string path_error;
    if (!normalize_resource_path(target_path, normalized_target, path_error))
      return util::error_detail("path rejected", "path", path_error,
                                "use a res:// project resource path");
    target_path = normalized_target;
  }
  std::string target_uid;
  auto *it_uid = args.Find("uid");
  if (it_uid && it_uid->IsString()) {
    target_uid = it_uid->GetString();
  }
  std::string target_class;
  auto *it_class = args.Find("class_name");
  if (it_class && it_class->IsString()) {
    target_class = it_class->GetString();
  }
  if (target_path.empty() && target_uid.empty() && target_class.empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "missing target: provide one of path, uid or class_name");
    return e;
  }

  ResourceScanBudget ref_budget;
  bool ref_truncated = false;
  mcp::JsonValue hits = collect_reference_hits_budget(target_path, target_uid, target_class, ref_budget, ref_truncated);
  mcp::JsonValue ret(mcp::JsonValue::object_tag);
  ret["result"] = std::move(hits);
  if (ref_truncated) {
    ret["scan_truncated"] = mcp::JsonValue(true);
    mcp::JsonValue lim(mcp::JsonValue::object_tag);
    lim["files_listed"] = mcp::JsonValue(static_cast<int64_t>(ref_budget.files_listed));
    lim["files_scanned"] = mcp::JsonValue(static_cast<int64_t>(ref_budget.files_scanned));
    lim["bytes_scanned"] = mcp::JsonValue(static_cast<int64_t>(ref_budget.bytes_scanned));
    if (!ref_budget.limit_reason.empty()) lim["reason"] = mcp::JsonValue(ref_budget.limit_reason);
    ret["scan_limit"] = std::move(lim);
  }
  return ret;
}

mcp::JsonValue handle_get_dependencies(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();
  std::string normalized_path;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use a res:// project resource path");
  path = normalized_path;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::PackedStringArray deps =
      loader->get_dependencies(godot::String(path.c_str()));
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int i = 0; i < deps.size(); i++) {
    arr.PushBack(mcp::JsonValue(util::to_std(deps[i])));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(arr);
  return r;
}

mcp::JsonValue handle_has_dependency(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  auto *it_dep = args.Find("dependency");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  if (!it_dep || !it_dep->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: dependency");
    return e;
  }
  std::string path = it_path->GetString();
  std::string dep = it_dep->GetString();
  std::string normalized_path;
  std::string normalized_dep;
  std::string path_error;
  if (!normalize_resource_path(path, normalized_path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "use res:// project resource paths");
  if (!normalize_resource_path(dep, normalized_dep, path_error))
    return util::error_detail("path rejected", "dependency", path_error,
                              "use res:// project resource paths");
  path = normalized_path;
  dep = normalized_dep;

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::PackedStringArray deps =
      loader->get_dependencies(godot::String(path.c_str()));
  godot::String dep_gs(dep.c_str());
  bool found = false;
  for (int i = 0; i < deps.size(); i++) {
    if (deps[i] == dep_gs) {
      found = true;
      break;
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(found);
  return r;
}

mcp::JsonValue handle_reimport(const mcp::JsonValue &args) {
  auto *engine = godot::Engine::get_singleton();
  if (engine && engine->is_editor_hint() == false) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("resource_reimport is only available in editor mode");
    return e;
  }

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

  int count = 0;
  auto *it_files = args.Find("files");
  if (it_files && it_files->IsArray()) {
    const auto &arr = it_files->GetArray();
    godot::PackedStringArray files;
    for (const auto &f : arr) {
      if (f.IsString()) {
        std::string normalized_file;
        std::string path_error;
        if (!normalize_resource_path(f.GetString(), normalized_file,
                                     path_error))
          return util::error_detail("path rejected", "files", path_error,
                                    "reimport only res:// project files");
        files.append(godot::String(normalized_file.c_str()));
        count++;
      }
    }
    if (is_import_in_progress()) {
      if (count > 0)
        return busy_error();
    } else {
      efs->reimport_files(files);
    }
  }

  if (!args.Contains("files")) {
    auto *it_path = args.Find("path");
    if (it_path && it_path->IsString()) {
      if (is_import_in_progress())
        return busy_error();
      std::string normalized_path;
      std::string path_error;
      if (!normalize_resource_path(it_path->GetString(), normalized_path,
                                   path_error))
        return util::error_detail("path rejected", "path", path_error,
                                  "reimport only res:// project files");
      godot::PackedStringArray files;
      files.append(godot::String(normalized_path.c_str()));
      efs->reimport_files(files);
      count++;
    }
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("reimport queued for " + std::to_string(count) +
                               " file(s)");
  return r;
}

mcp::JsonValue handle_set_property(const mcp::JsonValue &args) {
  auto *it_prop = args.Find("property");
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  if (!args.Contains("value")) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: value");
    return e;
  }
  std::string prop = it_prop->GetString();

  std::string type_hint;
  auto *th = args.Find("type_hint");
  if (th && th->IsString())
    type_hint = th->GetString();

  bool has_oid = false;
  int64_t obj_id = 0;
  std::string resolve_err;
  godot::Ref<godot::Resource> res =
      resolve_resource(args, has_oid, obj_id, resolve_err);
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    std::string desc =
        resolve_err.empty() ? describe_target(args) : resolve_err;
    e["error"] = mcp::JsonValue("resource not found: " + desc);
    return e;
  }

  bool found = false;
  godot::TypedArray<godot::Dictionary> props = res->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        util::to_std(dict["name"].operator godot::String()) == prop) {
      found = true;
      type_hint = util::infer_type_hint(dict, std::move(type_hint));
      break;
    }
  }
  if (!found) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "property not found: " + prop + " on " + util::to_std(res->get_class()) +
        " — use resource_get_property_list or check the property name");
    return e;
  }

  godot::Variant value;
  std::string resource_error;
  bool resource_attached = false;
  if (try_resolve_resource_value(*args.Find("value"), value, resource_error)) {
    if (!resource_error.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(resource_error);
      return e;
    }
    resource_attached = true;
  } else {
    value = VariantJson::deserialize_strict(*args.Find("value"), type_hint);
  }

  godot::Variant old_val = res->get(godot::StringName(prop.c_str()));
  res->set(godot::StringName(prop.c_str()), value);

  godot::Variant new_val = res->get(godot::StringName(prop.c_str()));

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "resource_set_property: set " + prop + " on " + util::to_std(res->get_class()) +
          " (object_id=" +
          std::to_string(static_cast<int64_t>(res->get_instance_id())) + ")");

  std::string readback_detail;
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail, true);

  mcp::JsonValue j(mcp::JsonValue::object_tag);
  j["result"] = mcp::JsonValue("ok");
  j["class"] = mcp::JsonValue(util::to_std(res->get_class()));
  j["path"] = mcp::JsonValue(util::to_std(res->get_path()));
  j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  j["resource_attached"] = mcp::JsonValue(resource_attached);
  if (readback == util::ReadbackStatus::REJECTED) {
    return util::error_detail(
        "property rejected: '" + prop + "' on " + util::to_std(res->get_path()),
        util::to_std(res->get_path()), "readback equals set value",
        "property may not exist, be read-only, or require a type hint; use "
        "resource_get_property_list");
  }
  if (readback == util::ReadbackStatus::CONVERTED) {
    j["warning"] = mcp::JsonValue("set applied; " + readback_detail);
  }
  return j;
}

mcp::JsonValue handle_get_property(const mcp::JsonValue &args) {
  auto *it_prop = args.Find("property");
  if (!it_prop || !it_prop->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: property");
    return e;
  }
  std::string prop = it_prop->GetString();

  bool has_oid = false;
  int64_t obj_id = 0;
  std::string resolve_err;
  godot::Ref<godot::Resource> res =
      resolve_resource(args, has_oid, obj_id, resolve_err);
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    std::string desc =
        resolve_err.empty() ? describe_target(args) : resolve_err;
    e["error"] = mcp::JsonValue("resource not found: " + desc);
    return e;
  }

  godot::Variant value = res->get(godot::StringName(prop.c_str()));
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "resource_get_property: read " + prop + " on " +
                                util::to_std(res->get_class()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(value);
  return r;
}

} // namespace resource_ops
} // namespace godot_autopilot
