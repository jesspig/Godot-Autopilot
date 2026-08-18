#ifndef GODOT_AUTOPILOT_SCENE_TREE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_TREE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace scene_tree_ops {

mcp::JsonValue handle_call_group(const mcp::JsonValue &args);
mcp::JsonValue handle_create_timer(const mcp::JsonValue &args);
mcp::JsonValue handle_get_nodes_in_group(const mcp::JsonValue &args);
mcp::JsonValue handle_is_paused(const mcp::JsonValue &args);
mcp::JsonValue handle_notify_group(const mcp::JsonValue &args);
mcp::JsonValue handle_reload_current_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_set_debug_collisions(const mcp::JsonValue &args);
mcp::JsonValue handle_set_pause(const mcp::JsonValue &args);

} // namespace scene_tree_ops
} // namespace godot_autopilot

#endif
