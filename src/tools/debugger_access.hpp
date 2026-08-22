#ifndef GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {

bool debugger_capture_initialized();

bool debugger_broadcast_request(const std::string &payload,
                                int32_t *out_first_session_id);

void debugger_send_cancel(int32_t session_id, int64_t request_id);

std::vector<int32_t> debugger_breaked_session_ids();

void debugger_continue_session(int32_t session_id);

int32_t debugger_broadcast_reload_scripts(const std::vector<std::string> &script_paths);

namespace debugger_ops {

std::string capture_get_errors_text(size_t limit);

} // namespace debugger_ops
} // namespace godot_autopilot

#endif
