#ifndef GODOT_AUTOPILOT_MESH_OPS_HPP
#define GODOT_AUTOPILOT_MESH_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace mesh_ops {

godot_autopilot::util::JsonValue handle_surface_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_inspect_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_multimesh_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_multimesh_instance_action(const godot_autopilot::util::JsonValue &args);

} // namespace mesh_ops
} // namespace godot_autopilot

#endif
