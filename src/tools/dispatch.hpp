#ifndef GODOT_AUTOPILOT_DISPATCH_HPP
#define GODOT_AUTOPILOT_DISPATCH_HPP

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mcp/JsonValue.hpp>
#include <mcp/McpTypes.hpp>

namespace godot_autopilot {
namespace dispatch {

using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue &)>;

extern std::unordered_map<std::string, HandlerFn> g_handlers;
extern std::unordered_map<std::string, HandlerFn> g_meta_handlers;
extern const std::unordered_set<std::string> meta_tool_names;

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args);

mcp::CallToolResult export_blocked_result();

} // namespace dispatch
} // namespace godot_autopilot

#endif
