#ifndef GODOT_AUTOPILOT_CSHARP_BRIDGE_OPS_HPP
#define GODOT_AUTOPILOT_CSHARP_BRIDGE_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace csharp_bridge_ops {

godot_autopilot::util::JsonValue handle_csharp_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_call(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_subscribe(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_unsubscribe(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_member(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_csharp_event(const godot_autopilot::util::JsonValue &args);

} // namespace csharp_bridge_ops
} // namespace godot_autopilot

#endif
