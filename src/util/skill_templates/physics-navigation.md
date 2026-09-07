# Physics and Navigation RID Tools

godot-autopilot exposes the engine's server layer directly: `PhysicsServer2D`,
`PhysicsServer3D`, `NavigationServer2D` and `NavigationServer3D`. The tools in
this skill create and drive server-side objects through RID handles - plain
integer ids - instead of scene nodes. Use them for ray/shape/point queries,
server-side simulation and pathfinding setup.

## The RID mental model

- Every `create_*` tool returns a RID handle. Physics creation tools return an
  object with the numeric `id` and an `is_valid` flag; navigation tools return
  the id as `rid`. Keep the number and pass it as the `rid`/`map_rid`/
  `agent_rid` argument of follow-up tools.
- Server objects and scene nodes are separate worlds. A body created with
  `create_physics_2d_body` or `create_physics_3d_body` has **no scene node**:
  nothing appears in the editor scene tree and nothing is written to the scene
  file. It exists only in the running editor process and is driven purely
  through these tools.
- Bridge from scenes to servers with `get_physics_node_rid`: pass a `path` to
  a `CollisionObject2D` or `CollisionObject3D` node in the edited scene (for
  example a `RigidBody2D` or `CharacterBody3D`) and it returns that node's RID
  so the standard body tools can drive it.
- `get_debug_object_info` resolves objects the other way: pass a numeric
  object id (for example the `collider_id` of a query hit) and get back its
  class, name, `node_path` (for nodes) or `resource_path` (for resources).

> **Warning - RID shapes are not resources.** The RIDs returned by
> `create_physics_2d_circle_shape` and `create_physics_3d_sphere_shape`
> **cannot be assigned directly to `CollisionShape2D.shape` or
> `CollisionShape3D.shape`**. Those properties expect a `Shape2D`/`Shape3D`
> resource, and a server RID is not a resource. To attach collision geometry
> to a scene node, either create a `.tres` via `save_resource` with
> `class_type` "CircleShape2D"/"SphereShape3D" and assign it to the node with
> `property_set` using `type_hint` "Resource", or do the assignment inside
> `code_execute`. Use the RID forms only with the server tools listed below.

## 2D physics queries

Start with `get_physics_2d_space_direct_state`: pass the space `rid` and it
reports whether the space is valid, active and attached to a scene, plus the
available query entry points (`intersect_ray`, `intersect_point`,
`intersect_shape`, `cast_motion`, `collide_shape`, `get_rest_info`). Queries
fail on inactive spaces, so confirm first.

- `intersect_physics_2d_ray` casts a ray and returns the single closest hit
  (`position`, `normal`, `collider_id`, `rid`, `shape`) or null. `from` and
  `to` are required. `space_rid` may be omitted: the tool auto-detects the
  space from the open editor scene's 2D world; if that fails it asks you to
  provide `space_rid` or open a scene with a 2D viewport.
- `intersect_physics_2d_shape` intersects a shape RID with the space and
  returns **all** overlapping colliders. `space_rid` and `shape_rid` are
  required; `transform` places the shape and `motion` sweeps it along a
  vector.
- `intersect_physics_2d_point` returns everything touching one point;
  `space_rid` and `position` are required, `max_results` caps the reply
  (default 32).

Shared filters for all three: `collision_mask`, `exclude` (array of RIDs),
`collide_with_bodies`, `collide_with_areas`, plus `hit_back_faces` and
`hit_from_inside` on rays. Only the 2D ray auto-detects the space - shape and
point queries need an explicit `space_rid`.

## 2D bodies, joints and areas

`create_physics_2d_body` returns a RID. Then:

- `set_physics_2d_body_mode` - 0=static, 1=kinematic, 2=rigid, 3=rigid_linear.
  Only rigid modes respond to forces and impulses.
- `apply_physics_2d_body_force` (continuous) and
  `apply_physics_2d_body_impulse` (instantaneous - jumps, hits). Both take a
  `force`/`impulse` vector; an optional `position` applies it off-center,
  which adds rotation.
- `set_physics_2d_body_state` / `get_physics_2d_body_state` write and read
  fields by index: 0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping,
  4=can_sleep.

Shapes: `create_physics_2d_circle_shape` returns a RID and
`set_physics_2d_shape_data` sizes it, for example `data`:
`{"radius": 10}`. The shape RID feeds queries such as
`intersect_physics_2d_shape`.

