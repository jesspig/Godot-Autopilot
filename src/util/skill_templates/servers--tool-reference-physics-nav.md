# Physics and Navigation Tool Reference

Complete tool tables for the physics (48 tools) and navigation (15 tools)
domains. Every name below is a registered tool name - pass it to `call_tool`.
See `SKILL.md` in the parent folder for workflows and the RID mental model.

## 2D queries and spaces

| Tool | Purpose |
|---|---|
| `get_physics_2d_space_direct_state` | Direct state of a 2D space; confirms it is valid, active and in a scene, and lists the available queries. |
| `intersect_physics_2d_ray` | Closest-hit ray cast; `space_rid` may be omitted to auto-detect the open editor scene's 2D world. |
| `intersect_physics_2d_shape` | All colliders overlapping a shape RID; `transform` places it, `motion` sweeps it. |
| `intersect_physics_2d_point` | All colliders touching a point; `max_results` caps the reply (default 32). |

## 2D bodies

| Tool | Purpose |
|---|---|
| `create_physics_2d_body` | Create a server-side 2D body; returns the numeric RID `id` and `is_valid`. |
| `set_physics_2d_body_mode` | 0=static, 1=kinematic, 2=rigid, 3=rigid_linear; only rigid modes respond to forces. |
| `apply_physics_2d_body_force` | Continuous force; optional `position` makes it off-center. |
| `apply_physics_2d_body_impulse` | Instantaneous impulse for jumps/hits; optional off-center `position`. |
| `set_physics_2d_body_state` | Write a field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep. |
| `get_physics_2d_body_state` | Read the same fields back as JSON. |

## 2D joints, areas and shapes

| Tool | Purpose |
|---|---|
| `create_physics_2d_joint` | Joint between two bodies; `type` "pin", "groove" or "damped_spring". |
| `create_physics_2d_area` | 2D area that detects overlaps and can apply effects such as gravity. |
| `set_physics_2d_area_monitorable` | Whether other areas can monitor this area. |
| `create_physics_2d_circle_shape` | Circle shape RID; NOT assignable to `CollisionShape2D.shape` - server queries only. |
| `set_physics_2d_shape_data` | Size the circle shape, e.g. `data`: `{"radius": 10}`. |

## 3D queries and spaces

| Tool | Purpose |
|---|---|
| `get_physics_3d_space_direct_state` | Direct state of a 3D space; gateway for 3D queries, fails on inactive spaces. |
| `intersect_physics_3d_ray` | Closest-hit ray cast; `space_rid` required (no auto-detection in 3D). |
| `intersect_physics_3d_shape` | All colliders overlapping a shape RID; optional `transform` and `motion` sweep. |
| `intersect_physics_3d_point` | All colliders at a point; `max_results` caps the reply (default 32). |

## 3D bodies

| Tool | Purpose |
|---|---|
| `create_physics_3d_body` | Create a server-side 3D body; returns the numeric RID `id` and `is_valid`. |
| `set_physics_3d_body_mode` | 0=static, 1=kinematic, 2=rigid, 3=rigid_linear. |
| `set_physics_3d_body_transform` | Place or teleport the body with a Transform3D (basis array plus origin). |
| `apply_physics_3d_body_force` | Continuous force; rigid modes only. |
| `apply_physics_3d_body_impulse` | Instantaneous impulse; optional off-center `position`. |
| `apply_physics_3d_body_torque` | Rotational torque per axis; no 2D counterpart. |
| `set_physics_3d_body_state` | Write a field: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping, 4=can_sleep. |
| `get_physics_3d_body_state` | Read the same fields back as JSON. |
| `set_physics_3d_body_param` | BodyParameter enum: 0=bounce, 1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale, 6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp. |
| `set_physics_3d_body_axis_lock` | Bit flags: 1=linear_x, 2=linear_y, 4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z. |
| `add_physics_3d_body_shape` | Attach a shape RID as collision geometry; optional `transform`, `disabled`. |
| `add_physics_3d_body_collision_exception` | Stop colliding with `excepted_body_rid`. |
| `remove_physics_3d_body_collision_exception` | Re-enable collisions; pair with the add tool. |

## 3D shapes and soft bodies

