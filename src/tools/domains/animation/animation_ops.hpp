#ifndef GODOT_AUTOPILOT_ANIMATION_OPS_HPP
#define GODOT_AUTOPILOT_ANIMATION_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace animation_ops {

godot_autopilot::util::JsonValue handle_create_player(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_animation(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_remove_animation(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_create_track(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_insert_keyframe(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_track_bundle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_remove_track(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_runtime(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_tree(const godot_autopilot::util::JsonValue &args);

} // namespace animation_ops
} // namespace godot_autopilot

#endif
