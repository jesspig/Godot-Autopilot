#ifndef GODOT_AUTOPILOT_RUNTIME_OPS_HPP
#define GODOT_AUTOPILOT_RUNTIME_OPS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {

class CommandQueue;
CommandQueue &get_editor_queue();

namespace runtime_ops {

void set_editor_queue(godot_autopilot::CommandQueue *q);
bool has_editor_queue();

mcp::JsonValue handle_game_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_eval(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_wait(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_capture(const mcp::JsonValue &args);
mcp::JsonValue handle_game_reload_scripts(const mcp::JsonValue &args);

mcp::JsonValue handle_gda_send(const std::string &op,
                               const mcp::JsonValue &params,
                               int64_t timeout_ms);

void run_channel_self_check(int32_t p_session_id);

mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms);

mcp::JsonValue finalize_capture_response(const mcp::JsonValue &pending_result);

void handle_game_response(const std::string &json_str);

} // namespace runtime_ops
} // namespace godot_autopilot

#endif
