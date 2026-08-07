#include "resource_ops.hpp"
#include "core/log_system.hpp"
#include "core/resource_registry.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <string>

namespace godot_self_driving {
namespace resource_ops {

namespace {

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
            " (create it with resource_create/spriteframes_create first)";
        return true;
      }
      out = godot::Variant(res.ptr());
      return true;
    }
    const std::string res_prefix = "res://";
    if (str.compare(0, res_prefix.size(), res_prefix) == 0) {
      if (!godot::FileAccess::file_exists(godot::String(str.c_str()))) {
        out_error = "file does not exist: " + str;
        return true;
      }
      auto *loader = godot::ResourceLoader::get_singleton();
      godot::Ref<godot::Resource> res =
          loader ? loader->load(godot::String(str.c_str()))
                 : godot::Ref<godot::Resource>();
      if (res.is_null()) {
        out_error = "failed to load resource: " + str;
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
      if (!godot::FileAccess::file_exists(godot::String(name.c_str()))) {
        out_error = "file does not exist: " + name;
        return true;
      }
      auto *loader = godot::ResourceLoader::get_singleton();
      godot::Ref<godot::Resource> res =
          loader ? loader->load(godot::String(name.c_str()))
                 : godot::Ref<godot::Resource>();
      if (res.is_null()) {
        out_error =
            "failed to load resource: " + name +
            " (file exists but failed to load — not imported or wrong type)";
        return true;
      }
      out = godot::Variant(res.ptr());
      return true;
    }
    godot::Ref<godot::Resource> res = resolve_memory_resource(name);
    if (res.is_null()) {
      out_error =
          "memory resource not found: " + name +
          (is_mem ? " (create it with resource_create first)"
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
    if (!godot::FileAccess::file_exists(godot::String(path.c_str()))) {
      out_error = "file does not exist: " + path;
      return true;
    }
    auto *loader = godot::ResourceLoader::get_singleton();
    godot::Ref<godot::Resource> res =
        loader ? loader->load(godot::String(path.c_str()))
               : godot::Ref<godot::Resource>();
    if (res.is_null()) {
      out_error =
          "failed to load resource: " + path +
          " (file exists but failed to load — not imported or wrong type)";
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

  godot::String path_gs(path.c_str());
  if (!godot::FileAccess::file_exists(path_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("file does not exist: " + path);
    return e;
  }

  godot::Ref<godot::Resource> res =
      loader->load(path_gs, godot::String(type_hint.c_str()));
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to load resource: " + path +
        " (file exists but failed to load — not imported or wrong type)");
    return e;
  }
  if (util::to_std(res->get_path()).empty()) {
    res->set_path(godot::String(path.c_str()));
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
      godot::String(path.c_str()), godot::String(type_hint.c_str()),
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

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::ResourceLoader::ThreadLoadStatus status =
      loader->load_threaded_get_status(godot::String(path.c_str()));
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

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res =
      loader->load_threaded_get(godot::String(path.c_str()));
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

mcp::JsonValue handle_save(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  std::string dest_path = path;
  auto *dp = args.Find("dest_path");
  if (dp && dp->IsString())
    dest_path = dp->GetString();

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
          "resource_create) or class_type + name to create a new resource in "
          "memory");
    } else {
      e["error"] =
          mcp::JsonValue("failed to resolve resource: provide object_id (from "
                         "resource_create) or class_type + name to create a "
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

  std::string save_dir = dest_path;
  bool dirs_created = false;
  std::string case_conflict;
  size_t last_slash = save_dir.find_last_of('/');
  if (last_slash != std::string::npos) {
    save_dir = save_dir.substr(0, last_slash);
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
    if (!target.empty()) {
      godot::String target_lower = godot::String(target.c_str()).to_lower();
      auto parent_dir = godot::DirAccess::open(godot::String(parent.c_str()));
      if (parent_dir.is_valid()) {
        parent_dir->list_dir_begin();
        godot::String entry = parent_dir->get_next();
        while (!entry.is_empty()) {
          if (parent_dir->current_is_dir() &&
              entry != godot::String(target.c_str()) &&
              entry.to_lower() == target_lower) {
            case_conflict =
                "directory case mismatch: 保存目录 \"" + target +
                "\" 与已有目录 \"" + util::to_std(entry) +
                "\" 仅大小写不同（Windows "
                "大小写不敏感，可能引发资源加载警告）— 建议统一目录名大小写";
            break;
          }
          entry = parent_dir->get_next();
        }
        parent_dir->list_dir_end();
      }
    }
    auto dir = godot::DirAccess::open(godot::String("res://"));
    if (dir.is_valid()) {
      if (!dir->dir_exists(godot::String(save_dir.c_str()))) {
        godot::Error mk_err =
            dir->make_dir_recursive(godot::String(save_dir.c_str()));
        if (mk_err != godot::Error::OK) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(
              "failed to create directory: " + save_dir + " (error " +
              std::to_string(static_cast<int>(mk_err)) +
              ") — expected the directory to be creatable before saving the "
              "resource");
          return e;
        }
        dirs_created = true;
      }
    }
  }

  godot::Error err = saver->save(
      res, godot::String(dest_path.c_str()),
      static_cast<godot::BitField<godot::ResourceSaver::SaverFlags>>(flags));
  if (err == godot::OK) {
    auto *editor = godot::EditorInterface::get_singleton();
    if (editor) {
      godot::String src_gs(path.c_str());
      godot::String dst_gs(dest_path.c_str());
      auto *efs = editor->get_resource_filesystem();
      if (efs) {
        efs->update_file(dst_gs);
      }
      if (dest_path != path) {
        auto *root = editor->get_edited_scene_root();
        if (root && root->get_scene_file_path() == src_gs) {
          root->set_scene_file_path(dst_gs);
        }
        if (efs) {
          efs->update_file(src_gs);
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
        if (editor) {
          auto *efs = editor->get_resource_filesystem();
          if (efs) {
            efs->reimport_files(godot::PackedStringArray());
          }
        }
      }
    }
  }
  bool verified = false;
  if (err == godot::OK) {
    auto *loader = godot::ResourceLoader::get_singleton();
    if (loader) {
      godot::Ref<godot::Resource> verify =
          loader->load(godot::String(dest_path.c_str()));
      verified = verify.is_valid();
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

  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Resource"))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(type + " is not a Resource subclass");
    return e;
  }

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
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
  godot::Ref<godot::Resource> res(obj);

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

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::String path_gs(path.c_str());
  if (!godot::FileAccess::file_exists(path_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("file does not exist: " + path);
    return e;
  }

  godot::Ref<godot::Resource> res = loader->load(path_gs);
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to load resource: " + path +
        " (file exists but failed to load — not imported or wrong type)");
    return e;
  }

  godot::Ref<godot::Resource> dup = res->duplicate(deep);
  if (dup.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("duplicate returned null");
    return e;
  }

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

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::String path_gs(path.c_str());
  if (!godot::FileAccess::file_exists(path_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("file does not exist: " + path);
    return e;
  }

  godot::Ref<godot::Resource> res = loader->load(path_gs);
  if (res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "failed to load resource: " + path +
        " (file exists but failed to load — not imported or wrong type)");
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

  godot::PackedStringArray all = cdbs->get_class_list();
  mcp::JsonValue types(mcp::JsonValue::array_tag);
  for (int i = 0; i < all.size(); i++) {
    std::string cls = util::to_std(all[i]);
    if (cdbs->is_parent_class(godot::StringName(cls.c_str()),
                              godot::StringName("Resource")) &&
        cdbs->can_instantiate(godot::StringName(cls.c_str()))) {
      types.PushBack(mcp::JsonValue(cls));
    }
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

mcp::JsonValue handle_get_uid(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

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
    if (efs) {
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
  godot::String path_gs(path.c_str());

  auto *loader = godot::ResourceLoader::get_singleton();
  if (loader && loader->has_cached(path_gs)) {
    auto cached = loader->get_cached_ref(path_gs);
    if (cached.is_valid()) {
      cached->set_path("");
    }
  }

  godot::Error err = godot::DirAccess::remove_absolute(path_gs);

  auto *editor = godot::EditorInterface::get_singleton();
  if (editor) {
    auto *efs = editor->get_resource_filesystem();
    if (efs) {
      efs->update_file(path_gs);
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
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

  godot::Error err = godot::DirAccess::rename_absolute(
      godot::String(from.c_str()), godot::String(to.c_str()));
  if (err != godot::OK) {
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
  }

  godot::String from_gs(from.c_str());
  godot::String to_gs(to.c_str());

  auto *loader = godot::ResourceLoader::get_singleton();
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
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
  return r;
}

mcp::JsonValue handle_get_dependencies(const mcp::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

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

mcp::JsonValue handle_import(const mcp::JsonValue &args) {
  auto *engine = godot::Engine::get_singleton();
  if (engine && engine->is_editor_hint() == false) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("resource_import is only available in editor mode");
    return e;
  }
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = it_path->GetString();

  if (!godot::FileAccess::file_exists(godot::String(path.c_str()))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("file does not exist: " + path);
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

  godot::PackedStringArray files;
  files.append(godot::String(path.c_str()));
  efs->reimport_files(files);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("import queued for: " + path);
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
        files.append(godot::String(f.GetString().c_str()));
        count++;
      }
    }
    efs->reimport_files(files);
  }

  if (!args.Contains("files")) {
    auto *it_path = args.Find("path");
    if (it_path && it_path->IsString()) {
      godot::PackedStringArray files;
      files.append(godot::String(it_path->GetString().c_str()));
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
      if (type_hint.empty() && dict.has("type")) {
        int type_id = static_cast<int>(dict["type"]);
        int hint_val = 0;
        if (dict.has("hint")) {
          hint_val = static_cast<int>(dict["hint"]);
        }
        bool is_object_type = static_cast<godot::Variant::Type>(type_id) ==
                              godot::Variant::OBJECT;
        bool is_resource_hint = hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE;
        if ((is_object_type || is_resource_hint) && dict.has("hint_string")) {
          std::string hint_str =
              util::to_std(dict["hint_string"].operator godot::String());
          if (!hint_str.empty()) {
            type_hint = hint_str;
          }
        }
        if (type_hint.empty()) {
          type_hint = util::to_std(godot::Variant::get_type_name(
              static_cast<godot::Variant::Type>(type_id)));
        }
      }
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
    value = VariantJson::deserialize(*args.Find("value"), type_hint);
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
      util::check_readback(value, old_val, new_val, readback_detail);

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
} // namespace godot_self_driving
