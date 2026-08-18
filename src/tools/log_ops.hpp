#ifndef GODOT_AUTOPILOT_LOG_OPS_HPP
#define GODOT_AUTOPILOT_LOG_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace log_ops {

mcp::JsonValue handle_log_get_game_entries(const mcp::JsonValue &args);

}
} // namespace godot_autopilot

#endif
