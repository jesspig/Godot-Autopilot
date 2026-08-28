#ifndef GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP
#define GODOT_AUTOPILOT_PLUGIN_CONFIG_HPP

namespace godot_autopilot {

class PluginConfig {
public:
  static int load_port();
  static bool save_port(int port);

  static bool load_show_time();
  static bool save_show_time(bool show);
};

} // namespace godot_autopilot

#endif
