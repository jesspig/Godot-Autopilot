# Rendering and Text Tool Reference

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

## See also

- `godot-autopilot-servers` (`SKILL.md`) - RID mental model, lifecycle and timing semantics.
- `references/tool-reference-physics-nav.md` - the full physics + navigation tool tables.
- `godot-autopilot-scene-system` - the scene nodes that own canvas items these tools draw to.
- `godot-autopilot-content` - content-domain workflows (tilemaps, animation, audio, UI).
