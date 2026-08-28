#include "plugin_config.hpp"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>

#include <mcp/JsonValue.hpp>

#include "log_system.hpp"

namespace godot_autopilot {

namespace {
constexpr const char *kConfigPath = "user://godot_autopilot/config.json";
constexpr const char *kConfigDir = "user://godot_autopilot";
constexpr const char *kPortKey = "port";
constexpr const char *kShowTimeKey = "show_time";
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
    } catch (...) {
      LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                                "Plugin config parse failed, overwriting: " +
                                    std::string(kConfigPath));
    }
  }

  doc[key] = value;
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::WRITE);
  if (file.is_null()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config write failed: " +
                                  std::string(kConfigPath));
    return false;
  }
  file->store_string(
      godot::String(mcp::JsonValue(std::move(doc)).Dump(2).c_str()));
  file->close();
  return true;
}
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

} // namespace godot_autopilot