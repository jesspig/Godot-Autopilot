#ifndef GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_MAP_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/input_map/input_map_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace input_map_tools {

namespace {

const std::vector<ParamSpec> kSaveInputMapParams = {
    {"actions", "array", "Optional whitelist of action names to persist; when omitted, actions prefixed with 'ui_' or containing '/' are skipped", false},
};

const std::vector<ParamSpec> kGetInputMapParams = {
    {"name", "string", "Action name: for action='list' it filters to [name] when the action exists and [] otherwise; for action='has' it is the action to test; for action='query' it is the required name of the persisted action whose configuration is read from ProjectSettings input/<name>", false},
};

const std::vector<ParamSpec> kSetInputMapActionParams = {
    {"name", "string", "Action name identifier to create or update, e.g. 'mario_jump'", true},
    {"events", "array", "Optional array of input event objects to append, each with a concrete 'class' field, e.g. {'class':'InputEventKey','keycode':'KEY_A'}; keycode and physical_keycode accept numeric codes or KEY_* names; unknown key names are rejected; when omitted no events are bound", false},
    {"deadzone", "number", "Optional deadzone 0.0 to 1.0, the analog threshold below which input is ignored; defaults to 0.5 for a newly created action, existing deadzone is kept when omitted", false},
    {"persist", "boolean", "Whether to write ProjectSettings input/<action> and save to disk (default: false); false only modifies the editor InputMap at runtime", false},
};

const std::vector<ParamSpec> kEraseInputMapParams = {
    {"name", "string", "Action name identifier to erase, e.g. 'mario_jump'", true},
    {"event_index", "integer", "Zero-based index of the event to remove for action='event' within the action's event list", false},
};

inline std::vector<std::string> input_map_actions(
    std::initializer_list<const char *> actions) {
  std::vector<std::string> out;
  out.reserve(actions.size());
  for (const char *action : actions) {
    out.emplace_back(action);
  }
  return out;
}

inline std::vector<std::string> input_map_examples(
    std::initializer_list<const char *> examples) {
  std::vector<std::string> out;
  out.reserve(examples.size());
  for (const char *example : examples) {
    out.emplace_back(example);
  }
  return out;
}

inline ToolSpec input_map_action_spec(
    ToolSpec spec, std::initializer_list<const char *> actions,
    std::initializer_list<const char *> examples) {
  spec.raw_schema = schema::build_action_schema(
      input_map_actions(actions), spec.params,
      "Action to perform; per-action parameters are documented in the tool "
      "description");
  spec.examples = input_map_examples(examples);
  return spec;
}

inline mcp::JsonValue input_map_args_with_action(const mcp::JsonValue &args,
                                                 const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue input_map_target_args(const mcp::JsonValue &args,
                                            const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out.GetObject().erase("action");
  const mcp::JsonValue *target = args.Find("action");
  if (target != nullptr) {
    out["name"] = *target;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue input_map_add_args(const mcp::JsonValue &args) {
  mcp::JsonValue out = input_map_target_args(args, "set");
  if (out.Find("deadzone") == nullptr) {
    out["deadzone"] = mcp::JsonValue(0.5);
  }
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_event_args(const mcp::JsonValue &args) {
  mcp::JsonValue out = input_map_target_args(args, "set");
  mcp::JsonValue events(mcp::JsonValue::array_tag);
  const mcp::JsonValue *event = args.Find("event");
  if (event != nullptr) {
    events.PushBack(*event);
  }
  out["events"] = std::move(events);
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_deadzone_args(const mcp::JsonValue &args) {
  mcp::JsonValue out = input_map_target_args(args, "set");
  out["persist"] = mcp::JsonValue(true);
  return out;
}

inline mcp::JsonValue input_map_has_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *target = args.Find("action");
  if (target != nullptr) {
    out["name"] = *target;
  }
  out["action"] = mcp::JsonValue("has");
  return out;
}

inline mcp::JsonValue input_map_erase_action_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *target = args.Find("action");
  if (target != nullptr) {
    out["name"] = *target;
  }
  out["action"] = mcp::JsonValue("action");
  return out;
}

inline mcp::JsonValue input_map_erase_event_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *target = args.Find("action");
  if (target != nullptr) {
    out["name"] = *target;
  }
  const mcp::JsonValue *index = args.Find("event_index");
  if (index != nullptr) {
    out["event_index"] = *index;
  }
  out["action"] = mcp::JsonValue("event");
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

inline mcp::JsonValue validate_input_map_erase_event(
    const mcp::JsonValue &args) {
  mcp::JsonValue action_error =
      validate_input_map_action(args, "missing required parameter: action");
  if (!action_error.IsNull()) return action_error;
  const mcp::JsonValue *index = args.Find("event_index");
  if (index == nullptr || !index->IsInt()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: event_index");
    return error;
  }
  return mcp::JsonValue();
}

inline bool input_map_result_has_error(const mcp::JsonValue &result) {
  return result.IsObject() &&
         (result.Find("error") != nullptr ||
          result.Find("structured_error") != nullptr ||
          result.Find("runtime_error") != nullptr);
}

inline mcp::JsonValue project_input_map_ok(const mcp::JsonValue &result) {
  if (input_map_result_has_error(result)) {
    return result;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue("ok");
  return out;
}

inline mcp::JsonValue project_input_map_has(const mcp::JsonValue &result) {
  if (input_map_result_has_error(result)) {
    return result;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *inner = result.Find("result");
  bool found = false;
  if (inner != nullptr && inner->IsBool()) {
    found = inner->GetBool();
  } else if (inner != nullptr && inner->IsArray() && inner->Size() > 0) {
    found = true;
  }
  out["result"] = mcp::JsonValue(found);
  return out;
}

inline ToolAlias input_map_alias(const char *name, const char *canonical,
                                 const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return input_map_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(with_aliases(
      input_map_action_spec(
          ToolSpec{
              "get_input_map",
              "Read input action names from the editor project InputMap, actions: list (all action names including built-in 'ui_*'; with name it returns [name] when the action exists and [] otherwise), has (boolean existence check for name, reported as result and found) and query (requires name; reads the persisted configuration of input/<name> from ProjectSettings and returns deadzone, events and the full config, so custom actions not loaded into the editor InputMap can be verified). A running game process has its own InputMap loaded at startup, so list and has reflect only the editor project state. Deprecated aliases: get_input_map_actions, has_input_map_action.",
              "Input",
              {"input", "map", "actions", "get_input_map_actions",
               "has_input_map_action"},
              SideEffect::None,
              tool_flags::kNone,
              kGetInputMapParams,
              input_map_ops::handle_get_input_map},
          {"list", "has", "query"},
          {"{\"action\":\"list\"}",
           "{\"action\":\"has\",\"name\":\"mario_jump\"}",
           "{\"action\":\"query\",\"name\":\"mario_jump\"}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              mcp::JsonValue out(mcp::JsonValue::object_tag);
              const mcp::JsonValue *name = args.Find("action");
              if (name != nullptr) {
                out["name"] = *name;
              }
              out["action"] = mcp::JsonValue("list");
              return out;
            };
            alias.validate_args = [](const mcp::JsonValue &args) {
              const mcp::JsonValue *action = args.Find("action");
              if (action != nullptr && !action->IsString()) {
                mcp::JsonValue error(mcp::JsonValue::object_tag);
                error["error"] = mcp::JsonValue(
                    "invalid parameter: action must be a string");
                return error;
              }
              return mcp::JsonValue();
            };
            return alias;
          }(input_map_alias("get_input_map_actions", "get_input_map", "list")),
          [](ToolAlias alias) {
            alias.adapt_args = input_map_has_args;
            alias.project_result = project_input_map_has;
            alias.validate_args = [](const mcp::JsonValue &args) {
              return validate_input_map_action(
                  args, "missing required parameter: action");
            };
            return alias;
          }(input_map_alias("has_input_map_action", "get_input_map", "has")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_map_action_spec(
          ToolSpec{
              "set_input_map_action",
              "Ensure an input action exists and apply deadzone plus events in one call in the editor project InputMap, actions: set (name is required; events optionally appends each event object with a concrete 'class' plus keycode/physical_keycode KEY_* handling; deadzone optionally sets the analog threshold, 0.5 for a new action and the existing value kept when omitted; persist=false only modifies the editor InputMap at runtime while persist=true also writes ProjectSettings input/<action> and saves). A running game process will not see the change until it restarts. Returns result ok plus action, events_added, deadzone and persisted. Deprecated aliases: set_input_map_action_bundle, add_input_map_action, add_input_map_action_event, set_input_map_action_deadzone.",
              "Input",
              {"input", "map", "action", "set", "bundle",
               "set_input_map_action_bundle", "add_input_map_action",
               "add_input_map_action_event", "set_input_map_action_deadzone"},
              SideEffect::WritesConfig,
              tool_flags::kMutating,
              kSetInputMapActionParams,
              input_map_ops::handle_set_input_map_action},
          {"set"},
          {"{\"action\":\"set\",\"name\":\"mario_jump\",\"events\":[{\"class\":\"InputEventKey\",\"keycode\":\"KEY_SPACE\"}],\"persist\":true}",
           "{\"action\":\"set\",\"name\":\"mario_jump\",\"deadzone\":0.3}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              return input_map_target_args(args, "set");
            };
            alias.validate_args = [](const mcp::JsonValue &args) {
              return validate_input_map_action(
                  args, "missing required parameter: action");
            };
            return alias;
          }(input_map_alias("set_input_map_action_bundle",
                            "set_input_map_action", "set")),
          [](ToolAlias alias) {
            alias.adapt_args = input_map_add_args;
            alias.project_result = project_input_map_ok;
            alias.validate_args = [](const mcp::JsonValue &args) {
              return validate_input_map_action(
                  args, "missing required parameter: action");
            };
            return alias;
          }(input_map_alias("add_input_map_action", "set_input_map_action",
                            "set")),
          [](ToolAlias alias) {
            alias.adapt_args = input_map_event_args;
            alias.project_result = project_input_map_ok;
            alias.validate_args = validate_input_map_event;
            return alias;
          }(input_map_alias("add_input_map_action_event",
                            "set_input_map_action", "set")),
          [](ToolAlias alias) {
            alias.adapt_args = input_map_deadzone_args;
            alias.project_result = project_input_map_ok;
            alias.validate_args = validate_input_map_deadzone;
            return alias;
          }(input_map_alias("set_input_map_action_deadzone",
                            "set_input_map_action", "set")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_map_action_spec(
          ToolSpec{
              "erase_input_map",
              "Remove input map content from the editor project InputMap and persist the change to project settings, actions: action (erase the whole action given by name and clear its input/<action> setting) and event (remove the event at the zero-based event_index from the action given by name; an out-of-range index returns an error). A running game process will not see the change until it restarts. Returns result ok. Deprecated aliases: erase_input_map_action, erase_input_map_action_event.",
              "Input",
              {"input", "map", "erase", "action", "event",
               "erase_input_map_action", "erase_input_map_action_event"},
              SideEffect::WritesConfig,
              tool_flags::kMutating,
              kEraseInputMapParams,
              input_map_ops::handle_erase_input_map},
          {"action", "event"},
          {"{\"action\":\"action\",\"name\":\"mario_jump\"}",
           "{\"action\":\"event\",\"name\":\"mario_jump\",\"event_index\":0}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = input_map_erase_action_args;
            alias.validate_args = [](const mcp::JsonValue &args) {
              return validate_input_map_action(
                  args, "missing required parameter: action");
            };
            return alias;
          }(input_map_alias("erase_input_map_action", "erase_input_map",
                            "action")),
          [](ToolAlias alias) {
            alias.adapt_args = input_map_erase_event_args;
            alias.validate_args = validate_input_map_erase_event;
            return alias;
          }(input_map_alias("erase_input_map_action_event", "erase_input_map",
                            "event")),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "save_input_map",
      "Write the current editor InputMap into ProjectSettings and save it to disk. Optional 'actions' restricts persistence to the listed action names; when omitted, actions prefixed with 'ui_' or containing '/' are skipped. Returns 'result' set to 'persisted', plus 'actions_persisted', 'readback_verified', 'skipped' and 'save_error' fields. The running game loads persisted actions at startup; the editor does not reload them until restart.",
      "Input", {"input", "map", "persist", "save"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSaveInputMapParams, input_map_ops::handle_persist}));
  return v;
}

} // namespace input_map_tools
} // namespace godot_autopilot

#endif
