#include "util/skill_gen.hpp"

namespace godot_autopilot::skill_gen {

namespace {

const char *kPhysicsNavigationDescription =
    R"gda_skill(PhysicsServer and NavigationServer RID-based tools via godot-autopilot: ray, shape and point queries, server-side bodies, joints, areas and soft bodies, and nav maps, regions and agents. RID handles are server objects, not scene resources. Use for physics queries, simulation or pathfinding.)gda_skill";

const char *kRenderingTextDescription =
    R"gda_skill(RenderingServer and TextServer RID-based tools via godot-autopilot: canvas items, meshes, materials, shaders, cameras, lights, viewports, environment post-effects, particles, and font/text shaping measurement. Use for low-level drawing or rendering setup without scene nodes.)gda_skill";

const char *kAudioDescription =
    R"gda_skill(Audio bus layout and playback control via godot-autopilot: inspect and replace the bus layout, add or bypass effects, control players (play, stop, volume, pitch, seek) and switch audio devices. Use when configuring sound or triggering playback in the editor.)gda_skill";

const char *kPhysicsNavigationSkill =
    R"gda_skill(# Physics and Navigation RID Tools

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
)gda_skill";

const char *kPhysicsNavigationToolReference =
    R"gda_skill(# Physics and Navigation Tool Reference

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
)gda_skill";

const char *kRenderingTextSkill =
    R"gda_skill(# Rendering and Text RID Tools

These tools drive `RenderingServer` and `TextServer` directly. Like the
physics tools they work on RID handles - server-side objects that exist
independently of the scene tree. Use them for custom 2D drawing, render-only
3D setups, environment post-effects, shaders, and font/text shaping
measurement.

## The RID mental model

- `create_render_*` tools return the RID as `result.rid`. Pass it back as the
  `canvas_item_rid`/`scenario_rid`/`camera_rid`/... argument of follow-up
  tools.
- Nothing here creates scene nodes and nothing is persisted: RIDs live only
  in the running editor process.
- Two bridges connect RIDs to the edited scene:
  - `get_render_canvas_item_rid` - RID of a `CanvasItem` node in the edited
    scene (node `path` in, RID out; a leading slash and a `root/` prefix are
    tolerated). The result feeds the `add_render_canvas_item_*` draw tools.
  - `find_render_node_from_rid` - the reverse lookup: checks an RID and
    returns `is_valid` plus a `nodes` array (`node_path`, `type`) when
    edited-scene nodes use it. Returns "ok" even for invalid RIDs.
- Several tools accept RIDs that no tool can create (environments, instances).
  Those RIDs must come from external sources such as `code_execute`; the tool
  descriptions call this out explicitly.

## 2D canvas items

1. `create_render_canvas_item` - returns the canvas item RID.
2. Draw commands (all take the canvas item RID):
   - `add_render_canvas_item_rect` - `rect` (`position` + `size`) and
     `color`; optional `antialiased`.
   - `add_render_canvas_item_circle` - center `position`, `radius`, `color`.
   - `add_render_canvas_item_texture_rect` - destination `rect` plus a
     texture RID from `create_render_texture_from_image`; optional `tile`,
     `modulate`, `transpose`.
   - `add_render_canvas_item_line` - `from`, `to`, `color`, optional `width`.
3. `set_render_canvas_item_transform` - `x`/`y` axis scaling plus
   `origin_x`/`origin_y` offset. `set_render_canvas_item_visible` - hard
   on/off, not a toggle.

Example - an overlay rectangle independent of any scene node:

```json
{"name": "create_render_canvas_item", "arguments": {}}
{"name": "add_render_canvas_item_rect",
 "arguments": {"canvas_item_rid": 1,
               "rect": {"position": {"x": 8, "y": 8},
                        "size": {"x": 120, "y": 24}},
               "color": {"r": 0, "g": 0, "b": 0, "a": 0.6}}}
```

## 3D: scenario, camera and lights

- `create_render_scenario` hosts cameras, lights, meshes and fog;
  `set_render_scenario_environment` attaches an environment (the environment
  RID must come from an external source - no create tool exists).
- `create_render_camera` + `set_render_camera_transform` (only the origin is
  applied - the camera always looks down -Z) + `set_render_camera_perspective`
  (`fovy_degrees` default 75, `z_near` 0.01, `z_far` 4000) or
  `set_render_camera_orthogonal` (uniform `size`, default 10).
