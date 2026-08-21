#ifndef GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/input_map_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace input_map_tools {

GDA_TOOL_CLASS_SIDE(SaveInputMapTool, "save_input_map",
               "Write the current editor InputMap into ProjectSettings and save it to disk. Optional 'actions' restricts persistence to the listed action names; when omitted, actions prefixed with 'ui_' or containing '/' are skipped. Returns 'result' set to 'persisted', plus 'actions_persisted', 'readback_verified', 'skipped' and 'save_error' fields. The running game loads persisted actions at startup; the editor does not reload them until restart.",
               "Input", std::vector<std::string>({"input", "map", "persist", "save"}), input_map_ops::handle_persist, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(AddInputMapActionTool, "add_input_map_action",
               "Add a new empty input action to the editor project InputMap. 'deadzone' defaults to 0.5. Only the action is created; no events are bound, so use add_input_map_action_event to attach keys or buttons afterwards. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_add_action, true)

GDA_TOOL_CLASS_SIDE(AddInputMapActionEventTool, "add_input_map_action_event",
               "Bind an input event to an action in the editor project InputMap. 'event' must be an object with a concrete 'class' field, for example InputEventKey with 'keycode' 65, or a KEY_* name string. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_action_add_event, true, ::godot_autopilot::SideEffect::WritesConfig)

GDA_TOOL_CLASS(EraseInputMapActionEventTool, "erase_input_map_action_event",
               "Remove an input event from an action in the editor project InputMap. 'event_index' is the zero-based index of the event to delete within the action's event list; an out-of-range index returns an error. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_action_erase_event, true)

GDA_TOOL_CLASS(SetInputMapActionDeadzoneTool, "set_input_map_action_deadzone",
               "Set the deadzone of an input action, the analog threshold below which input is ignored, in the range 0.0 to 1.0. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_action_set_deadzone, true)

GDA_TOOL_CLASS(EraseInputMapActionTool, "erase_input_map_action",
               "Remove an input action from the editor project InputMap and clear its persisted project setting. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_erase_action, true)

GDA_TOOL_CLASS(GetInputMapActionsTool, "get_input_map_actions",
               "Get all input action names from the editor project InputMap, including built-in 'ui_*' actions. A running game process has its own InputMap loaded at startup, so this reflects only the editor project state. Returns an array of action name strings in 'result'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_get_actions, true)

GDA_TOOL_CLASS(HasInputMapActionTool, "has_input_map_action",
               "Check whether an input action exists in the editor project InputMap. Use it to decide whether add_input_map_action is needed before binding events. A running game process has its own InputMap loaded at startup, so this reflects only the editor project state. Returns a boolean in 'result'.",
               "Input", std::vector<std::string>({"input", "map"}), input_map_ops::handle_has_action, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(std::make_unique<SaveInputMapTool>());
  v.push_back(std::make_unique<AddInputMapActionTool>());
  v.push_back(std::make_unique<AddInputMapActionEventTool>());
  v.push_back(std::make_unique<EraseInputMapActionEventTool>());
  v.push_back(std::make_unique<SetInputMapActionDeadzoneTool>());
  v.push_back(std::make_unique<EraseInputMapActionTool>());
  v.push_back(std::make_unique<GetInputMapActionsTool>());
  v.push_back(std::make_unique<HasInputMapActionTool>());
  return v;
}

} // namespace input_map_tools
} // namespace godot_autopilot

#endif