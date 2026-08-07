#ifndef GODOT_SELF_DRIVING_LOG_OPS_HPP
#define GODOT_SELF_DRIVING_LOG_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace log_ops {

mcp::JsonValue handle_log_get_game_entries(const mcp::JsonValue &args);

}
} // namespace godot_self_driving

#endif
