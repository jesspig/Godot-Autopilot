#ifndef GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace input_map_ops {

mcp::JsonValue handle_action_add_event(const mcp::JsonValue &args);
mcp::JsonValue handle_action_erase_event(const mcp::JsonValue &args);
mcp::JsonValue handle_action_set_deadzone(const mcp::JsonValue &args);
mcp::JsonValue handle_add_action(const mcp::JsonValue &args);
mcp::JsonValue handle_erase_action(const mcp::JsonValue &args);
mcp::JsonValue handle_get_actions(const mcp::JsonValue &args);
mcp::JsonValue handle_has_action(const mcp::JsonValue &args);
mcp::JsonValue handle_persist(const mcp::JsonValue &args);

} // namespace input_map_ops
} // namespace godot_autopilot

#endif
