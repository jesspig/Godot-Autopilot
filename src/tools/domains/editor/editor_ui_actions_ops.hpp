#ifndef GODOT_AUTOPILOT_EDITOR_UI_ACTIONS_HPP
#define GODOT_AUTOPILOT_EDITOR_UI_ACTIONS_HPP

#include <godot_cpp/classes/global_constants.hpp>
#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace editor_ui_actions {

godot::Key parse_function_key(const std::string &key_lower);
godot_autopilot::util::JsonValue handle_click_editor_element(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_type_editor_element_text(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_run_editor_shortcut(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_ui_action(const godot_autopilot::util::JsonValue &args);

} // namespace editor_ui_actions
} // namespace godot_autopilot

#endif
