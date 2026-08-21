#ifndef GODOT_AUTOPILOT_PHYSICS_TOOLS_HPP
#define GODOT_AUTOPILOT_PHYSICS_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/physics_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace physics_tools {

GDA_TOOL_CLASS(GetPhysics2dSpaceDirectStateTool, "get_physics_2d_space_direct_state",
               "Get the direct state of a 2D physics space, the gateway for physics intersection queries. Pass the space RID and use this before the intersect_physics_2d_* tools to confirm the space is active and attached to a scene. Returns the space RID, a valid flag and the available queries (intersect_ray, intersect_point, intersect_shape, cast_motion, collide_shape, get_rest_info). Fails when the space is inactive or not in a scene",
               "Physics", std::vector<std::string>({"physics", "2d", "space"}), physics_ops::handle_2d_space_get_direct_state, false)

GDA_TOOL_CLASS(IntersectPhysics2dRayTool, "intersect_physics_2d_ray",
               "Cast a ray in 2D physics space and return the first collision hit. Omit space_rid to auto-detect the space from the open editor scene's 2D world, or pass the space RID explicitly; from and to are required. Returns a single hit object with position, normal, collider_id, rid and shape, or null when nothing was hit. Unlike intersect_physics_2d_shape and intersect_physics_2d_point, this reports only the closest hit along the ray",
               "Physics", std::vector<std::string>({"physics", "2d", "ray"}), physics_ops::handle_2d_ray_cast, true)

GDA_TOOL_CLASS(IntersectPhysics2dShapeTool, "intersect_physics_2d_shape",
               "Intersect a 2D physics space with a shape RID and return every overlapping collider. Requires space_rid and shape_rid, where shape_rid comes from create_physics_2d_circle_shape; pass transform to place the shape, motion to sweep it along a Vector2, plus filters such as collision_mask, exclude, collide_with_bodies and collide_with_areas. Returns an array of hits, empty when nothing overlaps. Unlike intersect_physics_2d_ray, this reports all hits rather than just the closest",
               "Physics", std::vector<std::string>({"physics", "2d", "intersect", "shape"}), physics_ops::handle_2d_intersect_shape, false)

GDA_TOOL_CLASS(IntersectPhysics2dPointTool, "intersect_physics_2d_point",
               "Intersect a 2D physics space at a single point and return the colliders touching it. Requires space_rid and position; optional collision_mask, exclude, collide_with_bodies and collide_with_areas filter the query, and max_results caps the reply (default 32). Returns an array of hits, each with position, normal, collider_id, rid and shape. Use this instead of intersect_physics_2d_ray when you need everything at one spot rather than a closest hit",
               "Physics", std::vector<std::string>({"physics", "2d", "intersect", "point"}), physics_ops::handle_2d_intersect_point, false)

GDA_TOOL_CLASS(CreatePhysics2dBodyTool, "create_physics_2d_body",
               "Create a 2D physics body in the physics server and return its RID handle. Pass the returned RID to set_physics_2d_body_mode, apply_physics_2d_body_force, apply_physics_2d_body_impulse, set_physics_2d_body_state and get_physics_2d_body_state. The body is a server-side object with no scene node, so it is driven only through these tools and nothing appears in the editor scene tree. Returns an object with the numeric RID id and an is_valid flag",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "create"}), physics_ops::handle_2d_body_create, false)

GDA_TOOL_CLASS(SetPhysics2dBodyModeTool, "set_physics_2d_body_mode",
               "Set the simulation mode of a 2D physics body created by create_physics_2d_body. Requires rid and mode: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear. Only rigid modes respond to forces and impulses from the apply_physics_2d_body_* tools. Returns 'ok' or an error for invalid RIDs and out-of-range modes",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "mode"}), physics_ops::handle_2d_body_set_mode, false)

GDA_TOOL_CLASS(ApplyPhysics2dBodyForceTool, "apply_physics_2d_body_force",
               "Apply a continuous force to a 2D physics body, accelerating it while the force is applied. Requires rid and force (Vector2 with x and y); optional position offsets the force to a specific point, while omitting it applies a central force through the center of mass. Only affects bodies in rigid modes. Returns 'ok' or an error when the body RID or force is missing",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "force"}), physics_ops::handle_2d_body_apply_force, false)

