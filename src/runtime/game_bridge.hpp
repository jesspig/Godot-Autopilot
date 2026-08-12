#ifndef GODOT_AUTOPILOT_RUNTIME_GAME_BRIDGE_HPP
#define GODOT_AUTOPILOT_RUNTIME_GAME_BRIDGE_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace runtime {
namespace game_bridge {

using JV = mcp::JsonValue;

void register_listener();

void unregister_listener();

void register_eval_bridge_classes();

void register_input_bridge_classes();

struct EvalErrorDelta {
  std::string text;
  JV structured;
};

JV error_result(const std::string &message);

JV ok_result(JV result);

godot::SceneTree *get_scene_tree();

godot::Node *resolve_node(const std::string &node_path);

void send_response(int64_t request_id, JV body);

std::string truncate_error_text(const std::string &text);

EvalErrorDelta eval_error_delta(uint64_t since_seq);

void append_eval_runtime_errors(JV &body, uint64_t since_seq);

uint64_t current_error_seq();

void push_game_error(const std::string &file, const std::string &func, int line,
                     const std::string &error, const std::string &descr,
                     bool is_warning, std::vector<std::string> stack);

extern std::unordered_map<int64_t, std::function<void()>> g_cancel_handlers;

void register_cancel_handler(int64_t request_id,
                             std::function<void()> handler);

void unregister_cancel_handler(int64_t request_id);

JV op_eval(const JV &params, int64_t request_id);

JV op_input(const JV &params, int64_t request_id);

JV op_input_wait(const JV &params, int64_t request_id);

JV op_input_status(const JV &params);

} // namespace game_bridge
} // namespace runtime
} // namespace godot_autopilot

#endif
