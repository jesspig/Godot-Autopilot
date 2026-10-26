#include "sanitize_policy.hpp"

#include <cstdlib>

#include "plugin_config.hpp"

namespace godot_autopilot {
namespace sanitize_policy {

void initialize() {
  set_enabled(resolve_enabled(std::getenv(kEnvKey),
                              PluginConfig::load_desensitize()));
}

} // namespace sanitize_policy
} // namespace godot_autopilot
