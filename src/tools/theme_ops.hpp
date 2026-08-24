#ifndef GODOT_AUTOPILOT_THEME_OPS_HPP
#define GODOT_AUTOPILOT_THEME_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace theme_ops {

mcp::JsonValue handle_create_theme_resource(const mcp::JsonValue &args);
mcp::JsonValue handle_set_theme_color(const mcp::JsonValue &args);
mcp::JsonValue handle_set_theme_constant(const mcp::JsonValue &args);
mcp::JsonValue handle_set_theme_font_size(const mcp::JsonValue &args);
mcp::JsonValue handle_set_theme_stylebox_flat(const mcp::JsonValue &args);
mcp::JsonValue handle_get_theme_info(const mcp::JsonValue &args);
mcp::JsonValue handle_apply_theme_to_control(const mcp::JsonValue &args);
mcp::JsonValue handle_set_control_anchor_preset(const mcp::JsonValue &args);

} // namespace theme_ops
} // namespace godot_autopilot

#endif
