#ifndef GODOT_AUTOPILOT_GROUP_OPS_HPP
#define GODOT_AUTOPILOT_GROUP_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace group_ops {

mcp::JsonValue handle_add_node_to_group(const mcp::JsonValue &args);
mcp::JsonValue handle_remove_node_from_group(const mcp::JsonValue &args);
mcp::JsonValue handle_has_node_in_group(const mcp::JsonValue &args);

} // namespace group_ops
} // namespace godot_autopilot

#endif