GDA_TOOL_CLASS(ApplyPhysics2dBodyImpulseTool, "apply_physics_2d_body_impulse",
               "Apply an instantaneous impulse to a 2D physics body, causing an immediate change in velocity. Requires rid and impulse (Vector2 with x and y); optional position applies the impulse off-center, which adds rotation, while omitting it applies a central impulse. Use this for single quick events such as jumps or hits instead of apply_physics_2d_body_force. Returns 'ok' or an error for missing parameters",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "impulse"}), physics_ops::handle_2d_body_apply_impulse, false)

GDA_TOOL_CLASS(SetPhysics2dBodyStateTool, "set_physics_2d_body_state",
               "Set a state field on a 2D physics body, such as its transform or velocity. Requires rid, state and value; state selects the field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep, and value must match (transform object, Vector2 or boolean). Pair with get_physics_2d_body_state to read the same fields. Returns 'ok' or an error for invalid state values",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "state"}), physics_ops::handle_2d_body_set_state, false)

GDA_TOOL_CLASS(GetPhysics2dBodyStateTool, "get_physics_2d_body_state",
               "Read a state field from a 2D physics body, the counterpart of set_physics_2d_body_state. Requires rid and state, where state is 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping or 4=can_sleep. Returns the field value serialized as JSON, such as a transform object, a Vector2 or a boolean. Use this to track body movement during server-side simulations",
               "Physics", std::vector<std::string>({"physics", "2d", "body", "state"}), physics_ops::handle_2d_body_get_state, false)

GDA_TOOL_CLASS(CreatePhysics2dJointTool, "create_physics_2d_joint",
               "Create a 2D physics joint linking two bodies and return its RID handle. Requires type: pin, groove or damped_spring; pin needs anchor and optionally body_a/body_b, groove needs groove1_a, groove2_a and anchor_b, damped_spring needs anchor_a, anchor_b and body_a. Bodies are the RIDs from create_physics_2d_body. Returns the joint RID, or an error when required per-type parameters are missing",
               "Physics", std::vector<std::string>({"physics", "2d", "joint", "create"}), physics_ops::handle_2d_joint_create, false)

GDA_TOOL_CLASS(CreatePhysics2dAreaTool, "create_physics_2d_area",
               "Create a 2D physics area and return its RID handle. Areas detect overlaps and can apply effects such as gravity over their volume, unlike bodies which collide; use set_physics_2d_area_monitorable to control whether other areas can monitor it. Returns an object with the numeric RID id and an is_valid flag",
               "Physics", std::vector<std::string>({"physics", "2d", "area", "create"}), physics_ops::handle_2d_area_create, false)

