#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_physics(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["physics_2d_ray_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space (omit to auto-detect from editor scene)", false},
            {"from", "object", "Ray origin (Vector2 with x and y fields)", true},
            {"to", "object", "Ray destination (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"hit_from_inside", "boolean", "Should hit shapes the ray starts inside of (default: false)", false},
        });
        m["physics_2d_space_get_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
        });
        m["physics_2d_shape_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"shape_rid", "integer", "RID of the shape to cast (from physics_2d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform2D: origin/rotation/scale)", false},
            {"motion", "object", "Motion vector (Vector2 with x and y fields)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_point_query"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"position", "object", "Query position (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_intersect_shape"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"shape_rid", "integer", "RID of the shape to intersect (from physics_2d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform2D: origin/rotation/scale)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_intersect_point"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"position", "object", "Query position (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_body_create"] = schema::build_schema({});
        m["physics_2d_body_set_mode"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"mode", "number", "Body mode: 0=static,1=kinematic,2=rigid,3=rigid_linear", true},
        });
        m["physics_2d_body_apply_force"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"force", "object", "Force vector (Vector2 with x and y fields)", true},
            {"position", "object", "Force application point (Vector2 with x and y fields)", false},
        });
        m["physics_2d_body_apply_impulse"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"impulse", "object", "Impulse vector (Vector2 with x and y fields)", true},
            {"position", "object", "Impulse application point (Vector2 with x and y fields)", false},
        });
        m["physics_2d_body_set_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
            {"value", "object", "State value (transform object, Vector2, or bool)", true},
        });
        m["physics_2d_body_get_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
        });
        m["physics_2d_joint_create"] = schema::build_schema({
            {"type", "string", "Joint type: pin, groove, damped_spring", true},
            {"anchor", "object", "Pin joint anchor (Vector2 with x and y fields)", false},
            {"body_a", "integer", "RID of the first body", false},
            {"body_b", "integer", "RID of the second body", false},
            {"groove1_a", "object", "Groove joint first groove point (Vector2)", false},
            {"groove2_a", "object", "Groove joint second groove point (Vector2)", false},
            {"anchor_b", "object", "Groove joint anchor on body B (Vector2)", false},
            {"anchor_a", "object", "Damped spring anchor on body A (Vector2)", false},
        });
        m["physics_2d_area_create"] = schema::build_schema({});
        m["physics_2d_area_set_monitorable"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics area", true},
            {"monitorable", "boolean", "Whether the area can be monitored by other areas", true},
        });
        m["physics_2d_shape_create"] = schema::build_schema({});
        m["physics_2d_shape_set_data"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics shape", true},
            {"data", "object", "Shape data (e.g. {\"radius\": 10})", true},
        });

        m["physics_3d_space_get_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
        });
        m["physics_3d_ray_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"from", "object", "Ray origin (Vector3 with x, y and z fields)", true},
            {"to", "object", "Ray destination (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"hit_from_inside", "boolean", "Should hit shapes the ray starts inside of (default: false)", false},
            {"hit_back_faces", "boolean", "Should hit back faces (default: false)", false},
        });
        m["physics_3d_shape_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"shape_rid", "integer", "RID of the shape to cast (from physics_3d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"motion", "object", "Motion vector (Vector3 with x, y and z fields)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_point_query"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"position", "object", "Query position (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_intersect_shape"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"shape_rid", "integer", "RID of the shape to intersect (from physics_3d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_intersect_point"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"position", "object", "Query position (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_body_create"] = schema::build_schema({});
        m["physics_3d_body_set_mode"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"mode", "number", "Body mode: 0=static,1=kinematic,2=rigid,3=rigid_linear", true},
        });
        m["physics_3d_body_apply_force"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"force", "object", "Force vector (Vector3 with x, y and z fields)", true},
            {"position", "object", "Force application point (Vector3 with x, y and z fields)", false},
        });
        m["physics_3d_body_apply_impulse"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"impulse", "object", "Impulse vector (Vector3 with x, y and z fields)", true},
            {"position", "object", "Impulse application point (Vector3 with x, y and z fields)", false},
        });
        m["physics_3d_body_set_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
            {"value", "object", "State value (transform object, Vector3, or bool)", true},
        });
        m["physics_3d_body_get_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
        });
        m["physics_3d_joint_create"] = schema::build_schema({
            {"type", "string", "Joint type: pin, hinge, slider, cone_twist, generic_6dof", true},
            {"body_a_rid", "integer", "RID of the first body", true},
            {"body_b_rid", "integer", "RID of the second body", false},
            {"local_a", "object", "Pin joint local anchor on body A (Vector3)", false},
            {"local_b", "object", "Pin joint local anchor on body B (Vector3)", false},
            {"hinge_a", "object", "Hinge joint transform on body A (Transform3D)", false},
            {"hinge_b", "object", "Hinge joint transform on body B (Transform3D)", false},
            {"ref_a", "object", "Reference transform on body A (Transform3D)", false},
            {"ref_b", "object", "Reference transform on body B (Transform3D)", false},
        });
        m["physics_3d_area_create"] = schema::build_schema({});
        m["physics_3d_area_set_monitorable"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"monitorable", "boolean", "Whether the area can be monitored by other areas", true},
        });
        m["physics_3d_body_apply_torque"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"torque", "object", "Torque vector (Vector3 with x, y and z fields)", true},
        });
        m["physics_3d_body_set_axis_lock"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"axis", "number", "Axis: 1=linear_x,2=linear_y,4=linear_z,8=angular_x,16=angular_y,32=angular_z", true},
            {"lock", "boolean", "Lock (true) or unlock (false) the axis", true},
        });
        m["physics_3d_body_add_collision_exception"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"excepted_body_rid", "integer", "RID of the body to exclude from collision", true},
        });
        m["physics_3d_body_remove_collision_exception"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"excepted_body_rid", "integer", "RID of the body to remove from collision exceptions", true},
        });
        m["physics_3d_joint_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics joint", true},
            {"solver_priority", "integer", "Joint solver priority", false},
            {"disable_collision", "boolean", "Disable collisions between the jointed bodies", false},
        });
        m["physics_3d_area_set_space_override"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"space_rid", "integer", "RID of the 3D physics space to attach the area to", true},
        });
        m["physics_3d_space_set_gravity"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Solver iterations", false},
        });
        m["physics_3d_space_set_debug"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Solver iterations", false},
            {"contact_max_allowed_penetration", "number", "Maximum allowed penetration depth", false},
        });
        m["physics_3d_soft_body_create"] = schema::build_schema({});
        m["physics_3d_soft_body_set_mesh"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D soft body", true},
            {"mesh_rid", "integer", "RID of the mesh to assign to the soft body", true},
        });
        m["physics_3d_shape_create"] = schema::build_schema({});
        m["physics_3d_shape_set_data"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics shape", true},
            {"data", "object", "Shape data (e.g. {\"radius\": 0.5})", true},
        });
        m["physics_3d_body_add_shape"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"shape_rid", "integer", "RID of the shape to add", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"disabled", "boolean", "Add the shape as disabled (default: false)", false},
        });
        m["physics_3d_body_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"param", "number", "Body parameter index (see PhysicsServer3D.BodyParameter)", true},
            {"value", "number", "Parameter value", true},
        });
        m["physics_3d_area_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"param", "number", "Area parameter index (see PhysicsServer3D.AreaParameter)", true},
            {"value", "object", "Parameter value", true},
        });
        m["physics_3d_space_set_param"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"param", "number", "Space parameter index (see PhysicsServer3D.SpaceParameter)", true},
            {"value", "number", "Parameter value", true},
        });
        m["physics_3d_area_set_transform"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"transform", "object", "Transform3D with basis array and origin", true},
        });
        m["physics_3d_body_set_transform"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"transform", "object", "Transform3D with basis array and origin", true},
        });
        m["physics_node_get_rid"] = schema::build_schema({
            {"path", "string", "Node path of a CollisionObject2D or CollisionObject3D node", true},
        });
        m["resolve_object"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID (ObjectID) to resolve", true},
        });

        m["nav_2d_map_create"] = schema::build_schema({
            {"active", "boolean", "Set the map active (default: false)", false},
        });
        m["nav_2d_region_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"enabled", "boolean", "Whether the region is enabled (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_2d_path_query"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"origin", "object", "Path origin (Vector2 with x and y fields)", true},
            {"destination", "object", "Path destination (Vector2 with x and y fields)", true},
            {"optimize", "boolean", "Optimize the path (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_2d_agent_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"position", "object", "Agent position (Vector2 with x and y fields)", true},
            {"radius", "number", "Agent avoidance radius", false},
            {"max_speed", "number", "Agent maximum speed", false},
            {"avoidance_enabled", "boolean", "Enable avoidance (default: false)", false},
        });
        m["nav_2d_agent_set_target"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 2D navigation agent", true},
            {"velocity", "object", "Agent velocity (Vector2 with x and y fields)", true},
        });
        m["nav_3d_map_create"] = schema::build_schema({
            {"active", "boolean", "Set the map active (default: false)", false},
            {"cell_size", "number", "Map cell size in meters", false},
            {"cell_height", "number", "Map cell height in meters", false},
            {"up", "object", "Up direction (Vector3 with x, y and z fields)", false},
        });
        m["nav_3d_map_set_cell_size"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"cell_size", "number", "Cell size in meters", true},
        });
        m["nav_3d_region_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"enabled", "boolean", "Whether the region is enabled (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_3d_region_set_nav_mesh"] = schema::build_schema({
            {"region_rid", "integer", "RID of the 3D navigation region", true},
            {"mesh_path", "string", "Path to a NavigationMesh resource (.tres, .obj)", true},
        });
        m["nav_3d_path_query"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"origin", "object", "Path origin (Vector3 with x, y and z fields)", true},
            {"destination", "object", "Path destination (Vector3 with x, y and z fields)", true},
            {"optimize", "boolean", "Optimize the path (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_3d_path_query_segment"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"start", "object", "Segment start (Vector3 with x, y and z fields)", true},
            {"end", "object", "Segment end (Vector3 with x, y and z fields)", true},
            {"use_collision", "boolean", "Use collision when finding closest point (default: false)", false},
        });
        m["nav_3d_agent_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"position", "object", "Agent position (Vector3 with x, y and z fields)", true},
            {"radius", "number", "Agent avoidance radius", false},
            {"height", "number", "Agent height for avoidance", false},
            {"max_speed", "number", "Agent maximum speed", false},
            {"use_3d_avoidance", "boolean", "Use 3D avoidance (default: false)", false},
        });
        m["nav_3d_agent_set_velocity"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 3D navigation agent", true},
            {"velocity", "object", "Agent velocity (Vector3 with x, y and z fields)", true},
        });
        m["nav_3d_agent_get_next_path"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 3D navigation agent", true},
        });
        m["nav_3d_obstacle_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"position", "object", "Obstacle position (Vector3 with x, y and z fields)", true},
            {"radius", "number", "Obstacle radius", false},
            {"height", "number", "Obstacle height", false},
        });
}

} // namespace godot_autopilot
