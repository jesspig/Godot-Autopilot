#ifndef GODOT_AUTOPILOT_MONITOR_OPS_HPP
#define GODOT_AUTOPILOT_MONITOR_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace monitor_ops {

godot_autopilot::util::JsonValue handle_game_monitor(const godot_autopilot::util::JsonValue &args);

} // namespace monitor_ops
} // namespace godot_autopilot

#endif
