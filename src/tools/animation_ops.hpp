#ifndef GODOT_AUTOPILOT_ANIMATION_OPS_HPP
#define GODOT_AUTOPILOT_ANIMATION_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace animation_ops {

mcp::JsonValue handle_create_player(const mcp::JsonValue &args);
mcp::JsonValue handle_create_animation(const mcp::JsonValue &args);
mcp::JsonValue handle_remove_animation(const mcp::JsonValue &args);
mcp::JsonValue handle_get_list(const mcp::JsonValue &args);
mcp::JsonValue handle_create_track(const mcp::JsonValue &args);
mcp::JsonValue handle_insert_keyframe(const mcp::JsonValue &args);
mcp::JsonValue handle_remove_track(const mcp::JsonValue &args);
mcp::JsonValue handle_create_tree(const mcp::JsonValue &args);
mcp::JsonValue handle_add_state(const mcp::JsonValue &args);
mcp::JsonValue handle_connect_states(const mcp::JsonValue &args);

} // namespace animation_ops
} // namespace godot_autopilot

#endif
