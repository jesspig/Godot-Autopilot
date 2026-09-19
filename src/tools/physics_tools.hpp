#ifndef GODOT_AUTOPILOT_PHYSICS_TOOLS_HPP
#define GODOT_AUTOPILOT_PHYSICS_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/physics_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace physics_tools {

namespace {

const std::vector<ParamSpec> kGetPhysics2dSpaceDirectStateParams = {
    {"space_rid", "integer", "Required. RID of the 2D physics space to inspect; it must be active and attached to a scene or the call fails", true},
};

const std::vector<ParamSpec> kIntersectPhysics2dRayParams = {
    {"space_rid", "integer", "Optional RID of the 2D physics space to query; omit to auto-detect the World2D space of the edited scene root's SubViewport in the editor process, which is separate from the running game's physics world", false},
    {"from", "object", "Required. Ray origin as a Vector2 object with x and y fields, e.g. {'x': 0, 'y': 0}", true},
    {"to", "object", "Required. Ray destination as a Vector2 object with x and y fields", true},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers the ray hits; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query, e.g. [12345]", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"hit_from_inside", "boolean", "Optional. Whether shapes the ray starts inside of are reported; default false", false},
};

const std::vector<ParamSpec> kIntersectPhysics2dShapeParams = {
    {"space_rid", "integer", "Required. RID of the 2D physics space to query", true},
    {"shape_rid", "integer", "Required. RID of the shape to probe with, e.g. from create_physics_2d_circle_shape", true},
    {"transform", "object", "Optional. Transform2D placing the shape: object with origin (Vector2), rotation (radians) and scale (Vector2)", false},
    {"motion", "object", "Optional. Vector2 sweep distance: the shape is moved along this vector while checking for collisions, e.g. {'x': 50, 'y': 0}", false},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
};

const std::vector<ParamSpec> kIntersectPhysics2dPointParams = {
    {"space_rid", "integer", "Required. RID of the 2D physics space to query", true},
    {"position", "object", "Required. Query position as a Vector2 object with x and y fields, e.g. {'x': 100, 'y': 50}", true},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
};

const std::vector<ParamSpec> kCreatePhysics2dBodyParams = {};

const std::vector<ParamSpec> kSetPhysics2dBodyModeParams = {
    {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
    {"mode", "number", "Required. Body mode enum index: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear; only rigid modes react to forces and impulses", true},
};

const std::vector<ParamSpec> kApplyPhysics2dBodyForceParams = {
    {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
    {"force", "object", "Required. Force vector as a Vector2 object with x and y fields; applied to the center of mass when position is omitted", true},
    {"position", "object", "Optional. Force application point as a Vector2 object with x and y fields; omitting it applies a central force", false},
};

const std::vector<ParamSpec> kApplyPhysics2dBodyImpulseParams = {
    {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
    {"impulse", "object", "Required. Impulse vector as a Vector2 object with x and y fields; applied to the center of mass when position is omitted", true},
    {"position", "object", "Optional. Impulse application point as a Vector2 object with x and y fields; off-center impulses add rotation", false},
};

const std::vector<ParamSpec> kSetPhysics2dBodyStateParams = {
    {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
    {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
    {"value", "object", "Required. New state value matching the state: a transform object (origin/rotation/scale) for 0, a Vector2 object for 1 and 2, a boolean for 3 and 4", true},
};

const std::vector<ParamSpec> kGetPhysics2dBodyStateParams = {
    {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
    {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
};

const std::vector<ParamSpec> kCreatePhysics2dJointParams = {
    {"type", "string", "Required. Joint kind: 'pin', 'groove' or 'damped_spring'; each kind needs its own parameter set (see below)", true},
    {"anchor", "object", "Optional. Pin joint anchor as a Vector2 object with x and y fields (used for type='pin')", false},
    {"body_a", "integer", "Optional. RID id of the first body, e.g. from create_physics_2d_body", false},
    {"body_b", "integer", "Optional. RID id of the second body; the joint connects body A to the world when omitted", false},
    {"groove1_a", "object", "Optional. First groove point as a Vector2 object (used for type='groove')", false},
    {"groove2_a", "object", "Optional. Second groove point as a Vector2 object (used for type='groove')", false},
    {"anchor_b", "object", "Optional. Groove joint anchor on body B as a Vector2 object (used for type='groove')", false},
    {"anchor_a", "object", "Optional. Damped spring anchor on body A as a Vector2 object (used for type='damped_spring')", false},
};

const std::vector<ParamSpec> kCreatePhysics2dAreaParams = {};

const std::vector<ParamSpec> kSetPhysics2dAreaMonitorableParams = {
    {"rid", "integer", "Required. RID of the 2D physics area, e.g. from create_physics_2d_area", true},
    {"monitorable", "boolean", "Required. True lets other areas monitor this area for overlap detection; bodies are unaffected", true},
};

const std::vector<ParamSpec> kCreatePhysics2dCircleShapeParams = {};

const std::vector<ParamSpec> kSetPhysics2dShapeDataParams = {
    {"rid", "integer", "Required. RID of the 2D shape, e.g. from create_physics_2d_circle_shape", true},
    {"data", "object", "Required. Shape geometry object, e.g. {'radius': 10} for a circle", true},
};

const std::vector<ParamSpec> kGetPhysics3dSpaceDirectStateParams = {
    {"space_rid", "integer", "Required. RID of the 3D physics space to inspect; it must be active and attached to a scene or the call fails", true},
};

const std::vector<ParamSpec> kIntersectPhysics3dRayParams = {
    {"space_rid", "integer", "Required. RID of the 3D physics space to query; unlike the 2D ray tool there is no auto-detection, so this is mandatory", true},
    {"from", "object", "Required. Ray origin as a Vector3 object with x, y and z fields, e.g. {'x': 0, 'y': 0, 'z': 0}", true},
    {"to", "object", "Required. Ray destination as a Vector3 object with x, y and z fields", true},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers the ray hits; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query, e.g. [12345]", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"hit_from_inside", "boolean", "Optional. Whether shapes the ray starts inside of are reported; default false", false},
    {"hit_back_faces", "boolean", "Optional. Whether back faces of double-sided geometry are hit; default false", false},
};

const std::vector<ParamSpec> kIntersectPhysics3dShapeParams = {
    {"space_rid", "integer", "Required. RID of the 3D physics space to query", true},
    {"shape_rid", "integer", "Required. RID of the shape to probe with, e.g. from create_physics_3d_sphere_shape", true},
    {"transform", "object", "Optional. Transform3D placing the shape: object with basis (array of 3 row arrays) and origin (Vector3 object)", false},
    {"motion", "object", "Optional. Vector3 sweep distance: the shape is moved along this vector while checking for collisions, e.g. {'x': 0, 'y': 0, 'z': 50}", false},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
};

const std::vector<ParamSpec> kIntersectPhysics3dPointParams = {
    {"space_rid", "integer", "Required. RID of the 3D physics space to query", true},
    {"position", "object", "Required. Query position as a Vector3 object with x, y and z fields, e.g. {'x': 0, 'y': 1, 'z': 0}", true},
    {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
    {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
    {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
    {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
    {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
};

const std::vector<ParamSpec> kCreatePhysics3dBodyParams = {};

const std::vector<ParamSpec> kSetPhysics3dBodyModeParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"mode", "number", "Required. Body mode enum index: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear; only rigid modes react to forces and impulses", true},
};

const std::vector<ParamSpec> kApplyPhysics3dBodyForceParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"force", "object", "Required. Force vector as a Vector3 object with x, y and z fields; applied to the center of mass when position is omitted", true},
    {"position", "object", "Optional. Force application point as a Vector3 object with x, y and z fields; omitting it applies a central force", false},
};

const std::vector<ParamSpec> kApplyPhysics3dBodyImpulseParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"impulse", "object", "Required. Impulse vector as a Vector3 object with x, y and z fields; applied to the center of mass when position is omitted", true},
    {"position", "object", "Optional. Impulse application point as a Vector3 object with x, y and z fields; off-center impulses add rotation", false},
};

const std::vector<ParamSpec> kSetPhysics3dBodyStateParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
    {"value", "object", "Required. New state value matching the state: a transform object (basis/origin) for 0, a Vector3 object for 1 and 2, a boolean for 3 and 4", true},
};

const std::vector<ParamSpec> kGetPhysics3dBodyStateParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
};

const std::vector<ParamSpec> kCreatePhysics3dJointParams = {
    {"type", "string", "Required. Joint kind: 'pin', 'hinge', 'slider', 'cone_twist' or 'generic_6dof'; each kind uses its own transform parameters", true},
    {"body_a_rid", "integer", "Required. RID id of the first body, e.g. from create_physics_3d_body", true},
    {"body_b_rid", "integer", "Optional. RID id of the second body; the joint connects body A to the world when omitted", false},
    {"local_a", "object", "Optional. Pin joint local anchor on body A as a Vector3 object (used for type='pin')", false},
    {"local_b", "object", "Optional. Pin joint local anchor on body B as a Vector3 object (used for type='pin')", false},
    {"hinge_a", "object", "Optional. Hinge joint frame on body A as a Transform3D object (used for type='hinge')", false},
    {"hinge_b", "object", "Optional. Hinge joint frame on body B as a Transform3D object (used for type='hinge')", false},
    {"ref_a", "object", "Optional. Reference frame on body A as a Transform3D object (used for 'slider', 'cone_twist' and 'generic_6dof')", false},
    {"ref_b", "object", "Optional. Reference frame on body B as a Transform3D object (used for 'slider', 'cone_twist' and 'generic_6dof')", false},
};

const std::vector<ParamSpec> kCreatePhysics3dAreaParams = {};

const std::vector<ParamSpec> kSetPhysics3dAreaMonitorableParams = {
    {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
    {"monitorable", "boolean", "Required. True lets other areas monitor this area for overlap detection; bodies are unaffected", true},
};

const std::vector<ParamSpec> kApplyPhysics3dBodyTorqueParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"torque", "object", "Required. Torque vector as a Vector3 object with x, y and z fields; each component rotates the body around that axis", true},
};

const std::vector<ParamSpec> kSetPhysics3dBodyAxisLockParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"axis", "number", "Required. Axis bit flag: 1=linear_x, 2=linear_y, 4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z; only one flag per call", true},
    {"lock", "boolean", "Required. True locks the axis, false unlocks it", true},
};

const std::vector<ParamSpec> kAddPhysics3dBodyCollisionExceptionParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"excepted_body_rid", "integer", "Required. RID id of the body to exclude from collision", true},
};

