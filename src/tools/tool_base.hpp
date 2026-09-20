#ifndef GODOT_AUTOPILOT_TOOL_BASE_HPP
#define GODOT_AUTOPILOT_TOOL_BASE_HPP

#include <cstdint>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <tools/schema_builder.hpp>
#include <util/error_util.hpp>
#include "tools/authorization.hpp"

namespace godot_autopilot {

enum class SideEffect { None, WritesFile, WritesConfig, ShowsAlert, ModifiesWindow, Process,
                        CodeExecute, GameRuntime };

inline const char* side_effect_name(SideEffect e) {
  switch (e) {
    case SideEffect::None: return "";
    case SideEffect::WritesFile: return "writes_file";
    case SideEffect::WritesConfig: return "writes_config";
    case SideEffect::ShowsAlert: return "shows_alert";
    case SideEffect::ModifiesWindow: return "modifies_window";
    case SideEffect::Process: return "process";
    case SideEffect::CodeExecute: return "code_execute";
    case SideEffect::GameRuntime: return "game_runtime";
  }
  return "";
}

struct ToolMeta {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
};

class ISideEffect {
public:
  virtual ~ISideEffect() = default;
  virtual SideEffect side_effects() const = 0;
};

class ToolBase {
public:
  virtual ~ToolBase() = default;
  virtual const ToolMeta &meta() const = 0;
  virtual mcp::JsonValue execute(const mcp::JsonValue &args) = 0;
  virtual mcp::JsonValue input_schema() const = 0;
  virtual uint32_t tool_flags() const { return 0; }
};

inline SideEffect side_effect_of(const ToolBase &t) {
  const auto *side = dynamic_cast<const ISideEffect *>(&t);
  return side ? side->side_effects() : SideEffect::None;
}

namespace authorization {
inline const char *capability_for_tool(std::string_view name,
                                       SideEffect effect) {
  if (effect == SideEffect::Process)
    return "process";
  if (effect == SideEffect::CodeExecute || name == "code_execute" ||
      name == "execute_script")
    return "code_execute";
  if (effect == SideEffect::GameRuntime || name == "execute_game_script")
    return "game_runtime";
  return nullptr;
}
} // namespace authorization

} // namespace godot_autopilot

#endif
