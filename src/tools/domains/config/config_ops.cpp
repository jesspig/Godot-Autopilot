#include "tools/domains/config/config_ops.hpp"
#include "core/config.hpp"
#include "core/observability/log_system.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/project_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace config_ops {

namespace {

constexpr const char *kProtectedSettingPrefixes[] = {"autoload/"};

mcp::JsonValue parse_enum_hint_options(const std::string &hint_string) {
  mcp::JsonValue options(mcp::JsonValue::array_tag);
  size_t start = 0;
  int64_t index = 0;
  while (start < hint_string.size()) {
    size_t comma = hint_string.find(',', start);
    std::string entry =
        comma == std::string::npos ? hint_string.substr(start)
                                   : hint_string.substr(start, comma - start);
    mcp::JsonValue option(mcp::JsonValue::object_tag);
    size_t colon = entry.find(':');
    if (colon == std::string::npos) {
      option["label"] = mcp::JsonValue(entry);
      option["value"] = mcp::JsonValue(index);
    } else {
      option["label"] = mcp::JsonValue(entry.substr(0, colon));
      std::string value_text = entry.substr(colon + 1);
      char *end = nullptr;
      long long parsed = std::strtoll(value_text.c_str(), &end, 10);
      if (end != value_text.c_str() && *end == '\0') {
        option["value"] = mcp::JsonValue(static_cast<int64_t>(parsed));
      }
    }
    options.PushBack(std::move(option));
    ++index;
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  return options;
}

constexpr const char *kAutoloadSettingPrefix = "autoload/";

struct AutoloadEntry {
  std::string name;
  std::string path;
  bool enabled = true;
  int32_t order = 0;
};

void collect_autoloads(godot::ProjectSettings *ps,
                       std::vector<AutoloadEntry> &out) {
  out.clear();
  const std::string prefix(kAutoloadSettingPrefix);
  godot::TypedArray<godot::Dictionary> props = ps->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name")) {
      continue;
    }
    const std::string setting =
        util::to_std(dict["name"].operator godot::String());
    if (setting.size() <= prefix.size() ||
        setting.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    godot::Variant value = ps->get_setting(godot::String(setting.c_str()));
    if (value.get_type() != godot::Variant::STRING) {
      continue;
    }
    const std::string raw = util::to_std(value.operator godot::String());
    AutoloadEntry entry;
    entry.name = setting.substr(prefix.size());
    entry.enabled = !raw.empty() && raw[0] == '*';
    entry.path = entry.enabled ? raw.substr(1) : raw;
    entry.order = ps->get_order(godot::String(setting.c_str()));
    out.push_back(std::move(entry));
  }
  std::sort(out.begin(), out.end(),
            [](const AutoloadEntry &a, const AutoloadEntry &b) {
              if (a.order != b.order) {
                return a.order < b.order;
              }
              return a.name < b.name;
            });
}

int64_t find_autoload(const std::vector<AutoloadEntry> &entries,
                      const std::string &name) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].name == name) {
      return static_cast<int64_t>(i);
    }
  }
  return -1;
}

mcp::JsonValue autoload_save_error(godot::Error err) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(
      "project settings save failed (error code " +
      std::to_string(static_cast<int64_t>(err)) + ")");
  return e;
}

struct AutoloadRefScan {
  std::vector<std::string> files;
  bool truncated = false;
};

bool is_autoload_ref_entry(const godot::String &entry) {
  return entry.ends_with(".tscn") || entry.ends_with(".tres") ||
         entry.ends_with(".gd");
}

void collect_autoload_ref_files(const std::string &dir, AutoloadRefScan &scan,
                                size_t depth) {
  if (scan.truncated) {
    return;
  }
  if (depth > GDA_SCAN_MAX_DEPTH ||
      scan.files.size() >= GDA_SCAN_MAX_FILES) {
    scan.truncated = true;
    return;
  }
  godot::Ref<godot::DirAccess> da =
      godot::DirAccess::open(godot::String(dir.c_str()));
  if (da.is_null()) {
    return;
  }
  da->list_dir_begin();
  godot::String entry = da->get_next();
  while (entry != godot::String()) {
    if (scan.truncated) {
      break;
    }
    if (entry != "." && entry != "..") {
      const std::string name = util::to_std(entry);
      const std::string fpath =
          dir + (dir.empty() || dir.back() == '/' ? std::string() : "/") +
          name;
      if (da->current_is_dir()) {
        collect_autoload_ref_files(fpath, scan, depth + 1);
      } else if (is_autoload_ref_entry(entry)) {
        if (scan.files.size() >= GDA_SCAN_MAX_FILES) {
          scan.truncated = true;
          break;
        }
        scan.files.push_back(fpath);
      }
    }
    entry = da->get_next();
  }
  da->list_dir_end();
}