GDA_TOOL_CLASS(SetPhysics2dAreaMonitorableTool, "set_physics_2d_area_monitorable",
               "Set whether a 2D physics area can be monitored by other areas, controlling overlap detection between areas. Requires rid and monitorable; with monitorable false, other areas will not receive overlap events involving this area, while bodies are unaffected. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "2d", "area", "monitorable"}), physics_ops::handle_2d_area_set_monitorable, false)

GDA_TOOL_CLASS(CreatePhysics2dCircleShapeTool, "create_physics_2d_circle_shape",
               "Create a 2D circle shape in PhysicsServer2D and return its RID handle. The RID cannot be assigned directly to CollisionShape2D.shape, which expects a Shape2D resource; to bridge it to a node, create a .tres via save_resource(class_type='CircleShape2D') and set CollisionShape2D.shape with set_property using type_hint='Resource'. The RID is only usable with physics server queries such as set_physics_2d_shape_data and intersect_physics_2d_shape",
               "Physics", std::vector<std::string>({"physics", "2d", "shape"}), physics_ops::handle_2d_shape_create, false)

GDA_TOOL_CLASS(SetPhysics2dShapeDataTool, "set_physics_2d_shape_data",
               "Set the geometric data of a 2D circle shape RID, configuring its size for physics queries. Requires rid and data, where data is an object such as {'radius': 10} for a circle. Use after create_physics_2d_circle_shape and before intersect_physics_2d_shape so the query uses the correct dimensions. Returns 'ok' or an error for missing parameters",
               "Physics", std::vector<std::string>({"physics", "2d", "shape"}), physics_ops::handle_2d_shape_set_data, false)

GDA_TOOL_CLASS(GetPhysics3dSpaceDirectStateTool, "get_physics_3d_space_direct_state",
               "Get the direct state of a 3D physics space, the gateway for 3D physics intersection queries. Pass the space RID and use this before the intersect_physics_3d_* tools to confirm the space is active and attached to a scene, since 3D queries fail on inactive spaces. Returns the space RID, a valid flag and the available queries (intersect_ray, intersect_point, intersect_shape, cast_motion, collide_shape, get_rest_info)",
               "Physics", std::vector<std::string>({"physics", "3d", "space"}), physics_ops::handle_3d_space_get_direct_state, false)

GDA_TOOL_CLASS(IntersectPhysics3dRayTool, "intersect_physics_3d_ray",
               "Cast a ray in 3D physics space and return the first collision hit. Unlike the 2D ray tool, space_rid is required here: pass the space RID explicitly, since 3D has no auto-detection from the editor scene. Requires from and to; returns a single hit object with position, normal, collider_id, rid and shape, or null when nothing was hit",
               "Physics", std::vector<std::string>({"physics", "3d", "ray"}), physics_ops::handle_3d_ray_cast, false)

GDA_TOOL_CLASS(IntersectPhysics3dShapeTool, "intersect_physics_3d_shape",
               "Intersect a 3D physics space with a shape RID and return every overlapping collider. Requires space_rid and shape_rid, where shape_rid comes from create_physics_3d_sphere_shape; pass transform to place the shape, motion to sweep it along a Vector3, plus filters such as collision_mask, exclude, collide_with_bodies and collide_with_areas. Returns an array of hits, empty when nothing overlaps. Unlike intersect_physics_3d_ray, this reports all hits",
               "Physics", std::vector<std::string>({"physics", "3d", "intersect", "shape"}), physics_ops::handle_3d_intersect_shape, false)

GDA_TOOL_CLASS(IntersectPhysics3dPointTool, "intersect_physics_3d_point",
               "Intersect a 3D physics space at a single point and return the colliders touching it. Requires space_rid and position; optional collision_mask, exclude, collide_with_bodies and collide_with_areas filter the query, and max_results caps the reply (default 32). Returns an array of hits, each with position, normal, collider_id, rid and shape. Use this instead of intersect_physics_3d_ray when you need everything at one spot",
               "Physics", std::vector<std::string>({"physics", "3d", "intersect", "point"}), physics_ops::handle_3d_intersect_point, false)

GDA_TOOL_CLASS(CreatePhysics3dBodyTool, "create_physics_3d_body",
               "Create a 3D physics body in the physics server and return its RID handle. Pass the returned RID to set_physics_3d_body_mode, set_physics_3d_body_transform, apply_physics_3d_body_force, apply_physics_3d_body_impulse, apply_physics_3d_body_torque and set_physics_3d_body_state. The body is a server-side object with no scene node, driven only through these tools. Returns an object with the numeric RID id and an is_valid flag",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "create"}), physics_ops::handle_3d_body_create, false)

GDA_TOOL_CLASS(SetPhysics3dBodyModeTool, "set_physics_3d_body_mode",
               "Set the simulation mode of a 3D physics body created by create_physics_3d_body. Requires rid and mode: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear. Only rigid modes respond to forces, impulses and torque from the apply_physics_3d_body_* tools. Returns 'ok' or an error for invalid RIDs and out-of-range modes",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "mode"}), physics_ops::handle_3d_body_set_mode, false)

GDA_TOOL_CLASS(ApplyPhysics3dBodyForceTool, "apply_physics_3d_body_force",
               "Apply a continuous force to a 3D physics body, accelerating it while the force is applied. Requires rid and force (Vector3 with x, y and z); optional position offsets the force to a specific point, while omitting it applies a central force through the center of mass. Only affects bodies in rigid modes. Returns 'ok' or an error when the body RID or force is missing",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "force"}), physics_ops::handle_3d_body_apply_force, false)

GDA_TOOL_CLASS(ApplyPhysics3dBodyImpulseTool, "apply_physics_3d_body_impulse",
               "Apply an instantaneous impulse to a 3D physics body, causing an immediate change in velocity. Requires rid and impulse (Vector3 with x, y and z); optional position applies the impulse off-center, adding rotation, while omitting it applies a central impulse. Use this for single quick events such as jumps or hits instead of apply_physics_3d_body_force. Returns 'ok' or an error for missing parameters",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "impulse"}), physics_ops::handle_3d_body_apply_impulse, false)

