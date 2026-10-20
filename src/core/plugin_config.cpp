#include "plugin_config.hpp"

#include <exception>
#include <string>
#include <utility>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include <mcp/JsonValue.hpp>
#include <mcp/McpError.hpp>

#include "core/path_util.hpp"
#include "core/storage_paths.hpp"
#include "log_system.hpp"
#include "tools/authorization.hpp"
#include "util/project_path.hpp"

namespace godot_autopilot {

namespace {
constexpr const char *kPortKey = "port";
constexpr const char *kShowTimeKey = "show_time";
constexpr const char *kDesensitizeKey = "desensitize";
constexpr const char *kAllowKey = "allow";
} // namespace

namespace {

enum class ConfigReadStatus { kOk, kMissing, kParseFailed };

std::string global_config_file_path() {
  return StoragePaths::global_config_path();
}

std::string project_config_file_path() {
  return StoragePaths::project_config_path(util::current_project_root_path());
}

ConfigReadStatus read_config_object(const std::string &path,
                                    mcp::JsonValue::Object &doc,
                                    std::string &error) {
  error.clear();
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(godot::String::utf8(path.c_str()),
                              godot::FileAccess::READ);
  if (file.is_null()) {
    return ConfigReadStatus::kMissing;
  }
  godot::String content = file->get_as_text();
  file->close();
  try {
    mcp::JsonValue parsed = mcp::JsonValue::Parse(content.utf8().get_data());
    if (!parsed.IsObject()) {
      error = "config root is not a JSON object";
      return ConfigReadStatus::kParseFailed;
    }
    doc = std::move(parsed.GetObject());
    return ConfigReadStatus::kOk;
  } catch (const mcp::McpError &e) {
    error = std::string(e.ErrorMessage());
  } catch (const std::exception &e) {
    error = e.what();
  } catch (...) {
    error = "unknown exception";
  }
  return ConfigReadStatus::kParseFailed;
}

bool find_config_value(const std::string &path, const char *key,
                       mcp::JsonValue &out) {
  mcp::JsonValue::Object doc;
  std::string error;
  if (read_config_object(path, doc, error) == ConfigReadStatus::kParseFailed) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System, "Plugin config parse failed",
        std::string("path=") + path + " reason=" + error);
    return false;
  }
  const mcp::JsonValue document(std::move(doc));
  const mcp::JsonValue *found = document.Find(key);
  if (found == nullptr) {
    return false;
  }
  out = *found;
  return true;
}

bool write_config_value(const std::string &path, const char *key,
                        const mcp::JsonValue &value) {
  if (path.empty()) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System,
        "Plugin config write rejected: storage path unavailable",
        std::string("key=") + key);
    return false;
  }
  const std::string dir = parent_dir(path, 1);
  if (dir.empty() ||
      godot::DirAccess::make_dir_recursive_absolute(
          godot::String::utf8(dir.c_str())) != godot::OK) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System,
        "Plugin config directory creation failed",
        std::string("path=") + path);
    return false;
  }
  mcp::JsonValue::Object doc;
  std::string error;
  if (read_config_object(path, doc, error) == ConfigReadStatus::kParseFailed) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System,
        "Plugin config parse failed, overwriting",
        std::string("path=") + path + " reason=" + error);
  }
  doc[key] = value;
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(godot::String::utf8(path.c_str()),
                              godot::FileAccess::WRITE);
  if (file.is_null()) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System, "Plugin config write failed",
        std::string("path=") + path +
            " reason=FileAccess::open returned null");
    return false;
  }
  file->store_string(
      godot::String(mcp::JsonValue(std::move(doc)).Dump(2).c_str()));
  file->close();
  return true;
}

} // namespace

namespace {
struct AllowProviderRegistration {
  AllowProviderRegistration() {
    authorization::set_allow_provider(&PluginConfig::load_allow);
  }
} g_allow_provider_registration;
} // namespace

int PluginConfig::load_port() {
  const std::string path = project_config_file_path();
  if (path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "Plugin config path unavailable for port lookup");
    return -1;
  }
  mcp::JsonValue value;
  if (!find_config_value(path, kPortKey, value) || !value.IsInt()) {
    return -1;
  }
  return static_cast<int>(value.GetInt());
}

bool PluginConfig::save_port(int port) {
  return write_config_value(project_config_file_path(), kPortKey,
                             mcp::JsonValue(static_cast<int64_t>(port)));
}

bool PluginConfig::load_show_time() {
  const std::string path = global_config_file_path();
  if (path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "Plugin config path unavailable for show_time lookup");
    return true;
  }
  mcp::JsonValue value;
  if (!find_config_value(path, kShowTimeKey, value) || !value.IsBool()) {
    return true;
  }
  return value.GetBool();
}

bool PluginConfig::save_show_time(bool show) {
  return write_config_value(global_config_file_path(), kShowTimeKey,
                             mcp::JsonValue(show));
}

bool PluginConfig::load_desensitize() {
  const std::string path = global_config_file_path();
  if (path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "Plugin config path unavailable for desensitize lookup");
    return true;
  }
  mcp::JsonValue value;
  if (!find_config_value(path, kDesensitizeKey, value) || !value.IsBool()) {
    return true;
  }
  return value.GetBool();
}

bool PluginConfig::save_desensitize(bool value) {
  return write_config_value(global_config_file_path(), kDesensitizeKey,
                             mcp::JsonValue(value));
}

std::string PluginConfig::load_allow() {
  const std::string path = global_config_file_path();
  if (path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "Plugin config path unavailable for allow lookup");
    return std::string();
  }
  mcp::JsonValue value;
  if (!find_config_value(path, kAllowKey, value) || !value.IsString()) {
    return std::string();
  }
  return value.GetString();
}

bool PluginConfig::save_allow(const std::string &allow) {
  return write_config_value(global_config_file_path(), kAllowKey,
                             mcp::JsonValue(allow));
}

} // namespace godot_autopilot