- `create_render_light` - `type` "directional" (default), "omni" or "spot";
  `set_render_light_param` takes 0=energy, 1=specular, 2=range, 3=size,
  4=attenuation (`range` only affects omni and spot) and
  `set_render_light_color` takes a color, alpha ignored.

## Meshes, surfaces and materials

- `create_render_mesh` holds geometry; `add_render_mesh_surface` adds a
  surface from arrays (`vertices`, `normals`, `tangents`, `colors`, `uvs`,
  `indices`; `primitive` defaults to triangles; surfaces index from 0 in call
  order).
- `create_render_material` + `set_render_material_param` set named shader
  parameters (`type_hint` controls Variant decoding, e.g. "Vector2",
  "Color"); `set_render_mesh_surface_material` assigns a material RID to one
  surface.
- Known gap: **no tool attaches a shader to a material**. Materials only take
  effect with external shader setup such as `code_execute`.

## Viewports and particles

- `create_render_viewport` + `set_render_viewport_size` (default 640x480) +
  `set_render_viewport_clear_mode` (0=always, 1=never, 2=only_next_frame).
  No tool can attach a camera or canvas to a viewport, so standalone
  viewports cannot be rendered on their own - mainly useful for external
  composition.
- `create_render_particles` (`mode` 0=2D, 1=3D) +
  `set_render_particles_emitting`, `set_render_particles_lifetime` (seconds,
  default 1.0) and `restart_render_particles` (fresh emission cycle for
  one-shot bursts). No tool instantiates particles into a scenario - visible
  emission needs external setup.

## Environment and post-effects

Every environment tool takes an `environment_rid` from an external source
(for example `code_execute`); no create tool exists. Available controls:

- `set_render_environment_bg_color` - background color; effective when the
  background mode is color.
- `set_render_environment_ambient_light` - `source` 0=bg, 1=disabled,
  2=color, 3=sky, plus `energy`; fills shadows so unlit surfaces stay
  visible.
- `set_render_environment_glow` - bloom; all parameters optional.
- `set_render_environment_ssr` - screen-space reflections without cubemaps.
- `set_render_environment_tonemap` - `tone_mapper`, `exposure`, `white`.
- `set_render_environment_sdfgi` - global illumination for large scenes.
- `set_render_environment_volumetric_fog` - light-scattering fog.
- `create_render_fog_volume` + `set_render_fog_volume_shape` - localized fog
  volumes, visible when the environment has volumetric fog enabled.

## Shaders and textures

- `create_render_shader` (optional `code` at creation) and
  `set_render_shader_code` (replaces it; compilation happens server-side).
- `set_render_shader_parameter_global` sets a global shader parameter - the
  shader must declare it with the `global` keyword; `type_hint` controls
  Variant decoding.
- `create_render_texture_from_image` loads an image file into a texture RID.
  `image_path` accepts `res://`, `user://` or absolute paths; relative paths
  are not supported. The RID feeds `add_render_canvas_item_texture_rect`.

## Sky, decals, probes and instances

- `create_render_sky` + `set_render_sky_material` (material RID defines the
  appearance, e.g. a procedural sky). No tool can attach a sky to an
  environment.
- `create_render_decal` projects a texture onto nearby surfaces; no tool
  instantiates it into a scenario yet.
- `create_render_reflection_probe` captures surroundings for reflective
  materials; same instantiation caveat.
- `set_render_instance_visible` / `set_render_instance_layer_mask` control
  server-side instances; instance RIDs must come from an external source.

## TextServer: fonts

1. `create_text_font` - font RID; every font tool requires it.
2. `set_text_font_data` - `data` is a path to a `.ttf`/`.otf`/`.woff2` file.
   `get_text_font_system_path` resolves an installed system font
   (`font_name`, optional `weight` default 400, `stretch` default 100,
   `italic`) to an absolute path you can pass as `data` directly.
3. `set_text_font_antialiasing` - 0=none, 1=gray, 2=grayscale, 3=subpixel
   (use 0 for pixel-art fonts). `set_text_font_hinting` - 0=none, 1=light,
   2=normal (light is the typical UI default). Apply both after
   `set_text_font_data` so they affect the loaded font.
