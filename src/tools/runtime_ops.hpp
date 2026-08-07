#ifndef GODOT_SELF_DRIVING_RUNTIME_OPS_HPP
#define GODOT_SELF_DRIVING_RUNTIME_OPS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_self_driving {

class CommandQueue;
CommandQueue &get_editor_queue();

namespace runtime_ops {

void set_editor_queue(godot_self_driving::CommandQueue *q);
bool has_editor_queue();

mcp::JsonValue handle_game_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_eval(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_wait(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_capture(const mcp::JsonValue &args);

mcp::JsonValue handle_gsd_send(const std::string &op,
                               const mcp::JsonValue &params,
                               int64_t timeout_ms);

mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms);

mcp::JsonValue finalize_capture_response(const mcp::JsonValue &pending_result);

void handle_game_response(const std::string &json_str);

} // namespace runtime_ops
} // namespace godot_self_driving

#endif
