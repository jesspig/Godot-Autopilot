# Server RID Tools

The server layer is exposed directly: `RenderingServer`, `PhysicsServer2D/3D`,
and `NavigationServer2D/3D`. These tools create and drive server objects through
RID handles (integer ids) instead of scene nodes.

## RID mental model

- Every `create_*` tool returns an RID. Physics tools return `{id, is_valid}`;
  navigation tools return `rid`; render tools return `result.rid`. Pass the
  number as `rid`, `space_rid`, `map_rid`, or `canvas_item_rid`.
- Server objects and scene nodes are separate. A body created with
  `create_physics_2d_body` or `create_physics_3d_body` has no scene node: nothing
  appears in the tree or scene file. RIDs live only in the editor process.
- Bridges: `get_physics_node_rid` for a `CollisionObject2D/3D` node;
  `get_render_canvas_item_rid` for a `CanvasItem`; `find_render_node_from_rid`
  for the reverse lookup.
- RID shapes are not resources. Shape RIDs cannot be assigned to
  `CollisionShape2D.shape` or `CollisionShape3D.shape`; those expect
  `Shape2D`/`Shape3D`. Create a `.tres` with `save_resource` and assign it, or
  assign inside `code_execute`.

## Physics queries

2D: confirm the space with `get_physics_2d_space_direct_state`. The ray, shape,
and point queries return the closest hit, overlapping colliders, or everything
touching a point; `max_results` defaults to 32. `space_rid` may be omitted and
the edited scene's 2D world is auto-detected.

3D: the same trio, but `space_rid` is always required. Get the space with
`code_execute` and confirm it with `get_physics_3d_space_direct_state`.

Shared filters: `collision_mask`, `exclude`, `collide_with_bodies`,
`collide_with_areas`; rays also take `hit_back_faces` and `hit_from_inside`.

## Bodies, joints, areas

- `create_physics_*_body` returns a RID. Body modes: 0 static, 1 kinematic,
  2 rigid, 3 rigid_linear; only rigid modes respond to forces and impulses.
- `apply_physics_*_body_force` is continuous; `apply_physics_*_body_impulse` is
  instantaneous. An optional `position` applies off-center and adds rotation. 3D
  adds torque.
- `set_physics_*_body_state` / `get_physics_*_body_state` address fields by
  index: 0 transform, 1 linear velocity, 2 angular velocity, 3 sleeping,
  4 can_sleep.
- 3D extras: transform, body parameters (bounce, friction, mass, inertia, damp),
  axis locks, and collision exceptions.
- Shapes: circle/sphere RIDs sized with `set_physics_*_shape_data`; they feed
  queries and 3D body shapes.
- Joints: 2D pin/groove/damped_spring; 3D pin/hinge/slider/cone_twist/
  generic_6dof. `body_a_rid` is required.
- Areas detect overlaps and apply effects. 3D adds area parameters, transform,
  and space.

## Navigation

Map -> region -> (3D navmesh) -> path query. Navigation RIDs are never persisted.

1. Create a 2D/3D map with `active: true`; maps default to inactive. 3D also
   takes cell size, cell height, and up; match the navmesh bake.
2. Create a region on the map. A map with no region yields an empty path. For
   3D, assign a `NavigationMesh` with `set_nav_3d_region_navigation_mesh`.
3. Query with `get_nav_2d_map_path` / `get_nav_3d_map_path`; the result is empty
   without baked regions. `get_nav_3d_map_closest_point_to_segment` snaps a
   position onto the mesh.

Agents: create on a map with `map_rid` and `position`; avoidance is off by
default. Obstacles affect only avoidance-enabled agents.

## Rendering

2D canvas: `create_render_canvas_item` returns the item RID. Draw with
`add_render_canvas_item_rect`, `_circle`, `_texture_rect`, and `_line`.
`set_render_canvas_item_transform` sets scale plus origin;
`set_render_canvas_item_visible` is a hard on/off.

3D: `create_render_scenario` hosts cameras, lights, meshes, and fog;
`set_render_scenario_environment` takes an external environment RID.
`create_render_camera` applies only the origin and looks down -Z; use a
perspective or orthogonal setter. `create_render_light` takes
directional/omni/spot and energy, range, size, and color. `create_render_mesh`
holds geometry; `add_render_mesh_surface` adds a surface from arrays. Materials
set named shader parameters, but no tool attaches a shader to a material.

Viewports, particles, environment, shaders, and textures use the same RID
pattern; environment and instance RIDs must come from `code_execute`.

## RID lifecycle and silent failure

- Invalid RIDs fail silently. Any tool, including `free`, on a stale or
  never-existing RID neither crashes nor errors; the server drops the call.
  Track which create call produced which id.
- Canvas teardown does not cascade: freeing a canvas RID orphans its canvas
  items, which keep drawing until freed one by one.
- Shapes detach automatically: freeing a shape removes it from every body that
  references it; the bodies stay alive.
- No leak reporting: leaked RIDs accumulate silently until the editor exits.

## Timing and threads

- The editor forces single-threaded rendering, so RenderingServer calls on the
  main thread execute synchronously; a change is in effect by the next draw.
- Physics tick order: state sync, query flush, `_physics_process`, navigation,
  message flush, integration, flush. Node callbacks observe the previous step.
- A direct state object fetched outside the sync window returns null; while a
  space is locked, intersection queries return empty. Query from
  `_physics_process`, not inside a physics callback.
- Navigation setters are queued and applied at the next sync; an immediate read
  returns the old value without an error.
- Map rebuilds use double-buffered iteration slots; poll
  `map_get_iteration_id` to track progress. Before the first sync, path queries
  may return empty in release builds.
- Navigation syncs twice per frame: process (sync only) and physics (sync plus
  avoidance). Avoidance-adjusted velocities update on physics ticks.
- Each frame runs `_process`, message flush, navigation sync, render sync, then
  draw. Forcing a render sync every frame for more than 5 frames logs a warning.
## Failure signatures

| Symptom | Cause | First move |
|---|---|---|
| A server call succeeds but nothing changes | Stale or never-created RID | Track ids; check `is_valid` |
| Shape assignment to a node fails | RID shape is not a resource | Save a `.tres` and assign that |
| 3D physics query fails | Missing `space_rid` | Get the world space first |
| Navigation path is empty | No region, no navmesh, or not synced | Add a region; wait one sync |
