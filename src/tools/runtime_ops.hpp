#ifndef GODOT_AUTOPILOT_RUNTIME_OPS_HPP
#define GODOT_AUTOPILOT_RUNTIME_OPS_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mcp/JsonValue.hpp>
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {

class CommandQueue;
CommandQueue &get_editor_queue();

namespace runtime_ops {

struct LateResult {
  int64_t request_id = 0;
  std::string op;
  int64_t age_ms = 0;
  std::string summary;
};

std::string late_result_summary(const mcp::JsonValue &response);

void push_late_result(std::deque<LateResult> &buffer, LateResult entry,
                       size_t cap);

struct GameJob {
  int64_t job_id = 0;
  int64_t request_id = 0;
  std::string op;
  int64_t started_ms = 0;
  int64_t timeout_ms = 0;
};

struct GameJobTable {
  static constexpr size_t kMaxJobs = 16;
  static constexpr int64_t kExpiryGraceMs = 2000;
  struct CollectOutcome {
    bool found = false;
    bool expired = false;
    int64_t request_id = 0;
    bool had_suppression = false;
  };
  int64_t next_id = 1;
  std::map<int64_t, GameJob> jobs;
  int64_t register_job(int64_t request_id, const std::string &op,
                       int64_t timeout_ms, int64_t now_ms);
  bool contains(int64_t job_id) const;
  CollectOutcome collect(int64_t job_id, int64_t now_ms);
  int expire_stale(int64_t now_ms);
  size_t sweep_on_start(int64_t now_ms) { return expire_stale(now_ms); }
};

GameJobTable &game_jobs();
std::mutex &game_jobs_mutex();

void set_editor_queue(godot_autopilot::CommandQueue *q);
bool has_editor_queue();

mcp::JsonValue handle_game_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_eval(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_wait(const mcp::JsonValue &args);
mcp::JsonValue handle_game_input_status(const mcp::JsonValue &args);
mcp::JsonValue handle_game_capture(const mcp::JsonValue &args);
mcp::JsonValue handle_game_reload_scripts(const mcp::JsonValue &args);
mcp::JsonValue handle_game_job_start(const mcp::JsonValue &args);
mcp::JsonValue handle_game_job_get(const mcp::JsonValue &args);

mcp::JsonValue handle_gda_send(const std::string &op,
                               const mcp::JsonValue &params,
                               int64_t timeout_ms,
                               bool suppress_error_breaks = false);

bool needs_error_break_suppression(const std::string &op,
                                   const std::string &action);

void suppress_error_breaks_for_eval();

void restore_error_breaks();

void run_channel_self_check(int32_t p_session_id);

mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms);

mcp::JsonValue start_pending_op(const std::string &op,
                                const mcp::JsonValue &params,
                                bool suppress_error_breaks,
                                int64_t *out_request_id);

bool try_collect_response(int64_t request_id, mcp::JsonValue &out_response);

void cancel_game_op_request(int64_t request_id);

bool payload_is_pending(const mcp::JsonValue &payload, int64_t *out_request_id);

bool discard_pending(int64_t request_id, std::string &out_detail);

mcp::JsonValue finalize_capture_response(const mcp::JsonValue &pending_result);

void handle_game_response(const std::string &json_str);

} // namespace runtime_ops
} // namespace godot_autopilot

#endif
