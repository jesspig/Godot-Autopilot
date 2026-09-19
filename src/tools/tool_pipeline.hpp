#ifndef GODOT_AUTOPILOT_TOOL_PIPELINE_HPP
#define GODOT_AUTOPILOT_TOOL_PIPELINE_HPP

#include <mcp/JsonValue.hpp>

#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace pipeline {

mcp::JsonValue run_post(const ToolSpec &spec, const mcp::JsonValue &args,
                        mcp::JsonValue result);

} // namespace pipeline
} // namespace godot_autopilot

#endif