4. `has_text_feature` checks TextServer capabilities before relying on them:
   1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures; more
   flags are listed in the schema.

## TextServer: shaped text measurement

1. `create_shaped_text` - `direction` 0=auto, 1=ltr, 2=rtl; `orientation`
   0=horizontal, 1=vertical.
2. `add_shaped_text_string` - `shaped_rid`, `text`, `font_rid` (configured
   with `set_text_font_data` first) and `size` in pixels; an optional
   `language` code improves shaping.
3. `get_shaped_text_size` - returns the pixel dimensions. Call it only after
   adding strings, otherwise the reported size is zero. Use it to size labels
   before creating them.
4. `is_text_locale_right_to_left` - BCP-47 locale in, true/false out. Use it
   to pick alignment or set `direction` before shaping RTL text.

## See also

- `godot-autopilot-physics-navigation` - the PhysicsServer/NavigationServer RID tools, same mental model.
- `godot-autopilot-audio` - AudioServer buses and playback control.
- `godot-autopilot-scene-building` - the scene-node workflows these RID tools bypass.
- `godot-autopilot-properties-signals` - property writes on the nodes that own canvas items.
- `references/TOOL_REFERENCE.md` in this folder - the full render + text tool tables.
)gda_skill";

const char *kRenderingTextToolReference =
    R"gda_skill(# Rendering and Text Tool Reference

Complete tool tables for the render (49 tools) and text (10 tools) domains.
Every name below is a registered tool name - pass it to `call_tool`. See
`SKILL.md` in the parent folder for workflows and the RID mental model.

## 2D canvas items

| Tool | Purpose |
|---|---|
| `create_render_canvas_item` | Server-side 2D canvas item RID for custom drawing. |
| `add_render_canvas_item_rect` | Filled rectangle draw command. |
| `add_render_canvas_item_circle` | Filled circle draw command. |
| `add_render_canvas_item_texture_rect` | Draw a texture inside a rectangle; optional `tile`, `modulate`, `transpose`. |
| `add_render_canvas_item_line` | Line draw command. |
| `set_render_canvas_item_transform` | 2D transform: axis scaling plus `origin_x`/`origin_y` offset. |
| `set_render_canvas_item_visible` | Visibility on/off, not a toggle. |

## Scenario and camera

| Tool | Purpose |
|---|---|
| `create_render_scenario` | 3D scenario hosting cameras, lights, meshes and fog. |
| `set_render_scenario_environment` | Attach an environment (external RID only). |
| `create_render_camera` | Server-side 3D camera RID. |
| `set_render_camera_transform` | Position only; the camera looks down -Z. |
| `set_render_camera_perspective` | Perspective projection (`fovy_degrees`, `z_near`, `z_far`). |
| `set_render_camera_orthogonal` | Orthogonal projection (uniform `size`, `z_near`, `z_far`). |

## Lights

| Tool | Purpose |
|---|---|
| `create_render_light` | `type` "directional" (default), "omni" or "spot". |
| `set_render_light_param` | 0=energy, 1=specular, 2=range, 3=size, 4=attenuation. |
| `set_render_light_color` | Light color; alpha ignored. |

## Meshes and materials

| Tool | Purpose |
|---|---|
| `create_render_mesh` | Mesh RID holding geometry surfaces. |
| `add_render_mesh_surface` | Surface from arrays (`vertices`, `normals`, `tangents`, `colors`, `uvs`, `indices`). |
| `set_render_mesh_surface_material` | Assign a material RID to a 0-based surface index. |
| `create_render_material` | Material RID; needs external shader setup to take effect. |
| `set_render_material_param` | Named shader parameter; `type_hint` controls Variant decoding. |

## Viewports

| Tool | Purpose |
|---|---|
| `create_render_viewport` | Viewport RID; no tool attaches a camera or canvas to it. |
| `set_render_viewport_size` | Pixel size (default 640x480). |
| `set_render_viewport_clear_mode` | 0=always, 1=never, 2=only_next_frame. |

## Particles

| Tool | Purpose |
|---|---|
| `create_render_particles` | Particle system RID; `mode` 0=2D, 1=3D. |
| `set_render_particles_emitting` | Emission on/off. |
| `restart_render_particles` | Force a fresh emission cycle. |
| `set_render_particles_lifetime` | Particle lifetime in seconds (default 1.0). |