| Tool | Purpose |
|---|---|
| `create_physics_3d_sphere_shape` | Sphere shape RID; NOT assignable to `CollisionShape3D.shape` - server queries only. |
| `set_physics_3d_shape_data` | Size the sphere shape, e.g. `data`: `{"radius": 0.5}`. |
| `create_physics_3d_soft_body` | Soft body RID that deforms under forces. |
| `set_physics_3d_soft_body_mesh` | Assign the deforming mesh via `mesh_rid`. |

## 3D joints, areas and space solver

| Tool | Purpose |
|---|---|
| `create_physics_3d_joint` | `type` "pin" (`local_a`/`local_b`), "hinge" (`hinge_a`/`hinge_b`), "slider", "cone_twist" or "generic_6dof" (`ref_a`/`ref_b`); `body_a_rid` required. |
| `set_physics_3d_joint_param` | Tune `solver_priority` and `disable_collision`. |
| `create_physics_3d_area` | 3D area for overlap detection and effects such as gravity and damping. |
| `set_physics_3d_area_param` | AreaParameter enum: 0=gravity_override_mode, 1=gravity, 2=gravity_vector, 3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode, 6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority. |
| `set_physics_3d_area_transform` | Place the area's detection volume. |
| `set_physics_3d_area_space` | Attach the area to a space (`space_rid`). |
| `set_physics_3d_area_monitorable` | Whether other areas can monitor this area. |
| `set_physics_3d_space_solver_iterations` | Solver iterations: accuracy versus CPU. |
| `set_physics_3d_space_solver_params` | `solver_iterations` and `contact_max_allowed_penetration`. |
| `set_physics_3d_space_param` | SpaceParameter enum 0-7 (contact recycle/separation/penetration, bias, sleep thresholds, time to sleep, solver iterations). |

## Scene bridging and debugging

| Tool | Purpose |
|---|---|
| `get_physics_node_rid` | RID of a `CollisionObject2D`/`CollisionObject3D` node in the edited scene, for the body tools. |
| `get_debug_object_info` | Resolve an object id (e.g. a hit's `collider_id`) to class, name, node path or resource path. |

## Navigation 2D

| Tool | Purpose |
|---|---|
| `create_nav_2d_map` | 2D map RID; `active` defaults to false. |
| `create_nav_2d_region` | Region on a map; defines the walkable area. |
| `get_nav_2d_map_path` | Path query (`origin`, `destination`); empty when the map has no regions. |
| `create_nav_2d_agent` | Agent for avoidance; optional `radius`, `max_speed`, `avoidance_enabled` (default false). |
| `set_nav_2d_agent_velocity` | Desired velocity, fed every frame to drive avoidance. |

## Navigation 3D

| Tool | Purpose |
|---|---|
| `create_nav_3d_map` | 3D map RID; optional `cell_size`, `cell_height`, `up`. |
| `set_nav_3d_map_cell_size` | Grid granularity; match the cell size baked into region navmeshes. |
| `create_nav_3d_region` | Region on a map; assign a navmesh before querying paths. |
| `set_nav_3d_region_navigation_mesh` | Assign a `NavigationMesh` from `mesh_path` (`res://` `.tres` or `.obj`). |
| `get_nav_3d_map_path` | Path query; empty when the map has no baked mesh. |
| `get_nav_3d_map_closest_point_to_segment` | Closest map point to a segment; snaps positions onto the navmesh. |
| `create_nav_3d_agent` | Agent for avoidance; optional `radius`, `height`, `max_speed`, `use_3d_avoidance`. |
| `set_nav_3d_agent_velocity` | Desired velocity, fed every frame; readable via the state tool. |
| `get_nav_3d_agent_state` | Registered `position` and last supplied `velocity`. |
| `create_nav_3d_obstacle` | Obstacle that avoidance-enabled agents steer around. |

Navigation RIDs are process-local: they are never persisted to scene files and
become invalid when the editor closes.

## See also

- `godot-autopilot-servers` (`SKILL.md`) - RID mental model, lifecycle and timing semantics.
- `references/tool-reference-rendering.md` - the full render + text tool tables.
- `godot-autopilot-scene-system` - the scene nodes and `CollisionShape` properties these tools bridge to.
- `godot-autopilot-content` - content-domain workflows (tilemaps, animation, audio, UI).
