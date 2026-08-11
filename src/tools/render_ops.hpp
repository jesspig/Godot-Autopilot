#ifndef GODOT_AUTOPILOT_RENDER_OPS_HPP
#define GODOT_AUTOPILOT_RENDER_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace render_ops {

mcp::JsonValue handle_canvas_item_create(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_draw_rect(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_draw_circle(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_draw_texture(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_draw_line(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_set_transform(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_set_visible(const mcp::JsonValue &args);
mcp::JsonValue handle_scenario_create(const mcp::JsonValue &args);
mcp::JsonValue handle_scenario_set_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_camera_create(const mcp::JsonValue &args);
mcp::JsonValue handle_camera_set_transform(const mcp::JsonValue &args);
mcp::JsonValue handle_camera_set_perspective(const mcp::JsonValue &args);
mcp::JsonValue handle_camera_set_orthogonal(const mcp::JsonValue &args);
mcp::JsonValue handle_light_create(const mcp::JsonValue &args);
mcp::JsonValue handle_light_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_light_set_color(const mcp::JsonValue &args);
mcp::JsonValue handle_mesh_create(const mcp::JsonValue &args);
mcp::JsonValue handle_mesh_add_surface(const mcp::JsonValue &args);
mcp::JsonValue handle_mesh_set_material(const mcp::JsonValue &args);
mcp::JsonValue handle_material_create(const mcp::JsonValue &args);
mcp::JsonValue handle_material_set_param(const mcp::JsonValue &args);
mcp::JsonValue handle_viewport_create(const mcp::JsonValue &args);
mcp::JsonValue handle_viewport_set_size(const mcp::JsonValue &args);
mcp::JsonValue handle_viewport_set_clear_mode(const mcp::JsonValue &args);
mcp::JsonValue handle_particle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_bg_color(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_ambient(const mcp::JsonValue &args);
mcp::JsonValue handle_fog_create(const mcp::JsonValue &args);
mcp::JsonValue handle_shader_create(const mcp::JsonValue &args);
mcp::JsonValue handle_texture_create_2d(const mcp::JsonValue &args);
mcp::JsonValue handle_shader_set_code(const mcp::JsonValue &args);
mcp::JsonValue handle_shader_get_parameter_list(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_glow(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_ssr(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_tonemap(const mcp::JsonValue &args);
mcp::JsonValue handle_environment_set_sdfgi(const mcp::JsonValue &args);
mcp::JsonValue
handle_environment_set_volumetric_fog(const mcp::JsonValue &args);
mcp::JsonValue handle_sky_create(const mcp::JsonValue &args);
mcp::JsonValue handle_sky_set_material(const mcp::JsonValue &args);
mcp::JsonValue handle_particles_set_emitting(const mcp::JsonValue &args);
mcp::JsonValue handle_particles_restart(const mcp::JsonValue &args);
mcp::JsonValue handle_particles_set_lifetime(const mcp::JsonValue &args);
mcp::JsonValue handle_reflection_probe_create(const mcp::JsonValue &args);
mcp::JsonValue handle_decal_create(const mcp::JsonValue &args);
mcp::JsonValue handle_fog_volume_set_shape(const mcp::JsonValue &args);
mcp::JsonValue handle_instance_set_visible(const mcp::JsonValue &args);
mcp::JsonValue handle_instance_set_layer_mask(const mcp::JsonValue &args);
mcp::JsonValue handle_global_shader_parameter_set(const mcp::JsonValue &args);
mcp::JsonValue handle_canvas_item_get_rid(const mcp::JsonValue &args);
mcp::JsonValue handle_resolve_rid(const mcp::JsonValue &args);

} // namespace render_ops
} // namespace godot_autopilot

#endif
