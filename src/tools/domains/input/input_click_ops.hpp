#ifndef GODOT_AUTOPILOT_INPUT_CLICK_OPS_HPP
#define GODOT_AUTOPILOT_INPUT_CLICK_OPS_HPP

#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace input_click_ops {

godot::Key parse_key_name(const std::string &key_str);
godot::MouseButton parse_mouse_button_name(const std::string &button_str);
bool warp_to(godot::Vector2 pos);
bool inject_mouse_motion(godot::Vector2 pos, godot::Vector2 relative,
                         int window_id = 0);
bool inject_mouse_button(godot::Vector2 pos, godot::MouseButton button,
                         bool pressed, int window_id = 0);
bool inject_wheel(godot::Vector2 pos, godot::MouseButton wheel, int amount,
                  int window_id = 0);
bool inject_key(godot::Key key, bool pressed, bool ctrl, bool shift, bool alt,
                int window_id = 0);
mcp::JsonValue handle_click_mouse(const mcp::JsonValue &args);
mcp::JsonValue handle_scroll_mouse(const mcp::JsonValue &args);
mcp::JsonValue handle_drag_mouse(const mcp::JsonValue &args);
mcp::JsonValue handle_type_text(const mcp::JsonValue &args);

} // namespace input_click_ops
} // namespace godot_autopilot

#endif
