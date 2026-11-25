#ifndef GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace scene_spec_ops {

inline constexpr int SPEC_MAX_DEPTH = 32;
inline constexpr int SPEC_MAX_NODES = 500;

godot_autopilot::util::JsonValue handle_build_from_spec(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_spawn_bundle(const godot_autopilot::util::JsonValue &args);

} // namespace scene_spec_ops
} // namespace godot_autopilot

#endif
