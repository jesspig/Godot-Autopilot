# Godot Server RID Tools

godot-autopilot exposes the engine's server layer directly: `RenderingServer`,
`TextServer`, `PhysicsServer2D`, `PhysicsServer3D`, `NavigationServer2D` and
`NavigationServer3D`. These tools create and drive server-side objects through
RID handles - plain integer ids - instead of scene nodes. Use them for
ray/shape/point queries, server-side simulation, pathfinding setup, custom 2D
drawing, render-only 3D setups, environment post-effects, shaders, and font
shaping measurement.

## The RID mental model

- Every `create_*` tool returns an RID handle. Physics creation tools return an
  object with the numeric `id` and an `is_valid` flag; navigation tools return
  the id as `rid`; render tools return it as `result.rid`. Keep the number and
  pass it as the `rid`/`space_rid`/`map_rid`/`canvas_item_rid` argument of
  follow-up tools.
- Server objects and scene nodes are separate worlds. A body created with
  `create_physics_2d_body` or `create_physics_3d_body` has **no scene node**:
  nothing appears in the editor scene tree and nothing is written to the scene
  file. RIDs live only in the running editor process.
- Bridges connect RIDs to the edited scene: `get_physics_node_rid` (RID of a
  `CollisionObject2D`/`CollisionObject3D` node such as a `RigidBody2D` or
  `CharacterBody3D`); `get_render_canvas_item_rid` (RID of a `CanvasItem` node,
  feeds the `add_render_canvas_item_*` draw tools); `find_render_node_from_rid`
  (reverse lookup: `is_valid` plus a `nodes` array of `node_path`/`type`);
  `get_debug_object_info` (resolve an object id, e.g. a hit's `collider_id`, to
  class, name, `node_path` or `resource_path`).
- **RID shapes are not resources.** The RIDs from `create_physics_2d_circle_shape`
  and `create_physics_3d_sphere_shape` **cannot be assigned to
  `CollisionShape2D.shape` or `CollisionShape3D.shape`** - those properties expect
  a `Shape2D`/`Shape3D` resource. To attach collision geometry to a scene node,
  create a `.tres` via `save_resource` with `class_type`
  "CircleShape2D"/"SphereShape3D" and assign it with `property_set` (`type_hint`
  "Resource"), or assign inside `code_execute`. Use the RID forms only with the
  server tools.
- Several render tools accept RIDs that no tool can create (environments,
  instances, sky/viewport attachments); those must come from external sources
  such as `code_execute`. The tool descriptions call this out.

## Physics queries

2D: start with `get_physics_2d_space_direct_state` - it reports whether the space
is valid, active and attached to a scene, plus the query entry points; queries
fail on inactive spaces.

- `intersect_physics_2d_ray` - closest hit (`position`, `normal`, `collider_id`,
  `rid`, `shape`) or null. `space_rid` may be omitted: the tool auto-detects the
  open editor scene's 2D world.
- `intersect_physics_2d_shape` - all colliders overlapping a shape RID;
  `transform` places it, `motion` sweeps it along a vector.
- `intersect_physics_2d_point` - everything touching one point; `max_results`
  caps the reply (default 32).

3D: same trio, but **`space_rid` is always required** - no auto-detection. Obtain the space
via `code_execute` (e.g. `SceneRoot.get_node("Player").get_world_3d().space`), then confirm
with `get_physics_3d_space_direct_state`.

Shared filters: `collision_mask`, `exclude` (array of RIDs),
`collide_with_bodies`, `collide_with_areas`; rays also take `hit_back_faces` and
`hit_from_inside`. Only the 2D ray auto-detects the space.

## Physics bodies, joints and areas

`create_physics_2d_body` / `create_physics_3d_body` return a RID, then:

- `set_physics_2d_body_mode` / `set_physics_3d_body_mode` - 0=static, 1=kinematic,
  2=rigid, 3=rigid_linear. Only rigid modes respond to forces and impulses.
- `apply_physics_2d_body_force` / `apply_physics_3d_body_force` (continuous) and
  `apply_physics_2d_body_impulse` / `apply_physics_3d_body_impulse` (instantaneous -
  jumps, hits); an optional `position` applies off-center, which adds rotation. 3D adds
  `apply_physics_3d_body_torque` (no 2D counterpart - 2D rotation is scalar).
- `set_physics_2d_body_state` / `get_physics_2d_body_state` (and the 3D pair)
  write and read fields by index: 0=transform, 1=linear_vel, 2=angular_vel,
  3=sleeping, 4=can_sleep.
