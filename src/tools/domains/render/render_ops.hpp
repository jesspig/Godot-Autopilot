#ifndef GODOT_AUTOPILOT_RENDER_OPS_HPP
#define GODOT_AUTOPILOT_RENDER_OPS_HPP

#include <cstdint>
#include "util/json_value.hpp"

namespace godot_autopilot {
namespace render_ops {

struct RenderRidDomain {};

godot_autopilot::util::JsonValue handle_environment_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_instance_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_viewport_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_light_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_particles_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_material_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shader_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_sky_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_fog_volume_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_decal_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reflection_probe_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mesh_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_texture_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_inspect_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_free_rid(const godot_autopilot::util::JsonValue &args);
void forget_environment_state(int64_t rid_id);

godot_autopilot::util::JsonValue handle_canvas_item_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_draw_rect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_draw_circle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_draw_texture(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_draw_line(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_set_transform(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_set_visible(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_scenario_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_scenario_set_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_set_transform(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_set_perspective(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_camera_set_orthogonal(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_light_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_light_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_light_set_color(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mesh_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mesh_add_surface(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_mesh_set_material(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_material_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_material_set_param(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_viewport_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_viewport_set_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_viewport_set_clear_mode(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_particle_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_fog_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shader_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_texture_create_2d(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_shader_set_code(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_environment_set_glow(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_environment_set_ssr(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_environment_set_tonemap(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_environment_set_sdfgi(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue
handle_environment_set_volumetric_fog(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_sky_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_sky_set_material(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_particles_set_emitting(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_particles_restart(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_particles_set_lifetime(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reflection_probe_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_decal_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_fog_volume_set_shape(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_instance_set_visible(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_instance_set_layer_mask(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_global_shader_parameter_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_canvas_item_get_rid(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_resolve_rid(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_node_particles_restart(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_node_particles_set_params(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_node_particles_get_state(const godot_autopilot::util::JsonValue &args);

} // namespace render_ops
} // namespace godot_autopilot

#endif
