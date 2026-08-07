#ifndef GODOT_SELF_DRIVING_INPUT_OPS_HPP
#define GODOT_SELF_DRIVING_INPUT_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace input_ops {

mcp::JsonValue handle_action_press(const mcp::JsonValue &args);
mcp::JsonValue handle_action_release(const mcp::JsonValue &args);
mcp::JsonValue handle_is_action_pressed(const mcp::JsonValue &args);
mcp::JsonValue handle_is_action_just_pressed(const mcp::JsonValue &args);
mcp::JsonValue handle_key_press(const mcp::JsonValue &args);
mcp::JsonValue handle_key_release(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_move(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_button_press(const mcp::JsonValue &args);
mcp::JsonValue handle_mouse_button_release(const mcp::JsonValue &args);
mcp::JsonValue handle_gamepad_simulate(const mcp::JsonValue &args);

} // namespace input_ops
} // namespace godot_self_driving
#endif
