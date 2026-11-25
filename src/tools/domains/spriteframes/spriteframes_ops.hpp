#ifndef GODOT_AUTOPILOT_SPRITEFRAMES_OPS_HPP
#define GODOT_AUTOPILOT_SPRITEFRAMES_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace spriteframes_ops {

godot_autopilot::util::JsonValue handle_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_add_animation(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_add_frame(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_spriteframes(const godot_autopilot::util::JsonValue &args);

} // namespace spriteframes_ops
} // namespace godot_autopilot

#endif
