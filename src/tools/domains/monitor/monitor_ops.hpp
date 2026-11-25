#ifndef GODOT_AUTOPILOT_MONITOR_OPS_HPP
#define GODOT_AUTOPILOT_MONITOR_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace monitor_ops {

mcp::JsonValue handle_game_monitor(const mcp::JsonValue &args);

} // namespace monitor_ops
} // namespace godot_autopilot

#endif