Joints: `create_physics_2d_joint` with `type` "pin" (needs `anchor`),
"groove" (needs `groove1_a`, `groove2_a`, `anchor_b`) or "damped_spring"
(needs `anchor_a`, `anchor_b`, `body_a`). Bodies are RIDs from
`create_physics_2d_body`.

Areas: `create_physics_2d_area` detects overlaps and can apply effects such as
gravity over its volume; `set_physics_2d_area_monitorable` controls whether
other areas can monitor it.

## 3D physics queries

Same trio as 2D with one difference: **`space_rid` is always required** - 3D
has no auto-detection from the editor scene. Confirm the space first with
`get_physics_3d_space_direct_state`, which fails on inactive spaces. To obtain
a 3D space RID, use `code_execute` against the edited scene (for example
`SceneRoot.get_node("Player").get_world_3d().space`).

- `intersect_physics_3d_ray` - closest hit for a ray (`from`, `to` required).
- `intersect_physics_3d_shape` - all overlaps of a `shape_rid`; `motion`
  sweeps it along a vector.
- `intersect_physics_3d_point` - all colliders at `position`, capped by
  `max_results` (default 32).

## 3D bodies

`create_physics_3d_body` returns a RID; then configure it:

- `set_physics_3d_body_mode` - 0=static, 1=kinematic, 2=rigid, 3=rigid_linear.
- `set_physics_3d_body_transform` - place or teleport the body with a
  Transform3D (`basis` array plus `origin`).
- `apply_physics_3d_body_force` / `apply_physics_3d_body_impulse` - linear
  push; an optional `position` makes it off-center.
- `apply_physics_3d_body_torque` - rotation per axis. There is no 2D
  counterpart because 2D rotation is scalar.
- `set_physics_3d_body_state` / `get_physics_3d_body_state` - same field
  indexes as 2D (0=transform, 1=linear_vel, 2=angular_vel, 3=sleeping,
  4=can_sleep) with vector values.
- `set_physics_3d_body_param` - numeric `BodyParameter` enum: 0=bounce,
  1=friction, 2=mass, 3=inertia, 4=center_of_mass, 5=gravity_scale,
  6=linear_damp_mode, 7=angular_damp_mode, 8=linear_damp, 9=angular_damp.
- `set_physics_3d_body_axis_lock` - bit flags: 1=linear_x, 2=linear_y,
  4=linear_z, 8=angular_x, 16=angular_y, 32=angular_z; `lock` true locks,
  false unlocks.
- `add_physics_3d_body_shape` - attach a shape RID as collision geometry,
  with optional relative `transform` and `disabled`.
- `add_physics_3d_body_collision_exception` /
  `remove_physics_3d_body_collision_exception` - add and remove with the same
  `excepted_body_rid` pair so the exception list stays accurate.

Collision geometry: `create_physics_3d_sphere_shape` +
`set_physics_3d_shape_data` (`data`: `{"radius": 0.5}`), then
`add_physics_3d_body_shape`.

## 3D joints, areas and spaces

Joints: `create_physics_3d_joint` with `type` "pin", "hinge", "slider",
"cone_twist" or "generic_6dof". `body_a_rid` is required; `body_b_rid` and the
type-specific transforms are optional - "pin" takes `local_a`/`local_b`,
"hinge" takes `hinge_a`/`hinge_b`, the rest take `ref_a`/`ref_b`.
`set_physics_3d_joint_param` tunes `solver_priority` and `disable_collision`.

Areas: `create_physics_3d_area`, then `set_physics_3d_area_param`
(AreaParameter enum: 0=gravity_override_mode, 1=gravity, 2=gravity_vector,
3=gravity_is_point, 4=gravity_point_unit_distance, 5=linear_damp_override_mode,
6=linear_damp, 7=angular_damp_override_mode, 8=angular_damp, 9=priority),
`set_physics_3d_area_transform` to place its detection volume,
`set_physics_3d_area_space` to move it between spaces, and
`set_physics_3d_area_monitorable` for area-to-area overlap.

