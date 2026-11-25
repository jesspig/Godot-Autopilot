#ifndef GODOT_AUTOPILOT_INPUT_OPS_HPP
#define GODOT_AUTOPILOT_INPUT_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace input_ops {

godot_autopilot::util::JsonValue handle_get_input_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_input_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_input_key(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_input_gamepad(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_action_press(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_action_release(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_is_action_pressed(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_is_action_just_pressed(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_key_press(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_key_release(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_move(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_button_press(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mouse_button_release(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_gamepad_vibration_start(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_gamepad_vibration_stop(const godot_autopilot::util::JsonValue &args);

} // namespace input_ops
} // namespace godot_autopilot
#endif
