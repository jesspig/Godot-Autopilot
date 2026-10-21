#ifndef GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP
#define GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP

#include <string>

namespace godot_autopilot {

class PluginConfig {
public:
  static int load_port();
  static bool save_port(int port);

  static bool load_show_time();
  static bool save_show_time(bool show);

  static bool load_desensitize();
  static bool save_desensitize(bool value);

  static std::string load_allow();
  static bool save_allow(const std::string &allow);

  static std::string load_auth_mode();
  static bool save_auth_mode(const std::string &mode);
};

} // namespace godot_autopilot

#endif
