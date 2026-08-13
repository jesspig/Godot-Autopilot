#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_physics(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["intersect_physics_2d_ray"] = schema::build_schema({
            {"space_rid", "integer", "Optional RID of the 2D physics space to query; omit to auto-detect the space from the open editor scene's 2D world", false},
            {"from", "object", "Required. Ray origin as a Vector2 object with x and y fields, e.g. {'x': 0, 'y': 0}", true},
            {"to", "object", "Required. Ray destination as a Vector2 object with x and y fields", true},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers the ray hits; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query, e.g. [12345]", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"hit_from_inside", "boolean", "Optional. Whether shapes the ray starts inside of are reported; default false", false},
        });
        m["get_physics_2d_space_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 2D physics space to inspect; it must be active and attached to a scene or the call fails", true},
        });
        m["intersect_physics_2d_shape"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 2D physics space to query", true},
            {"shape_rid", "integer", "Required. RID of the shape to probe with, e.g. from create_physics_2d_circle_shape", true},
            {"transform", "object", "Optional. Transform2D placing the shape: object with origin (Vector2), rotation (radians) and scale (Vector2)", false},
            {"motion", "object", "Optional. Vector2 sweep distance: the shape is moved along this vector while checking for collisions, e.g. {'x': 50, 'y': 0}", false},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
        });
        m["intersect_physics_2d_point"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 2D physics space to query", true},
            {"position", "object", "Required. Query position as a Vector2 object with x and y fields, e.g. {'x': 100, 'y': 50}", true},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
        });
        m["create_physics_2d_body"] = schema::build_schema({});
        m["set_physics_2d_body_mode"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
            {"mode", "number", "Required. Body mode enum index: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear; only rigid modes react to forces and impulses", true},
        });
        m["apply_physics_2d_body_force"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
            {"force", "object", "Required. Force vector as a Vector2 object with x and y fields; applied to the center of mass when position is omitted", true},
            {"position", "object", "Optional. Force application point as a Vector2 object with x and y fields; omitting it applies a central force", false},
        });
        m["apply_physics_2d_body_impulse"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
            {"impulse", "object", "Required. Impulse vector as a Vector2 object with x and y fields; applied to the center of mass when position is omitted", true},
            {"position", "object", "Optional. Impulse application point as a Vector2 object with x and y fields; off-center impulses add rotation", false},
        });
        m["set_physics_2d_body_state"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
            {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
            {"value", "object", "Required. New state value matching the state: a transform object (origin/rotation/scale) for 0, a Vector2 object for 1 and 2, a boolean for 3 and 4", true},
        });
        m["get_physics_2d_body_state"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics body, e.g. from create_physics_2d_body", true},
            {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
        });
        m["create_physics_2d_joint"] = schema::build_schema({
            {"type", "string", "Required. Joint kind: 'pin', 'groove' or 'damped_spring'; each kind needs its own parameter set (see below)", true},
            {"anchor", "object", "Optional. Pin joint anchor as a Vector2 object with x and y fields (used for type='pin')", false},
            {"body_a", "integer", "Optional. RID id of the first body, e.g. from create_physics_2d_body", false},
            {"body_b", "integer", "Optional. RID id of the second body; the joint connects body A to the world when omitted", false},
            {"groove1_a", "object", "Optional. First groove point as a Vector2 object (used for type='groove')", false},
            {"groove2_a", "object", "Optional. Second groove point as a Vector2 object (used for type='groove')", false},
            {"anchor_b", "object", "Optional. Groove joint anchor on body B as a Vector2 object (used for type='groove')", false},
            {"anchor_a", "object", "Optional. Damped spring anchor on body A as a Vector2 object (used for type='damped_spring')", false},
        });
        m["create_physics_2d_area"] = schema::build_schema({});
        m["set_physics_2d_area_monitorable"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D physics area, e.g. from create_physics_2d_area", true},
            {"monitorable", "boolean", "Required. True lets other areas monitor this area for overlap detection; bodies are unaffected", true},
        });
        m["create_physics_2d_circle_shape"] = schema::build_schema({});
        m["set_physics_2d_shape_data"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 2D shape, e.g. from create_physics_2d_circle_shape", true},
            {"data", "object", "Required. Shape geometry object, e.g. {'radius': 10} for a circle", true},
        });

        m["get_physics_3d_space_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 3D physics space to inspect; it must be active and attached to a scene or the call fails", true},
        });
        m["intersect_physics_3d_ray"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 3D physics space to query; unlike the 2D ray tool there is no auto-detection, so this is mandatory", true},
            {"from", "object", "Required. Ray origin as a Vector3 object with x, y and z fields, e.g. {'x': 0, 'y': 0, 'z': 0}", true},
            {"to", "object", "Required. Ray destination as a Vector3 object with x, y and z fields", true},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers the ray hits; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query, e.g. [12345]", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"hit_from_inside", "boolean", "Optional. Whether shapes the ray starts inside of are reported; default false", false},
            {"hit_back_faces", "boolean", "Optional. Whether back faces of double-sided geometry are hit; default false", false},
        });
        m["intersect_physics_3d_shape"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 3D physics space to query", true},
            {"shape_rid", "integer", "Required. RID of the shape to probe with, e.g. from create_physics_3d_sphere_shape", true},
            {"transform", "object", "Optional. Transform3D placing the shape: object with basis (array of 3 row arrays) and origin (Vector3 object)", false},
            {"motion", "object", "Optional. Vector3 sweep distance: the shape is moved along this vector while checking for collisions, e.g. {'x': 0, 'y': 0, 'z': 50}", false},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
        });
        m["intersect_physics_3d_point"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 3D physics space to query", true},
            {"position", "object", "Required. Query position as a Vector3 object with x, y and z fields, e.g. {'x': 0, 'y': 1, 'z': 0}", true},
            {"collision_mask", "integer", "Optional. Layer mask (bitfield) restricting which layers are intersected; default is all layers", false},
            {"exclude", "array", "Optional. Array of RID ids to skip during the query", false},
            {"collide_with_bodies", "boolean", "Optional. Whether physics bodies are hit; default true", false},
            {"collide_with_areas", "boolean", "Optional. Whether areas are hit; default false", false},
            {"max_results", "integer", "Optional. Maximum number of hits to return; default 32", false},
        });
        m["create_physics_3d_body"] = schema::build_schema({});
        m["set_physics_3d_body_mode"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"mode", "number", "Required. Body mode enum index: 0=static, 1=kinematic, 2=rigid, 3=rigid_linear; only rigid modes react to forces and impulses", true},
        });
        m["apply_physics_3d_body_force"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"force", "object", "Required. Force vector as a Vector3 object with x, y and z fields; applied to the center of mass when position is omitted", true},
            {"position", "object", "Optional. Force application point as a Vector3 object with x, y and z fields; omitting it applies a central force", false},
        });
        m["apply_physics_3d_body_impulse"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"impulse", "object", "Required. Impulse vector as a Vector3 object with x, y and z fields; applied to the center of mass when position is omitted", true},
            {"position", "object", "Optional. Impulse application point as a Vector3 object with x, y and z fields; off-center impulses add rotation", false},
        });
        m["set_physics_3d_body_state"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
            {"value", "object", "Required. New state value matching the state: a transform object (basis/origin) for 0, a Vector3 object for 1 and 2, a boolean for 3 and 4", true},
        });
        m["get_physics_3d_body_state"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"state", "number", "Required. Body state enum index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep", true},
        });
        m["create_physics_3d_joint"] = schema::build_schema({
            {"type", "string", "Required. Joint kind: 'pin', 'hinge', 'slider', 'cone_twist' or 'generic_6dof'; each kind uses its own transform parameters", true},
            {"body_a_rid", "integer", "Required. RID id of the first body, e.g. from create_physics_3d_body", true},
            {"body_b_rid", "integer", "Optional. RID id of the second body; the joint connects body A to the world when omitted", false},
            {"local_a", "object", "Optional. Pin joint local anchor on body A as a Vector3 object (used for type='pin')", false},
            {"local_b", "object", "Optional. Pin joint local anchor on body B as a Vector3 object (used for type='pin')", false},
            {"hinge_a", "object", "Optional. Hinge joint frame on body A as a Transform3D object (used for type='hinge')", false},
            {"hinge_b", "object", "Optional. Hinge joint frame on body B as a Transform3D object (used for type='hinge')", false},
            {"ref_a", "object", "Optional. Reference frame on body A as a Transform3D object (used for 'slider', 'cone_twist' and 'generic_6dof')", false},
            {"ref_b", "object", "Optional. Reference frame on body B as a Transform3D object (used for 'slider', 'cone_twist' and 'generic_6dof')", false},
        });
        m["create_physics_3d_area"] = schema::build_schema({});
        m["set_physics_3d_area_monitorable"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
            {"monitorable", "boolean", "Required. True lets other areas monitor this area for overlap detection; bodies are unaffected", true},
        });
        m["apply_physics_3d_body_torque"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"torque", "object", "Required. Torque vector as a Vector3 object with x, y and z fields; each component rotates the body around that axis", true},
        });
        m["set_physics_3d_body_axis_lock"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"axis", "number", "Required. Axis bit flag: 1=linear_x, 2=linear_y, 4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z; only one flag per call", true},
            {"lock", "boolean", "Required. True locks the axis, false unlocks it", true},
        });
        m["add_physics_3d_body_collision_exception"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"excepted_body_rid", "integer", "Required. RID id of the body to exclude from collision", true},
        });
        m["remove_physics_3d_body_collision_exception"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"excepted_body_rid", "integer", "Required. RID id of the body to remove from the collision exception list", true},
        });
        m["set_physics_3d_joint_param"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D joint, e.g. from create_physics_3d_joint", true},
            {"solver_priority", "integer", "Optional. Integer priority for solver order; higher values solve the joint first", false},
            {"disable_collision", "boolean", "Optional. True disables collisions between the two jointed bodies", false},
        });
        m["set_physics_3d_area_space"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D area, e.g. from create_physics_3d_area", true},
            {"space_rid", "integer", "Required. RID of the 3D physics space to attach the area to; the area then interacts with bodies and areas in that space", true},
        });
        m["set_physics_3d_space_solver_iterations"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Optional. Number of solver iterations (integer); higher values improve accuracy at higher CPU cost. Does not change gravity itself", false},
        });
        m["set_physics_3d_space_solver_params"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Optional. Number of solver iterations (integer); higher values improve constraint accuracy at higher CPU cost", false},
            {"contact_max_allowed_penetration", "number", "Optional. Maximum allowed penetration depth (number) before the solver pushes bodies apart", false},
        });
        m["create_physics_3d_soft_body"] = schema::build_schema({});
        m["set_physics_3d_soft_body_mesh"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D soft body, e.g. from create_physics_3d_soft_body", true},
            {"mesh_rid", "integer", "Required. RID id of the mesh to assign to the soft body", true},
        });
        m["create_physics_3d_sphere_shape"] = schema::build_schema({});
        m["set_physics_3d_shape_data"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D shape, e.g. from create_physics_3d_sphere_shape", true},
            {"data", "object", "Required. Shape geometry object, e.g. {'radius': 0.5} for a sphere", true},
        });
        m["add_physics_3d_body_shape"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"shape_rid", "integer", "Required. RID of the shape to attach, e.g. from create_physics_3d_sphere_shape", true},
            {"transform", "object", "Optional. Transform3D placing the shape relative to the body: object with basis (array of 3 row arrays) and origin (Vector3 object)", false},
            {"disabled", "boolean", "Optional. True adds the shape disabled so it does not collide until re-enabled; default false", false},
        });
        m["set_physics_3d_body_param"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"param", "number", "Required. Body parameter enum index: 0=bounce, 1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale, 6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp", true},
            {"value", "number", "Required. Numeric value for the selected parameter, e.g. 0.5 for friction", true},
        });
        m["set_physics_3d_area_param"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
            {"param", "number", "Required. Area parameter enum index: 0=gravity_override_mode, 1=gravity, 2=gravity_vector, 3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode, 6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority", true},
            {"value", "object", "Required. Value matching the selected parameter: a number for scalar params (e.g. gravity, damp), a Vector3 object like {'x': 0, 'y': -9.8, 'z': 0} for vectors, or a boolean", true},
        });
        m["set_physics_3d_space_param"] = schema::build_schema({
            {"space_rid", "integer", "Required. RID of the 3D physics space", true},
            {"param", "number", "Required. Space parameter enum index: 0=contact_recycle_radius, 1=contact_max_separation, 2=contact_max_allowed_penetration, 3=contact_default_bias, 4=body_linear_velocity_sleep_threshold, 5=body_angular_velocity_sleep_threshold, 6=body_time_to_sleep, 7=solver_iterations", true},
            {"value", "number", "Required. Numeric value for the selected parameter", true},
        });
        m["set_physics_3d_area_transform"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics area, e.g. from create_physics_3d_area", true},
            {"transform", "object", "Required. Transform3D as an object with basis (array of 3 row arrays, each with 3 numbers) and origin (Vector3 object with x, y and z)", true},
        });
        m["set_physics_3d_body_transform"] = schema::build_schema({
            {"rid", "integer", "Required. RID of the 3D physics body, e.g. from create_physics_3d_body", true},
            {"transform", "object", "Required. New body transform: object with basis (array of 3 row arrays) and origin (Vector3 object); sets the body's global transform directly", true},
        });
        m["get_physics_node_rid"] = schema::build_schema({
            {"path", "string", "Required. Node path of a CollisionObject2D or CollisionObject3D node in the edited scene, e.g. 'RigidBody3D' or 'MyBody/CharacterBody2D'; other node types fail", true},
        });
        m["get_debug_object_info"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID (ObjectID) to resolve, as returned by scene or physics tools; looked up in ObjectDB. Returns class, name, node_path (Node) or resource_path (Resource)", true},
        });

        m["create_nav_2d_map"] = schema::build_schema({
            {"active", "boolean", "Mark the map active for pathfinding (boolean, default: false)", false},
        });
        m["create_nav_2d_region"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
            {"enabled", "boolean", "Whether the region participates in pathfinding (boolean, default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (integer, default: 1 — layer 1 only)", false},
        });
        m["get_nav_2d_map_path"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
            {"origin", "object", "Path start point (object with x and y numbers, e.g. {\"x\": 0, \"y\": 0})", true},
            {"destination", "object", "Path end point (object with x and y numbers, e.g. {\"x\": 100, \"y\": 50})", true},
            {"optimize", "boolean", "Simplify the path (boolean, default: true)", false},
            {"navigation_layers", "integer", "Layers the path may traverse (integer bitmask, default: 1)", false},
        });
        m["create_nav_2d_agent"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
            {"position", "object", "Agent start position (object with x and y numbers)", true},
            {"radius", "number", "Agent radius used for avoidance (number)", false},
            {"max_speed", "number", "Maximum speed in meters per second used for avoidance (number)", false},
            {"avoidance_enabled", "boolean", "Enable velocity avoidance for this agent (boolean, default: false)", false},
        });
        m["set_nav_2d_agent_velocity"] = schema::build_schema({
            {"agent_rid", "integer", "RID (integer) of the 2D navigation agent, from create_nav_2d_agent", true},
            {"velocity", "object", "Desired velocity (object with x and y numbers, e.g. {\"x\": 100, \"y\": 0})", true},
        });
        m["create_nav_3d_map"] = schema::build_schema({
            {"active", "boolean", "Mark the map active for pathfinding (boolean, default: false)", false},
            {"cell_size", "number", "Map cell size in meters (number, default: 1.0)", false},
            {"cell_height", "number", "Map cell height in meters (number, default: 1.0)", false},
            {"up", "object", "Up direction (object with x, y and z numbers, default: {\"x\": 0, \"y\": 1, \"z\": 0})", false},
        });
        m["set_nav_3d_map_cell_size"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"cell_size", "number", "New cell size in meters (number, e.g. 0.5)", true},
        });
        m["create_nav_3d_region"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"enabled", "boolean", "Whether the region participates in pathfinding (boolean, default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (integer, default: 1 — layer 1 only)", false},
        });
        m["set_nav_3d_region_navigation_mesh"] = schema::build_schema({
            {"region_rid", "integer", "RID (integer) of the 3D navigation region, from create_nav_3d_region", true},
            {"mesh_path", "string", "Path to a NavigationMesh resource (string, res:// path to a .tres or .obj file)", true},
        });
        m["get_nav_3d_map_path"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"origin", "object", "Path start point (object with x, y and z numbers, e.g. {\"x\": 0, \"y\": 0, \"z\": 0})", true},
            {"destination", "object", "Path end point (object with x, y and z numbers)", true},
            {"optimize", "boolean", "Simplify the path (boolean, default: true)", false},
            {"navigation_layers", "integer", "Layers the path may traverse (integer bitmask, default: 1)", false},
        });
        m["get_nav_3d_map_closest_point_to_segment"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"start", "object", "Segment start point (object with x, y and z numbers)", true},
            {"end", "object", "Segment end point (object with x, y and z numbers)", true},
            {"use_collision", "boolean", "Consider collision geometry when finding the closest point (boolean, default: false)", false},
        });
        m["create_nav_3d_agent"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"position", "object", "Agent start position (object with x, y and z numbers)", true},
            {"radius", "number", "Agent radius used for avoidance (number)", false},
            {"height", "number", "Agent height used for 3D avoidance (number)", false},
            {"max_speed", "number", "Maximum speed in meters per second used for avoidance (number)", false},
            {"use_3d_avoidance", "boolean", "Use 3D avoidance instead of 2D ground-plane avoidance (boolean, default: false)", false},
        });
        m["set_nav_3d_agent_velocity"] = schema::build_schema({
            {"agent_rid", "integer", "RID (integer) of the 3D navigation agent, from create_nav_3d_agent", true},
            {"velocity", "object", "Desired velocity (object with x, y and z numbers)", true},
        });
        m["get_nav_3d_agent_state"] = schema::build_schema({
            {"agent_rid", "integer", "RID (integer) of the 3D navigation agent, from create_nav_3d_agent", true},
        });
        m["create_nav_3d_obstacle"] = schema::build_schema({
            {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
            {"position", "object", "Obstacle position (object with x, y and z numbers)", true},
            {"radius", "number", "Obstacle radius (number)", false},
            {"height", "number", "Obstacle height (number)", false},
        });
}

} // namespace godot_autopilot
