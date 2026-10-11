#include "sanitize_policy.hpp"

#include <cstdlib>
#include <string>

#include "plugin_config.hpp"

namespace godot_autopilot {
namespace sanitize_policy {

namespace {

constexpr const char *kEnvKey = "GODOT_AUTOPILOT_DESENSITIZE";

bool parse_env_flag(const char *raw) {
  const std::string value(raw);
  return !(value == "0" || value == "false" || value == "off");
}

} // namespace

void initialize() {
  const char *raw = std::getenv(kEnvKey);
  if (raw != nullptr && *raw != '\0') {
    set_enabled(parse_env_flag(raw));
    return;
  }
  set_enabled(PluginConfig::load_desensitize());
}

} // namespace sanitize_policy
} // namespace godot_autopilot
