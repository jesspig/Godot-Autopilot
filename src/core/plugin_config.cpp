#include "plugin_config.hpp"

#include <exception>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include <mcp/JsonValue.hpp>

#include "log_system.hpp"
#include "tools/authorization.hpp"

namespace godot_autopilot {

namespace {
constexpr const char *kConfigPath = "user://godot_autopilot/config.json";
constexpr const char *kConfigDir = "user://godot_autopilot";
constexpr const char *kPortKey = "port";
constexpr const char *kShowTimeKey = "show_time";
constexpr const char *kDesensitizeKey = "desensitize";
constexpr const char *kAllowKey = "allow";
} // namespace

namespace {
bool save_config_value(const char *key, const mcp::JsonValue &value) {
  godot::DirAccess::make_dir_recursive_absolute(kConfigDir);

  mcp::JsonValue::Object doc;
  godot::Ref<godot::FileAccess> read =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::READ);
  if (!read.is_null()) {
    godot::String content = read->get_as_text();
    read->close();
    try {
      mcp::JsonValue existing =
          mcp::JsonValue::Parse(content.utf8().get_data());
      if (existing.IsObject()) {
        doc = std::move(existing.GetObject());
      }
    } catch (const std::exception &e) {
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::System,
          "Plugin config parse failed, overwriting",
          std::string("path=") + kConfigPath + " reason=" + e.what());
    } catch (...) {
      LogSystem::instance().log_detailed(
          LogLevel::Error, LogCategory::System,
          "Plugin config parse failed, overwriting",
          std::string("path=") + kConfigPath + " reason=unknown exception");
    }
  }

  doc[key] = value;
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::WRITE);
  if (file.is_null()) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System, "Plugin config write failed",
        std::string("path=") + kConfigPath +
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
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::READ);
  if (file.is_null()) {
    return -1;
  }
  godot::String content = file->get_as_text();
  file->close();

  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(content.utf8().get_data());
  } catch (...) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config parse failed: " +
                                  std::string(kConfigPath));
    return -1;
  }
  const auto *port = doc.Find(kPortKey);
  if (port == nullptr || !port->IsInt()) {
    return -1;
  }
  return static_cast<int>(port->GetInt());
}

bool PluginConfig::save_port(int port) {
  return save_config_value(kPortKey,
                           mcp::JsonValue(static_cast<int64_t>(port)));
}

bool PluginConfig::load_show_time() {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::READ);
  if (file.is_null()) {
    return true;
  }
  godot::String content = file->get_as_text();
  file->close();

  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(content.utf8().get_data());
  } catch (...) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config parse failed: " +
                                  std::string(kConfigPath));
    return true;
  }
  const auto *show = doc.Find(kShowTimeKey);
  if (show == nullptr || !show->IsBool()) {
    return true;
  }
  return show->GetBool();
}

bool PluginConfig::save_show_time(bool show) {
  return save_config_value(kShowTimeKey, mcp::JsonValue(show));
}

bool PluginConfig::load_desensitize() {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::READ);
  if (file.is_null()) {
    return true;
  }
  godot::String content = file->get_as_text();
  file->close();

  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(content.utf8().get_data());
  } catch (...) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config parse failed: " +
                                  std::string(kConfigPath));
    return true;
  }
  const auto *desensitize = doc.Find(kDesensitizeKey);
  if (desensitize == nullptr || !desensitize->IsBool()) {
    return true;
  }
  return desensitize->GetBool();
}

bool PluginConfig::save_desensitize(bool value) {
  return save_config_value(kDesensitizeKey, mcp::JsonValue(value));
}

std::string PluginConfig::load_allow() {
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::READ);
  if (file.is_null()) {
    return std::string();
  }
  godot::String content = file->get_as_text();
  file->close();

  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(content.utf8().get_data());
  } catch (...) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config parse failed: " +
                                  std::string(kConfigPath));
    return std::string();
  }
  const auto *allow = doc.Find(kAllowKey);
  if (allow == nullptr || !allow->IsString()) {
    return std::string();
  }
  return allow->GetString();
}

bool PluginConfig::save_allow(const std::string &allow) {
  return save_config_value(kAllowKey, mcp::JsonValue(allow));
}

} // namespace godot_autopilot
