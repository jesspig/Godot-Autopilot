#ifndef GODOT_AUTOPILOT_SCENE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_OPS_HPP

#include "util/json_value.hpp"

#include <godot_cpp/classes/node.hpp>

#include <string>

namespace godot_autopilot {
namespace scene_ops {

std::string scene_relative_path(godot::Node *node, godot::Node *scene_root);

godot_autopilot::util::JsonValue handle_create_scene_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_edit_scene_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_pack_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_scene_camera(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_delete(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_rename(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reparent(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_instance(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_tree(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_node_screen_rect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_snapshot(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_path_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_advance_path_follow(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_focus_camera_on_node(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_camera_params(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_camera_view_rect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_projection(const godot_autopilot::util::JsonValue &args);

} // namespace scene_ops
} // namespace godot_autopilot

#endif