mcp::JsonValue scan_autoload_refs(const std::string &autoload_name,
                                  const std::string &autoload_path,
                                  bool &truncated_out) {
  mcp::JsonValue stale(mcp::JsonValue::array_tag);
  truncated_out = false;
  AutoloadRefScan scan;
  collect_autoload_ref_files("res://", scan, 0);
  size_t bytes_scanned = 0;
  for (const std::string &fp : scan.files) {
    if (scan.truncated) {
      break;
    }
    godot::Ref<godot::FileAccess> fa = godot::FileAccess::open(
        godot::String(fp.c_str()), godot::FileAccess::READ);
    if (fa.is_null()) {
      continue;
    }
    const int64_t len = fa->get_length();
    const size_t file_bytes = len > 0 ? static_cast<size_t>(len) : 0;
    if (file_bytes > GDA_SCAN_MAX_FILE_BYTES) {
      scan.truncated = true;
      break;
    }
    if (bytes_scanned > GDA_SCAN_MAX_TOTAL_BYTES - file_bytes) {
      scan.truncated = true;
      break;
    }
    bytes_scanned += file_bytes;
    const std::string text = util::to_std(fa->get_as_text());
    if (!autoload_path.empty() &&
        text.find(autoload_path) != std::string::npos) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["file"] = mcp::JsonValue(fp);
      item["token"] = mcp::JsonValue(autoload_path);
      stale.PushBack(std::move(item));
    }
    if (!autoload_name.empty() &&
        text.find(autoload_name) != std::string::npos) {
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["file"] = mcp::JsonValue(fp);
      item["token"] = mcp::JsonValue(autoload_name);
      stale.PushBack(std::move(item));
    }
  }
  truncated_out = scan.truncated;
  return stale;
}

} // namespace

bool setting_key_is_protected(const std::string &key) {
  const std::string lowered = util::ascii_lowercase(key);
  for (const char *prefix : kProtectedSettingPrefixes) {
    const std::string p(prefix);
    if (lowered.size() >= p.size() && lowered.compare(0, p.size(), p) == 0) {
      return true;
    }
  }
  return false;
}

mcp::JsonValue handle_project_settings_get(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_project_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  std::string name = np->GetString();
  godot::Variant default_var;
  auto *dp = args.Find("default");
  if (dp && !dp->IsNull()) {
    default_var = VariantJson::deserialize(*dp);
  }
  godot::Variant result =
      ps->get_setting(godot::String(name.c_str()), default_var);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_project_settings completed");
  return r;
}

mcp::JsonValue handle_project_settings_set(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_project_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *vp = args.Find("value");
  if (!vp) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: value");
    return e;
  }
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  std::string name = np->GetString();
  if (setting_key_is_protected(name)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "setting key '" + name +
        "' is protected; use the dedicated tool for this setting (e.g. "
        "update_autoloads for autoloads)");
    return e;
  }
  godot::Variant value;
  if (vp->IsObject()) {
    auto *type_field = vp->Find("type");
    auto *value_field = vp->Find("value");
    if (type_field && type_field->IsString() && value_field) {

      value = VariantJson::deserialize(*value_field, type_field->GetString());
    } else {

      value = VariantJson::deserialize(*vp);
    }
  } else if (vp->IsInt()) {

    value = VariantJson::deserialize(*vp, "int");
  } else if (vp->IsDouble()) {

    value = VariantJson::deserialize(*vp, "float");
  } else if (vp->IsBool()) {

    value = VariantJson::deserialize(*vp, "bool");
  } else if (vp->IsString()) {

    auto existing = godot::ProjectSettings::get_singleton()->get_setting(
        godot::String(name.c_str()));
    if (existing.get_type() != godot::Variant::NIL) {
      value = VariantJson::deserialize(
          *vp, util::to_std(godot::Variant::get_type_name(existing.get_type())));
    } else {
      value = VariantJson::deserialize(*vp, "String");
    }
  } else {
    value = VariantJson::deserialize(*vp);
  }
  ps->set_setting(godot::String(name.c_str()), value);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_project_settings completed");
  return r;
}

