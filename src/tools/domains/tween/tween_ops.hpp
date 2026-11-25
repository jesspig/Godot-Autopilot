#ifndef GODOT_AUTOPILOT_TWEEN_OPS_HPP
#define GODOT_AUTOPILOT_TWEEN_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace tween_ops {

godot_autopilot::util::JsonValue handle_create_game_tween(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_control_game_tween(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_tween_bundle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_game_tween_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tween_editor_property(const godot_autopilot::util::JsonValue &args);

} // namespace tween_ops
} // namespace godot_autopilot

#endif
