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
  godot::DirAccess::make_dir_recursive_absolute(kConfigDir);
  godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(kConfigPath, godot::FileAccess::WRITE);
  if (file.is_null()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "Plugin config write failed: " +
                                  std::string(kConfigPath));
    return false;
  }
  mcp::JsonValue::Object doc;
  doc[kPortKey] = mcp::JsonValue(static_cast<int64_t>(port));
  file->store_string(
      godot::String(mcp::JsonValue(std::move(doc)).Dump(2).c_str()));
  file->close();
  return true;
}

} // namespace godot_autopilot