Spaces: `set_physics_3d_space_solver_iterations` trades constraint accuracy
for CPU; `set_physics_3d_space_solver_params` sets `solver_iterations` and
`contact_max_allowed_penetration`; `set_physics_3d_space_param` takes the raw
SpaceParameter enum (0=contact_recycle_radius, 1=contact_max_separation,
2=contact_max_allowed_penetration, 3=contact_default_bias,
4=body_linear_velocity_sleep_threshold,
5=body_angular_velocity_sleep_threshold, 6=body_time_to_sleep,
7=solver_iterations). Solver settings do not change gravity - configure
gravity through `set_physics_3d_area_param` or the project's physics settings.

Soft bodies: `create_physics_3d_soft_body` deforms under forces; assign a mesh
with `set_physics_3d_soft_body_mesh` (`mesh_rid` from a created or loaded
mesh).

## Navigation maps, regions and paths

Navigation follows the same RID pattern on the navigation servers:
map -> region -> (3D: navmesh) -> path query. Navigation RIDs come back as
`rid` and are never persisted to scene files.

1. `create_nav_2d_map` / `create_nav_3d_map`. Pass `active`: true - maps
   default to inactive. 3D extras: `cell_size`, `cell_height` (meters) and
   `up`. `set_nav_3d_map_cell_size` adjusts grid granularity later; match it
   with the cell size baked into the region navmeshes.
2. `create_nav_2d_region` / `create_nav_3d_region` on the map (`map_rid`;
   optional `enabled`, `navigation_layers` bitmask, default 1). Regions
   define the walkable area - a map with no region yields an empty path. For
   3D, assign a navmesh with `set_nav_3d_region_navigation_mesh`: `mesh_path`
   is a `res://` path to a `NavigationMesh` resource (`.tres` or `.obj`).
3. Query with `get_nav_2d_map_path` / `get_nav_3d_map_path` (`origin`,
   `destination`; optional `optimize`, `navigation_layers`). Returns `path`
   as an array of points - empty when the map has no baked regions or mesh.
   `get_nav_3d_map_closest_point_to_segment` snaps a position onto the
   navigation mesh (`start`, `end`, optional `use_collision`).

## Navigation agents and obstacles

Agents implement avoidance-based movement:

- `create_nav_2d_agent` / `create_nav_3d_agent` on a map (`map_rid`,
  `position`; optional `radius`, `max_speed`). Avoidance is off by default -
  pass `avoidance_enabled` (2D) or `use_3d_avoidance` (3D) true to enable it.
- Drive the agent every frame with `set_nav_2d_agent_velocity` /
  `set_nav_3d_agent_velocity`: the agent adjusts the desired velocity around
  obstacles before the value is used for movement.
- `get_nav_3d_agent_state` reads back the registered `position` and the last
  supplied `velocity` - use it to verify the agent is registered and
  responding.
- `create_nav_3d_obstacle` places an obstacle (`radius`, `height`) that
  avoidance-enabled agents steer around; plain agents ignore it.

## Minimal workflows

2D ray query against the open scene (space auto-detected):

```json
{"name": "intersect_physics_2d_ray",
 "arguments": {"from": {"x": 0, "y": 0}, "to": {"x": 100, "y": 200}}}
```

3D query chain (space RID obtained via `code_execute`, shape RID from the
create call):

```json
{"name": "get_physics_3d_space_direct_state",
 "arguments": {"space_rid": 1}}
{"name": "create_physics_3d_sphere_shape", "arguments": {}}
{"name": "set_physics_3d_shape_data",
 "arguments": {"rid": 2, "data": {"radius": 0.5}}}
{"name": "intersect_physics_3d_shape",
 "arguments": {"space_rid": 1, "shape_rid": 2}}
```

Navigation setup and path query:

```json
{"name": "create_nav_2d_map", "arguments": {"active": true}}
{"name": "create_nav_2d_region", "arguments": {"map_rid": 1}}
{"name": "get_nav_2d_map_path",
 "arguments": {"map_rid": 1, "origin": {"x": 0, "y": 0},
               "destination": {"x": 200, "y": 100}}}
```

Run such sequences in order with `batch_execute`.

## See also

- `godot-autopilot-rendering-text` - the RenderingServer/TextServer RID tools, same mental model.
- `godot-autopilot-audio` - AudioServer buses and playback control.
- `godot-autopilot-scene-building` - building the scene nodes these RID tools can bridge to.
- `godot-autopilot-properties-signals` - assigning shape resources to `CollisionShape` nodes with `property_set`.
- `references/TOOL_REFERENCE.md` in this folder - the full physics + navigation tool tables.
