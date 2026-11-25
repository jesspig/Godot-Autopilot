#ifndef GODOT_AUTOPILOT_DEBUG_OPS_HPP
#define GODOT_AUTOPILOT_DEBUG_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace debug_ops {

godot_autopilot::util::JsonValue handle_print(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_print_stack(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_physics_fps(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_collision_debug(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_navigation_debug(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_performance_debug(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_all_monitors(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_remove_custom_monitor(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_custom_monitor(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_visuals(const godot_autopilot::util::JsonValue &args);

} // namespace debug_ops
} // namespace godot_autopilot
#endif
