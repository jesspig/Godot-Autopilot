#ifndef GODOT_AUTOPILOT_AUTHORIZATION_SWITCHES_HPP
#define GODOT_AUTOPILOT_AUTHORIZATION_SWITCHES_HPP

#include <cstddef>
#include <string_view>

#include "tools/authorization.hpp"

namespace godot_autopilot {
namespace authorization_switches {

struct SwitchLabel {
  std::string_view capability;
  std::string_view label;
  std::string_view tooltip;
};

inline constexpr SwitchLabel kSwitchLabels[] = {
    {authorization::kCapabilityCodeExecute, "Allow code_execute:",
     "Allow code_execute (and the execute_script tool): read on the next "
     "tool call, no restart needed. When the GODOT_AUTOPILOT_ALLOW "
     "environment variable is set it wins over this setting."},
    {authorization::kCapabilityGameRuntime, "Allow game_runtime:",
     "Allow game_runtime (and the game runtime tools): read on the next "
     "tool call, no restart needed. When the GODOT_AUTOPILOT_ALLOW "
     "environment variable is set it wins over this setting."},
    {authorization::kCapabilityUserTools, "Allow user tools:",
     "Allow user tools registered from scripts through the AutopilotTools "
     "engine singleton; read on the next tool call, no restart needed. When "
     "the GODOT_AUTOPILOT_ALLOW environment variable is set it wins over "
     "this setting."},
    {authorization::kCapabilityWrite, "Allow write:",
     "Allow the write tier (write files, write project or editor config, "
     "show alerts, modify the editor window, inject mouse and keyboard "
     "input): read on the next tool call, no restart needed. When the "
     "GODOT_AUTOPILOT_ALLOW environment variable is set it wins over this "
     "setting."},
    {authorization::kCapabilityDestructive, "Allow destructive:",
     "Allow the destructive tier: irreversible operations that destroy an "
     "object or entity that already exists inside the editor host, such as "
     "delete_scene_node, delete_display_window, remove_animation, "
     "remove_animation_track, remove_audio_bus_effect and "
     "remove_debug_custom_monitor. Read on the next tool call, no restart "
     "needed. When the GODOT_AUTOPILOT_ALLOW environment variable is set it "
     "wins over this setting."},
    {authorization::kCapabilityProcess, "Allow process:",
     "Allow the process tier (dangerous: runs external programs and OS "
     "commands such as create_os_process, execute_os_process and "
     "run_os_shell): read on the next tool call, no restart needed. When the "
     "GODOT_AUTOPILOT_ALLOW environment variable is set it wins over this "
     "setting."},
};

inline const SwitchLabel *find_switch_label(std::string_view capability) {
  for (const SwitchLabel &entry : kSwitchLabels) {
    if (entry.capability == capability) {
      return &entry;
    }
  }
  return nullptr;
}

inline bool switch_initial_state(std::string_view capability) {
  return authorization::capability_enabled(capability);
}

} // namespace authorization_switches
} // namespace godot_autopilot
#endif
