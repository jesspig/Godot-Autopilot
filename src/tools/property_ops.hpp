#ifndef GODOT_AUTOPILOT_PROPERTY_OPS_HPP
#define GODOT_AUTOPILOT_PROPERTY_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace property_ops {

mcp::JsonValue handle_get(const mcp::JsonValue &args);
mcp::JsonValue handle_set(const mcp::JsonValue &args);
mcp::JsonValue handle_get_list(const mcp::JsonValue &args);
mcp::JsonValue handle_signal_connect(const mcp::JsonValue &args);
mcp::JsonValue handle_signal_disconnect(const mcp::JsonValue &args);

} // namespace property_ops
} // namespace godot_autopilot

#endif
