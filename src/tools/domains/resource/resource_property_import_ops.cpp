#include "tools/domains/resource/resource_ops.hpp"
#include "tools/domains/resource/reimport_validation.hpp"
#include "tools/domains/editor/editor_ops.hpp"
#include "core/config.hpp"
#include "core/editor_readiness.hpp"
#include "core/observability/log_system.hpp"
#include "core/resource_registry.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/json_godot.hpp"
#include "util/project_path.hpp"
#include "util/resource_fs.hpp"
#include "util/scene_path.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace resource_ops {



godot_autopilot::util::JsonValue handle_reimport(const godot_autopilot::util::JsonValue &args) {
  auto *engine = godot::Engine::get_singleton();
  if (engine && engine->is_editor_hint() == false) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("resource_reimport is only available in editor mode");
    return e;
  }

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

  const bool has_files = args.Contains("files");
  const auto *it_files_raw = args.Find("files");
  const bool files_is_array = it_files_raw != nullptr && it_files_raw->IsArray();
  std::size_t string_count = 0;
  if (files_is_array) {
    for (const auto &f : it_files_raw->GetArray()) {
      if (f.IsString()) string_count++;
    }
  }
  const auto *it_path_raw = args.Find("path");
  resource_ops_detail::ReimportCounts counts;
  counts.has_files = has_files;
  counts.files_is_array = files_is_array;
  counts.string_count = string_count;
  counts.path_is_string = it_path_raw != nullptr && it_path_raw->IsString();
  if (!resource_ops_detail::validate_reimport_counts(counts, 256)) {
    return util::error_json(
        "import requires a non-empty 'files' string array (max 256) or a string 'path'");
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

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("reimport queued for " + std::to_string(count) +
                               " file(s)");
  return r;
}

godot_autopilot::util::JsonValue handle_set_import_options(const godot_autopilot::util::JsonValue &args) {
  auto *it_path = args.Find("path");
  if (!it_path || !it_path->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  auto *it_opts = args.Find("options");
  if (!it_opts || !it_opts->IsObject() || it_opts->GetObject().empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "missing required parameter: options (non-empty object of import "
        "option keys to values)");
    return e;
  }
  std::string path;
  std::string path_error;
  if (!normalize_resource_path(it_path->GetString(), path, path_error))
    return util::error_detail("path rejected", "path", path_error,
                              "set import options only inside res://");
  auto *engine = godot::Engine::get_singleton();
  if (engine && engine->is_editor_hint() == false) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "set_resource_import_options is only available in editor mode");
    return e;
  }
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
  if (is_import_in_progress()) {
    return busy_error();
  }
  if (!godot::FileAccess::file_exists(godot::String(path.c_str()))) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("source does not exist: " + path +
                                " — check the path with get_resource_dir_files");
    return e;
  }
  const std::string import_path = path + ".import";
  if (!godot::FileAccess::file_exists(godot::String(import_path.c_str()))) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "no .import sidecar for " + path +
        ": file is not imported — refresh text resources with reload_resource "
        "instead of setting import options");
    return e;
  }
  static const char *allowed[] = {
      "loop",         "loop_mode",     "loop_begin",
      "loop_end",     "compression",   "compress/mode",
      "compress/high_quality",         "compress/lossy_quality",
      "compress/hdr_compression",      "compress/bptc_ldr",
      "compress/normal_map",           "mipmaps/generate",
      "mipmaps/limit",                 "generate_mipmaps",
      "filter",       "anisotropy",    "roughness/mode",
      "roughness/src_normal",          "process/hdr_as_srgb",
      "process/fix_alpha_border",      "process/premult_alpha",
      "process/normal_map_invert_y",   "process/invert_color",
      "process/hdr_clamp_exposure",    "stream",
      "size_limit",   "detect_3d/compress_to"};
  std::vector<std::string> keys;
  for (const auto &entry : it_opts->GetObject()) {
    keys.push_back(entry.first);
  }
  for (const std::string &key : keys) {
    bool ok = false;
    for (const char *a : allowed) {
      if (key == a) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      std::string allowed_list;
      for (const char *a : allowed) {
        if (!allowed_list.empty()) {
          allowed_list += ", ";
        }
        allowed_list += a;
      }
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("unsupported import option: '" + key +
                                  "' — allowed keys: " + allowed_list);
      return e;
    }
  }
  godot::Ref<godot::ConfigFile> cfg;
  cfg.instantiate();
  godot::String import_gs(import_path.c_str());
  if (cfg->load(import_gs) != godot::OK) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to read .import file: " + import_path +
                                " — check the file with read_file");
    return e;
  }
  for (const std::string &key : keys) {
    const godot_autopilot::util::JsonValue *val = it_opts->Find(key.c_str());
    godot::Variant v = VariantJson::deserialize_strict(*val, "");
    cfg->set_value(godot::String("params"), godot::String(key.c_str()), v);
  }
  if (cfg->save(import_gs) != godot::OK) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to write .import file: " + import_path);
    return e;
  }
  godot::PackedStringArray files;
  files.append(godot::String(path.c_str()));
  efs->reimport_files(files);
  godot_autopilot::util::JsonValue applied(godot_autopilot::util::JsonValue::array_tag);
  for (const std::string &key : keys) {
    applied.PushBack(godot_autopilot::util::JsonValue(key));
  }
  godot_autopilot::util::JsonValue ro(godot_autopilot::util::JsonValue::object_tag);
  ro["result"] = godot_autopilot::util::JsonValue("ok");
  ro["path"] = godot_autopilot::util::JsonValue(path);
  ro["applied"] = std::move(applied);
  ro["reimport"] = godot_autopilot::util::JsonValue("queued");
  ro["poll"] = godot_autopilot::util::JsonValue(
      "poll get_editor_file_system_status until scanning is false, then verify "
      "with get_resource_type or read_file on the .import [params]");
  return ro;
}

