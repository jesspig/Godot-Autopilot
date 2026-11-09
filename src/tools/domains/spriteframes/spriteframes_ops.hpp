#ifndef GODOT_AUTOPILOT_SPRITEFRAMES_OPS_HPP
#define GODOT_AUTOPILOT_SPRITEFRAMES_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace spriteframes_ops {

mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_add_animation(const mcp::JsonValue &args);
mcp::JsonValue handle_add_frame(const mcp::JsonValue &args);

} // namespace spriteframes_ops
} // namespace godot_autopilot

#endif