const std::vector<ParamSpec> kRemovePhysics3dBodyCollisionExceptionParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"excepted_body_rid", "integer", "Required. RID id of the body to remove from the collision exception list", true},
};

const std::vector<ParamSpec> kSetPhysics3dJointParamParams = {
    {"rid", "integer", "Required. RID of the 3D joint, e.g. from create_physics_3d_joint", true},
    {"solver_priority", "integer", "Optional. Integer priority for solver order; higher values solve the joint first", false},
    {"disable_collision", "boolean", "Optional. True disables collisions between the two jointed bodies", false},
};

const std::vector<ParamSpec> kSetPhysics3dAreaSpaceParams = {
    {"rid", "integer", "Required. RID of the 3D area, e.g. from create_physics_3d_area", true},
    {"space_rid", "integer", "Required. RID of the 3D physics space to attach the area to; the area then interacts with bodies and areas in that space", true},
};

const std::vector<ParamSpec> kSetPhysics3dSpaceSolverIterationsParams = {
    {"rid", "integer", "Required. RID of the 3D physics space", true},
    {"solver_iterations", "integer", "Optional. Number of solver iterations (integer); higher values improve accuracy at higher CPU cost. Does not change gravity itself", false},
};

const std::vector<ParamSpec> kSetPhysics3dSpaceSolverParamsParams = {
    {"rid", "integer", "Required. RID of the 3D physics space", true},
    {"solver_iterations", "integer", "Optional. Number of solver iterations (integer); higher values improve constraint accuracy at higher CPU cost", false},
    {"contact_max_allowed_penetration", "number", "Optional. Maximum allowed penetration depth (number) before the solver pushes bodies apart", false},
};

