#ifndef GODOT_AUTOPILOT_TOOL_BASE_HPP
#define GODOT_AUTOPILOT_TOOL_BASE_HPP

#include <cstdint>
#include <initializer_list>
#include "util/json_value.hpp"
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <tools/base/schema_builder.hpp>
#include <util/error_util.hpp>

namespace godot_autopilot {

enum class SideEffect { None, WritesFile, WritesConfig, ShowsAlert, ModifiesWindow, Process,
                        CodeExecute, GameRuntime, Destructive, WritesScene };

inline const char* side_effect_name(SideEffect e) {
  switch (e) {
    case SideEffect::None: return "none";
    case SideEffect::WritesFile: return "writes_file";
    case SideEffect::WritesConfig: return "writes_config";
    case SideEffect::ShowsAlert: return "shows_alert";
    case SideEffect::ModifiesWindow: return "modifies_window";
    case SideEffect::Process: return "process";
    case SideEffect::CodeExecute: return "code_execute";
    case SideEffect::GameRuntime: return "game_runtime";
    case SideEffect::Destructive: return "destructive";
    case SideEffect::WritesScene: return "writes_scene";
  }
  return "none";
}

inline bool parse_side_effect(const std::string& text, SideEffect& out) {
  if (text.empty()) {
    out = SideEffect::None;
    return true;
  }
  if (text == "writes_file") out = SideEffect::WritesFile;
  else if (text == "writes_config") out = SideEffect::WritesConfig;
  else if (text == "shows_alert") out = SideEffect::ShowsAlert;
  else if (text == "modifies_window") out = SideEffect::ModifiesWindow;
  else if (text == "process") out = SideEffect::Process;
  else if (text == "code_execute") out = SideEffect::CodeExecute;
  else if (text == "game_runtime") out = SideEffect::GameRuntime;
  else if (text == "writes_scene") out = SideEffect::WritesScene;
  else if (text == "destructive") out = SideEffect::Destructive;
  else return false;
  return true;
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
  virtual godot_autopilot::util::JsonValue execute(const godot_autopilot::util::JsonValue &args) = 0;
  virtual godot_autopilot::util::JsonValue input_schema() const = 0;
  virtual uint32_t tool_flags() const { return 0; }
};

inline SideEffect side_effect_of(const ToolBase &t) {
  const auto *side = dynamic_cast<const ISideEffect *>(&t);
  return side ? side->side_effects() : SideEffect::None;
}

} // namespace godot_autopilot

#endif