mcp::JsonValue handle_project_settings_has(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_project_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  std::string name = np->GetString();
  bool has = ps->has_setting(godot::String(name.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(has);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_project_settings completed");
  return r;
}

mcp::JsonValue handle_project_settings_save(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_project_settings called");
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  godot::Error err = ps->save();
  if (err != godot::Error::OK) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("project settings save failed (error code " +
                       std::to_string(static_cast<int64_t>(err)) + ")");
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "save_project_settings completed");
  return r;
}

mcp::JsonValue handle_engine_get_version(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_version called");
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  godot::Dictionary info = engine->get_version_info();
  mcp::JsonValue j(mcp::JsonValue::object_tag);
  if (info.has("major")) {
    j["major"] =
        mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["major"])));
  }
  if (info.has("minor")) {
    j["minor"] =
        mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["minor"])));
  }
  if (info.has("patch")) {
    j["patch"] =
        mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["patch"])));
  }
  if (info.has("string")) {
    godot::String s = info["string"];
    j["string"] = mcp::JsonValue(util::to_std(s));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(j);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_version completed");
  return r;
}

mcp::JsonValue handle_engine_get_fps(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_fps called");
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  double fps = engine->get_frames_per_second();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(fps);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_fps completed");
  return r;
}

mcp::JsonValue handle_engine_get_frames_drawn(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_frames_drawn called");
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  uint64_t count = engine->get_frames_drawn();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(static_cast<int64_t>(count));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_frames_drawn completed");
  return r;
}

mcp::JsonValue handle_engine_set_time_scale(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_engine_time_scale called");
  auto *sp = args.Find("scale");
  if (!sp || !sp->IsNumber()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: scale (number)");
    return e;
  }
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  double scale =
      sp->IsDouble() ? sp->GetDouble() : static_cast<double>(sp->GetInt());
  if (!util::time_scale_is_valid(scale)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "invalid parameter: scale must be a finite number between 0 and " +
        std::to_string(util::kMaxTimeScale));
    return e;
  }
  engine->set_time_scale(scale);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_engine_time_scale completed");
  return r;
}

mcp::JsonValue handle_engine_get_time_scale(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_time_scale called");
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  double scale = engine->get_time_scale();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(scale);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_engine_time_scale completed");
  return r;
}

mcp::JsonValue handle_engine_set_max_fps(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_engine_max_fps called");
  auto *fp = args.Find("fps");
  if (!fp || !fp->IsInt()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: fps (integer)");
    return e;
  }
  auto *engine = godot::Engine::get_singleton();
  if (!engine) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Engine not available");
    return e;
  }
  int64_t fps = fp->GetInt();
  if (!util::max_fps_is_valid(fps)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid parameter: fps must be between " +
                                std::to_string(util::kMinMaxFps) + " and " +
                                std::to_string(util::kMaxMaxFps));
    return e;
  }
  engine->set_max_fps(static_cast<int32_t>(fps));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_engine_max_fps completed");
  return r;
}

