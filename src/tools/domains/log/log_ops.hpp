#ifndef GODOT_AUTOPILOT_LOG_OPS_HPP
#define GODOT_AUTOPILOT_LOG_OPS_HPP

#include <mcp/JsonValue.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace log_ops {

std::vector<std::string> filter_log_lines(const std::vector<std::string> &lines,
                                          const std::string &filter,
                                          int64_t limit);

mcp::JsonValue handle_log_get_game_entries(const mcp::JsonValue &args);

}
} // namespace godot_autopilot

#endif
