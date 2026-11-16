#ifndef GODOT_AUTOPILOT_IMAGE_OPS_HPP
#define GODOT_AUTOPILOT_IMAGE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace image_ops {

mcp::JsonValue handle_edit_action(const mcp::JsonValue &args);
mcp::JsonValue handle_pixel_action(const mcp::JsonValue &args);
mcp::JsonValue handle_save_action(const mcp::JsonValue &args);
mcp::JsonValue handle_texture_gen_action(const mcp::JsonValue &args);

} // namespace image_ops
} // namespace godot_autopilot

#endif
