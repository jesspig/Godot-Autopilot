#ifndef GODOT_AUTOPILOT_TOOL_CONFIRMATION_HPP
#define GODOT_AUTOPILOT_TOOL_CONFIRMATION_HPP

#include <string>

#include <mcp/JsonValue.hpp>

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

inline bool provided(const mcp::JsonValue &args) {
  if (!args.IsObject()) return false;
  const mcp::JsonValue *value = args.Find("confirm");
  return value != nullptr && value->IsBool() && value->GetBool();
}

inline mcp::JsonValue error(const std::string &name, SideEffect effect) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] =
      mcp::JsonValue("confirmation required before calling '" + name + "'");
  result["code"] = mcp::JsonValue("confirmation_required");
  result["confirm_required"] = mcp::JsonValue(true);
  result["side_effect"] = mcp::JsonValue(side_effect_name(effect));
  result["hint"] =
      mcp::JsonValue("Retry with arguments.confirm=true after reviewing the "
                     "tool description and target.");
  return result;
}

} // namespace confirmation
} // namespace godot_autopilot

#endif
