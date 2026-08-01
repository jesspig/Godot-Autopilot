#ifndef GODOT_SELF_DRIVING_RUNTIME_OPS_HPP
#define GODOT_SELF_DRIVING_RUNTIME_OPS_HPP

#include <mcp/JsonValue.hpp>
#include <cstdint>
#include <string>

namespace godot_self_driving {

// Forward declaration — defined in main.cpp, returns the editor main-thread queue.
class CommandQueue;
CommandQueue& get_editor_queue();

namespace runtime_ops {

mcp::JsonValue handle_game_status(const mcp::JsonValue& args);
mcp::JsonValue handle_game_eval(const mcp::JsonValue& args);
mcp::JsonValue handle_game_input(const mcp::JsonValue& args);
mcp::JsonValue handle_game_capture(const mcp::JsonValue& args);

// Executes send_request safely from the current thread (direct call on the
// editor main thread, queue round-trip otherwise). Returns the pending marker
// {"__gsd_pending": id, "timeout_ms": T} or send_request's error. The caller
// (HTTP thread) must follow up with wait_pending_response().
mcp::JsonValue handle_gsd_send(const std::string& op, const mcp::JsonValue& params, int64_t timeout_ms);

// Blocks the calling (HTTP) thread until the game responds or the timeout hits.
mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms);

// game_capture only: after the pending wait, reads the captured file on the
// editor main thread and assembles the final base64 response.
mcp::JsonValue finalize_capture_response(const mcp::JsonValue& pending_result);

// Called on the editor main thread from DebugCapturePlugin::_capture when a
// "gsd:response" message arrives. Matches request_id against the pending table
// and notifies the waiting HTTP thread.
void handle_game_response(const std::string& json_str);

} // namespace runtime_ops
} // namespace godot_self_driving

#endif
