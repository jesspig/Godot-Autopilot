#ifndef GODOT_AUTOPILOT_CSHARP_BRIDGE_OPS_HPP
#define GODOT_AUTOPILOT_CSHARP_BRIDGE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace csharp_bridge_ops {

mcp::JsonValue handle_csharp_get(const mcp::JsonValue &args);
mcp::JsonValue handle_csharp_set(const mcp::JsonValue &args);
mcp::JsonValue handle_csharp_call(const mcp::JsonValue &args);
mcp::JsonValue handle_csharp_subscribe(const mcp::JsonValue &args);
mcp::JsonValue handle_csharp_unsubscribe(const mcp::JsonValue &args);

} // namespace csharp_bridge_ops
} // namespace godot_autopilot

#endif