godot_autopilot::util::JsonValue handle_set_property(const godot_autopilot::util::JsonValue &args) {
  auto *it_prop = args.Find("property");
  if (!it_prop || !it_prop->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: property");
    return e;
  }
  if (!args.Contains("value")) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: value");
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
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    std::string desc =
        resolve_err.empty() ? describe_target(args) : resolve_err;
    e["error"] = godot_autopilot::util::JsonValue("resource not found: " + desc);
    return e;
  }

  std::vector<std::string> prop_parts;
  size_t part_begin = 0;
  while (true) {
    size_t dot = prop.find('.', part_begin);
    if (dot == std::string::npos) {
      prop_parts.push_back(prop.substr(part_begin));
      break;
    }
    prop_parts.push_back(prop.substr(part_begin, dot - part_begin));
    part_begin = dot + 1;
  }
  for (const std::string &part : prop_parts) {
    if (part.empty()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "invalid property path: '" + prop +
          "' contains an empty segment — use dot-separated names like 'a.b.c'");
      return e;
    }
  }
  godot::Object *target_obj = res.ptr();
  std::string walked;
  for (size_t i = 0; i + 1 < prop_parts.size(); i++) {
    const std::string &seg = prop_parts[i];
    godot::Variant seg_val = target_obj->get(godot::StringName(seg.c_str()));
    godot::Object *next_obj = nullptr;
    if (seg_val.get_type() == godot::Variant::OBJECT) {
      next_obj = seg_val.operator godot::Object *();
    }
    if (next_obj == nullptr) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      std::string at = walked.empty() ? seg : walked + "." + seg;
      e["error"] = godot_autopilot::util::JsonValue(
          "property path '" + prop + "' failed at '" + at +
          "': value is null or not an object on " +
          util::to_std(target_obj->get_class()) +
          " — use resource_get_property_list or check the property name");
      return e;
    }
    target_obj = next_obj;
    walked = walked.empty() ? seg : walked + "." + seg;
  }
  const std::string &leaf = prop_parts.back();
  bool found = false;
  godot::TypedArray<godot::Dictionary> props = target_obj->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (dict.has("name") &&
        util::to_std(dict["name"].operator godot::String()) == leaf) {
      found = true;
      type_hint = util::infer_type_hint(dict, std::move(type_hint));
      break;
    }
  }
  if (!found) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "property not found: " + prop + " on " + util::to_std(res->get_class()) +
        " — use resource_get_property_list or check the property name");
    return e;
  }

  godot::Variant value;
  std::string resource_error;
  bool resource_attached = false;
  if (try_resolve_resource_value(*args.Find("value"), value, resource_error)) {
    if (!resource_error.empty()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(resource_error);
      return e;
    }
    resource_attached = true;
  } else {
    value = VariantJson::deserialize_strict(*args.Find("value"), type_hint);
  }

  godot::Variant old_val = target_obj->get(godot::StringName(leaf.c_str()));
  target_obj->set(godot::StringName(leaf.c_str()), value);

  godot::Variant new_val = target_obj->get(godot::StringName(leaf.c_str()));

  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "resource_set_property: set " + prop + " on " + util::to_std(res->get_class()) +
          " (object_id=" +
          std::to_string(static_cast<int64_t>(res->get_instance_id())) + ")");

  std::string readback_detail;
  util::ReadbackStatus readback =
      util::check_readback(value, old_val, new_val, readback_detail, true);

  godot_autopilot::util::JsonValue j(godot_autopilot::util::JsonValue::object_tag);
  j["result"] = godot_autopilot::util::JsonValue("ok");
  j["class"] = godot_autopilot::util::JsonValue(util::to_std(res->get_class()));
  j["path"] = godot_autopilot::util::JsonValue(util::to_std(res->get_path()));
  j["object_id"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(res->get_instance_id()));
  j["resource_attached"] = godot_autopilot::util::JsonValue(resource_attached);
  if (readback == util::ReadbackStatus::REJECTED) {
    return util::error_detail(
        "property rejected: '" + prop + "' on " + util::to_std(res->get_path()),
        util::to_std(res->get_path()), "readback equals set value",
        "property may not exist, be read-only, or require a type hint; use "
        "resource_get_property_list");
  }
  if (readback == util::ReadbackStatus::CONVERTED) {
    j["warning"] = godot_autopilot::util::JsonValue("set applied; " + readback_detail);
  }
  return j;
}

godot_autopilot::util::JsonValue handle_get_property(const godot_autopilot::util::JsonValue &args) {
  auto *it_prop = args.Find("property");
  if (!it_prop || !it_prop->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: property");
    return e;
  }
  std::string prop = it_prop->GetString();

  bool has_oid = false;
  int64_t obj_id = 0;
  std::string resolve_err;
  godot::Ref<godot::Resource> res =
      resolve_resource(args, has_oid, obj_id, resolve_err);
  if (res.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    std::string desc =
        resolve_err.empty() ? describe_target(args) : resolve_err;
    e["error"] = godot_autopilot::util::JsonValue("resource not found: " + desc);
    return e;
  }

  godot::Variant value = res->get(godot::StringName(prop.c_str()));
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "resource_get_property: read " + prop + " on " +
                                util::to_std(res->get_class()));

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(value);
  return r;
}








} // namespace resource_ops
} // namespace godot_autopilot