mcp::JsonValue handle_editor_settings_get(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorSettings not available");
    return e;
  }
  std::string name = np->GetString();
  godot::Variant result = es->get_setting(godot::String(name.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = VariantJson::serialize(result);
  godot::TypedArray<godot::Dictionary> props = es->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name"))
      continue;
    if (util::to_std(dict["name"].operator godot::String()) != name)
      continue;
    int hint_value = -1;
    if (dict.has("hint")) {
      hint_value = static_cast<int>(dict["hint"]);
      r["hint"] = mcp::JsonValue(static_cast<int64_t>(hint_value));
    }
    std::string hint_string;
    if (dict.has("hint_string"))
      hint_string = util::to_std(dict["hint_string"].operator godot::String());
    if (!hint_string.empty())
      r["hint_string"] = mcp::JsonValue(hint_string);
    if (hint_value == static_cast<int>(godot::PROPERTY_HINT_ENUM) &&
        !hint_string.empty()) {
      r["enum_options"] = parse_enum_hint_options(hint_string);
    }
    break;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_editor_settings completed");
  return r;
}

mcp::JsonValue handle_editor_settings_set(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *vp = args.Find("value");
  if (!vp) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: value");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorSettings not available");
    return e;
  }
  std::string name = np->GetString();
  godot::Variant value = VariantJson::deserialize(*vp);
  es->set_setting(godot::String(name.c_str()), value);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_editor_settings completed");
  return r;
}

mcp::JsonValue handle_editor_settings_has(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_editor_settings called");
  auto *np = args.Find("name");
  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
    return e;
  }
  auto es = editor->get_editor_settings();
  if (es.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorSettings not available");
    return e;
  }
  std::string name = np->GetString();
  bool has = es->has_setting(godot::String(name.c_str()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(has);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "has_editor_settings completed");
  return r;
}

mcp::JsonValue handle_autoload_list(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_autoload_list called");
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  std::vector<AutoloadEntry> entries;
  collect_autoloads(ps, entries);
  mcp::JsonValue list(mcp::JsonValue::array_tag);
  for (const AutoloadEntry &entry : entries) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["name"] = mcp::JsonValue(entry.name);
    item["path"] = mcp::JsonValue(entry.path);
    item["enabled"] = mcp::JsonValue(entry.enabled);
    item["order"] = mcp::JsonValue(static_cast<int64_t>(entry.order));
    list.PushBack(std::move(item));
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(list);
  r["count"] = mcp::JsonValue(static_cast<int64_t>(entries.size()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_autoload_list completed");
  return r;
}

mcp::JsonValue handle_autoload_add(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_autoload called");
  auto *np = args.Find("name");
  if (!np || !np->IsString() || np->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString() || pp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  const std::string name = np->GetString();
  if (name.find('/') != std::string::npos) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid autoload name '" + name +
                                "': must not contain '/'");
    return e;
  }
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  const std::string key = std::string(kAutoloadSettingPrefix) + name;
  if (ps->has_setting(godot::String(key.c_str()))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("autoload already exists: " + name);
    return e;
  }
  const util::ProjectPath checked =
      util::normalize_project_path(pp->GetString(), false, false);
  if (!checked.valid()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid autoload path '" +
                                pp->GetString() + "': " + checked.error);
    return e;
  }
  if (checked.value.compare(0, 6, "res://") != 0) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("autoload path must be inside res:// (got '" +
                                checked.value + "')");
    return e;
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }
  const godot::String target(checked.value.c_str());
  if (!loader->exists(target)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "autoload file does not exist or cannot be loaded: " + checked.value);
    return e;
  }
  godot::Ref<godot::Resource> res = loader->load(target);
  bool instantiable = false;
  if (!res.is_null()) {
    auto *packed = godot::Object::cast_to<godot::PackedScene>(res.ptr());
    auto *script = godot::Object::cast_to<godot::Script>(res.ptr());
    if (packed) {
      instantiable = packed->can_instantiate();
    } else if (script) {
      instantiable = script->can_instantiate();
    }
  }
  if (!instantiable) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "autoload path must be a scene or script that can be instantiated: " +
        checked.value);
    return e;
  }
  bool enabled = true;
  auto *ep = args.Find("enabled");
  if (ep && ep->IsBool()) {
    enabled = ep->GetBool();
  }
  std::vector<AutoloadEntry> existing;
  collect_autoloads(ps, existing);
  int32_t order = 0;
  for (const AutoloadEntry &entry : existing) {
    if (entry.order >= order) {
      order = entry.order + 1;
    }
  }
  const std::string stored =
      (enabled ? std::string("*") : std::string()) + checked.value;
  ps->set_setting(godot::String(key.c_str()),
                  godot::Variant(godot::String(stored.c_str())));
  ps->set_order(godot::String(key.c_str()), order);
  const godot::Error save_err = ps->save();
  if (save_err != godot::Error::OK) {
    return autoload_save_error(save_err);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["name"] = mcp::JsonValue(name);
  r["path"] = mcp::JsonValue(checked.value);
  r["enabled"] = mcp::JsonValue(enabled);
  r["order"] = mcp::JsonValue(static_cast<int64_t>(order));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "add_autoload completed");
  return r;
}

