#ifndef GODOT_AUTOPILOT_MESH_OPS_HPP
#define GODOT_AUTOPILOT_MESH_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace mesh_ops {

mcp::JsonValue handle_surface_action(const mcp::JsonValue &args);
mcp::JsonValue handle_inspect_action(const mcp::JsonValue &args);
mcp::JsonValue handle_multimesh_action(const mcp::JsonValue &args);
mcp::JsonValue handle_multimesh_instance_action(const mcp::JsonValue &args);

} // namespace mesh_ops
} // namespace godot_autopilot

#endif
