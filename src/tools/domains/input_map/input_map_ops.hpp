#ifndef GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP

#include <godot_cpp/classes/global_constants.hpp>
#include <mcp/JsonValue.hpp>
#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace input_map_ops {

int64_t resolve_key_name_code(const std::string &name);
godot::Key resolve_input_key(const std::string &name);
godot::Key resolve_function_key(const std::string &name);
mcp::JsonValue handle_get_input_map(const mcp::JsonValue &args);
mcp::JsonValue handle_set_input_map_action(const mcp::JsonValue &args);
mcp::JsonValue handle_erase_input_map(const mcp::JsonValue &args);
mcp::JsonValue handle_action_add_event(const mcp::JsonValue &args);
mcp::JsonValue handle_action_erase_event(const mcp::JsonValue &args);
mcp::JsonValue handle_action_set_deadzone(const mcp::JsonValue &args);
mcp::JsonValue handle_add_action(const mcp::JsonValue &args);
mcp::JsonValue handle_set_action_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_erase_action(const mcp::JsonValue &args);
mcp::JsonValue handle_get_actions(const mcp::JsonValue &args);
mcp::JsonValue handle_has_action(const mcp::JsonValue &args);
mcp::JsonValue handle_persist(const mcp::JsonValue &args);

} // namespace input_map_ops
} // namespace godot_autopilot

#endif