mcp::JsonValue handle_autoload_remove(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_autoload called");
  auto *np = args.Find("name");
  if (!np || !np->IsString() || np->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  const std::string name = np->GetString();
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  const std::string key = std::string(kAutoloadSettingPrefix) + name;
  const godot::String key_gs(key.c_str());
  if (!ps->has_setting(key_gs)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("autoload not found: " + name);
    return e;
  }
  std::string old_path;
  bool old_enabled = true;
  const godot::Variant old_value = ps->get_setting(key_gs);
  if (old_value.get_type() == godot::Variant::STRING) {
    const std::string raw = util::to_std(old_value.operator godot::String());
    old_enabled = !raw.empty() && raw[0] == '*';
    old_path = old_enabled ? raw.substr(1) : raw;
  }
  bool scan_truncated = false;
  mcp::JsonValue stale = scan_autoload_refs(name, old_path, scan_truncated);
  ps->clear(key_gs);
  const godot::Error save_err = ps->save();
  if (save_err != godot::Error::OK) {
    return autoload_save_error(save_err);
  }
  mcp::JsonValue removed(mcp::JsonValue::object_tag);
  removed["name"] = mcp::JsonValue(name);
  removed["path"] = mcp::JsonValue(old_path);
  removed["enabled"] = mcp::JsonValue(old_enabled);
  mcp::JsonValue updated(mcp::JsonValue::array_tag);
  mcp::JsonValue updated_item(mcp::JsonValue::object_tag);
  updated_item["file"] = mcp::JsonValue("res://project.godot");
  mcp::JsonValue changes(mcp::JsonValue::array_tag);
  changes.PushBack(mcp::JsonValue("removed " + key));
  updated_item["changes"] = std::move(changes);
  updated.PushBack(std::move(updated_item));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["removed"] = std::move(removed);
  r["updated_files"] = std::move(updated);
  r["stale_references"] = std::move(stale);
  if (scan_truncated) {
    r["scan_truncated"] = mcp::JsonValue(true);
  }
  r["note"] = mcp::JsonValue(
      std::string("updated_files lists project files rewritten by the "
                  "removal; stale_references entries are {file, token} "
                  "objects where token is the leftover autoload name or path "
                  "text found in .tscn/.tres/.gd files — name matches are "
                  "substring-based and may include false positives; review "
                  "them before deleting the autoload target file"));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "remove_autoload completed");
  return r;
}

