#ifndef GODOT_AUTOPILOT_SCENE_TREE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_TREE_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace scene_tree_ops {

godot_autopilot::util::JsonValue handle_game_scene_tree(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_call_group(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_timer(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_nodes_in_group(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_is_paused(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_notify_group(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reload_current_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_debug_collisions(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_pause(const godot_autopilot::util::JsonValue &args);

} // namespace scene_tree_ops
} // namespace godot_autopilot

#endif
