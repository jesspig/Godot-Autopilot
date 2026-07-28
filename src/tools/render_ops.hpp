#ifndef GODOT_SELF_DRIVING_RENDER_OPS_HPP
#define GODOT_SELF_DRIVING_RENDER_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace render_ops {

mcp::JsonValue handle_canvas_item_create(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_draw_rect(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_draw_circle(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_draw_texture(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_draw_line(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_set_transform(const mcp::JsonValue& args);
mcp::JsonValue handle_canvas_item_set_visible(const mcp::JsonValue& args);
mcp::JsonValue handle_scenario_create(const mcp::JsonValue& args);
mcp::JsonValue handle_scenario_set_environment(const mcp::JsonValue& args);
mcp::JsonValue handle_camera_create(const mcp::JsonValue& args);
mcp::JsonValue handle_camera_set_transform(const mcp::JsonValue& args);
mcp::JsonValue handle_camera_set_perspective(const mcp::JsonValue& args);
mcp::JsonValue handle_camera_set_orthogonal(const mcp::JsonValue& args);
mcp::JsonValue handle_light_create(const mcp::JsonValue& args);
mcp::JsonValue handle_light_set_param(const mcp::JsonValue& args);
mcp::JsonValue handle_light_set_color(const mcp::JsonValue& args);
mcp::JsonValue handle_mesh_create(const mcp::JsonValue& args);
mcp::JsonValue handle_mesh_add_surface(const mcp::JsonValue& args);
mcp::JsonValue handle_mesh_set_material(const mcp::JsonValue& args);
mcp::JsonValue handle_material_create(const mcp::JsonValue& args);
mcp::JsonValue handle_material_set_param(const mcp::JsonValue& args);
mcp::JsonValue handle_viewport_create(const mcp::JsonValue& args);
mcp::JsonValue handle_viewport_set_size(const mcp::JsonValue& args);
mcp::JsonValue handle_viewport_set_clear_mode(const mcp::JsonValue& args);
mcp::JsonValue handle_particle_create(const mcp::JsonValue& args);
mcp::JsonValue handle_environment_set_bg_color(const mcp::JsonValue& args);
mcp::JsonValue handle_environment_set_ambient(const mcp::JsonValue& args);
mcp::JsonValue handle_fog_create(const mcp::JsonValue& args);
mcp::JsonValue handle_shader_create(const mcp::JsonValue& args);

} // namespace render_ops
} // namespace godot_self_driving

#endif