const std::vector<ParamSpec> kCreatePhysics3dSoftBodyParams = {};

const std::vector<ParamSpec> kSetPhysics3dSoftBodyMeshParams = {
    {"rid", "integer", "Required. RID of the 3D soft body, e.g. from create_physics_3d_soft_body", true},
    {"mesh_rid", "integer", "Required. RID id of the mesh to assign to the soft body", true},
};

const std::vector<ParamSpec> kCreatePhysics3dSphereShapeParams = {};

const std::vector<ParamSpec> kSetPhysics3dShapeDataParams = {
    {"rid", "integer", "Required. RID of the 3D shape, e.g. from create_physics_3d_sphere_shape", true},
    {"data", "object", "Required. Shape geometry object, e.g. {'radius': 0.5} for a sphere", true},
};

const std::vector<ParamSpec> kAddPhysics3dBodyShapeParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"shape_rid", "integer", "Required. RID of the shape to attach, e.g. from create_physics_3d_sphere_shape", true},
    {"transform", "object", "Optional. Transform3D placing the shape relative to the body: object with basis (array of 3 row arrays) and origin (Vector3 object)", false},
    {"disabled", "boolean", "Optional. True adds the shape disabled so it does not collide until re-enabled; default false", false},
};

const std::vector<ParamSpec> kSetPhysics3dBodyParamParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"param", "number", "Required. Body parameter enum index: 0=bounce, 1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale, 6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp", true},
    {"value", "number", "Required. Numeric value for the selected parameter, e.g. 0.5 for friction", true},
};

