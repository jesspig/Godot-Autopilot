#ifndef GODOT_AUTOPILOT_TOOL_INVOKE_HPP
#define GODOT_AUTOPILOT_TOOL_INVOKE_HPP

#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace tools {

constexpr int kMaxInvokeDepth = 8;

mcp::JsonValue invoke_tool(const std::string &name, const mcp::JsonValue &args);

int invoke_depth();

} // namespace tools
} // namespace godot_autopilot

#endif
