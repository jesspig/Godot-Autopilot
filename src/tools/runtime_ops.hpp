#ifndef GODOT_SELF_DRIVING_RUNTIME_OPS_HPP
#define GODOT_SELF_DRIVING_RUNTIME_OPS_HPP

#include <mcp/JsonValue.hpp>
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

// Called on the editor main thread from DebugCapturePlugin::_capture when a
// "gsd:response" message arrives. Matches request_id against the pending table
// and notifies the waiting HTTP thread.
void handle_game_response(const std::string& json_str);

} // namespace runtime_ops
} // namespace godot_self_driving

#endif
