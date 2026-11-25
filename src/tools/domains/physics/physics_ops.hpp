#ifndef GODOT_AUTOPILOT_PHYSICS_OPS_HPP
#define GODOT_AUTOPILOT_PHYSICS_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace physics_ops {

struct PhysicsRidDomain {};

godot_autopilot::util::JsonValue handle_2d_space_get_direct_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_ray_cast(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_intersect_shape(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_intersect_point(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_set_mode(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_apply_force(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_apply_impulse(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_set_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_body_get_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_joint_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_area_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_area_set_monitorable(const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue handle_3d_space_get_direct_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_ray_cast(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_intersect_shape(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_intersect_point(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_set_mode(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_apply_force(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_apply_impulse(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_set_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_get_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_joint_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_area_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_area_set_monitorable(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_apply_torque(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_set_axis_lock(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue
handle_3d_body_add_collision_exception(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue
handle_3d_body_remove_collision_exception(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_joint_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_area_set_space_override(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_space_set_gravity(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_space_set_debug(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_soft_body_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_soft_body_set_mesh(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_shape_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_shape_set_data(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_add_shape(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_area_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_space_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_area_set_transform(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_body_set_transform(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_shape_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_shape_set_data(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_physics_node_get_rid(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_physics_query(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_resolve_object(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_body_bundle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_collision_shape_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_collision_shape_assign(const godot_autopilot::util::JsonValue &args);

} // namespace physics_ops
} // namespace godot_autopilot

#endif