- 3D extras: `set_physics_3d_body_transform` (place/teleport),
  `set_physics_3d_body_param` (BodyParameter enum 0-9: bounce, friction, mass,
  inertia, center_of_mass, gravity_scale, damp modes and damps),
  `set_physics_3d_body_axis_lock` (bit flags 1=linear_x through 32=angular_z),
  `add_physics_3d_body_shape` (optional `transform`, `disabled`) and the
  collision exception add/remove pair.

Shapes: `create_physics_2d_circle_shape` / `create_physics_3d_sphere_shape`, sized
with `set_physics_2d_shape_data` / `set_physics_3d_shape_data` (`data`:
`{"radius": ...}`). Shape RIDs feed shape queries and `add_physics_3d_body_shape`.

Joints: `create_physics_2d_joint` with `type` "pin" (needs `anchor`), "groove" or
"damped_spring". `create_physics_3d_joint` with "pin", "hinge", "slider",
"cone_twist" or "generic_6dof" - `body_a_rid` required, type-specific transforms
optional. `set_physics_3d_joint_param` tunes `solver_priority` and `disable_collision`.

Areas: `create_physics_2d_area` / `create_physics_3d_area` detect overlaps and
apply effects such as gravity; `set_physics_2d_area_monitorable` /
`set_physics_3d_area_monitorable` control area-to-area monitoring. 3D adds
`set_physics_3d_area_param` (AreaParameter enum 0-9), `set_physics_3d_area_transform`
and `set_physics_3d_area_space`.

Spaces: `set_physics_3d_space_solver_iterations`, `set_physics_3d_space_solver_params`
and `set_physics_3d_space_param` (raw SpaceParameter enum 0-7) trade constraint
accuracy for CPU; solver settings do not change gravity - configure gravity through
`set_physics_3d_area_param` or project settings. Soft bodies: `create_physics_3d_soft_body`
deforms under forces; assign a mesh with `set_physics_3d_soft_body_mesh`. (4.8: physics
enums moved into scoped namespaces on the engine side; the tools keep the same numeric
values. TODO(verify))

## Navigation maps, regions and paths

The navigation servers follow the same RID pattern: map -> region ->
(3D: navmesh) -> path query. Navigation RIDs come back as `rid` and are never
persisted to scene files.

1. `create_nav_2d_map` / `create_nav_3d_map` - pass `active`: true, maps default to inactive.
   3D extras: `cell_size`, `cell_height` (meters) and `up`; `set_nav_3d_map_cell_size`
   adjusts granularity later - match the cell size baked into the region navmeshes.
2. `create_nav_2d_region` / `create_nav_3d_region` on the map (`map_rid`; optional `enabled`,
   `navigation_layers` bitmask, default 1). Regions define the walkable area - a map with no
   region yields an empty path. For 3D, assign a navmesh with `set_nav_3d_region_navigation_mesh`:
   `mesh_path` is a `res://` path to a `NavigationMesh` resource (`.tres` or `.obj`).
3. Query with `get_nav_2d_map_path` / `get_nav_3d_map_path` (`origin`, `destination`; optional
   `optimize`, `navigation_layers`). Returns `path` as an array of points - empty when the map
   has no baked regions or mesh. `get_nav_3d_map_closest_point_to_segment` snaps a position
   onto the navigation mesh (`start`, `end`, optional `use_collision`).

## Navigation agents and obstacles

- `create_nav_2d_agent` / `create_nav_3d_agent` on a map (`map_rid`, `position`;
  optional `radius`, `max_speed`). Avoidance is off by default - pass
  `avoidance_enabled` (2D) or `use_3d_avoidance` (3D) true to enable it.
- Drive the agent every frame with `set_nav_2d_agent_velocity` /
  `set_nav_3d_agent_velocity`: the agent adjusts the desired velocity around
  obstacles before the value is used for movement.
- `get_nav_3d_agent_state` reads back the registered `position` and the last
  supplied `velocity` - use it to verify the agent is registered.
- `create_nav_3d_obstacle` places an obstacle (`radius`, `height`) that
  avoidance-enabled agents steer around; plain agents ignore it.

## Rendering: 2D canvas items

`create_render_canvas_item` returns the item RID. Draw commands (all take it):
`add_render_canvas_item_rect` (`rect` + `color`; optional `antialiased`),
`add_render_canvas_item_circle`, `add_render_canvas_item_texture_rect` (texture RID from
`create_render_texture_from_image`; optional `tile`, `modulate`, `transpose`) and
`add_render_canvas_item_line`. `set_render_canvas_item_transform` sets axis scaling plus
origin offset; `set_render_canvas_item_visible` is a hard on/off, not a toggle.

