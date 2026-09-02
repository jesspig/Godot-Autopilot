#ifndef GODOT_AUTOPILOT_TOOL_DECL_HPP
#define GODOT_AUTOPILOT_TOOL_DECL_HPP

#include "tools/register_all.hpp"
#include "tools/tool_base.hpp"

#include <string>
#include <mcp/JsonValue.hpp>
#include <vector>

#define GDA_TOOL_CLASS_MEMBERS(ToolName, Desc, Category, TagsList, ExecFn, BasicSchema)                   \
  static ::godot_autopilot::ToolMeta tool_meta_static() {                                                 \
    return ::godot_autopilot::ToolMeta{std::string(ToolName), std::string(Desc),                          \
                                       std::string(Category), TagsList, BasicSchema};                     \
  }                                                                                                       \
  const ::godot_autopilot::ToolMeta& meta() const override {                                              \
    static const ::godot_autopilot::ToolMeta m = tool_meta_static();                                      \
    return m;                                                                                             \
  }                                                                                                       \
  mcp::JsonValue execute(const mcp::JsonValue& args) override {                                               \
    mcp::JsonValue denied = ::godot_autopilot::authorization::deny_if_unauthorized(                         \
        ToolName, ::godot_autopilot::side_effect_of(*this));                                                 \
    if (!denied.IsNull()) return denied;                                                                      \
    return (ExecFn)(args);                                                                                    \
  }                                                                                                           \
  mcp::JsonValue input_schema() const override {                                                          \
    return ::godot_autopilot::tool_input_schema(ToolName, BasicSchema);                                   \
  }

#define GDA_TOOL_CLASS(ClassName, ToolName, Desc, Category, TagsList, ExecFn, BasicSchema)                \
class ClassName : public ::godot_autopilot::ToolBase {                                                    \
public:                                                                                                   \
  GDA_TOOL_CLASS_MEMBERS(ToolName, Desc, Category, TagsList, ExecFn, BasicSchema)                         \
};

#define GDA_TOOL_CLASS_SIDE(ClassName, ToolName, Desc, Category, TagsList, ExecFn, BasicSchema, SideEff)  \
class ClassName : public ::godot_autopilot::ToolBase, public ::godot_autopilot::ISideEffect {             \
public:                                                                                                   \
  GDA_TOOL_CLASS_MEMBERS(ToolName, Desc, Category, TagsList, ExecFn, BasicSchema)                         \
  ::godot_autopilot::SideEffect side_effects() const override { return SideEff; }                         \
};

#endif