## Environment post-effects

All take an `environment_rid` from an external source (no create tool).

| Tool | Purpose |
|---|---|
| `set_render_environment_bg_color` | Background color. |
| `set_render_environment_ambient_light` | Ambient `source` (0=bg, 1=disabled, 2=color, 3=sky) and `energy`. |
| `set_render_environment_glow` | Glow/bloom post-processing. |
| `set_render_environment_ssr` | Screen-space reflections. |
| `set_render_environment_tonemap` | Tone mapping: `tone_mapper`, `exposure`, `white`. |
| `set_render_environment_sdfgi` | SDFGI global illumination. |
| `set_render_environment_volumetric_fog` | Volumetric fog. |

## Fog volumes

| Tool | Purpose |
|---|---|
| `create_render_fog_volume` | Localized volumetric fog volume; `shape` and `size` settable at creation. |
| `set_render_fog_volume_shape` | Change the volume shape after creation. |

## Shaders and textures

| Tool | Purpose |
|---|---|
| `create_render_shader` | Shader RID; optional `code` at creation. |
| `set_render_shader_code` | Replace the shader source. |
| `set_render_shader_parameter_global` | Global shader parameter (declared with the `global` keyword). |
| `create_render_texture_from_image` | Texture RID from `res://`, `user://` or absolute image path. |

## Sky, decals and probes

| Tool | Purpose |
|---|---|
| `create_render_sky` | Sky RID; no tool attaches it to an environment. |
| `set_render_sky_material` | Material RID defining the sky appearance. |
| `create_render_decal` | Decal RID projecting a texture onto surfaces. |
| `create_render_reflection_probe` | Reflection probe RID for reflective materials. |

## Instances

| Tool | Purpose |
|---|---|
| `set_render_instance_visible` | Instance visibility; external instance RID only. |
| `set_render_instance_layer_mask` | Layer bitfield controlling which cameras see the instance. |

## RID bridging

| Tool | Purpose |
|---|---|
| `get_render_canvas_item_rid` | Canvas item RID of a `CanvasItem` node in the edited scene. |
| `find_render_node_from_rid` | Reverse lookup: validity plus matching edited-scene nodes. |

## Text: fonts

| Tool | Purpose |
|---|---|
| `create_text_font` | Font RID required by every font tool. |
| `set_text_font_data` | Load `.ttf`/`.otf`/`.woff2` bytes into the font. |
| `set_text_font_antialiasing` | 0=none, 1=gray, 2=grayscale, 3=subpixel. |
| `set_text_font_hinting` | 0=none, 1=light, 2=normal. |
| `get_text_font_system_path` | Resolve an installed system font to an absolute path (`font_name`, `weight`, `stretch`, `italic`). |
| `has_text_feature` | TextServer capability check (layout/shaping feature flags). |

## Text: shaped text

| Tool | Purpose |
|---|---|
| `create_shaped_text` | Shaped text RID; `direction` 0=auto, 1=ltr, 2=rtl, `orientation` 0=horizontal, 1=vertical. |
| `add_shaped_text_string` | Add text with a configured font and pixel `size`. |
| `get_shaped_text_size` | Measured pixel size; zero until a string is added. |
| `is_text_locale_right_to_left` | RTL check for a BCP-47 locale code. |

Rendering and text RIDs are process-local: they are never persisted to scene
files and become invalid when the editor closes.
)gda_skill";

const char *kAudioSkill =
    R"gda_skill(# Audio Bus and Playback Tools

These tools control the editor process's audio: bus layout inspection and
replacement, effect chains, per-player playback control, and audio device
switching. They act on the editor process - the device tools note explicitly
that a running game process keeps its own device.

## Reading the bus layout

- `get_audio_bus_layout` returns the complete layout: every bus (the Master
  bus always occupies index 0) with its volume, mute/solo state and effect
  chain. Snapshot it before making changes.
- `get_audio_bus_count` returns the number of buses and bounds the valid
  `bus_index` range; `get_audio_bus_name` maps a zero-based index to a name.
  An out-of-range index returns "audio bus not found at index: N".

## Bus volume, mute, solo and bypass

