#ifndef GODOT_AUTOPILOT_EDITOR_UI_ACTIONS_HPP
#define GODOT_AUTOPILOT_EDITOR_UI_ACTIONS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace editor_ui_actions {

mcp::JsonValue handle_click_editor_element(const mcp::JsonValue &args);
mcp::JsonValue handle_type_editor_element_text(const mcp::JsonValue &args);
mcp::JsonValue handle_run_editor_shortcut(const mcp::JsonValue &args);

} // namespace editor_ui_actions
} // namespace godot_autopilot

#endif
