#ifndef GODOT_AUTOPILOT_ANALYZE_OPS_HPP
#define GODOT_AUTOPILOT_ANALYZE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace analyze_ops {

mcp::JsonValue handle_validate_scene_file(const mcp::JsonValue &args);
mcp::JsonValue handle_find_unused_resources(const mcp::JsonValue &args);
mcp::JsonValue handle_trace_signal_flow(const mcp::JsonValue &args);

} // namespace analyze_ops
} // namespace godot_autopilot

#endif