All four take a zero-based `bus_index` and apply immediately to editor
playback:

- `set_audio_bus_volume_db` - decibels; negative attenuates, positive
  amplifies (typical range -80 to +24).
- `set_audio_bus_mute` - `muted` true silences the bus regardless of volume.
- `set_audio_bus_solo` - `solo` true mutes all other buses; soloing several
  buses keeps exactly those audible.
- `set_audio_bus_bypass_effects` - `bypass` true skips the whole effect chain
  without removing it; the chain stays in place for later re-enable.

## Bus effects

- `add_audio_bus_effect` - `effect_type` must be the class name of an
  instantiable `AudioEffect` subclass, for example "AudioEffectReverb" or
  "AudioEffectDistortion"; unknown or abstract names return an error.
  `at_position` inserts at a zero-based slot (default -1 appends at the end).
- `remove_audio_bus_effect` - `effect_index` within the bus effect chain;
  out-of-range errors. Read the current chain with `get_audio_bus_layout`
  first.

> **Warning - layout replacement is whole-object.** `set_audio_bus_layout`
> replaces the entire layout: bus count, names, volumes and effect chains.
> Never write back a hand-made partial object. The flow is always
> `get_audio_bus_layout` -> edit the returned object -> pass it back with
> only your intended change. The change applies immediately to editor
> playback.

## Player control

Player tools accept `AudioStreamPlayer`, `AudioStreamPlayer2D` and
`AudioStreamPlayer3D` nodes. `node_path` is a scene-relative path
("Level1/Player") or an absolute "/root/..." path.

- `play_audio_player` - optional `stream_path` loads an audio resource before
  playing; optional `from_position` starts at an offset in seconds. If the
  node has no stream assigned and no `stream_path` is given, an error
  explains it.
- `stop_audio_player` - the stream stays assigned, so the node can be played
  again later.
- `set_audio_player_volume_db` - per-player decibels applied on top of the
  bus volume.
- `set_audio_player_pitch_scale` - `pitch_scale` 1.0 is normal speed; above
  raises pitch and playback speed, below lowers them.
- `get_audio_player_playback_position` - current position in seconds.
- `seek_audio_player` - `to_position` in seconds; works while the node is
  playing or stopped.

## Audio devices

- `get_audio_device_outputs` / `get_audio_device_inputs` list the hardware
  device names (speakers, headphones, microphones). The list reflects
  attached hardware, not project settings.
- `set_audio_device_output` / `set_audio_device_input` switch the editor
  process's device; `device` must be one of the names returned by the list
  tools.

## Minimal example: a reverb on Master and a shot sound

```json
{"name": "get_audio_bus_layout", "arguments": {}}
{"name": "add_audio_bus_effect",
 "arguments": {"bus_index": 0, "effect_type": "AudioEffectReverb"}}
{"name": "play_audio_player",
 "arguments": {"node_path": "Level1/Player",
               "stream_path": "res://audio/shot.wav"}}
```

Prefer a dedicated bus? Edit the layout object returned by
`get_audio_bus_layout` (append a bus entry), write it back with
`set_audio_bus_layout`, then add the effect at the new bus index and route
the player node to it.

## See also

- `godot-autopilot-physics-navigation` - the PhysicsServer/NavigationServer RID tools.
- `godot-autopilot-rendering-text` - the RenderingServer/TextServer RID tools.
- `godot-autopilot-scene-building` - creating the scenes that host audio player nodes.
- `godot-autopilot-properties-signals` - assigning streams and volume properties to player nodes.
)gda_skill";

} // namespace

std::vector<SkillSpec> make_server_skills() {
  return {
      {"godot-autopilot-physics-navigation", kPhysicsNavigationDescription,
       {{"SKILL.md", kPhysicsNavigationSkill},
        {"references/TOOL_REFERENCE.md", kPhysicsNavigationToolReference}}},
      {"godot-autopilot-rendering-text", kRenderingTextDescription,
       {{"SKILL.md", kRenderingTextSkill},
        {"references/TOOL_REFERENCE.md", kRenderingTextToolReference}}},
      {"godot-autopilot-audio", kAudioDescription,
       {{"SKILL.md", kAudioSkill}}},
  };
}

} // namespace godot_autopilot::skill_gen
