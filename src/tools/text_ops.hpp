#ifndef GODOT_AUTOPILOT_TEXT_OPS_HPP
#define GODOT_AUTOPILOT_TEXT_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace text_ops {

mcp::JsonValue handle_create_font(const mcp::JsonValue &args);
mcp::JsonValue handle_create_shaped_text(const mcp::JsonValue &args);
mcp::JsonValue handle_font_set_antialiasing(const mcp::JsonValue &args);
mcp::JsonValue handle_font_set_data(const mcp::JsonValue &args);
mcp::JsonValue handle_font_set_hinting(const mcp::JsonValue &args);
mcp::JsonValue handle_get_system_font_path(const mcp::JsonValue &args);
mcp::JsonValue handle_has_feature(const mcp::JsonValue &args);
mcp::JsonValue handle_is_locale_right_to_left(const mcp::JsonValue &args);
mcp::JsonValue handle_shaped_text_add_string(const mcp::JsonValue &args);
mcp::JsonValue handle_shaped_text_get_size(const mcp::JsonValue &args);
mcp::JsonValue handle_file_write(const mcp::JsonValue &args);

} // namespace text_ops
} // namespace godot_autopilot

#endif
