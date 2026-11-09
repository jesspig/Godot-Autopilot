#ifndef GODOT_AUTOPILOT_SCENE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_OPS_HPP

#include <mcp/JsonValue.hpp>

#include <godot_cpp/classes/node.hpp>

#include <string>

namespace godot_autopilot {
namespace scene_ops {

std::string scene_relative_path(godot::Node *node, godot::Node *scene_root);

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
