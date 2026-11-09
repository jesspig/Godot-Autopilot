#ifndef GODOT_AUTOPILOT_SCENE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace scene_ops {

mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_delete(const mcp::JsonValue &args);
mcp::JsonValue handle_rename(const mcp::JsonValue &args);
mcp::JsonValue handle_reparent(const mcp::JsonValue &args);
mcp::JsonValue handle_instance(const mcp::JsonValue &args);
mcp::JsonValue handle_get_tree(const mcp::JsonValue &args);
mcp::JsonValue handle_get_node_screen_rect(const mcp::JsonValue &args);
mcp::JsonValue handle_get_snapshot(const mcp::JsonValue &args);
mcp::JsonValue handle_create_path_node(const mcp::JsonValue &args);
mcp::JsonValue handle_advance_path_follow(const mcp::JsonValue &args);
mcp::JsonValue handle_focus_camera_on_node(const mcp::JsonValue &args);
mcp::JsonValue handle_set_camera_params(const mcp::JsonValue &args);
mcp::JsonValue handle_get_camera_view_rect(const mcp::JsonValue &args);

} // namespace scene_ops
} // namespace godot_autopilot

#endif
