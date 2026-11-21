#ifndef GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP
#define GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP

#include <string>
#include <utility>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

enum class ConfigReadStatus { kOk, kMissing, kParseFailed };

class PluginConfig {
public:
  static int load_port();
  static bool save_port(int port);

  static bool load_show_time();
  static bool save_show_time(bool show);

  static bool load_desensitize();
  static bool save_desensitize(bool value);

  static bool merge_value(ConfigReadStatus status,
                          const mcp::JsonValue::Object &doc, const char *key,
                          const mcp::JsonValue &value, std::string &out_text);
};

inline bool PluginConfig::merge_value(ConfigReadStatus status,
                                      const mcp::JsonValue::Object &doc,
                                      const char *key,
                                      const mcp::JsonValue &value,
                                      std::string &out_text) {
  if (status == ConfigReadStatus::kParseFailed) {
    return false;
  }
  mcp::JsonValue::Object merged = doc;
  merged[key] = value;
  out_text = mcp::JsonValue(std::move(merged)).Dump(2);
  return true;
}

} // namespace godot_autopilot

#endif
