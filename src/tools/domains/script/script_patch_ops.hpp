#ifndef GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP
#define GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace script_patch_ops {

godot_autopilot::util::JsonValue handle_patch(const godot_autopilot::util::JsonValue &args);

} // namespace script_patch_ops
} // namespace godot_autopilot

#endif
