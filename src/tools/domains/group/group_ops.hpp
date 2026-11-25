#ifndef GODOT_AUTOPILOT_GROUP_OPS_HPP
#define GODOT_AUTOPILOT_GROUP_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace group_ops {

godot_autopilot::util::JsonValue handle_node_group(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_add_node_to_group(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_remove_node_from_group(const godot_autopilot::util::JsonValue &args);

} // namespace group_ops
} // namespace godot_autopilot

#endif
