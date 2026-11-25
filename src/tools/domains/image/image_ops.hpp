#ifndef GODOT_AUTOPILOT_IMAGE_OPS_HPP
#define GODOT_AUTOPILOT_IMAGE_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace image_ops {

godot_autopilot::util::JsonValue handle_edit_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_pixel_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_texture_gen_action(const godot_autopilot::util::JsonValue &args);

} // namespace image_ops
} // namespace godot_autopilot

#endif
