#ifndef GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/input_map_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace input_map_tools {

namespace {

const std::vector<ParamSpec> kSaveInputMapParams = {
    {"actions", "array", "Optional whitelist of action names to persist; when omitted, actions prefixed with 'ui_' or containing '/' are skipped", false},
};

const std::vector<ParamSpec> kAddInputMapActionParams = {
    {"action", "string", "Action name identifier, e.g. 'mario_jump' or 'move_left'; a programmatic identifier, not a display name", true},
    {"deadzone", "number", "Deadzone 0.0 to 1.0, the analog threshold below which input is ignored (default: 0.5)", false},
};

const std::vector<ParamSpec> kAddInputMapActionEventParams = {
    {"action", "string", "Action name identifier to bind the event to, e.g. 'mario_jump'", true},
    {"event", "object", "Input event object with a concrete 'class' field, e.g. 'InputEventKey' with 'keycode' 65; keycode and physical_keycode accept numeric key codes or KEY_* names like 'KEY_A'", true},
};

const std::vector<ParamSpec> kEraseInputMapActionEventParams = {
    {"action", "string", "Action name identifier whose event to remove, e.g. 'mario_jump'", true},
    {"event_index", "integer", "Zero-based index of the event to remove within the action's event list", true},
};

const std::vector<ParamSpec> kSetInputMapActionDeadzoneParams = {
    {"action", "string", "Action name identifier to set the deadzone for, e.g. 'mario_jump'", true},
    {"deadzone", "number", "Deadzone 0.0 to 1.0, the analog threshold below which input is ignored", true},
};

const std::vector<ParamSpec> kEraseInputMapActionParams = {
    {"action", "string", "Action name identifier to remove, e.g. 'mario_jump'; erased from the editor project InputMap and its persisted setting", true},
};

const std::vector<ParamSpec> kGetInputMapActionsParams = {};

const std::vector<ParamSpec> kHasInputMapActionParams = {
    {"action", "string", "Action name identifier to check for existence, e.g. 'ui_accept'", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(ToolSpec{
      "save_input_map",
      "Write the current editor InputMap into ProjectSettings and save it to disk. Optional 'actions' restricts persistence to the listed action names; when omitted, actions prefixed with 'ui_' or containing '/' are skipped. Returns 'result' set to 'persisted', plus 'actions_persisted', 'readback_verified', 'skipped' and 'save_error' fields. The running game loads persisted actions at startup; the editor does not reload them until restart.",
      "Input", {"input", "map", "persist", "save"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSaveInputMapParams, input_map_ops::handle_persist}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_input_map_action",
      "Add a new empty input action to the editor project InputMap. 'deadzone' defaults to 0.5. Only the action is created; no events are bound, so use add_input_map_action_event to attach keys or buttons afterwards. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kAddInputMapActionParams, input_map_ops::handle_add_action}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_input_map_action_event",
      "Bind an input event to an action in the editor project InputMap. 'event' must be an object with a concrete 'class' field, for example InputEventKey with 'keycode' 65, or a KEY_* name string. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kAddInputMapActionEventParams, input_map_ops::handle_action_add_event}));
  v.push_back(make_spec_tool(ToolSpec{
      "erase_input_map_action_event",
      "Remove an input event from an action in the editor project InputMap. 'event_index' is the zero-based index of the event to delete within the action's event list; an out-of-range index returns an error. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kEraseInputMapActionEventParams, input_map_ops::handle_action_erase_event}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_input_map_action_deadzone",
      "Set the deadzone of an input action, the analog threshold below which input is ignored, in the range 0.0 to 1.0. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kSetInputMapActionDeadzoneParams, input_map_ops::handle_action_set_deadzone}));
  v.push_back(make_spec_tool(ToolSpec{
      "erase_input_map_action",
      "Remove an input action from the editor project InputMap and clear its persisted project setting. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kEraseInputMapActionParams, input_map_ops::handle_erase_action}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_input_map_actions",
      "Get all input action names from the editor project InputMap, including built-in 'ui_*' actions. A running game process has its own InputMap loaded at startup, so this reflects only the editor project state. Returns an array of action name strings in 'result'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kGetInputMapActionsParams, input_map_ops::handle_get_actions}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_input_map_action",
      "Check whether an input action exists in the editor project InputMap. Use it to decide whether add_input_map_action is needed before binding events. A running game process has its own InputMap loaded at startup, so this reflects only the editor project state. Returns a boolean in 'result'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kHasInputMapActionParams, input_map_ops::handle_has_action}));
  return v;
}

} // namespace input_map_tools
} // namespace godot_autopilot

#endif