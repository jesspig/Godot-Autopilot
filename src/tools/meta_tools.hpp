#ifndef GODOT_AUTOPILOT_META_TOOLS_HPP
#define GODOT_AUTOPILOT_META_TOOLS_HPP

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include <mcp/JsonValue.hpp>
#include <tools/tool_base.hpp>

namespace godot_autopilot {

class MetaTool : public ::godot_autopilot::ToolBase, public ::godot_autopilot::IMetaTool {
public:
  using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue&)>;

  MetaTool(::godot_autopilot::ToolMeta meta, HandlerFn handler, mcp::JsonValue schema)
      : meta_(std::move(meta)),
        handler_(std::move(handler)),
        schema_(std::move(schema)) {}

  const ::godot_autopilot::ToolMeta& meta() const override { return meta_; }
  mcp::JsonValue execute(const mcp::JsonValue& args) override { return handler_(args); }
  mcp::JsonValue input_schema() const override { return schema_; }

private:
  ::godot_autopilot::ToolMeta meta_;
  HandlerFn handler_;
  mcp::JsonValue schema_;
};

} // namespace godot_autopilot

#endif