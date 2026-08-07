#ifndef GODOT_SELF_DRIVING_PHYSICS_OPS_HPP
#define GODOT_SELF_DRIVING_PHYSICS_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace physics_ops {

mcp::JsonValue handle_2d_space_get_direct_state(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_ray_cast(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_shape_cast(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_point_query(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_intersect_shape(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_intersect_point(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_set_mode(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_apply_force(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_apply_impulse(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_set_state(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_body_get_state(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_joint_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_area_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_area_set_monitorable(const mcp::JsonValue &args);

mcp::JsonValue handle_3d_space_get_direct_state(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_ray_cast(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_shape_cast(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_point_query(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_intersect_shape(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_intersect_point(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_set_mode(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_apply_force(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_apply_impulse(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_set_state(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_get_state(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_joint_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_area_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_area_set_monitorable(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_apply_torque(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_set_axis_lock(const mcp::JsonValue &args);
mcp::JsonValue
handle_3d_body_add_collision_exception(const mcp::JsonValue &args);
mcp::JsonValue
handle_3d_body_remove_collision_exception(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_joint_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_area_set_space_override(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_space_set_gravity(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_space_set_debug(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_soft_body_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_soft_body_set_mesh(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_shape_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_shape_set_data(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_add_shape(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_area_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_space_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_area_set_transform(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_body_set_transform(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_shape_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_shape_set_data(const mcp::JsonValue &args);
mcp::JsonValue handle_physics_node_get_rid(const mcp::JsonValue &args);
mcp::JsonValue handle_resolve_object(const mcp::JsonValue &args);

} // namespace physics_ops
} // namespace godot_self_driving

#endif
