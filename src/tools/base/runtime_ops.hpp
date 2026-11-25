#ifndef GODOT_AUTOPILOT_RUNTIME_OPS_HPP
#define GODOT_AUTOPILOT_RUNTIME_OPS_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include "util/json_value.hpp"
#include <mutex>
#include <optional>
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

std::string late_result_summary(const godot_autopilot::util::JsonValue &response);

void push_late_result(std::deque<LateResult> &buffer, LateResult entry,
                       size_t cap);

struct GameTimeoutReceiptInput {
  std::string op;
  int64_t request_id = 0;
  int64_t timeout_ms = 0;
  int32_t session_count = 0;
  bool awaiting_ready = false;
  int64_t responses_received = 0;
  int64_t pending_count = 0;
  int64_t last_channel_response_age_ms = -1;
  bool late_response_for_request = false;
  bool game_error_during_wait = false;
  godot_autopilot::util::JsonValue late_results;
};

godot_autopilot::util::JsonValue build_game_timeout_receipt(const GameTimeoutReceiptInput &input);

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
  bool bind_request(int64_t job_id, int64_t request_id);
  bool release_job(int64_t job_id);
  bool contains(int64_t job_id) const;
  CollectOutcome collect(int64_t job_id, int64_t now_ms);
  int expire_stale(int64_t now_ms);
  size_t sweep_on_start(int64_t now_ms) { return expire_stale(now_ms); }
};

GameJobTable &game_jobs();
std::mutex &game_jobs_mutex();

void set_editor_queue(godot_autopilot::CommandQueue *q);
bool has_editor_queue();
bool on_editor_main_thread();

godot_autopilot::util::JsonValue handle_game_status(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_eval(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_input(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_input_wait(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_input_status(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_capture(const godot_autopilot::util::JsonValue &args);
void append_runtime_degradation_hint(godot_autopilot::util::JsonValue &result);
godot_autopilot::util::JsonValue handle_game_reload_scripts(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_job_start(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_job_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_sample_property(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_collect_evidence(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_game_validate_ui_layout(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_game_locale(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_game_locale(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_translation_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_translate_text(const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue handle_gda_send(const std::string &op,
                                const godot_autopilot::util::JsonValue &params,
                                int64_t timeout_ms,
                                bool suppress_error_breaks = false,
                                bool wait_ready = true,
                                std::optional<int32_t> session_id = std::nullopt);

bool needs_error_break_suppression(const std::string &op,
                                    const std::string &action);

void suppress_error_breaks_for_eval();

void restore_error_breaks();

void run_channel_self_check(int32_t session_id);

void on_session_ready(int32_t session_id);

godot_autopilot::util::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms);

bool wait_pending_completion(int64_t request_id, int64_t timeout_ms);

godot_autopilot::util::JsonValue wait_game_job(int64_t job_id, int64_t timeout_ms);

bool payload_is_game_job(const godot_autopilot::util::JsonValue &payload, int64_t *out_job_id);

godot_autopilot::util::JsonValue start_pending_op(const std::string &op,
                                 const godot_autopilot::util::JsonValue &params,
                                 bool suppress_error_breaks,
                                 int64_t *out_request_id,
                                 bool wait_ready = true,
                                 std::optional<int32_t> session_id = std::nullopt);

bool try_collect_response(int64_t request_id, godot_autopilot::util::JsonValue &out_response);

void cancel_game_op_request(int64_t request_id);

bool payload_is_pending(const godot_autopilot::util::JsonValue &payload, int64_t *out_request_id);

bool discard_pending(int64_t request_id, std::string &out_detail);

godot_autopilot::util::JsonValue finalize_capture_response(
    const godot_autopilot::util::JsonValue &pending_result,
    const godot_autopilot::util::JsonValue &options = godot_autopilot::util::JsonValue());

void handle_game_response(const std::string &json_str);

} // namespace runtime_ops
} // namespace godot_autopilot

#endif
