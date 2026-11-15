#ifndef GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/input_map/input_map_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace input_map_tools {

namespace {

const std::vector<ParamSpec> kSaveInputMapParams = {
    {"actions", "array", "Optional whitelist of action names to persist; when omitted, actions prefixed with 'ui_' or containing '/' are skipped", false},
};

const std::vector<ParamSpec> kEraseInputMapActionEventParams = {
    {"action", "string", "Action name identifier whose event to remove, e.g. 'mario_jump'", true},
    {"event_index", "integer", "Zero-based index of the event to remove within the action's event list", true},
};

const std::vector<ParamSpec> kEraseInputMapActionParams = {
    {"action", "string", "Action name identifier to remove, e.g. 'mario_jump'; erased from the editor project InputMap and its persisted setting", true},
};

const std::vector<ParamSpec> kSetInputMapActionBundleParams = {
    {"action", "string", "Action name identifier to create or update, e.g. 'mario_jump'", true},
    {"events", "array", "Optional array of input event objects to append, each with a concrete 'class' field, e.g. {'class':'InputEventKey','keycode':'KEY_A'}; keycode and physical_keycode accept numeric codes or KEY_* names; unknown key names are rejected; when omitted no events are bound", false},
    {"deadzone", "number", "Optional deadzone 0.0 to 1.0, the analog threshold below which input is ignored; defaults to 0.5 for a newly created action, existing deadzone is kept when omitted", false},
    {"persist", "boolean", "Whether to write ProjectSettings input/<action> and save to disk (default: false); false only modifies the editor InputMap at runtime", false},
};

const std::vector<ParamSpec> kGetInputMapActionsParams = {
    {"action", "string", "Optional action name to check; when present returns [action] if it exists or [] otherwise", false},
};

inline mcp::JsonValue input_map_add_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *action = args.Find("action");
  if (action != nullptr) out["action"] = *action;
  const mcp::JsonValue *deadzone = args.Find("deadzone");
  out["deadzone"] = deadzone != nullptr ? *deadzone : mcp::JsonValue(0.5);
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_event_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *action = args.Find("action");
  if (action != nullptr) out["action"] = *action;
  mcp::JsonValue events(mcp::JsonValue::array_tag);
  const mcp::JsonValue *event = args.Find("event");
  if (event != nullptr) events.PushBack(*event);
  out["events"] = std::move(events);
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_deadzone_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *action = args.Find("action");
  const mcp::JsonValue *deadzone = args.Find("deadzone");
  if (action != nullptr) out["action"] = *action;
  if (deadzone != nullptr) out["deadzone"] = *deadzone;
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_has_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *action = args.Find("action");
  if (action != nullptr) out["action"] = *action;
  return out;
}

inline mcp::JsonValue validate_input_map_action(
    const mcp::JsonValue &args, const char *missing_message) {
  const mcp::JsonValue *action = args.Find("action");
  if (action == nullptr || !action->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue(missing_message);
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue validate_input_map_event(const mcp::JsonValue &args) {
  const mcp::JsonValue *event = args.Find("event");
  mcp::JsonValue action_error =
      validate_input_map_action(args, "missing required parameter: action");
  if (!action_error.IsNull()) return action_error;
  if (event == nullptr || !event->IsObject()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: event");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue validate_input_map_deadzone(
    const mcp::JsonValue &args) {
  mcp::JsonValue action_error =
      validate_input_map_action(args, "missing required parameter: action");
  if (!action_error.IsNull()) return action_error;
  const mcp::JsonValue *deadzone = args.Find("deadzone");
  if (deadzone == nullptr || !deadzone->IsNumber()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: deadzone");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_input_map_ok(const mcp::JsonValue &) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  return out;
}

inline mcp::JsonValue project_input_map_has(const mcp::JsonValue &result) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *inner = result.Find("result");
  bool found = false;
  if (inner != nullptr && inner->IsArray() && inner->Size() > 0) {
    found = true;
  }
  out["result"] = mcp::JsonValue(found);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(5);
  v.push_back(make_spec_tool(ToolSpec{
      "save_input_map",
      "Write the current editor InputMap into ProjectSettings and save it to disk. Optional 'actions' restricts persistence to the listed action names; when omitted, actions prefixed with 'ui_' or containing '/' are skipped. Returns 'result' set to 'persisted', plus 'actions_persisted', 'readback_verified', 'skipped' and 'save_error' fields. The running game loads persisted actions at startup; the editor does not reload them until restart.",
      "Input", {"input", "map", "persist", "save"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSaveInputMapParams, input_map_ops::handle_persist}));
  v.push_back(make_spec_tool(ToolSpec{
      "erase_input_map_action_event",
      "Remove an input event from an action in the editor project InputMap. 'event_index' is the zero-based index of the event to delete within the action's event list; an out-of-range index returns an error. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kEraseInputMapActionEventParams, input_map_ops::handle_action_erase_event}));
  v.push_back(make_spec_tool(ToolSpec{
      "erase_input_map_action",
      "Remove an input action from the editor project InputMap and clear its persisted project setting. The change modifies the editor project InputMap and persists to project settings; a running game process will not see the change until it restarts. Returns 'result' set to 'ok'.",
      "Input", {"input", "map"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kEraseInputMapActionParams, input_map_ops::handle_erase_action}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_input_map_actions",
      "Get input action names from the editor project InputMap, including built-in 'ui_*' actions. Optional 'action' checks one name and returns [action] when it exists or [] otherwise. A running game process has its own InputMap loaded at startup, so this reflects only the editor project state. Returns an array of action name strings in 'result'.",
      "Input", {"input", "map"}, SideEffect::None, tool_flags::kNone,
      kGetInputMapActionsParams, input_map_ops::handle_get_actions},
      {
          ToolAlias{
              "has_input_map_action", "get_input_map_actions",
              input_map_has_args, project_input_map_has, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_input_map_action(
                    args, "missing required parameter: action");
              }},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_input_map_action_bundle",
      "Ensure an input action exists and apply deadzone plus events in one call in the editor project InputMap. 'action' is required; 'events' optionally appends each event object with a concrete 'class' plus keycode/physical_keycode KEY_* handling; 'deadzone' optionally sets the analog threshold (0.5 for a new action, existing value kept when omitted). Dual persistence: persist=false (default) only modifies the editor InputMap at runtime; persist=true additionally writes ProjectSettings input/<action> and saves to disk. A running game process will not see the change until it restarts. Returns 'result' set to 'ok' plus 'persisted'.",
      "Input", {"input", "map"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetInputMapActionBundleParams, input_map_ops::handle_set_action_bundle},
      {
          ToolAlias{
              "add_input_map_action", "set_input_map_action_bundle",
              input_map_add_args, project_input_map_ok, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                return validate_input_map_action(
                    args, "missing required parameter: action");
              }},
          ToolAlias{
              "add_input_map_action_event", "set_input_map_action_bundle",
              input_map_event_args, project_input_map_ok, "0.4.0", "0.6.0",
              validate_input_map_event},
          ToolAlias{
              "set_input_map_action_deadzone", "set_input_map_action_bundle",
              input_map_deadzone_args, project_input_map_ok, "0.4.0", "0.6.0",
              validate_input_map_deadzone},
      })));
  return v;
}

} // namespace input_map_tools
} // namespace godot_autopilot

#endif