mcp::JsonValue handle_autoload_move(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_autoload called");
  auto *np = args.Find("name");
  if (!np || !np->IsString() || np->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *ip = args.Find("new_index");
  if (!ip || !ip->IsInt()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("missing required parameter: new_index (integer)");
    return e;
  }
  const std::string name = np->GetString();
  const int64_t new_index = ip->GetInt();
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ProjectSettings not available");
    return e;
  }
  std::vector<AutoloadEntry> entries;
  collect_autoloads(ps, entries);
  const int64_t count = static_cast<int64_t>(entries.size());
  const int64_t old_index = find_autoload(entries, name);
  if (old_index < 0) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("autoload not found: " + name);
    return e;
  }
  if (new_index < 0 || new_index >= count) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("new_index out of range (got " +
                                std::to_string(new_index) +
                                ", expected 0.." + std::to_string(count - 1) +
                                ")");
    return e;
  }
  if (new_index != old_index) {
    AutoloadEntry moved = entries[static_cast<size_t>(old_index)];
    entries.erase(entries.begin() + old_index);
    entries.insert(entries.begin() + new_index, moved);
    for (int64_t i = 0; i < count; i++) {
      const std::string entry_key =
          std::string(kAutoloadSettingPrefix) +
          entries[static_cast<size_t>(i)].name;
      ps->set_order(godot::String(entry_key.c_str()),
                    static_cast<int32_t>(i));
    }
    const godot::Error save_err = ps->save();
    if (save_err != godot::Error::OK) {
      return autoload_save_error(save_err);
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["name"] = mcp::JsonValue(name);
  r["old_index"] = mcp::JsonValue(old_index);
  r["new_index"] = mcp::JsonValue(new_index);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_autoload completed");
  return r;
}

namespace {

mcp::JsonValue config_action_error(const std::string &message) {
  mcp::JsonValue error(mcp::JsonValue::object_tag);
  error["error"] = mcp::JsonValue(message);
  return error;
}

mcp::JsonValue strip_config_action(const mcp::JsonValue &args) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out.GetObject().erase("action");
  return out;
}

bool read_config_action(const mcp::JsonValue &args, const char *fallback,
                        std::string &out, mcp::JsonValue &error) {
  const mcp::JsonValue *action = args.Find("action");
  if (action == nullptr) {
    if (fallback == nullptr) {
      error = config_action_error("missing required parameter: action");
      return false;
    }
    out = fallback;
    return true;
  }
  if (!action->IsString()) {
    error = config_action_error("invalid parameter: action must be a string");
    return false;
  }
  out = action->GetString();
  return true;
}

mcp::JsonValue add_found_flag(mcp::JsonValue result) {
  if (result.IsObject() && result.Find("error") == nullptr) {
    const mcp::JsonValue *value = result.Find("result");
    if (value != nullptr && value->IsBool()) {
      result["found"] = *value;
    }
  }
  return result;
}

} // namespace

mcp::JsonValue handle_get_project_settings(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, "get", action, error)) {
    return error;
  }
  if (action == "get") {
    return handle_project_settings_get(strip_config_action(args));
  }
  if (action == "has") {
    return add_found_flag(handle_project_settings_has(strip_config_action(args)));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for get_project_settings (expected get, has)");
}

mcp::JsonValue handle_update_project_settings(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "set") {
    return handle_project_settings_set(strip_config_action(args));
  }
  if (action == "save") {
    return handle_project_settings_save(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for update_project_settings (expected set, save)");
}

mcp::JsonValue handle_get_editor_settings(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, "get", action, error)) {
    return error;
  }
  if (action == "get") {
    return handle_editor_settings_get(strip_config_action(args));
  }
  if (action == "has") {
    return add_found_flag(handle_editor_settings_has(strip_config_action(args)));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for get_editor_settings (expected get, has)");
}

mcp::JsonValue handle_update_editor_settings(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "set") {
    return handle_editor_settings_set(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for update_editor_settings (expected set)");
}

mcp::JsonValue handle_get_engine_state(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "version") {
    return handle_engine_get_version(strip_config_action(args));
  }
  if (action == "fps") {
    return handle_engine_get_fps(strip_config_action(args));
  }
  if (action == "frames_drawn") {
    return handle_engine_get_frames_drawn(strip_config_action(args));
  }
  if (action == "time_scale") {
    return handle_engine_get_time_scale(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for get_engine_state (expected version, fps, frames_drawn, "
      "time_scale)");
}

mcp::JsonValue handle_update_engine_state(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "time_scale") {
    return handle_engine_set_time_scale(strip_config_action(args));
  }
  if (action == "max_fps") {
    return handle_engine_set_max_fps(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for update_engine_state (expected time_scale, max_fps)");
}

mcp::JsonValue handle_get_autoloads(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "list") {
    return handle_autoload_list(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action + "' for get_autoloads (expected list)");
}

mcp::JsonValue handle_update_autoloads(const mcp::JsonValue &args) {
  std::string action;
  mcp::JsonValue error;
  if (!read_config_action(args, nullptr, action, error)) {
    return error;
  }
  if (action == "add") {
    return handle_autoload_add(strip_config_action(args));
  }
  if (action == "remove") {
    return handle_autoload_remove(strip_config_action(args));
  }
  if (action == "move") {
    return handle_autoload_move(strip_config_action(args));
  }
  return config_action_error(
      "unknown action '" + action +
      "' for update_autoloads (expected add, remove, move)");
}

} // namespace config_ops
} // namespace godot_autopilot