const std::vector<ParamSpec> kSetPhysics3dAreaParamParams = {
    {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
    {"param", "number", "Required. Area parameter enum index: 0=gravity_override_mode, 1=gravity, 2=gravity_vector, 3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode, 6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority", true},
    {"value", "object", "Required. Value matching the selected parameter: a number for scalar params (e.g. gravity, damp), a Vector3 object like {'x': 0, 'y': -9.8, 'z': 0} for vectors, or a boolean", true},
};

const std::vector<ParamSpec> kSetPhysics3dSpaceParamParams = {
    {"space_rid", "integer", "Required. RID of the 3D physics space", true},
    {"param", "number", "Required. Space parameter enum index: 0=contact_recycle_radius, 1=contact_max_separation, 2=contact_max_allowed_penetration, 3=contact_default_bias, 4=body_linear_velocity_sleep_threshold, 5=body_angular_velocity_sleep_threshold, 6=body_time_to_sleep, 7=solver_iterations", true},
    {"value", "number", "Required. Numeric value for the selected parameter", true},
};

const std::vector<ParamSpec> kSetPhysics3dAreaTransformParams = {
    {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
    {"transform", "object", "Required. Transform3D as an object with basis (array of 3 row arrays, each with 3 numbers) and origin (Vector3 object with x, y and z)", true},
};

const std::vector<ParamSpec> kSetPhysics3dBodyTransformParams = {
    {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
    {"transform", "object", "Required. New body transform: object with basis (array of 3 row arrays) and origin (Vector3 object); sets the body's global transform directly", true},
};

const std::vector<ParamSpec> kGetPhysicsNodeRidParams = {
    {"path", "string", "Required. Node path of a CollisionObject2D or CollisionObject3D node in the edited scene, e.g. 'RigidBody3D' or 'MyBody/CharacterBody2D'; other node types fail", true},
};

const std::vector<ParamSpec> kGetDebugObjectInfoParams = {
    {"object_id", "integer", "Object instance ID (ObjectID) to resolve, as returned by scene or physics tools; looked up in ObjectDB. Returns class, name, node_path (Node) or resource_path (Resource)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(48);
  v.push_back(make_spec_tool(ToolSpec{
      "get_physics_2d_space_direct_state",
      "Get the direct state of a 2D physics space, the gateway for physics intersection queries. Pass the space RID and use this before the intersect_physics_2d_* tools to confirm the space is active and attached to a scene. Returns the space RID, a valid flag and the available queries (intersect_ray, intersect_point, intersect_shape, cast_motion, collide_shape, get_rest_info). Fails when the space is inactive or not in a scene",
      "Physics", {"physics", "2d", "space"}, SideEffect::None, tool_flags::kNone,
      kGetPhysics2dSpaceDirectStateParams, physics_ops::handle_2d_space_get_direct_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_2d_ray",
      "Cast a ray in 2D physics space and return the first collision hit. Omit space_rid to auto-detect the space: this resolves the World2D space of the edited scene root's SubViewport inside the editor process, which is separate from the running game's physics world, so coordinates from the game never hit anything here. from and to are required. Returns a single hit object with position, normal, collider_id, rid and shape; a null result means the query ran but hit nothing, while the separate space_get_direct_state returned null error means the direct state of that space is unavailable, for example while the physics thread is running or the space is locked. Every reply carries space_rid (int64) and auto_detected so you can verify which space was queried; unlike intersect_physics_2d_shape and intersect_physics_2d_point, this reports only the closest hit along the ray",
      "Physics", {"physics", "2d", "ray"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics2dRayParams, physics_ops::handle_2d_ray_cast}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_2d_shape",
      "Intersect a 2D physics space with a shape RID and return every overlapping collider. Requires space_rid and shape_rid, where shape_rid comes from create_physics_2d_circle_shape; pass transform to place the shape, motion to sweep it along a Vector2, plus filters such as collision_mask, exclude, collide_with_bodies and collide_with_areas. Returns an array of hits, empty when nothing overlaps. Unlike intersect_physics_2d_ray, this reports all hits rather than just the closest",
      "Physics", {"physics", "2d", "intersect", "shape"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics2dShapeParams, physics_ops::handle_2d_intersect_shape}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_2d_point",
      "Intersect a 2D physics space at a single point and return the colliders touching it. Requires space_rid and position; optional collision_mask, exclude, collide_with_bodies and collide_with_areas filter the query, and max_results caps the reply (default 32). Returns an array of hits, each with position, normal, collider_id, rid and shape. Use this instead of intersect_physics_2d_ray when you need everything at one spot rather than a closest hit",
      "Physics", {"physics", "2d", "intersect", "point"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics2dPointParams, physics_ops::handle_2d_intersect_point}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_2d_body",
      "Create a 2D physics body in the physics server and return its RID handle. Pass the returned RID to set_physics_2d_body_mode, apply_physics_2d_body_force, apply_physics_2d_body_impulse, set_physics_2d_body_state and get_physics_2d_body_state. The body is a server-side object with no scene node, so it is driven only through these tools and nothing appears in the editor scene tree. Returns an object with the numeric RID id and an is_valid flag",
      "Physics", {"physics", "2d", "body", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics2dBodyParams, physics_ops::handle_2d_body_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_2d_body_mode",
      "Set the simulation mode of a 2D physics body created by create_physics_2d_body. Requires rid and mode: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear. Only rigid modes respond to forces and impulses from the apply_physics_2d_body_* tools. Returns 'ok' or an error for invalid RIDs and out-of-range modes",
      "Physics", {"physics", "2d", "body", "mode"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics2dBodyModeParams, physics_ops::handle_2d_body_set_mode}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_physics_2d_body_force",
      "Apply a continuous force to a 2D physics body, accelerating it while the force is applied. Requires rid and force (Vector2 with x and y); optional position offsets the force to a specific point, while omitting it applies a central force through the center of mass. Only affects bodies in rigid modes. Returns 'ok' or an error when the body RID or force is missing",
      "Physics", {"physics", "2d", "body", "force"}, SideEffect::None, tool_flags::kNone,
      kApplyPhysics2dBodyForceParams, physics_ops::handle_2d_body_apply_force}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_physics_2d_body_impulse",
      "Apply an instantaneous impulse to a 2D physics body, causing an immediate change in velocity. Requires rid and impulse (Vector2 with x and y); optional position applies the impulse off-center, which adds rotation, while omitting it applies a central impulse. Use this for single quick events such as jumps or hits instead of apply_physics_2d_body_force. Returns 'ok' or an error for missing parameters",
      "Physics", {"physics", "2d", "body", "impulse"}, SideEffect::None, tool_flags::kNone,
      kApplyPhysics2dBodyImpulseParams, physics_ops::handle_2d_body_apply_impulse}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_2d_body_state",
      "Set a state field on a 2D physics body, such as its transform or velocity. Requires rid, state and value; state selects the field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep, and value must match (transform object, Vector2 or boolean). Pair with get_physics_2d_body_state to read the same fields. Returns 'ok' or an error for invalid state values",
      "Physics", {"physics", "2d", "body", "state"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics2dBodyStateParams, physics_ops::handle_2d_body_set_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_physics_2d_body_state",
      "Read a state field from a 2D physics body, the counterpart of set_physics_2d_body_state. Requires rid and state, where state is 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping or 4=can_sleep. Returns the field value serialized as JSON, such as a transform object, a Vector2 or a boolean. Use this to track body movement during server-side simulations",
      "Physics", {"physics", "2d", "body", "state"}, SideEffect::None, tool_flags::kNone,
      kGetPhysics2dBodyStateParams, physics_ops::handle_2d_body_get_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_2d_joint",
      "Create a 2D physics joint linking two bodies and return its RID handle. Requires type: pin, groove or damped_spring; pin needs anchor and optionally body_a/body_b, groove needs groove1_a, groove2_a and anchor_b, damped_spring needs anchor_a, anchor_b and body_a. Bodies are the RIDs from create_physics_2d_body. Returns the joint RID, or an error when required per-type parameters are missing",
      "Physics", {"physics", "2d", "joint", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics2dJointParams, physics_ops::handle_2d_joint_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_2d_area",
      "Create a 2D physics area and return its RID handle. Areas detect overlaps and can apply effects such as gravity over their volume, unlike bodies which collide; use set_physics_2d_area_monitorable to control whether other areas can monitor it. Returns an object with the numeric RID id and an is_valid flag",
      "Physics", {"physics", "2d", "area", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics2dAreaParams, physics_ops::handle_2d_area_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_2d_area_monitorable",
      "Set whether a 2D physics area can be monitored by other areas, controlling overlap detection between areas. Requires rid and monitorable; with monitorable false, other areas will not receive overlap events involving this area, while bodies are unaffected. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "2d", "area", "monitorable"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics2dAreaMonitorableParams, physics_ops::handle_2d_area_set_monitorable}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_2d_circle_shape",
      "Create a 2D circle shape in PhysicsServer2D and return its RID handle. The RID cannot be assigned directly to CollisionShape2D.shape, which expects a Shape2D resource; to bridge it to a node, create a .tres via save_resource(class_type='CircleShape2D') and set CollisionShape2D.shape with set_property using type_hint='Resource'. The RID is only usable with physics server queries such as set_physics_2d_shape_data and intersect_physics_2d_shape",
      "Physics", {"physics", "2d", "shape"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics2dCircleShapeParams, physics_ops::handle_2d_shape_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_2d_shape_data",
      "Set the geometric data of a 2D circle shape RID, configuring its size for physics queries. Requires rid and data, where data is an object such as {'radius': 10} for a circle. Use after create_physics_2d_circle_shape and before intersect_physics_2d_shape so the query uses the correct dimensions. Returns 'ok' or an error for missing parameters",
      "Physics", {"physics", "2d", "shape"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics2dShapeDataParams, physics_ops::handle_2d_shape_set_data}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_physics_3d_space_direct_state",
      "Get the direct state of a 3D physics space, the gateway for 3D physics intersection queries. Pass the space RID and use this before the intersect_physics_3d_* tools to confirm the space is active and attached to a scene, since 3D queries fail on inactive spaces. Returns the space RID, a valid flag and the available queries (intersect_ray, intersect_point, intersect_shape, cast_motion, collide_shape, get_rest_info)",
      "Physics", {"physics", "3d", "space"}, SideEffect::None, tool_flags::kNone,
      kGetPhysics3dSpaceDirectStateParams, physics_ops::handle_3d_space_get_direct_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_3d_ray",
      "Cast a ray in 3D physics space and return the first collision hit. Unlike the 2D ray tool, space_rid is required here: pass the space RID explicitly, since 3D has no auto-detection from the editor scene. Requires from and to; returns a single hit object with position, normal, collider_id, rid and shape, or null when nothing was hit",
      "Physics", {"physics", "3d", "ray"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics3dRayParams, physics_ops::handle_3d_ray_cast}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_3d_shape",
      "Intersect a 3D physics space with a shape RID and return every overlapping collider. Requires space_rid and shape_rid, where shape_rid comes from create_physics_3d_sphere_shape; pass transform to place the shape, motion to sweep it along a Vector3, plus filters such as collision_mask, exclude, collide_with_bodies and collide_with_areas. Returns an array of hits, empty when nothing overlaps. Unlike intersect_physics_3d_ray, this reports all hits",
      "Physics", {"physics", "3d", "intersect", "shape"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics3dShapeParams, physics_ops::handle_3d_intersect_shape}));
  v.push_back(make_spec_tool(ToolSpec{
      "intersect_physics_3d_point",
      "Intersect a 3D physics space at a single point and return the colliders touching it. Requires space_rid and position; optional collision_mask, exclude, collide_with_bodies and collide_with_areas filter the query, and max_results caps the reply (default 32). Returns an array of hits, each with position, normal, collider_id, rid and shape. Use this instead of intersect_physics_3d_ray when you need everything at one spot",
      "Physics", {"physics", "3d", "intersect", "point"}, SideEffect::None, tool_flags::kNone,
      kIntersectPhysics3dPointParams, physics_ops::handle_3d_intersect_point}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_3d_body",
      "Create a 3D physics body in the physics server and return its RID handle. Pass the returned RID to set_physics_3d_body_mode, set_physics_3d_body_transform, apply_physics_3d_body_force, apply_physics_3d_body_impulse, apply_physics_3d_body_torque and set_physics_3d_body_state. The body is a server-side object with no scene node, driven only through these tools. Returns an object with the numeric RID id and an is_valid flag",
      "Physics", {"physics", "3d", "body", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics3dBodyParams, physics_ops::handle_3d_body_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_body_mode",
      "Set the simulation mode of a 3D physics body created by create_physics_3d_body. Requires rid and mode: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear. Only rigid modes respond to forces, impulses and torque from the apply_physics_3d_body_* tools. Returns 'ok' or an error for invalid RIDs and out-of-range modes",
      "Physics", {"physics", "3d", "body", "mode"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dBodyModeParams, physics_ops::handle_3d_body_set_mode}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_physics_3d_body_force",
      "Apply a continuous force to a 3D physics body, accelerating it while the force is applied. Requires rid and force (Vector3 with x, y and z); optional position offsets the force to a specific point, while omitting it applies a central force through the center of mass. Only affects bodies in rigid modes. Returns 'ok' or an error when the body RID or force is missing",
      "Physics", {"physics", "3d", "body", "force"}, SideEffect::None, tool_flags::kNone,
      kApplyPhysics3dBodyForceParams, physics_ops::handle_3d_body_apply_force}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_physics_3d_body_impulse",
      "Apply an instantaneous impulse to a 3D physics body, causing an immediate change in velocity. Requires rid and impulse (Vector3 with x, y and z); optional position applies the impulse off-center, adding rotation, while omitting it applies a central impulse. Use this for single quick events such as jumps or hits instead of apply_physics_3d_body_force. Returns 'ok' or an error for missing parameters",
      "Physics", {"physics", "3d", "body", "impulse"}, SideEffect::None, tool_flags::kNone,
      kApplyPhysics3dBodyImpulseParams, physics_ops::handle_3d_body_apply_impulse}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_body_state",
      "Set a state field on a 3D physics body, such as its transform or velocity. Requires rid, state and value; state selects the field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep, and value must match (transform object, Vector3 or boolean). Pair with get_physics_3d_body_state to read the same fields. Returns 'ok' or an error for invalid state values",
      "Physics", {"physics", "3d", "body", "state"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dBodyStateParams, physics_ops::handle_3d_body_set_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_physics_3d_body_state",
      "Read a state field from a 3D physics body, the counterpart of set_physics_3d_body_state. Requires rid and state, where state is 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping or 4=can_sleep. Returns the field value serialized as JSON, such as a transform object, a Vector3 or a boolean. Use this to track body movement during server-side simulations",
      "Physics", {"physics", "3d", "body", "state"}, SideEffect::None, tool_flags::kNone,
      kGetPhysics3dBodyStateParams, physics_ops::handle_3d_body_get_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_3d_joint",
      "Create a 3D physics joint linking two bodies and return its RID handle. Requires type: pin, hinge, slider, cone_twist or generic_6dof, and body_a_rid from create_physics_3d_body; body_b_rid and the joint-specific transforms are optional. Pin takes local_a/local_b, hinge takes hinge_a/hinge_b, and the remaining types take ref_a/ref_b. Returns the joint RID, or an error when required parameters are missing",
      "Physics", {"physics", "3d", "joint", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics3dJointParams, physics_ops::handle_3d_joint_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_3d_area",
      "Create a 3D physics area and return its RID handle. Areas detect overlaps and can apply effects such as gravity and damping over their volume, unlike bodies which collide; use set_physics_3d_area_param, set_physics_3d_area_transform and set_physics_3d_area_space to configure it. Returns an object with the numeric RID id and an is_valid flag",
      "Physics", {"physics", "3d", "area", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics3dAreaParams, physics_ops::handle_3d_area_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_area_monitorable",
      "Set whether a 3D physics area can be monitored by other areas, controlling overlap detection between areas. Requires rid and monitorable; with monitorable false, other areas will not receive overlap events involving this area, while bodies are unaffected. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "area", "monitorable"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dAreaMonitorableParams, physics_ops::handle_3d_area_set_monitorable}));
  v.push_back(make_spec_tool(ToolSpec{
      "apply_physics_3d_body_torque",
      "Apply a rotational torque to a 3D physics body, spinning it around the given axis. Requires rid and torque (Vector3 with x, y and z), where each component sets rotation around that axis. Only affects bodies in rigid modes; there is no 2D counterpart because 2D rotation is scalar. Returns 'ok' or an error for missing parameters",
      "Physics", {"physics", "3d", "body", "torque"}, SideEffect::None, tool_flags::kNone,
      kApplyPhysics3dBodyTorqueParams, physics_ops::handle_3d_body_apply_torque}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_body_axis_lock",
      "Lock or unlock a single movement axis on a 3D physics body, restricting its freedom of motion. Requires rid, axis and lock; axis is a bit flag: 1=linear_x, 2=linear_y, 4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z, and lock is true to lock or false to unlock. Works on bodies created by create_physics_3d_body. Returns 'ok' or an error for invalid axis values",
      "Physics", {"physics", "3d", "body", "axis_lock"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dBodyAxisLockParams, physics_ops::handle_3d_body_set_axis_lock}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_physics_3d_body_collision_exception",
      "Add a collision exception to a 3D physics body so it stops colliding with another body. Requires rid and excepted_body_rid, both RIDs from create_physics_3d_body. The exception persists until removed with remove_physics_3d_body_collision_exception. Returns 'ok' or an error when either RID is invalid",
      "Physics", {"physics", "3d", "collision", "exception"}, SideEffect::None, tool_flags::kNone,
      kAddPhysics3dBodyCollisionExceptionParams, physics_ops::handle_3d_body_add_collision_exception}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_physics_3d_body_collision_exception",
      "Remove a collision exception from a 3D physics body, re-enabling collisions with the previously excepted body. Requires rid and excepted_body_rid, matching the pair set with add_physics_3d_body_collision_exception. Collisions are only re-enabled after this call, so pairing the two tools keeps the exception list accurate. Returns 'ok' or an error when either RID is invalid",
      "Physics", {"physics", "3d", "collision", "exception"}, SideEffect::None, tool_flags::kNone,
      kRemovePhysics3dBodyCollisionExceptionParams, physics_ops::handle_3d_body_remove_collision_exception}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_joint_param",
      "Set a parameter on a 3D physics joint created by create_physics_3d_joint. Requires rid and at least one of solver_priority (integer, higher values solve the joint first) or disable_collision (boolean, stops the two jointed bodies from colliding); both are optional, and only the ones supplied are applied. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "joint", "param"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dJointParamParams, physics_ops::handle_3d_joint_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_area_space",
      "Attach a 3D area to a physics space (area_set_space semantics). Requires rid of the area and space_rid of the target space; the area then interacts with bodies and other areas in that space. Use this to move an area between spaces or to attach one created by create_physics_3d_area to a specific space. Returns 'ok' or an error when either RID is invalid",
      "Physics", {"physics", "3d", "area", "space_override"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dAreaSpaceParams, physics_ops::handle_3d_area_set_space_override}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_space_solver_iterations",
      "Set the solver iterations of a 3D physics space, trading simulation accuracy for performance. Requires rid; higher solver_iterations values improve constraint accuracy but cost more CPU. This does not change gravity itself: actual gravity is configured through set_physics_3d_area_param or the project's physics settings. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "space", "gravity"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dSpaceSolverIterationsParams, physics_ops::handle_3d_space_set_gravity}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_space_solver_params",
      "Set solver parameters on a 3D physics space: solver_iterations and contact_max_allowed_penetration. Requires rid; solver_iterations (integer) controls constraint accuracy, while contact_max_allowed_penetration (number) sets how deep bodies may overlap before the solver corrects them. Both are optional and applied only when supplied. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "space", "debug"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dSpaceSolverParamsParams, physics_ops::handle_3d_space_set_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_3d_soft_body",
      "Create a 3D soft body and return its RID handle. Soft bodies deform under physics forces instead of moving rigidly; assign a mesh with set_physics_3d_soft_body_mesh to give it shape. Returns an object with the numeric RID id and an is_valid flag",
      "Physics", {"physics", "3d", "soft_body", "create"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics3dSoftBodyParams, physics_ops::handle_3d_soft_body_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_soft_body_mesh",
      "Set the mesh of a 3D soft body, defining the surface that deforms under physics forces. Requires rid of the soft body from create_physics_3d_soft_body and mesh_rid of the mesh to assign. The mesh must be created or loaded before calling this. Returns 'ok' or an error when either RID is invalid",
      "Physics", {"physics", "3d", "soft_body", "mesh"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dSoftBodyMeshParams, physics_ops::handle_3d_soft_body_set_mesh}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_physics_3d_sphere_shape",
      "Create a sphere shape in the physics server and return its RID handle. The RID cannot be assigned directly to CollisionShape3D.shape, which expects a SphereShape3D resource; to bridge it to a node, create a .tres via save_resource(class_type='SphereShape3D') and assign it with set_property using type_hint='Resource'. The RID is only usable with physics server queries such as set_physics_3d_shape_data, add_physics_3d_body_shape and intersect_physics_3d_shape",
      "Physics", {"physics", "3d", "shape"}, SideEffect::None, tool_flags::kNone,
      kCreatePhysics3dSphereShapeParams, physics_ops::handle_3d_shape_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_shape_data",
      "Set the geometric data of a sphere shape RID, configuring its size for physics queries. Requires rid and data, where data is an object such as {'radius': 0.5} for a sphere. Use after create_physics_3d_sphere_shape and before queries such as intersect_physics_3d_shape so the shape has the correct dimensions. Returns 'ok' or an error for missing parameters",
      "Physics", {"physics", "3d", "shape"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dShapeDataParams, physics_ops::handle_3d_shape_set_data}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_physics_3d_body_shape",
      "Add a shape RID to a 3D physics body, giving it collision geometry. Requires rid of the body from create_physics_3d_body and shape_rid from create_physics_3d_sphere_shape; optional transform places the shape relative to the body, disabled starts it disabled (default false). Returns 'ok' or an error when either RID is invalid",
      "Physics", {"physics", "3d", "body"}, SideEffect::None, tool_flags::kNone,
      kAddPhysics3dBodyShapeParams, physics_ops::handle_3d_body_add_shape}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_body_param",
      "Set a physics parameter on a 3D body by enum index, such as mass, friction or gravity scale. Requires rid, param and value; param is a number matching the engine's BodyParameter enum (0=bounce, 1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale, 6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp), and value is the numeric setting. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "body"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dBodyParamParams, physics_ops::handle_3d_body_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_area_param",
      "Set a physics parameter on a 3D area by enum index, such as gravity, damping or priority. Requires rid, param and value; param is a number matching the engine's AreaParameter enum (0=gravity_override_mode, 1=gravity, 2=gravity_vector, 3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode, 6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority), and value matches the parameter's type (number, Vector3 object or boolean). Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "area"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dAreaParamParams, physics_ops::handle_3d_area_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_space_param",
      "Set a physics parameter on a 3D space by enum index, such as solver iterations or penetration limits. Requires space_rid, param and value; param is a number matching the engine's SpaceParameter enum (0=contact_recycle_radius, 1=contact_max_separation, 2=contact_max_allowed_penetration, 3=contact_default_bias, 4=body_linear_velocity_sleep_threshold, 5=body_angular_velocity_sleep_threshold, 6=body_time_to_sleep, 7=solver_iterations), and value is the numeric setting. Returns 'ok' or an error for invalid RIDs",
      "Physics", {"physics", "3d", "space"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dSpaceParamParams, physics_ops::handle_3d_space_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_area_transform",
      "Set the global transform of a 3D physics area, placing its detection volume in the space. Requires rid and transform (Transform3D with basis array and origin). Use this after create_physics_3d_area to position the area where it should detect overlaps. Returns 'ok' or an error when the transform is missing",
      "Physics", {"physics", "3d", "area"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dAreaTransformParams, physics_ops::handle_3d_area_set_transform}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_physics_3d_body_transform",
      "Set the global transform of a 3D physics body, teleporting it in the space. Requires rid and transform (Transform3D with basis array and origin). Use this on bodies created by create_physics_3d_body to place them before simulation or to snap them to a new pose. Returns 'ok' or an error when the transform is missing",
      "Physics", {"physics", "3d", "body"}, SideEffect::None, tool_flags::kNone,
      kSetPhysics3dBodyTransformParams, physics_ops::handle_3d_body_set_transform}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_physics_node_rid",
      "Get the RID of a physics node, bridging scene nodes to the server-side physics tools. Requires path to a CollisionObject2D or CollisionObject3D node (such as a RigidBody2D or CharacterBody3D) inside the edited scene; other node types fail. Returns the numeric RID id and an is_valid flag, which you can then pass as rid to the create_physics_3d_body family of tools",
      "Physics", {"physics", "rid", "bridge"}, SideEffect::None, tool_flags::kNone,
      kGetPhysicsNodeRidParams, physics_ops::handle_physics_node_get_rid}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_object_info",
      "Resolve an object instance ID (ObjectID) to its class and identity. Use it with object IDs obtained from scene or physics tools to identify what an object is. Returns result 'ok', class (Godot class name), name and node_path (for Nodes), plus resource_path (for Resources); errors out if the ID is stale or invalid.",
      "Debug", {"utility", "debug", "physics"}, SideEffect::None, tool_flags::kNone,
      kGetDebugObjectInfoParams, physics_ops::handle_resolve_object}));
  return v;
}

} // namespace physics_tools
} // namespace godot_autopilot

#endif