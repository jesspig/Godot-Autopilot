#ifndef GODOT_AUTOPILOT_DISPATCH_HPP
#define GODOT_AUTOPILOT_DISPATCH_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <mcp/JsonValue.hpp>
#include <mcp/McpTypes.hpp>

namespace godot_autopilot {
namespace dispatch {

using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue &)>;

using HandlerMap = std::unordered_map<std::string, HandlerFn>;

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers);
void clear_handlers();

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args);

mcp::CallToolResult export_blocked_result();

} // namespace dispatch
} // namespace godot_autopilot

#endif
