#ifndef GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_OPS_HPP

#include <godot_cpp/classes/global_constants.hpp>
#include "util/json_value.hpp"
#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace input_map_ops {

int64_t resolve_key_name_code(const std::string &name);
godot::Key resolve_input_key(const std::string &name);
godot::Key resolve_function_key(const std::string &name);
godot_autopilot::util::JsonValue handle_get_input_map(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_input_map_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_erase_input_map(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_action_add_event(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_action_erase_event(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_action_set_deadzone(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_add_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_action_bundle(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_erase_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_actions(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_has_action(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_persist(const godot_autopilot::util::JsonValue &args);

} // namespace input_map_ops
} // namespace godot_autopilot

#endif