## Rendering: scenario, camera, lights, meshes

- `create_render_scenario` hosts cameras, lights, meshes and fog;
  `set_render_scenario_environment` attaches an environment (external RID only -
  no create tool exists).
- `create_render_camera` + `set_render_camera_transform` (only the origin is
  applied - the camera always looks down -Z) + `set_render_camera_perspective`
  (`fovy_degrees` default 75, `z_near` 0.01, `z_far` 4000) or
  `set_render_camera_orthogonal` (uniform `size`, default 10).
- `create_render_light` - `type` "directional" (default), "omni" or "spot";
  `set_render_light_param` takes 0=energy, 1=specular, 2=range, 3=size,
  4=attenuation (`range` only affects omni and spot); `set_render_light_color`
  takes a color, alpha ignored.
- `create_render_mesh` holds geometry; `add_render_mesh_surface` adds a surface
  from arrays (`primitive` defaults to triangles; surfaces index from 0 in call
  order). `create_render_material` + `set_render_material_param` set named shader
  parameters (`type_hint` controls Variant decoding);
  `set_render_mesh_surface_material` assigns a material RID to one surface. Known
  gap: **no tool attaches a shader to a material** - materials only take effect
  with external shader setup such as `code_execute`.

## Rendering: viewports, particles, environment, shaders, textures

- `create_render_viewport` + `set_render_viewport_size` (default 640x480) +
  `set_render_viewport_clear_mode` (0=always, 1=never, 2=only_next_frame). No tool
  can attach a camera or canvas to a viewport, so standalone viewports cannot be
  rendered on their own - mainly useful for external composition.
- `create_render_particles` (`mode` 0=2D, 1=3D) + `set_render_particles_emitting`,
  `set_render_particles_lifetime` (seconds, default 1.0) and
  `restart_render_particles` (fresh emission cycle for one-shot bursts). No tool
  instantiates particles into a scenario.
- Environment tools all take an `environment_rid` from an external source (no create tool
  exists): `set_render_environment_bg_color`, `set_render_environment_ambient_light`
  (`source` 0=bg, 1=disabled, 2=color, 3=sky, plus `energy` - fills shadows so unlit
  surfaces stay visible), `set_render_environment_glow` (bloom),
  `set_render_environment_ssr`, `set_render_environment_tonemap`,
  `set_render_environment_sdfgi`, `set_render_environment_volumetric_fog`.
  `create_render_fog_volume` + `set_render_fog_volume_shape` add localized fog volumes
  (visible when the environment has volumetric fog enabled).
- `create_render_shader` (optional `code` at creation) and
  `set_render_shader_code` replace the source (compilation happens server-side).
  `set_render_shader_parameter_global` sets a global shader parameter - the shader
  must declare it with the `global` keyword. `create_render_texture_from_image`
  loads a texture RID from `res://`, `user://` or absolute image paths; relative
  paths are not supported.
- `create_render_sky` + `set_render_sky_material` (material RID defines the
  appearance; no tool attaches a sky to an environment); `create_render_decal` and
  `create_render_reflection_probe` have no instantiation tools yet;
  `set_render_instance_visible` / `set_render_instance_layer_mask` control
  server-side instances (instance RIDs must come from an external source).

## TextServer: fonts and shaped text

1. `create_text_font` returns the font RID every font tool requires.
   `set_text_font_data` loads a `.ttf`/`.otf`/`.woff2` file path;
   `get_text_font_system_path` resolves an installed system font (`font_name`,
   optional `weight` default 400, `stretch` default 100, `italic`) to an absolute
   path you can pass as `data` directly.
2. `set_text_font_antialiasing` (0=none, 1=gray, 2=grayscale, 3=subpixel; use 0
   for pixel-art fonts) and `set_text_font_hinting` (0=none, 1=light, 2=normal -
   light is the typical UI default) apply after `set_text_font_data` so they
   affect the loaded font.
3. `has_text_feature` checks TextServer capabilities before relying on them:
   1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures.
4. Shaped measurement: `create_shaped_text` (`direction` 0=auto, 1=ltr, 2=rtl;
   `orientation` 0=horizontal, 1=vertical) -> `add_shaped_text_string` (font configured
   first, `size` in pixels, optional `language` improves shaping) ->
   `get_shaped_text_size` returns the pixel dimensions - zero until strings are added;
   use it to size labels before creating them. `is_text_locale_right_to_left` (BCP-47
   in, boolean out) picks alignment or `direction` before shaping RTL text.

