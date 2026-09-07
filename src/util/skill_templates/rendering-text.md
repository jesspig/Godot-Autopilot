# Rendering and Text RID Tools

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
