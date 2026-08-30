#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_render_audio(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["capture_editor_viewport"] = schema::build_schema({
            {"target", "string", "Target to capture: 'editor' (default) grabs the editor 2D viewport with a fallback to the 3D viewport; 'game' captures the running game's root window over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension", false},
            {"timeout_ms", "integer", "Response timeout in milliseconds for target='game' (default: 5000, max: 30000); ignored for target='editor'", false},
        });

        m["create_render_canvas_item"] = schema::build_schema({});
        m["add_render_canvas_item_rect"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"rect", "object", "Rectangle as {position: {x, y}, size: {x, y}}; example: {position: {x: 10, y: 20}, size: {x: 100, y: 50}}", true},
            {"color", "object", "Fill color {r, g, b, a}, each in 0.0-1.0; example: {r: 1, g: 0.5, b: 0, a: 1}", true},
            {"antialiased", "boolean", "Antialias the rectangle edges (default: false)", false},
        });
        m["add_render_canvas_item_circle"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"position", "object", "Circle center {x, y}; example: {x: 50, y: 50}", true},
            {"color", "object", "Fill color {r, g, b, a}, each in 0.0-1.0; example: {r: 1, g: 0, b: 0, a: 1}", true},
            {"radius", "number", "Circle radius in pixels (default: 1.0)", false},
            {"antialiased", "boolean", "Antialias the circle edges (default: false)", false},
        });
        m["add_render_canvas_item_texture_rect"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"texture_rid", "integer", "RID of the texture to draw, from create_render_texture_from_image", true},
            {"rect", "object", "Destination rectangle {position: {x, y}, size: {x, y}}; example: {position: {x: 0, y: 0}, size: {x: 100, y: 100}}", true},
            {"tile", "boolean", "Tile the texture inside the rect (default: false)", false},
            {"modulate", "object", "Tint color {r, g, b, a}, each in 0.0-1.0; white leaves the texture unchanged", false},
            {"transpose", "boolean", "Swap the texture's x and y axes (default: false)", false},
        });
        m["add_render_canvas_item_line"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"from", "object", "Line start point {x, y}; example: {x: 0, y: 0}", true},
            {"to", "object", "Line end point {x, y}; example: {x: 100, y: 100}", true},
            {"color", "object", "Line color {r, g, b, a}, each in 0.0-1.0", true},
            {"width", "number", "Line width in pixels (default: -1.0, use project setting)", false},
            {"antialiased", "boolean", "Antialias the line (default: false)", false},
        });
        m["set_render_canvas_item_transform"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"x", "number", "Local X axis scale (default: 1.0)", false},
            {"y", "number", "Local Y axis scale (default: 1.0)", false},
            {"origin_x", "number", "Origin X offset in pixels (default: 0.0)", false},
            {"origin_y", "number", "Origin Y offset in pixels (default: 0.0)", false},
        });
        m["set_render_canvas_item_visible"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
            {"visible", "boolean", "Visibility state, true shows and false hides (default: true)", false},
        });
        m["get_render_canvas_item_rid"] = schema::build_schema({
            {"path", "string", "Node path of a CanvasItem node in the edited scene; example: '/root/Node2D/Sprite' (leading slash and root/ prefix are tolerated)", true},
        });
        m["create_render_scenario"] = schema::build_schema({});
        m["set_render_scenario_environment"] = schema::build_schema({
            {"scenario_rid", "integer", "RID of the render scenario, from create_render_scenario", true},
            {"environment_rid", "integer", "RID of the environment to attach; must come from an external source (e.g. code_execute), as no create tool exists", true},
        });
        m["create_render_camera"] = schema::build_schema({});
        m["set_render_camera_transform"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
            {"origin_x", "number", "Camera origin X (default: 0.0); example: 1.5", false},
            {"origin_y", "number", "Camera origin Y (default: 0.0); example: 2.0", false},
            {"origin_z", "number", "Camera origin Z (default: 0.0); example: -10.0", false},
        });
        m["set_render_camera_perspective"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
            {"fovy_degrees", "number", "Vertical field of view in degrees (default: 75); example: 60", false},
            {"z_near", "number", "Near clip distance, keep small but positive (default: 0.01)", false},
            {"z_far", "number", "Far clip distance, objects beyond it are culled (default: 4000)", false},
        });
        m["set_render_camera_orthogonal"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
            {"size", "number", "Orthogonal viewport size in units (default: 10)", false},
            {"z_near", "number", "Near clip distance, keep small but positive (default: 0.01)", false},
            {"z_far", "number", "Far clip distance, objects beyond it are culled (default: 4000)", false},
        });
        m["create_render_light"] = schema::build_schema({
            {"type", "string", "Light type: directional (sun-like, parallel rays), omni (point light), or spot (cone light); default: directional", false},
        });
        m["set_render_light_param"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light, from create_render_light", true},
            {"param", "number", "Light parameter index: 0=energy, 1=specular, 2=range, 3=size, 4=attenuation", true},
            {"value", "number", "Parameter value; units depend on param, e.g. range is in meters", true},
        });
        m["set_render_light_color"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light, from create_render_light", true},
            {"color", "object", "Light color {r, g, b, a}, each in 0.0-1.0; alpha is ignored, example: {r: 1, g: 0.9, b: 0.7, a: 1}", true},
        });
        m["create_render_mesh"] = schema::build_schema({});
        m["add_render_mesh_surface"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh, from create_render_mesh", true},
            {"primitive", "integer", "Primitive type: 0=points, 1=lines, 2=line_strip, 3=triangle_strip, 4=triangle_fan, 5=triangles (default: 5)", false},
            {"arrays", "object", "Surface arrays; keys: vertices and normals (arrays of {x, y, z} objects), tangents (flat array of floats, 4 per vertex), colors (array of {r, g, b, a} objects), uvs (array of {x, y} objects) and indices (array of integers)", false},
        });
        m["set_render_mesh_surface_material"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh, from create_render_mesh", true},
            {"surface", "integer", "Surface index, 0-based in add_render_mesh_surface call order; example: 0 for the first surface", true},
            {"material_rid", "integer", "RID of the material to assign, from create_render_material", true},
        });
        m["create_render_material"] = schema::build_schema({});
        m["set_render_material_param"] = schema::build_schema({
            {"material_rid", "integer", "RID of the render material, from create_render_material", true},
            {"parameter", "string", "Material parameter name; example: 'albedo_color'", true},
            {"value", "object", "Parameter value as JSON; plain numbers/strings/bools also work", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["create_render_viewport"] = schema::build_schema({});
        m["set_render_viewport_size"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport, from create_render_viewport", true},
            {"width", "integer", "Viewport width in pixels (default: 640)", false},
            {"height", "integer", "Viewport height in pixels (default: 480)", false},
        });
        m["set_render_viewport_clear_mode"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport, from create_render_viewport", true},
            {"clear_mode", "number", "Clear mode: 0=always, 1=never, 2=only_next_frame (default: 0)", false},
        });
        m["create_render_particles"] = schema::build_schema({
            {"mode", "integer", "Particle mode: 0=2D, 1=3D (default: 1)", false},
        });
        m["set_render_environment_bg_color"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"color", "object", "Background color {r, g, b, a}, each in 0.0-1.0; example: {r: 0.1, g: 0.2, b: 0.4, a: 1}", true},
        });
        m["set_render_environment_ambient_light"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"color", "object", "Ambient light color {r, g, b, a}, each in 0.0-1.0", true},
            {"source", "number", "Ambient source: 0=bg, 1=disabled, 2=color, 3=sky (default: 0)", false},
            {"energy", "number", "Ambient light energy, scales brightness (default: 1.0)", false},
        });
        m["create_render_fog_volume"] = schema::build_schema({
            {"shape", "number", "Fog volume shape: 0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world (default: 0); can be changed later with set_render_fog_volume_shape", false},
            {"size", "object", "Fog volume size {x, y, z}; example: {x: 4, y: 2, z: 4}", false},
        });
        m["create_render_shader"] = schema::build_schema({
            {"code", "string", "Shader source code (optional); can be replaced later with set_render_shader_code", false},
        });
        m["create_render_texture_from_image"] = schema::build_schema({
            {"image_path", "string", "Image file path; accepts res://, user:// or absolute paths (relative paths are not supported); example: 'res://icon.svg'", true},
        });
        m["set_render_shader_code"] = schema::build_schema({
            {"shader_rid", "integer", "RID of the shader, from create_render_shader", true},
            {"code", "string", "Shader source code; must be a full shader file, not a snippet", true},
        });
        m["set_render_environment_glow"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"enabled", "boolean", "Enable glow (default: true)", false},
            {"level", "number", "Glow level strength (default: 0.85)", false},
            {"intensity", "number", "Glow intensity (default: 0.8)", false},
            {"strength", "number", "Glow strength (default: 1.0)", false},
            {"mix", "number", "Glow mix with scene (default: 0.05)", false},
            {"bloom_threshold", "number", "Bloom threshold (default: 0.0)", false},
            {"blend_mode", "integer", "Glow blend mode: 0=additive, 1=screen, 2=softlight, 3=replace, 4=mix", false},
            {"hdr_bleed_threshold", "number", "HDR bleed threshold (default: 0.5)", false},
            {"hdr_bleed_scale", "number", "HDR bleed scale (default: 2.0)", false},
            {"hdr_luminance_cap", "number", "HDR luminance cap (default: 2.0)", false},
            {"glow_map_strength", "number", "Glow map strength (default: 1.0)", false},
        });
        m["set_render_environment_ssr"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"enabled", "boolean", "Enable screen-space reflections (default: true)", false},
            {"max_steps", "integer", "Maximum reflection steps (default: 64)", false},
            {"fade_in", "number", "Fade-in distance (default: 0.1)", false},
            {"fade_out", "number", "Fade-out distance (default: 0.1)", false},
            {"depth_tolerance", "number", "Depth tolerance (default: 0.1)", false},
        });
        m["set_render_environment_tonemap"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"tone_mapper", "integer", "Tone mapper: 0=linear, 1=reinhard, 2=filmic, 3=aces (default: 0)", false},
            {"exposure", "number", "Exposure, higher values brighten the image (default: 1.0)", false},
            {"white", "number", "White reference in ev; higher values darken highlights (default: 1.0)", false},
        });
        m["set_render_environment_sdfgi"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"enabled", "boolean", "Enable SDFGI (default: true)", false},
            {"cascades", "integer", "Number of cascades, 1-8 (default: 4)", false},
            {"min_cell_size", "number", "Minimum cell size in meters (default: 0.1)", false},
            {"y_scale", "integer", "Y scale mode: 0=50%, 1=25%, 2=12.5% (default: 0)", false},
            {"use_occlusion", "boolean", "Use occlusion culling (default: true)", false},
            {"bounce_feedback", "number", "Bounce feedback (default: 0.5)", false},
            {"read_sky", "boolean", "Read sky for lighting (default: true)", false},
            {"energy", "number", "GI energy (default: 1.0)", false},
            {"normal_bias", "number", "Normal bias (default: 1.0)", false},
            {"probe_bias", "number", "Probe bias (default: 1.0)", false},
        });
        m["set_render_environment_volumetric_fog"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"enabled", "boolean", "Enable volumetric fog (default: true)", false},
            {"density", "number", "Fog density, higher makes fog thicker (default: 0.05)", false},
            {"albedo", "object", "Fog albedo color {r, g, b, a}, each in 0.0-1.0", false},
            {"emission", "object", "Fog emission color {r, g, b, a}, each in 0.0-1.0", false},
            {"emission_energy", "number", "Emission energy (default: 1.0)", false},
            {"anisotropy", "number", "Light anisotropy, 0.0 scatters evenly (default: 0.0)", false},
            {"length", "number", "Fog length, controls spatial scale (default: 0.0)", false},
            {"detail_spread", "number", "Detail spread (default: 0.0)", false},
            {"gi_inject", "number", "GI injection (default: 0.0)", false},
            {"temporal_reprojection", "boolean", "Temporal reprojection smooths flickering (default: false)", false},
            {"temporal_reprojection_amount", "number", "Temporal reprojection amount (default: 0.5)", false},
            {"ambient_inject", "number", "Ambient injection (default: 0.0)", false},
            {"sky_affect", "number", "Sky affect (default: 0.0)", false},
        });
        m["create_render_sky"] = schema::build_schema({});
        m["set_render_sky_material"] = schema::build_schema({
            {"sky_rid", "integer", "RID of the sky, from create_render_sky", true},
            {"material_rid", "integer", "RID of the material to assign, from create_render_material", true},
        });
        m["set_render_particles_emitting"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
            {"emitting", "boolean", "Emit particles, true starts and false stops emission (default: true)", false},
        });
        m["restart_render_particles"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
        });
        m["set_render_particles_lifetime"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
            {"lifetime", "number", "Particle lifetime in seconds; controls how long each particle stays alive (default: 1.0)", false},
        });
        m["create_render_reflection_probe"] = schema::build_schema({});
        m["create_render_decal"] = schema::build_schema({});
        m["set_render_fog_volume_shape"] = schema::build_schema({
            {"fog_rid", "integer", "RID of the fog volume, from create_render_fog_volume", true},
            {"shape", "integer", "Fog volume shape: 0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world", true},
        });
        m["set_render_instance_visible"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"visible", "boolean", "Visibility state, true shows and false hides (default: true)", false},
        });
        m["set_render_instance_layer_mask"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance; must come from an external source (e.g. code_execute), as no create tool exists", true},
            {"mask", "integer", "Layer mask bitfield; bit n selects layer n+1, example: 1 for layer 1, 3 for layers 1 and 2", true},
        });
        m["set_render_shader_parameter_global"] = schema::build_schema({
            {"name", "string", "Global shader parameter name; the shader must declare a matching parameter with the global keyword", true},
            {"value", "object", "Parameter value as JSON; plain numbers/strings/bools also work", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["find_render_node_from_rid"] = schema::build_schema({
            {"rid", "integer", "RID to resolve, e.g. from get_render_canvas_item_rid; returns is_valid and matching nodes for debugging", true},
        });

        m["play_audio_player"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
            {"stream_path", "string", "Optional path to an audio resource file (.ogg, .mp3, .wav) to load and assign to the node before playing", false},
            {"from_position", "number", "Start playback from this time offset in seconds (default: 0.0)", false},
        });
        m["get_audio_bus_layout"] = schema::build_schema({});
        m["set_audio_bus_layout"] = schema::build_schema({
            {"layout", "object", "Serialized AudioBusLayout object as returned by get_audio_bus_layout, typically edited before applying", true},
        });
        m["get_audio_bus_count"] = schema::build_schema({});
        m["get_audio_bus_name"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus and the index must be below the count from get_audio_bus_count", true},
        });
        m["set_audio_bus_volume_db"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"volume_db", "number", "Volume in decibels; negative attenuates, positive amplifies (typical range -80 to +24)", true},
        });
        m["set_audio_bus_mute"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"muted", "boolean", "Mute state; true silences the bus regardless of volume", true},
        });
        m["set_audio_bus_bypass_effects"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"bypass", "boolean", "Bypass state; true skips all effects on the bus without removing them", true},
        });
        m["set_audio_bus_solo"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"solo", "boolean", "Solo state; true mutes all other buses so only this one is heard", true},
        });
        m["add_audio_bus_effect"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"effect_type", "string", "Class name of an instantiable AudioEffect subclass, e.g. 'AudioEffectReverb' or 'AudioEffectDistortion'", true},
            {"at_position", "integer", "Zero-based slot to insert the effect at; -1 (default) appends at the end", false},
        });
        m["remove_audio_bus_effect"] = schema::build_schema({
            {"bus_index", "integer", "Zero-based audio bus index; 0 is the Master bus", true},
            {"effect_index", "integer", "Zero-based index of the effect within the bus effect chain", true},
        });
        m["stop_audio_player"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
        });
        m["set_audio_player_volume_db"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
            {"volume_db", "number", "Volume in decibels; negative attenuates, positive amplifies (typical range -80 to +24)", true},
        });
        m["set_audio_player_pitch_scale"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
            {"pitch_scale", "number", "Pitch multiplier where 1.0 is normal speed; above 1.0 raises pitch and speed, below lowers them", true},
        });
        m["get_audio_player_playback_position"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
        });
        m["seek_audio_player"] = schema::build_schema({
            {"node_path", "string", "Scene-relative path to an AudioStreamPlayer, AudioStreamPlayer2D or AudioStreamPlayer3D node, e.g. 'Level1/Player' or absolute '/root/Level1/Player'", true},
            {"to_position", "number", "Playback position to seek to in seconds; must be within the stream length", true},
        });
        m["get_audio_device_outputs"] = schema::build_schema({});
        m["set_audio_device_output"] = schema::build_schema({
            {"device", "string", "Output device name as returned by get_audio_device_outputs", true},
        });
        m["get_audio_device_inputs"] = schema::build_schema({});
        m["set_audio_device_input"] = schema::build_schema({
            {"device", "string", "Input device name as returned by get_audio_device_inputs", true},
        });

        m["get_display_clipboard"] = schema::build_schema({});
        m["set_display_clipboard"] = schema::build_schema({
            {"text", "string", "Text to write into the system clipboard, replacing its current contents", true},
        });
        m["show_display_dialog"] = schema::build_schema({
            {"title", "string", "Title shown in the dialog's title bar", true},
            {"description", "string", "Message body shown inside the dialog", true},
            {"buttons", "array", "Array of button label strings, one button per entry, e.g. ['OK', 'Cancel']; note the dialog is fire-and-forget, so the clicked button is not reported back", true},
        });
        m["get_display_mouse_position"] = schema::build_schema({});
        m["set_display_mouse_mode"] = schema::build_schema({
            {"mode", "integer", "Cursor behavior: 0=visible, 1=hidden, 2=captured (pointer locked to the window, used for FPS controls), 3=confined, 4=confined_hidden; values outside 0-4 return an error", true},
        });
        m["warp_display_mouse"] = schema::build_schema({
            {"x", "integer", "Mouse X position in screen coordinates (pixels), e.g. 640", true},
            {"y", "integer", "Mouse Y position in screen coordinates (pixels), e.g. 360", true},
        });
        m["capture_display_screen"] = schema::build_schema({
            {"screen", "integer", "Index of the physical screen to capture, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
        });
        m["get_display_screen_count"] = schema::build_schema({});
        m["get_display_screen_dpi"] = schema::build_schema({
            {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
        });
        m["get_display_screen_position"] = schema::build_schema({
            {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
        });
        m["get_display_screen_refresh_rate"] = schema::build_schema({
            {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
        });
        m["get_display_screen_size"] = schema::build_schema({
            {"screen", "integer", "Index of the screen to query, default 0; valid range is 0 to count-1 from get_display_screen_count", false},
        });
        m["get_display_tts_voices"] = schema::build_schema({});
        m["speak_display_tts"] = schema::build_schema({
            {"text", "string", "Text to synthesize and speak aloud, e.g. 'Level complete'", true},
            {"voice", "string", "Voice id from get_display_tts_voices; omit to use the system default voice", false},
            {"volume", "integer", "Speech volume from 0 to 100, default 50", false},
            {"pitch", "number", "Speech pitch multiplier, default 1.0; values above 1.0 raise the pitch", false},
            {"rate", "number", "Speech speed multiplier, default 1.0; values above 1.0 speak faster", false},
        });
        m["stop_display_tts"] = schema::build_schema({});
        m["create_display_window"] = schema::build_schema({
            {"mode", "integer", "Window mode as a Window.Mode enum value, default 0 (windowed); note this enum differs from the DisplayServer.WindowMode used by set_display_window_mode", false},
            {"rect", "object", "Window rectangle as an object with integer x, y, w and h fields, e.g. {x: 100, y: 50, w: 800, h: 600}; defaults to 800x600 at the origin", false},
        });
        m["delete_display_window"] = schema::build_schema({
            {"window_id", "integer", "window_id as returned by create_display_window; errors when no window with this id exists", true},
        });
        m["move_display_window_to_foreground"] = schema::build_schema({
            {"window_id", "integer", "window_id of the window to raise, e.g. from create_display_window; defaults to the main window", false},
        });
        m["request_display_window_attention"] = schema::build_schema({
            {"window_id", "integer", "window_id of the window to flash in the taskbar, e.g. from create_display_window; defaults to the main window", false},
        });
        m["set_display_window_flag"] = schema::build_schema({
            {"flag", "integer", "Window flag as a DisplayServer.WindowFlags enum value, e.g. 0 for always-on-top or 1 for borderless", true},
            {"enabled", "boolean", "true applies the flag, false removes it", true},
            {"window_id", "integer", "window_id of the window to modify, e.g. from create_display_window; defaults to the main window", false},
        });
        m["set_display_window_mode"] = schema::build_schema({
            {"mode", "integer", "Window mode as a DisplayServer.WindowMode enum value, e.g. 1 for fullscreen or 2 for maximized; note this enum differs from the Window.Mode enum used by create_display_window", true},
            {"window_id", "integer", "window_id of the window to modify, e.g. from create_display_window; defaults to the main window", false},
        });
        m["set_display_window_position"] = schema::build_schema({
            {"x", "integer", "Window X position in screen coordinates (pixels)", true},
            {"y", "integer", "Window Y position in screen coordinates (pixels)", true},
            {"window_id", "integer", "window_id of the window to move, e.g. from create_display_window; defaults to the main window", false},
        });
        m["set_display_window_size"] = schema::build_schema({
            {"width", "integer", "Window width in pixels, e.g. 1024", true},
            {"height", "integer", "Window height in pixels, e.g. 768", true},
            {"window_id", "integer", "window_id of the window to resize, e.g. from create_display_window; defaults to the main window", false},
        });
        m["set_display_window_title"] = schema::build_schema({
            {"title", "string", "Title bar text to set, e.g. 'Output Preview'", true},
            {"window_id", "integer", "window_id of the window to retitle, e.g. from create_display_window; defaults to the main window", false},
        });
}

} // namespace godot_autopilot