## Minimal workflows

2D ray query against the open scene (space auto-detected):

```json
{"name": "intersect_physics_2d_ray", "arguments": {"from": {"x": 0, "y": 0}, "to": {"x": 100, "y": 200}}}
```

Navigation setup and path query (run such sequences in order with `batch_execute`):

```json
{"name": "create_nav_2d_map", "arguments": {"active": true}}
{"name": "create_nav_2d_region", "arguments": {"map_rid": 1}}
{"name": "get_nav_2d_map_path",
 "arguments": {"map_rid": 1, "origin": {"x": 0, "y": 0},
               "destination": {"x": 200, "y": 100}}}
```

## RID lifecycle and silent failure

- **Invalid RIDs fail silently.** Calling any tool - including `free` - on a
  stale, already-freed or never-existing RID neither crashes nor returns an error:
  the server resolves the RID, gets nothing and drops the call. At the tool layer
  this looks like a successful call with no effect. There is no "RID not found"
  error to catch, so track which create call produced which id yourself.
- **Canvas teardown does not cascade.** Freeing a canvas RID does not free the
  canvas items parented to it: their parent pointer is cleared and they become
  orphans that keep existing (and drawing) until freed one by one.
- **Shapes detach automatically.** Freeing a shape RID removes it from every body
  that references it; the bodies themselves are unaffected and stay alive - the
  opposite of the canvas behavior above.
- **No leak reporting.** RIDs leaked at editor exit produce no warning at the
  server layer (only lower-level native rendering-device RIDs get a leak warning).
  Repeatedly creating RIDs without freeing them accumulates silently across a long
  session; everything is reclaimed only when the editor process exits. Object
  creation is two-phase internally (allocate then initialize), invisible at the
  tool level.

## Timing and threads

- **Editor single-thread assumption.** The editor forces single-threaded rendering
  (multi-threaded rendering would crash on startup), so RenderingServer calls made
  on the main thread execute synchronously and immediately. MCP tools can assume
  "the change is in effect by the next draw" - there is no worker thread to wait for.
- **Physics tick order.** Each physics tick runs: state sync -> flush of query
  results (direct-state reads and integration callbacks see the previous step) ->
  node `_physics_process` -> navigation -> message queue flush -> integration step
  -> flush. Node physics callbacks therefore observe the previous step's state.
- **Direct state window and the space lock.** In threaded physics, a direct state object
  fetched outside the sync window returns null with an error, and while a space is
  locked, intersection queries return empty results. Re-entering queries from inside a
  physics callback silently gets nothing; querying from `_physics_process` is safe.
- **Navigation setters are deferred.** Every navigation setter goes through a
  command queue applied at the server's next sync; reading back immediately
  returns the old value, with no error for "not applied yet".
- **Map rebuilds use double-buffered iteration slots.** Region or navmesh changes rebuild
  the map in the other slot at the next sync (possibly asynchronously); slots swap only
  once the rebuild finishes, and if the old slot is still occupied the swap waits another
  round. Poll `map_get_iteration_id` to track rebuild progress (slots exist since 4.7).
- **First query returns an empty path.** Before a map has been synced at least
  once, path queries silently return an empty path in release builds; after
  setting up regions, allow one sync (frame) before trusting an empty result.
- **Nav sync runs twice per frame** - once from process (sync only) and once from
  physics_process (sync + avoidance step + avoidance callbacks). Avoidance-adjusted
  velocities therefore only update on physics ticks.
- **Same-frame ordering.** Each frame runs: node `_process` -> message queue flush ->
  navigation sync -> RenderingServer sync -> draw. A region changed from `_process`
  (e.g. via `code_execute`) is picked up by the same frame's navigation sync, and
  rendering happens after that - friendly to "change it and screenshot immediately".
- Forcing a RenderingServer sync every frame for more than 5 consecutive frames
  logs a persistent warning. Viewports with a dimension of 1px or less silently
  render nothing, and a viewport set to UPDATE_ONCE renders once and then flips
  to disabled.

## See also

- `godot-autopilot-content` - TileMap, animation, audio and UI theming workflows.
- `godot-autopilot-scene-system` - building and editing the scene nodes these RID tools bridge to.
- `godot-autopilot-resources` - saving shape resources as `.tres` for `CollisionShape` assignment.
- `godot-autopilot-scripting` - `code_execute` usage for the external-RID setups.
- `references/tool-reference-physics-nav.md` - the full physics + navigation tool tables.
- `references/tool-reference-rendering.md` - the full render + text tool tables.
