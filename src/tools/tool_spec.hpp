#ifndef GODOT_AUTOPILOT_TOOL_SPEC_HPP
#define GODOT_AUTOPILOT_TOOL_SPEC_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mcp/JsonValue.hpp>
#include <tools/schema_builder.hpp>
#include <tools/tool_base.hpp>

namespace godot_autopilot {

using ParamSpec = schema::ParamDef;

namespace tool_flags {
constexpr uint32_t kNone = 0;
constexpr uint32_t kMeta = 1u << 0;
constexpr uint32_t kDynamic = 1u << 1;
constexpr uint32_t kMutating = 1u << 2;
constexpr uint32_t kObserve = 1u << 3;
constexpr uint32_t kCaptureImage = 1u << 4;
constexpr uint32_t kSceneTarget = 1u << 5;
constexpr uint32_t kUndoable = 1u << 6;
} // namespace tool_flags

struct ToolSpec {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  SideEffect side_effect = SideEffect::None;
  uint32_t flags = tool_flags::kNone;
  std::vector<ParamSpec> params;
  std::function<mcp::JsonValue(const mcp::JsonValue &)> handler;
  mcp::JsonValue raw_schema;
};

namespace pipeline {

mcp::JsonValue run_post(const ToolSpec &spec, const mcp::JsonValue &args,
                        mcp::JsonValue result);

} // namespace pipeline

class SpecTool : public ToolBase, public ISideEffect {
public:
  explicit SpecTool(ToolSpec spec);

  const ToolMeta &meta() const override;
  mcp::JsonValue execute(const mcp::JsonValue &args) override;
  mcp::JsonValue input_schema() const override;
  SideEffect side_effects() const override;
  uint32_t tool_flags() const override;
  const ToolSpec &spec() const;

private:
  ToolSpec spec_;
  ToolMeta meta_;
  mcp::JsonValue schema_;
};

inline SpecTool::SpecTool(ToolSpec spec) : spec_(std::move(spec)) {
  schema_ = spec_.raw_schema.IsObject() ? spec_.raw_schema
                                        : schema::build_schema(spec_.params);
  meta_ = ToolMeta{spec_.name, spec_.description, spec_.category, spec_.tags};
}

inline const ToolMeta &SpecTool::meta() const { return meta_; }

inline mcp::JsonValue SpecTool::execute(const mcp::JsonValue &args) {
  mcp::JsonValue denied =
      authorization::deny_if_unauthorized(spec_.name, spec_.side_effect);
  if (!denied.IsNull())
    return denied;
  mcp::JsonValue result = spec_.handler(args);
  return pipeline::run_post(spec_, args, std::move(result));
}

inline mcp::JsonValue SpecTool::input_schema() const { return schema_; }

inline SideEffect SpecTool::side_effects() const { return spec_.side_effect; }

inline uint32_t SpecTool::tool_flags() const { return spec_.flags; }

inline const ToolSpec &SpecTool::spec() const { return spec_; }

inline std::unique_ptr<ToolBase> make_spec_tool(ToolSpec spec) {
  return std::make_unique<SpecTool>(std::move(spec));
}

} // namespace godot_autopilot

#endif
