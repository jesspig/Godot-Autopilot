#ifndef GODOT_AUTOPILOT_THEME_OPS_HPP
#define GODOT_AUTOPILOT_THEME_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace theme_ops {

bool parse_color_channels(const godot_autopilot::util::JsonValue &val,
                          double (&channels)[4],
                          std::string &out_error);

godot_autopilot::util::JsonValue handle_create_theme_resource(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_theme_color(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_theme_constant(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_theme_font_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_theme_stylebox_flat(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_theme_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_apply_theme_to_control(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_control_anchor_preset(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_control_layout(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_theme_item_bundle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_theme(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_control_layout(const godot_autopilot::util::JsonValue &args);

} // namespace theme_ops
} // namespace godot_autopilot

#endif
