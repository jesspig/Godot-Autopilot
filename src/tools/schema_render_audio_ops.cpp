#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_render_audio(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["editor_capture_viewport"] = schema::build_schema({
            {"target", "string", "Target to capture: \"editor\" (default); \"game\" is not supported yet", false},
        });

        m["canvas_item_create"] = schema::build_schema({});
        m["canvas_item_draw_rect"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"rect", "object", "Rectangle (Rect2 with position and size Vector2 fields)", true},
            {"color", "object", "Fill color (Color with r, g, b, a fields)", true},
            {"antialiased", "boolean", "Antialias the rectangle edges (default: false)", false},
        });
        m["canvas_item_draw_circle"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"position", "object", "Circle center (Vector2 with x and y fields)", true},
            {"color", "object", "Fill color (Color with r, g, b, a fields)", true},
            {"radius", "number", "Circle radius (default: 1.0)", false},
            {"antialiased", "boolean", "Antialias the circle edges (default: false)", false},
        });
        m["canvas_item_draw_texture"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"texture_rid", "integer", "RID of the texture to draw", true},
            {"rect", "object", "Destination rectangle (Rect2 with position and size Vector2 fields)", true},
            {"tile", "boolean", "Tile the texture (default: false)", false},
            {"modulate", "object", "Modulate color (Color with r, g, b, a fields)", false},
            {"transpose", "boolean", "Transpose the texture (default: false)", false},
        });
        m["canvas_item_draw_line"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"from", "object", "Line start point (Vector2 with x and y fields)", true},
            {"to", "object", "Line end point (Vector2 with x and y fields)", true},
            {"color", "object", "Line color (Color with r, g, b, a fields)", true},
            {"width", "number", "Line width (default: -1.0, use project setting)", false},
            {"antialiased", "boolean", "Antialias the line (default: false)", false},
        });
        m["canvas_item_set_transform"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"x", "number", "Transform X axis scale (default: 1.0)", false},
            {"y", "number", "Transform Y axis scale (default: 1.0)", false},
            {"origin_x", "number", "Transform origin X offset (default: 0.0)", false},
            {"origin_y", "number", "Transform origin Y offset (default: 0.0)", false},
        });
        m["canvas_item_set_visible"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"visible", "boolean", "Visibility state (default: true)", false},
        });
        m["canvas_item_get_rid"] = schema::build_schema({
            {"path", "string", "Node path of a CanvasItem node", true},
        });
        m["scenario_create"] = schema::build_schema({});
        m["scenario_set_environment"] = schema::build_schema({
            {"scenario_rid", "integer", "RID of the render scenario", true},
            {"environment_rid", "integer", "RID of the environment to attach", true},
        });
        m["camera_create"] = schema::build_schema({});
        m["camera_set_transform"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"origin_x", "number", "Camera origin X (default: 0.0)", false},
            {"origin_y", "number", "Camera origin Y (default: 0.0)", false},
            {"origin_z", "number", "Camera origin Z (default: 0.0)", false},
        });
        m["camera_set_perspective"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"fovy_degrees", "number", "Vertical field of view in degrees (default: 75)", false},
            {"z_near", "number", "Near clip distance (default: 0.01)", false},
            {"z_far", "number", "Far clip distance (default: 4000)", false},
        });
        m["camera_set_orthogonal"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"size", "number", "Orthogonal viewport size (default: 10)", false},
            {"z_near", "number", "Near clip distance (default: 0.01)", false},
            {"z_far", "number", "Far clip distance (default: 4000)", false},
        });
        m["light_create"] = schema::build_schema({
            {"type", "string", "Light type: directional, omni, or spot (default: directional)", false},
        });
        m["light_set_param"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light", true},
            {"param", "number", "Light parameter index (see RenderingServer.LightParam)", true},
            {"value", "number", "Parameter value", true},
        });
        m["light_set_color"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light", true},
            {"color", "object", "Light color (Color with r, g, b, a fields)", true},
        });
        m["mesh_create"] = schema::build_schema({});
        m["mesh_add_surface"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh", true},
            {"primitive", "integer", "Primitive type (default: triangles)", false},
            {"arrays", "object", "Surface arrays: vertices, normals, tangents, colors, uvs, indices", false},
        });
        m["mesh_set_material"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh", true},
            {"surface", "integer", "Surface index", true},
            {"material_rid", "integer", "RID of the material to assign", true},
        });
        m["material_create"] = schema::build_schema({});
        m["material_set_param"] = schema::build_schema({
            {"material_rid", "integer", "RID of the render material", true},
            {"parameter", "string", "Material parameter name", true},
            {"value", "object", "Parameter value", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["viewport_create"] = schema::build_schema({});
        m["viewport_set_size"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport", true},
            {"width", "integer", "Viewport width (default: 640)", false},
            {"height", "integer", "Viewport height (default: 480)", false},
        });
        m["viewport_set_clear_mode"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport", true},
            {"clear_mode", "number", "Clear mode: 0=always, 1=never, 2=only_next_frame (default: 0)", false},
        });
        m["particle_create"] = schema::build_schema({
            {"mode", "integer", "Particle mode: 0=2D, 1=3D (default: 1)", false},
        });
        m["environment_set_bg_color"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"color", "object", "Background color (Color with r, g, b, a fields)", true},
        });
        m["environment_set_ambient"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"color", "object", "Ambient light color (Color with r, g, b, a fields)", true},
            {"source", "number", "Ambient source: 0=bg, 1=disabled, 2=color, 3=sky (default: 0)", false},
            {"energy", "number", "Ambient light energy (default: 1.0)", false},
        });
        m["fog_create"] = schema::build_schema({
            {"shape", "number", "Fog volume shape (see RenderingServer.FogVolumeShape)", false},
            {"size", "object", "Fog volume size (Vector3 with x, y and z fields)", false},
        });
        m["shader_create"] = schema::build_schema({
            {"code", "string", "Shader source code (optional)", false},
        });
        m["render_texture_create_2d"] = schema::build_schema({
            {"image_path", "string", "Image file path to create the texture from", true},
        });
        m["render_shader_set_code"] = schema::build_schema({
            {"shader_rid", "integer", "RID of the shader", true},
            {"code", "string", "Shader source code", true},
        });
        m["render_shader_get_parameter_list"] = schema::build_schema({});
        m["render_environment_set_glow"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable glow (default: true)", false},
            {"level", "number", "Glow level strength (default: 0.85)", false},
            {"intensity", "number", "Glow intensity (default: 0.8)", false},
            {"strength", "number", "Glow strength (default: 1.0)", false},
            {"mix", "number", "Glow mix with scene (default: 0.05)", false},
            {"bloom_threshold", "number", "Bloom threshold (default: 0.0)", false},
            {"blend_mode", "integer", "Glow blend mode (see EnvironmentGlowBlendMode)", false},
            {"hdr_bleed_threshold", "number", "HDR bleed threshold (default: 0.5)", false},
            {"hdr_bleed_scale", "number", "HDR bleed scale (default: 2.0)", false},
            {"hdr_luminance_cap", "number", "HDR luminance cap (default: 2.0)", false},
            {"glow_map_strength", "number", "Glow map strength (default: 1.0)", false},
        });
        m["render_environment_set_ssr"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable screen-space reflections (default: true)", false},
            {"max_steps", "integer", "Maximum reflection steps (default: 64)", false},
            {"fade_in", "number", "Fade-in distance (default: 0.1)", false},
            {"fade_out", "number", "Fade-out distance (default: 0.1)", false},
            {"depth_tolerance", "number", "Depth tolerance (default: 0.1)", false},
        });
        m["render_environment_set_tonemap"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"tone_mapper", "integer", "Tone mapper: 0=linear, 1=reinhard, 2=filmic, 3=aces (default: 0)", false},
            {"exposure", "number", "Exposure (default: 1.0)", false},
            {"white", "number", "White reference (default: 1.0)", false},
        });
        m["render_environment_set_sdfgi"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable SDFGI (default: true)", false},
            {"cascades", "integer", "Number of cascades (default: 4)", false},
            {"min_cell_size", "number", "Minimum cell size (default: 0.1)", false},
            {"y_scale", "integer", "Y scale mode (see EnvironmentSDFGIYScale)", false},
            {"use_occlusion", "boolean", "Use occlusion culling (default: true)", false},
            {"bounce_feedback", "number", "Bounce feedback (default: 0.5)", false},
            {"read_sky", "boolean", "Read sky for lighting (default: true)", false},
            {"energy", "number", "GI energy (default: 1.0)", false},
            {"normal_bias", "number", "Normal bias (default: 1.0)", false},
            {"probe_bias", "number", "Probe bias (default: 1.0)", false},
        });
        m["render_environment_set_volumetric_fog"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable volumetric fog (default: true)", false},
            {"density", "number", "Fog density (default: 0.05)", false},
            {"albedo", "object", "Fog albedo color (Color with r, g, b, a fields)", false},
            {"emission", "object", "Fog emission color (Color with r, g, b, a fields)", false},
            {"emission_energy", "number", "Emission energy (default: 1.0)", false},
            {"anisotropy", "number", "Light anisotropy (default: 0.0)", false},
            {"length", "number", "Fog length (default: 0.0)", false},
            {"detail_spread", "number", "Detail spread (default: 0.0)", false},
            {"gi_inject", "number", "GI injection (default: 0.0)", false},
            {"temporal_reprojection", "boolean", "Temporal reprojection (default: false)", false},
            {"temporal_reprojection_amount", "number", "Temporal reprojection amount (default: 0.5)", false},
            {"ambient_inject", "number", "Ambient injection (default: 0.0)", false},
            {"sky_affect", "number", "Sky affect (default: 0.0)", false},
        });
        m["render_sky_create"] = schema::build_schema({});
        m["render_sky_set_material"] = schema::build_schema({
            {"sky_rid", "integer", "RID of the sky", true},
            {"material_rid", "integer", "RID of the material to assign", true},
        });
        m["render_particles_set_emitting"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
            {"emitting", "boolean", "Emit particles (default: true)", false},
        });
        m["render_particles_restart"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
        });
        m["render_particles_set_lifetime"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
            {"lifetime", "number", "Particle lifetime in seconds (default: 1.0)", false},
        });
        m["render_reflection_probe_create"] = schema::build_schema({});
        m["render_decal_create"] = schema::build_schema({});
        m["render_fog_volume_set_shape"] = schema::build_schema({
            {"fog_rid", "integer", "RID of the fog volume", true},
            {"shape", "integer", "Fog volume shape (see RenderingServer.FogVolumeShape)", true},
        });
        m["render_instance_set_visible"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance", true},
            {"visible", "boolean", "Visibility state (default: true)", false},
        });
        m["render_instance_set_layer_mask"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance", true},
            {"mask", "integer", "Layer mask bitfield", true},
        });
        m["render_global_shader_parameter_set"] = schema::build_schema({
            {"name", "string", "Global shader parameter name", true},
            {"value", "object", "Parameter value", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["resolve_rid"] = schema::build_schema({
            {"rid", "integer", "RID to resolve", true},
        });

        m["audio_stream_play"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"stream_path", "string", "Path to an audio resource file (.ogg, .mp3, .wav) to load and play", false},
            {"from_position", "number", "Start playback position in seconds", false},
        });
        m["audio_bus_get_layout"] = schema::build_schema({});
        m["audio_bus_set_layout"] = schema::build_schema({
            {"layout", "object", "Audio bus layout (from audio_bus_get_layout)", true},
        });
        m["audio_bus_get_count"] = schema::build_schema({});
        m["audio_bus_get_name"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
        });
        m["audio_bus_set_volume"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"volume_db", "number", "Volume in decibels", true},
        });
        m["audio_bus_set_mute"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"muted", "boolean", "Mute state", true},
        });
        m["audio_bus_set_bypass"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"bypass", "boolean", "Bypass effects state", true},
        });
        m["audio_bus_set_solo"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"solo", "boolean", "Solo state", true},
        });
        m["audio_effect_add"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"effect_type", "string", "AudioEffect class name (e.g. AudioEffectReverb)", true},
            {"at_position", "integer", "Effect slot position (default: -1, append)", false},
        });
        m["audio_effect_remove"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"effect_index", "integer", "Index of the effect to remove", true},
        });
        m["audio_stream_stop"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
        });
        m["audio_stream_set_volume"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"volume_db", "number", "Volume in decibels", true},
        });
        m["audio_stream_set_pitch"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"pitch_scale", "number", "Pitch scale (1.0 = normal)", true},
        });
        m["audio_stream_get_playback_position"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
        });
        m["audio_stream_seek"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"to_position", "number", "Playback position in seconds", true},
        });
        m["audio_get_output_device_list"] = schema::build_schema({});
        m["audio_set_output_device"] = schema::build_schema({
            {"device", "string", "Audio output device name", true},
        });
        m["audio_get_input_device_list"] = schema::build_schema({});
        m["audio_set_input_device"] = schema::build_schema({
            {"device", "string", "Audio input device name", true},
        });

        m["display_clipboard_get"] = schema::build_schema({});
        m["display_clipboard_set"] = schema::build_schema({
            {"text", "string", "Text to copy to the clipboard", true},
        });
        m["display_dialog_show"] = schema::build_schema({
            {"title", "string", "Dialog title", true},
            {"description", "string", "Dialog description text", true},
            {"buttons", "array", "Array of button label strings", true},
        });
        m["display_mouse_get_position"] = schema::build_schema({});
        m["display_mouse_set_mode"] = schema::build_schema({
            {"mode", "integer", "Mouse mode: 0=visible, 1=hidden, 2=captured, 3=confined, 4=confined_hidden", true},
        });
        m["display_mouse_warp"] = schema::build_schema({
            {"x", "integer", "Mouse X position (screen coordinates)", true},
            {"y", "integer", "Mouse Y position (screen coordinates)", true},
        });
        m["display_screen_capture"] = schema::build_schema({
            {"screen", "integer", "Screen index to capture (default: 0)", false},
        });
        m["display_screen_get_count"] = schema::build_schema({});
        m["display_screen_get_dpi"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_position"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_refresh_rate"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_size"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_tts_get_voices"] = schema::build_schema({});
        m["display_tts_speak"] = schema::build_schema({
            {"text", "string", "Text to speak aloud", true},
            {"voice", "string", "Voice ID (from display_tts_get_voices)", false},
            {"volume", "integer", "Volume 0-100 (default: 50)", false},
            {"pitch", "number", "Speech pitch (default: 1.0)", false},
            {"rate", "number", "Speech rate (default: 1.0)", false},
        });
        m["display_tts_stop"] = schema::build_schema({});
        m["display_window_create"] = schema::build_schema({
            {"mode", "integer", "Window mode (see Window.Mode, default: 0)", false},
            {"rect", "object", "Window rect {\"x\":int,\"y\":int,\"w\":int,\"h\":int} (default: 800x600 at 0,0)", false},
        });
        m["display_window_delete"] = schema::build_schema({
            {"window_id", "integer", "Window ID of the sub-window to delete", true},
        });
        m["display_window_move_to_foreground"] = schema::build_schema({
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_request_attention"] = schema::build_schema({
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_flag"] = schema::build_schema({
            {"flag", "integer", "Window flag (see DisplayServer.WindowFlags)", true},
            {"enabled", "boolean", "Flag state", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_mode"] = schema::build_schema({
            {"mode", "integer", "Window mode (see DisplayServer.WindowMode)", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_position"] = schema::build_schema({
            {"x", "integer", "Window X position (screen coordinates)", true},
            {"y", "integer", "Window Y position (screen coordinates)", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_size"] = schema::build_schema({
            {"width", "integer", "Window width in pixels", true},
            {"height", "integer", "Window height in pixels", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_title"] = schema::build_schema({
            {"title", "string", "Window title text", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
}

} // namespace godot_autopilot
