#ifndef GODOT_AUTOPILOT_TOOL_CONFIRMATION_HPP
#define GODOT_AUTOPILOT_TOOL_CONFIRMATION_HPP

#include <optional>
#include <string>

#include "util/json_value.hpp"

#include "tools/base/tool_base.hpp"

namespace godot_autopilot {
namespace confirmation {

inline bool required_for(SideEffect effect) {
  switch (effect) {
    case SideEffect::Destructive:
    case SideEffect::Process:
    case SideEffect::CodeExecute:
    case SideEffect::GameRuntime:
      return true;
    case SideEffect::None:
    case SideEffect::WritesFile:
    case SideEffect::WritesConfig:
    case SideEffect::ShowsAlert:
    case SideEffect::ModifiesWindow:
    case SideEffect::WritesScene:
      return false;
  }
  return false;
}

inline bool provided(const godot_autopilot::util::JsonValue &args) {
  if (!args.IsObject()) return false;
  const godot_autopilot::util::JsonValue *value = args.Find("confirm");
  return value != nullptr && value->IsBool() && value->GetBool();
}

enum class ConfirmationDecision {
  kMissing,
  kAccepted,
  kDeclined,
  kMalformed,
};

struct ConfirmationResolution {
  ConfirmationDecision decision = ConfirmationDecision::kMissing;
  bool from_arguments = false;
  bool from_input_responses = false;
};

inline ConfirmationResolution resolve_confirmation(
    const godot_autopilot::util::JsonValue &args, bool is_call_tool,
    const godot_autopilot::util::JsonValue *input_responses, std::string *error_out) {
  ConfirmationResolution out;
  if (error_out != nullptr)
    error_out->clear();
  if (!args.IsObject()) {
    if (error_out != nullptr)
      *error_out = "tool arguments must be an object";
    out.decision = ConfirmationDecision::kMalformed;
    return out;
  }
  const godot_autopilot::util::JsonValue *arg_confirm = args.Find("confirm");
  const godot_autopilot::util::JsonValue *nested_confirm = nullptr;
  if (is_call_tool) {
    const godot_autopilot::util::JsonValue *nested = args.Find("arguments");
    if (nested != nullptr && nested->IsObject())
      nested_confirm = nested->Find("confirm");
  }
  const bool wrapper_true = arg_confirm != nullptr && arg_confirm->IsBool() &&
                            arg_confirm->GetBool();
  const bool nested_true = nested_confirm != nullptr &&
                           nested_confirm->IsBool() &&
                           nested_confirm->GetBool();
  if (wrapper_true || nested_true) {
    out.decision = ConfirmationDecision::kAccepted;
    out.from_arguments = true;
    return out;
  }
  const bool wrapper_false = arg_confirm != nullptr && arg_confirm->IsBool() &&
                             !arg_confirm->GetBool();
  const bool nested_false = nested_confirm != nullptr &&
                            nested_confirm->IsBool() &&
                            !nested_confirm->GetBool();
  if (wrapper_false || nested_false) {
    out.decision = ConfirmationDecision::kDeclined;
    return out;
  }
  if (input_responses == nullptr)
    return out;
  if (!input_responses->IsObject()) {
    if (error_out != nullptr)
      *error_out = "inputResponses must be an object";
    out.decision = ConfirmationDecision::kMalformed;
    return out;
  }
  const godot_autopilot::util::JsonValue *confirm = input_responses->Find("confirm");
  if (confirm == nullptr)
    return out;
  if (confirm->IsBool()) {
    out.decision = confirm->GetBool() ? ConfirmationDecision::kAccepted
                                      : ConfirmationDecision::kDeclined;
    out.from_input_responses = true;
    return out;
  }
  if (confirm->IsObject()) {
    const godot_autopilot::util::JsonValue *action = confirm->Find("action");
    if (action != nullptr && action->IsString()) {
      const std::string value = action->GetString();
      if (value == "accept") {
        out.decision = ConfirmationDecision::kAccepted;
        out.from_input_responses = true;
        return out;
      }
      if (value == "decline") {
        out.decision = ConfirmationDecision::kDeclined;
        out.from_input_responses = true;
        return out;
      }
    }
  }
  if (error_out != nullptr)
    *error_out = "inputResponses.confirm must be a boolean or an object with action accept or decline";
  out.decision = ConfirmationDecision::kMalformed;
  return out;
}

inline godot_autopilot::util::JsonValue error(const std::string &name, SideEffect effect) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] =
      godot_autopilot::util::JsonValue("confirmation required before calling '" + name + "'");
  result["code"] = godot_autopilot::util::JsonValue("confirmation_required");
  result["confirm_required"] = godot_autopilot::util::JsonValue(true);
  result["side_effect"] = godot_autopilot::util::JsonValue(side_effect_name(effect));
  result["hint"] =
      godot_autopilot::util::JsonValue("Retry with arguments.confirm=true after reviewing the "
                     "tool description and target.");
  return result;
}

} // namespace confirmation
} // namespace godot_autopilot

#endif
