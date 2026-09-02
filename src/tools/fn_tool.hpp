#ifndef GODOT_AUTOPILOT_FN_TOOL_HPP
#define GODOT_AUTOPILOT_FN_TOOL_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <tools/tool_base.hpp>

namespace godot_autopilot {

class FnTool : public ToolBase, public ISideEffect {
public:
  using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue&)>;

  FnTool(ToolMeta meta, HandlerFn handler, mcp::JsonValue schema,
         SideEffect effect = SideEffect::None)
      : meta_(std::move(meta)),
        handler_(std::move(handler)),
        schema_(std::move(schema)),
        side_(effect) {}

  const ToolMeta& meta() const override { return meta_; }
  mcp::JsonValue execute(const mcp::JsonValue& args) override {
    mcp::JsonValue denied = authorization::deny_if_unauthorized(
        meta_.name, side_);
    if (!denied.IsNull())
      return denied;
    return handler_(args);
  }
  mcp::JsonValue input_schema() const override { return schema_; }
  SideEffect side_effects() const override { return side_; }

private:
  ToolMeta meta_;
  HandlerFn handler_;
  mcp::JsonValue schema_;
  SideEffect side_;
};

inline std::unique_ptr<FnTool> make_fn_tool(ToolMeta meta, FnTool::HandlerFn handler,
                                            mcp::JsonValue schema) {
  return std::make_unique<FnTool>(std::move(meta), std::move(handler), std::move(schema));
}

} // namespace godot_autopilot

#endif
