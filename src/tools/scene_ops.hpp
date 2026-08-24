#ifndef GODOT_AUTOPILOT_SCENE_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace scene_ops {

mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_delete(const mcp::JsonValue &args);
mcp::JsonValue handle_rename(const mcp::JsonValue &args);
mcp::JsonValue handle_reparent(const mcp::JsonValue &args);
mcp::JsonValue handle_instance(const mcp::JsonValue &args);
mcp::JsonValue handle_get_tree(const mcp::JsonValue &args);

} // namespace scene_ops
} // namespace godot_autopilot

#endif
