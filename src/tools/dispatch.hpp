#ifndef GODOT_AUTOPILOT_DISPATCH_HPP
#define GODOT_AUTOPILOT_DISPATCH_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <mcp/JsonValue.hpp>
#include <mcp/McpTypes.hpp>

namespace godot_autopilot {
class CommandQueue;
namespace dispatch {

using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue &)>;

using HandlerMap = std::unordered_map<std::string, HandlerFn>;

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers);
void clear_handlers();

int64_t main_thread_wait_budget_ms(uint32_t tool_flags, int64_t default_ms,
                                   int64_t health_ms);

mcp::JsonValue run_on_main_thread_with_budget(
    CommandQueue &queue, const std::string &tool_name, uint32_t tool_flags,
    std::function<mcp::JsonValue()> fn);

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args);

mcp::CallToolResult export_blocked_result();

} // namespace dispatch
} // namespace godot_autopilot

#endif