GDA_TOOL_CLASS(SetPhysics3dBodyStateTool, "set_physics_3d_body_state",
               "Set a state field on a 3D physics body, such as its transform or velocity. Requires rid, state and value; state selects the field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep, and value must match (transform object, Vector3 or boolean). Pair with get_physics_3d_body_state to read the same fields. Returns 'ok' or an error for invalid state values",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "state"}), physics_ops::handle_3d_body_set_state, false)

GDA_TOOL_CLASS(GetPhysics3dBodyStateTool, "get_physics_3d_body_state",
               "Read a state field from a 3D physics body, the counterpart of set_physics_3d_body_state. Requires rid and state, where state is 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping or 4=can_sleep. Returns the field value serialized as JSON, such as a transform object, a Vector3 or a boolean. Use this to track body movement during server-side simulations",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "state"}), physics_ops::handle_3d_body_get_state, false)

GDA_TOOL_CLASS(CreatePhysics3dJointTool, "create_physics_3d_joint",
               "Create a 3D physics joint linking two bodies and return its RID handle. Requires type: pin, hinge, slider, cone_twist or generic_6dof, and body_a_rid from create_physics_3d_body; body_b_rid and the joint-specific transforms are optional. Pin takes local_a/local_b, hinge takes hinge_a/hinge_b, and the remaining types take ref_a/ref_b. Returns the joint RID, or an error when required parameters are missing",
               "Physics", std::vector<std::string>({"physics", "3d", "joint", "create"}), physics_ops::handle_3d_joint_create, false)

GDA_TOOL_CLASS(CreatePhysics3dAreaTool, "create_physics_3d_area",
               "Create a 3D physics area and return its RID handle. Areas detect overlaps and can apply effects such as gravity and damping over their volume, unlike bodies which collide; use set_physics_3d_area_param, set_physics_3d_area_transform and set_physics_3d_area_space to configure it. Returns an object with the numeric RID id and an is_valid flag",
               "Physics", std::vector<std::string>({"physics", "3d", "area", "create"}), physics_ops::handle_3d_area_create, false)

GDA_TOOL_CLASS(SetPhysics3dAreaMonitorableTool, "set_physics_3d_area_monitorable",
               "Set whether a 3D physics area can be monitored by other areas, controlling overlap detection between areas. Requires rid and monitorable; with monitorable false, other areas will not receive overlap events involving this area, while bodies are unaffected. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "area", "monitorable"}), physics_ops::handle_3d_area_set_monitorable, false)

GDA_TOOL_CLASS(ApplyPhysics3dBodyTorqueTool, "apply_physics_3d_body_torque",
               "Apply a rotational torque to a 3D physics body, spinning it around the given axis. Requires rid and torque (Vector3 with x, y and z), where each component sets rotation around that axis. Only affects bodies in rigid modes; there is no 2D counterpart because 2D rotation is scalar. Returns 'ok' or an error for missing parameters",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "torque"}), physics_ops::handle_3d_body_apply_torque, false)

GDA_TOOL_CLASS(SetPhysics3dBodyAxisLockTool, "set_physics_3d_body_axis_lock",
               "Lock or unlock a single movement axis on a 3D physics body, restricting its freedom of motion. Requires rid, axis and lock; axis is a bit flag: 1=linear_x, 2=linear_y, 4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z, and lock is true to lock or false to unlock. Works on bodies created by create_physics_3d_body. Returns 'ok' or an error for invalid axis values",
               "Physics", std::vector<std::string>({"physics", "3d", "body", "axis_lock"}), physics_ops::handle_3d_body_set_axis_lock, false)

GDA_TOOL_CLASS(AddPhysics3dBodyCollisionExceptionTool, "add_physics_3d_body_collision_exception",
               "Add a collision exception to a 3D physics body so it stops colliding with another body. Requires rid and excepted_body_rid, both RIDs from create_physics_3d_body. The exception persists until removed with remove_physics_3d_body_collision_exception. Returns 'ok' or an error when either RID is invalid",
               "Physics", std::vector<std::string>({"physics", "3d", "collision", "exception"}), physics_ops::handle_3d_body_add_collision_exception, false)

GDA_TOOL_CLASS(RemovePhysics3dBodyCollisionExceptionTool, "remove_physics_3d_body_collision_exception",
               "Remove a collision exception from a 3D physics body, re-enabling collisions with the previously excepted body. Requires rid and excepted_body_rid, matching the pair set with add_physics_3d_body_collision_exception. Collisions are only re-enabled after this call, so pairing the two tools keeps the exception list accurate. Returns 'ok' or an error when either RID is invalid",
               "Physics", std::vector<std::string>({"physics", "3d", "collision", "exception"}), physics_ops::handle_3d_body_remove_collision_exception, false)

GDA_TOOL_CLASS(SetPhysics3dJointParamTool, "set_physics_3d_joint_param",
               "Set a parameter on a 3D physics joint created by create_physics_3d_joint. Requires rid and at least one of solver_priority (integer, higher values solve the joint first) or disable_collision (boolean, stops the two jointed bodies from colliding); both are optional, and only the ones supplied are applied. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "joint", "param"}), physics_ops::handle_3d_joint_set_param, false)

GDA_TOOL_CLASS(SetPhysics3dAreaSpaceTool, "set_physics_3d_area_space",
               "Attach a 3D area to a physics space (area_set_space semantics). Requires rid of the area and space_rid of the target space; the area then interacts with bodies and other areas in that space. Use this to move an area between spaces or to attach one created by create_physics_3d_area to a specific space. Returns 'ok' or an error when either RID is invalid",
               "Physics", std::vector<std::string>({"physics", "3d", "area", "space_override"}), physics_ops::handle_3d_area_set_space_override, false)

GDA_TOOL_CLASS(SetPhysics3dSpaceSolverIterationsTool, "set_physics_3d_space_solver_iterations",
               "Set the solver iterations of a 3D physics space, trading simulation accuracy for performance. Requires rid; higher solver_iterations values improve constraint accuracy but cost more CPU. This does not change gravity itself: actual gravity is configured through set_physics_3d_area_param or the project's physics settings. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "space", "gravity"}), physics_ops::handle_3d_space_set_gravity, false)

GDA_TOOL_CLASS(SetPhysics3dSpaceSolverParamsTool, "set_physics_3d_space_solver_params",
               "Set solver parameters on a 3D physics space: solver_iterations and contact_max_allowed_penetration. Requires rid; solver_iterations (integer) controls constraint accuracy, while contact_max_allowed_penetration (number) sets how deep bodies may overlap before the solver corrects them. Both are optional and applied only when supplied. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "space", "debug"}), physics_ops::handle_3d_space_set_debug, false)

GDA_TOOL_CLASS(CreatePhysics3dSoftBodyTool, "create_physics_3d_soft_body",
               "Create a 3D soft body and return its RID handle. Soft bodies deform under physics forces instead of moving rigidly; assign a mesh with set_physics_3d_soft_body_mesh to give it shape. Returns an object with the numeric RID id and an is_valid flag",
               "Physics", std::vector<std::string>({"physics", "3d", "soft_body", "create"}), physics_ops::handle_3d_soft_body_create, false)

GDA_TOOL_CLASS(SetPhysics3dSoftBodyMeshTool, "set_physics_3d_soft_body_mesh",
               "Set the mesh of a 3D soft body, defining the surface that deforms under physics forces. Requires rid of the soft body from create_physics_3d_soft_body and mesh_rid of the mesh to assign. The mesh must be created or loaded before calling this. Returns 'ok' or an error when either RID is invalid",
               "Physics", std::vector<std::string>({"physics", "3d", "soft_body", "mesh"}), physics_ops::handle_3d_soft_body_set_mesh, false)

GDA_TOOL_CLASS(CreatePhysics3dSphereShapeTool, "create_physics_3d_sphere_shape",
               "Create a sphere shape in the physics server and return its RID handle. The RID cannot be assigned directly to CollisionShape3D.shape, which expects a SphereShape3D resource; to bridge it to a node, create a .tres via save_resource(class_type='SphereShape3D') and assign it with set_property using type_hint='Resource'. The RID is only usable with physics server queries such as set_physics_3d_shape_data, add_physics_3d_body_shape and intersect_physics_3d_shape",
               "Physics", std::vector<std::string>({"physics", "3d", "shape"}), physics_ops::handle_3d_shape_create, false)

GDA_TOOL_CLASS(SetPhysics3dShapeDataTool, "set_physics_3d_shape_data",
               "Set the geometric data of a sphere shape RID, configuring its size for physics queries. Requires rid and data, where data is an object such as {'radius': 0.5} for a sphere. Use after create_physics_3d_sphere_shape and before queries such as intersect_physics_3d_shape so the shape has the correct dimensions. Returns 'ok' or an error for missing parameters",
               "Physics", std::vector<std::string>({"physics", "3d", "shape"}), physics_ops::handle_3d_shape_set_data, false)

GDA_TOOL_CLASS(AddPhysics3dBodyShapeTool, "add_physics_3d_body_shape",
               "Add a shape RID to a 3D physics body, giving it collision geometry. Requires rid of the body from create_physics_3d_body and shape_rid from create_physics_3d_sphere_shape; optional transform places the shape relative to the body, disabled starts it disabled (default false). Returns 'ok' or an error when either RID is invalid",
               "Physics", std::vector<std::string>({"physics", "3d", "body"}), physics_ops::handle_3d_body_add_shape, false)

GDA_TOOL_CLASS(SetPhysics3dBodyParamTool, "set_physics_3d_body_param",
               "Set a physics parameter on a 3D body by enum index, such as mass, friction or gravity scale. Requires rid, param and value; param is a number matching the engine's BodyParameter enum (0=bounce, 1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale, 6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp), and value is the numeric setting. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "body"}), physics_ops::handle_3d_body_set_param, false)

GDA_TOOL_CLASS(SetPhysics3dAreaParamTool, "set_physics_3d_area_param",
               "Set a physics parameter on a 3D area by enum index, such as gravity, damping or priority. Requires rid, param and value; param is a number matching the engine's AreaParameter enum (0=gravity_override_mode, 1=gravity, 2=gravity_vector, 3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode, 6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority), and value matches the parameter's type (number, Vector3 object or boolean). Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "area"}), physics_ops::handle_3d_area_set_param, false)

GDA_TOOL_CLASS(SetPhysics3dSpaceParamTool, "set_physics_3d_space_param",
               "Set a physics parameter on a 3D space by enum index, such as solver iterations or penetration limits. Requires space_rid, param and value; param is a number matching the engine's SpaceParameter enum (0=contact_recycle_radius, 1=contact_max_separation, 2=contact_max_allowed_penetration, 3=contact_default_bias, 4=body_linear_velocity_sleep_threshold, 5=body_angular_velocity_sleep_threshold, 6=body_time_to_sleep, 7=solver_iterations), and value is the numeric setting. Returns 'ok' or an error for invalid RIDs",
               "Physics", std::vector<std::string>({"physics", "3d", "space"}), physics_ops::handle_3d_space_set_param, false)

GDA_TOOL_CLASS(SetPhysics3dAreaTransformTool, "set_physics_3d_area_transform",
               "Set the global transform of a 3D physics area, placing its detection volume in the space. Requires rid and transform (Transform3D with basis array and origin). Use this after create_physics_3d_area to position the area where it should detect overlaps. Returns 'ok' or an error when the transform is missing",
               "Physics", std::vector<std::string>({"physics", "3d", "area"}), physics_ops::handle_3d_area_set_transform, false)

GDA_TOOL_CLASS(SetPhysics3dBodyTransformTool, "set_physics_3d_body_transform",
               "Set the global transform of a 3D physics body, teleporting it in the space. Requires rid and transform (Transform3D with basis array and origin). Use this on bodies created by create_physics_3d_body to place them before simulation or to snap them to a new pose. Returns 'ok' or an error when the transform is missing",
               "Physics", std::vector<std::string>({"physics", "3d", "body"}), physics_ops::handle_3d_body_set_transform, false)

GDA_TOOL_CLASS(GetPhysicsNodeRidTool, "get_physics_node_rid",
               "Get the RID of a physics node, bridging scene nodes to the server-side physics tools. Requires path to a CollisionObject2D or CollisionObject3D node (such as a RigidBody2D or CharacterBody3D) inside the edited scene; other node types fail. Returns the numeric RID id and an is_valid flag, which you can then pass as rid to the create_physics_3d_body family of tools",
               "Physics", std::vector<std::string>({"physics", "rid", "bridge"}), physics_ops::handle_physics_node_get_rid, false)

GDA_TOOL_CLASS(GetDebugObjectInfoTool, "get_debug_object_info",
               "Resolve an object instance ID (ObjectID) to its class and identity. Use it with object IDs obtained from scene or physics tools to identify what an object is. Returns result 'ok', class (Godot class name), name and node_path (for Nodes), plus resource_path (for Resources); errors out if the ID is stale or invalid.",
               "Debug", std::vector<std::string>({"utility", "debug", "physics"}), physics_ops::handle_resolve_object, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(48);
  v.push_back(std::make_unique<GetPhysics2dSpaceDirectStateTool>());
  v.push_back(std::make_unique<IntersectPhysics2dRayTool>());
  v.push_back(std::make_unique<IntersectPhysics2dShapeTool>());
  v.push_back(std::make_unique<IntersectPhysics2dPointTool>());
  v.push_back(std::make_unique<CreatePhysics2dBodyTool>());
  v.push_back(std::make_unique<SetPhysics2dBodyModeTool>());
  v.push_back(std::make_unique<ApplyPhysics2dBodyForceTool>());
  v.push_back(std::make_unique<ApplyPhysics2dBodyImpulseTool>());
  v.push_back(std::make_unique<SetPhysics2dBodyStateTool>());
  v.push_back(std::make_unique<GetPhysics2dBodyStateTool>());
  v.push_back(std::make_unique<CreatePhysics2dJointTool>());
  v.push_back(std::make_unique<CreatePhysics2dAreaTool>());
  v.push_back(std::make_unique<SetPhysics2dAreaMonitorableTool>());
  v.push_back(std::make_unique<CreatePhysics2dCircleShapeTool>());
  v.push_back(std::make_unique<SetPhysics2dShapeDataTool>());
  v.push_back(std::make_unique<GetPhysics3dSpaceDirectStateTool>());
  v.push_back(std::make_unique<IntersectPhysics3dRayTool>());
  v.push_back(std::make_unique<IntersectPhysics3dShapeTool>());
  v.push_back(std::make_unique<IntersectPhysics3dPointTool>());
  v.push_back(std::make_unique<CreatePhysics3dBodyTool>());
  v.push_back(std::make_unique<SetPhysics3dBodyModeTool>());
  v.push_back(std::make_unique<ApplyPhysics3dBodyForceTool>());
  v.push_back(std::make_unique<ApplyPhysics3dBodyImpulseTool>());
  v.push_back(std::make_unique<SetPhysics3dBodyStateTool>());
  v.push_back(std::make_unique<GetPhysics3dBodyStateTool>());
  v.push_back(std::make_unique<CreatePhysics3dJointTool>());
  v.push_back(std::make_unique<CreatePhysics3dAreaTool>());
  v.push_back(std::make_unique<SetPhysics3dAreaMonitorableTool>());
  v.push_back(std::make_unique<ApplyPhysics3dBodyTorqueTool>());
  v.push_back(std::make_unique<SetPhysics3dBodyAxisLockTool>());
  v.push_back(std::make_unique<AddPhysics3dBodyCollisionExceptionTool>());
  v.push_back(std::make_unique<RemovePhysics3dBodyCollisionExceptionTool>());
  v.push_back(std::make_unique<SetPhysics3dJointParamTool>());
  v.push_back(std::make_unique<SetPhysics3dAreaSpaceTool>());
  v.push_back(std::make_unique<SetPhysics3dSpaceSolverIterationsTool>());
  v.push_back(std::make_unique<SetPhysics3dSpaceSolverParamsTool>());
  v.push_back(std::make_unique<CreatePhysics3dSoftBodyTool>());
  v.push_back(std::make_unique<SetPhysics3dSoftBodyMeshTool>());
  v.push_back(std::make_unique<CreatePhysics3dSphereShapeTool>());
  v.push_back(std::make_unique<SetPhysics3dShapeDataTool>());
  v.push_back(std::make_unique<AddPhysics3dBodyShapeTool>());
  v.push_back(std::make_unique<SetPhysics3dBodyParamTool>());
  v.push_back(std::make_unique<SetPhysics3dAreaParamTool>());
  v.push_back(std::make_unique<SetPhysics3dSpaceParamTool>());
  v.push_back(std::make_unique<SetPhysics3dAreaTransformTool>());
  v.push_back(std::make_unique<SetPhysics3dBodyTransformTool>());
  v.push_back(std::make_unique<GetPhysicsNodeRidTool>());
  v.push_back(std::make_unique<GetDebugObjectInfoTool>());
  return v;
}

} // namespace physics_tools
} // namespace godot_autopilot

#endif