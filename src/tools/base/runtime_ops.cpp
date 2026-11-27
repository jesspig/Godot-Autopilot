#include "tools/base/runtime_ops.hpp"
#include "tools/domains/debugger/debugger_ops.hpp"

#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "core/error_watermark.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/monitor.hpp"
#include "runtime/game_response_envelope.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/base/ready_policy.hpp"
#include "tools/domains/capture/capture_ops.hpp"
#include "tools/base/debugger_access.hpp"
#include "tools/base/dispatch.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace runtime_ops {

using godot_autopilot::runtime::response_envelope::carry_transport_fields;
using godot_autopilot::util::error_json;
using JV = godot_autopilot::util::JsonValue;

namespace {

CommandQueue *g_editor_queue = nullptr;
std::mutex g_editor_queue_mtx;

constexpr int64_t RESPONSE_GRACE_MS = GDA_RESPONSE_GRACE_MS;
constexpr int64_t PHYSICS_STALL_DETECT_MS = 1000;
constexpr size_t MAX_PAYLOAD_SUMMARY_CHARS = 256;
constexpr size_t RECENT_REQUEST_MAX = 64;

int64_t g_last_status_physics_frame = -1;
std::chrono::steady_clock::time_point g_last_status_time;
bool g_last_status_time_valid = false;

bool env_flag_disabled(const std::string &value) {
  return value == "0" || value == "false";
}

int64_t steady_now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string summarize_payload(const std::string &payload) {
  if (payload.size() <= MAX_PAYLOAD_SUMMARY_CHARS)
    return payload;
  return payload.substr(0, MAX_PAYLOAD_SUMMARY_CHARS) + "...";
}

std::atomic<int64_t> g_next_request_id{1};
std::atomic<int64_t> g_game_error_responses{0};
std::atomic<int64_t> g_last_response_steady_ms{-1};

struct PendingRequest {
  enum class State { Waiting, Completed, Cancelled };

  std::mutex mtx;
  std::condition_variable cv;
  State state = State::Waiting;
  JV response;
  std::string op;
  std::string payload;
  std::optional<int32_t> requested_session_id;
  int32_t session_id = -1;
  int32_t session_count = 0;
  bool suppress_error_breaks = false;
  bool error_breaks_suppressed = false;
  bool awaiting_ready = false;
  int64_t responses_received = 0;
  int64_t error_responses_at_dispatch = 0;
};

struct MainThreadTaskState {
  JV result;
};

std::mutex g_pending_mtx;
std::map<int64_t, std::shared_ptr<PendingRequest>> g_pending;

GameJobTable g_game_jobs;
std::mutex g_game_jobs_mtx;

struct SentRequestRecord {
  int64_t request_id = 0;
  std::string op;
  int64_t sent_ms = 0;
};

std::mutex g_recent_requests_mtx;
std::deque<SentRequestRecord> g_recent_requests;

std::mutex g_late_results_mtx;
std::deque<LateResult> g_late_results;

void record_sent_request(int64_t request_id, const std::string &op) {
  SentRequestRecord record;
  record.request_id = request_id;
  record.op = op;
  record.sent_ms = steady_now_ms();
  std::lock_guard<std::mutex> lock(g_recent_requests_mtx);
  g_recent_requests.push_back(std::move(record));
  while (g_recent_requests.size() > RECENT_REQUEST_MAX)
    g_recent_requests.pop_front();
}

void record_late_result(int64_t request_id, const JV &parsed) {
  LateResult entry;
  entry.request_id = request_id;
  entry.summary = late_result_summary(parsed);
  {
    std::lock_guard<std::mutex> lock(g_recent_requests_mtx);
    for (auto it = g_recent_requests.rbegin(); it != g_recent_requests.rend();
         ++it) {
      if (it->request_id == request_id) {
        entry.op = it->op;
        entry.age_ms = steady_now_ms() - it->sent_ms;
        break;
      }
    }
  }
  std::lock_guard<std::mutex> lock(g_late_results_mtx);
  push_late_result(g_late_results, std::move(entry),
                   GDA_LATE_RESULT_BUFFER_MAX);
}

JV late_results_snapshot() {
  std::lock_guard<std::mutex> lock(g_late_results_mtx);
  JV arr(JV::array_tag);
  for (const LateResult &entry : g_late_results) {
    JV item(JV::object_tag);
    item["request_id"] = JV(entry.request_id);
    item["op"] = JV(entry.op);
    item["age_ms"] = JV(entry.age_ms);
    item["summary"] = JV(entry.summary);
    arr.PushBack(std::move(item));
  }
  return arr;
}

constexpr const char *ENGINE_CMD_IGNORE_ERROR_BREAKS =
    "set_ignore_error_breaks";

std::mutex g_error_break_mtx;
int g_error_break_suppression_owners = 0;
bool g_error_break_ignore_active = false;

bool broadcast_error_break_ignore(bool ignore) {
  godot::Array data;
  data.push_back(ignore);
  return debugger_broadcast_engine_command(
      godot::String(ENGINE_CMD_IGNORE_ERROR_BREAKS), data);
}

void send_error_break_restore() {
  if (broadcast_error_break_ignore(false)) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                              "ignore-error-breaks restored after game eval "
                              "script");
    return;
  }
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Transport,
      "ignore-error-breaks could not be restored: no active game debug "
      "session — error breaks may stay disabled in the game until it "
      "restarts");
}

void erase_pending(int64_t request_id,
                   const std::shared_ptr<PendingRequest> &pending) {
  std::lock_guard<std::mutex> lock(g_pending_mtx);
  auto it = g_pending.find(request_id);
  if (it != g_pending.end() && it->second == pending)
    g_pending.erase(it);
}

std::shared_ptr<PendingRequest> create_pending_request(
    int64_t request_id, const std::string &op,
    std::optional<int32_t> requested_session_id) {
  auto pending = std::make_shared<PendingRequest>();
  pending->op = op;
  pending->requested_session_id = requested_session_id;
  pending->error_responses_at_dispatch = g_game_error_responses.load();
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    g_pending[request_id] = pending;
  }
  return pending;
}

void abandon_pending_request(int64_t request_id,
                             const std::shared_ptr<PendingRequest> &pending) {
  bool release_error_breaks = false;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state == PendingRequest::State::Waiting)
      pending->state = PendingRequest::State::Cancelled;
    release_error_breaks = pending->error_breaks_suppressed;
    pending->error_breaks_suppressed = false;
  }
  pending->cv.notify_all();
  erase_pending(request_id, pending);
  if (release_error_breaks)
    restore_error_breaks();
}

bool is_eval_op(const std::string &op) {
  return op == std::string(GDA_OP_EVAL) || op == std::string(GDA_OP_EVAL_ASSERT);
}

const char *eval_timeout_cause_name(bool late_response_for_request,
                                    bool game_error_during_wait,
                                    bool channel_response_seen) {
  if (late_response_for_request || game_error_during_wait)
    return "script_error";
  if (!channel_response_seen)
    return "channel_lost";
  return "unknown";
}

bool late_results_contain(int64_t request_id) {
  std::lock_guard<std::mutex> lock(g_late_results_mtx);
  for (const LateResult &entry : g_late_results) {
    if (entry.request_id == request_id)
      return true;
  }
  return false;
}

CommandQueue *current_editor_queue() {
  std::lock_guard<std::mutex> lock(g_editor_queue_mtx);
  return g_editor_queue;
}

void cancel_all_pending(const std::string &reason) {
  std::vector<std::shared_ptr<PendingRequest>> pending_requests;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    for (auto &entry : g_pending)
      pending_requests.push_back(entry.second);
    g_pending.clear();
  }

  int released_suppressions = 0;
  for (const auto &pending : pending_requests) {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state != PendingRequest::State::Waiting)
      continue;
    pending->state = PendingRequest::State::Cancelled;
    pending->response = error_json(reason);
    if (pending->error_breaks_suppressed) {
      pending->error_breaks_suppressed = false;
      ++released_suppressions;
    }
    pending->cv.notify_all();
  }
  for (int i = 0; i < released_suppressions; ++i)
    restore_error_breaks();
}

JV session_selection_error(const std::string &code, const std::string &message,
                           const std::vector<int32_t> &session_ids) {
  JV error = error_json(message);
  error["code"] = JV(code);
  error["retryable"] = JV(false);
  JV ids(JV::array_tag);
  for (int32_t session_id : session_ids) {
    ids.PushBack(JV(static_cast<int64_t>(session_id)));
  }
  error["session_ids"] = std::move(ids);
  return error;
}

JV fail_pending_request(int64_t request_id,
                        const std::shared_ptr<PendingRequest> &pending,
                        JV error) {
  bool release_error_breaks = false;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    pending->state = PendingRequest::State::Cancelled;
    release_error_breaks = pending->error_breaks_suppressed;
    pending->error_breaks_suppressed = false;
  }
  pending->cv.notify_all();
  erase_pending(request_id, pending);
  if (release_error_breaks)
    restore_error_breaks();
  return error;
}

bool send_pending_to_session(int64_t request_id, const std::string &op,
                             const std::string &payload, int32_t session_id,
                             bool suppress_error_breaks,
                             const std::shared_ptr<PendingRequest> &pending) {
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state != PendingRequest::State::Waiting)
      return false;
    pending->session_id = session_id;
    pending->session_count = 1;
  }
  if (suppress_error_breaks) {
    suppress_error_breaks_for_eval();
    bool abandoned = false;
    {
      std::lock_guard<std::mutex> lock(pending->mtx);
      abandoned = pending->state != PendingRequest::State::Waiting;
      if (!abandoned)
        pending->error_breaks_suppressed = true;
    }
    if (abandoned) {
      {
        std::lock_guard<std::mutex> lock(pending->mtx);
        pending->session_id = -1;
        pending->session_count = 0;
      }
      restore_error_breaks();
      return false;
    }
  }
  if (!debugger_send_request(payload, session_id)) {
    bool release_error_breaks = false;
    {
      std::lock_guard<std::mutex> lock(pending->mtx);
      pending->session_id = -1;
      pending->session_count = 0;
      release_error_breaks = pending->error_breaks_suppressed;
      pending->error_breaks_suppressed = false;
    }
    if (release_error_breaks)
      restore_error_breaks();
    return false;
  }
  record_sent_request(request_id, op);
  return true;
}

JV queue_pending_for_ready(int64_t request_id, const std::string &op,
                           const std::string &payload,
                           bool suppress_error_breaks,
                           const std::shared_ptr<PendingRequest> &pending) {
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    pending->payload = payload;
    pending->awaiting_ready = true;
    pending->suppress_error_breaks = suppress_error_breaks;
  }
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Transport,
      "game op \"" + op +
          "\" queued until the game process reports gda:ready "
          "(request_id " +
          std::to_string(request_id) + ")");
  JV queued(JV::object_tag);
  queued["request_id"] = JV(request_id);
  queued["result"] = JV("sent");
  queued["queued_for_ready"] = JV(true);
  return queued;
}

JV send_request(int64_t request_id, const std::string &op, const JV &params,
                bool suppress_error_breaks, bool wait_ready,
                 std::optional<int32_t> requested_session_id,
                const std::shared_ptr<PendingRequest> &pending) {
  if (!debugger_capture_initialized()) {
    return fail_pending_request(
        request_id, pending, error_json("debugger capture plugin not initialized"));
  }

  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state != PendingRequest::State::Waiting)
      return fail_pending_request(
          request_id, pending,
          error_json("game request was abandoned before dispatch"));
  }

  JV payload(JV::object_tag);
  payload[GDA_FIELD_REQUEST_ID] = JV(request_id);
  payload[GDA_FIELD_OP] = JV(op);
  payload[GDA_FIELD_PARAMS] = params;

  const std::string payload_text = payload.Dump();
  const auto active_sessions = debugger_active_session_ids();
  const auto ready_sessions = debugger_request_ready_session_ids();
  const SessionSelectionResult selection =
      select_game_session(ready_sessions, active_sessions, requested_session_id);
  if (selection.kind == SessionSelection::Ambiguous) {
    std::string ready_text;
    for (int32_t session_id : ready_sessions) {
      if (!ready_text.empty())
        ready_text += ", ";
      ready_text += std::to_string(session_id);
    }
    return fail_pending_request(
        request_id, pending,
        session_selection_error(
            "ambiguous_session",
            "multiple game sessions are ready; pass session_id to select one "
            "of " +
                ready_text,
            ready_sessions));
  }
  if (selection.kind == SessionSelection::InvalidSession) {
    return fail_pending_request(
        request_id, pending,
        session_selection_error(
            "invalid_session",
            "game session_id " + std::to_string(*requested_session_id) +
                " is not active",
            active_sessions));
  }
  if (selection.kind == SessionSelection::Selected) {
    if (!send_pending_to_session(request_id, op, payload_text,
                                 selection.session_id, suppress_error_breaks,
                                 pending)) {
      JV error = error_json("failed to send game request to session " +
                            std::to_string(selection.session_id));
      error["code"] = JV("send_failed");
      error["retryable"] = JV(false);
      return fail_pending_request(request_id, pending, std::move(error));
    }
    JV result(JV::object_tag);
    result["request_id"] = JV(request_id);
    result["result"] = JV("sent");
    result["session_id"] = JV(static_cast<int64_t>(selection.session_id));
    return result;
  }

  const ReadyDecision ready_decision =
      selection.kind == SessionSelection::NotReady
          ? (wait_ready ? ReadyDecision::QueueForReady
                        : ReadyDecision::FailNotReady)
          : decide_game_request(
                static_cast<int>(ready_sessions.size()),
                static_cast<int>(ready_sessions.size()),
                static_cast<int>(active_sessions.size()), wait_ready);
  bool editor_playing_scene = false;
  if (ready_decision == ReadyDecision::FailNotRunning && wait_ready) {
    if (auto *editor = godot::EditorInterface::get_singleton())
      editor_playing_scene = editor->is_playing_scene();
  }
  if (should_queue_for_ready(ready_decision, wait_ready,
                             editor_playing_scene)) {
    return queue_pending_for_ready(request_id, op, payload_text,
                                   suppress_error_breaks, pending);
  }
  if (ready_decision == ReadyDecision::FailNotRunning) {
    return fail_pending_request(
        request_id, pending,
        error_json("game not running: no game process is attached — start "
                   "the game from the editor first (play the scene), then "
                   "retry; if the game was just started, wait a moment for "
                   "it to report ready"));
  }
  return fail_pending_request(
      request_id, pending,
      error_json(
          "game started but not ready: the game process has not reported gda "
          "ready yet — wait a moment after play, or verify the "
          "game project loads the godot-autopilot extension"));
}

JV dispatch_game_request(int64_t request_id, const std::string &op,
                         const JV &params, bool suppress_error_breaks,
                         bool wait_ready,
                         std::optional<int32_t> requested_session_id) {
  CommandQueue *queue = current_editor_queue();
  if (!queue)
    return error_json("editor command queue not initialized");
  auto pending = create_pending_request(request_id, op, requested_session_id);
  auto state = std::make_shared<MainThreadTaskState>();
  const std::string label = "game request \"" + op + "\"";
  dispatch::BoundedWaitOutcome outcome = dispatch::run_bounded_on_main_thread(
      *queue, label, state,
      [state, pending, request_id, op, params, suppress_error_breaks,
       wait_ready, requested_session_id](MainThreadTaskState &st) {
        st.result = send_request(request_id, op, params, suppress_error_breaks,
                                  wait_ready, requested_session_id, pending);
      },
      GDA_DISPATCH_WAIT_DEFAULT_MS);
  if (outcome.timed_out) {
    abandon_pending_request(request_id, pending);
    return dispatch::main_thread_timeout_result(label, outcome.waited_ms,
                                                outcome.cancelled);
  }
  if (!outcome.ok) {
    abandon_pending_request(request_id, pending);
    return error_json(outcome.error);
  }
  return state->result;
}

JV read_capture_file(const godot_autopilot::util::JsonValue &pending_result,
                     const godot_autopilot::util::JsonValue &options) {
  std::string path = pending_result["path"].GetString();
  godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
      godot::String(path.c_str()), godot::FileAccess::READ);
  if (file.is_null() || !file->is_open()) {
    return error_json("failed to read captured file: " + path);
  }
  uint64_t length = file->get_length();
  godot::PackedByteArray bytes =
      file->get_buffer(static_cast<int64_t>(length));
  file->close();
  if (bytes.size() <= 0) {
    return error_json("captured file is empty: " + path);
  }
  if (static_cast<size_t>(bytes.size()) > GDA_CAPTURE_MAX_PNG_BYTES) {
    JV error = error_json("captured PNG exceeds the capture limit of " +
                          std::to_string(GDA_CAPTURE_MAX_PNG_BYTES) + " bytes");
    JV details(JV::object_tag);
    details["code"] = JV("capture_bytes_exceeded");
    error["structured_error"] = std::move(details);
    return error;
  }
  std::string b64 =
      capture_ops::base64_encode(bytes.ptrw(), static_cast<size_t>(bytes.size()));
  JV r(JV::object_tag);
  r["data"] = JV(b64);
  r["format"] = JV("png");
  if (auto *w = pending_result.Find("width"))
    r["width"] = *w;
  if (auto *h = pending_result.Find("height"))
    r["height"] = *h;
  r["path"] = JV(path);
  static constexpr const char *kCapturePassthroughFields[] = {
      "region", "annotated", "elements", "source_width",
      "source_height", "elements_truncated", "node_elements",
      "node_truncated", "when_satisfied_frame", "captured_frame"};
  for (const char *key : kCapturePassthroughFields) {
    if (auto *value_p = pending_result.Find(key))
      r[key] = *value_p;
  }
  capture_ops::CaptureSaveOptions save_options;
  if (auto *save_p = options.Find("save"); save_p && save_p->IsBool())
    save_options.save = save_p->GetBool();
  if (auto *dir_p = options.Find("save_dir");
      dir_p && dir_p->IsString() && !dir_p->GetString().empty()) {
    save_options.save_dir = dir_p->GetString();
    save_options.save = true;
  }
  if (save_options.save) {
    JV saved = capture_ops::save_capture_bytes(bytes, save_options);
    if (saved.Contains("error"))
      return saved;
    r["saved_path"] = saved["saved_path"];
  }
  if (r.Dump().size() > GDA_MAX_JSON_RESPONSE_BYTES) {
    JV error = error_json("capture response exceeds the JSON response limit of " +
                          std::to_string(GDA_MAX_JSON_RESPONSE_BYTES) + " bytes");
    JV details(JV::object_tag);
    details["code"] = JV("response_too_large");
    error["structured_error"] = std::move(details);
    return error;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_game_viewport completed");
  return r;
}

void send_timeout_recovery(int32_t session_id, int64_t request_id) {
  if (session_id >= 0) {
    debugger_send_cancel(session_id, request_id);
    const auto breaked = debugger_breaked_session_ids();
    if (std::find(breaked.begin(), breaked.end(), session_id) != breaked.end()) {
      debugger_continue_session(session_id);
    }
  }
}

void schedule_timeout_recovery(int32_t session_id, int64_t request_id) {
  CommandQueue *queue = current_editor_queue();
  if (!queue || queue->is_closed())
    return;
  if (queue->is_main_thread()) {
    send_timeout_recovery(session_id, request_id);
    return;
  }
  try {
    queue->submit([session_id, request_id]() {
      send_timeout_recovery(session_id, request_id);
    });
  } catch (...) {
  }
}

} // namespace

std::string late_result_summary(const godot_autopilot::util::JsonValue &response) {
  const godot_autopilot::util::JsonValue *source = response.Find(GDA_FIELD_ERROR);
  if (!source)
    source = response.Find(GDA_FIELD_RESULT);
  if (!source)
    source = response.Find("data");
  if (!source)
    return "";
  std::string text = source->Dump();
  if (text.size() > GDA_LATE_RESULT_SUMMARY_CHARS)
    text.resize(GDA_LATE_RESULT_SUMMARY_CHARS);
  return text;
}

void push_late_result(std::deque<LateResult> &buffer, LateResult entry,
                       size_t cap) {
  if (cap == 0)
    return;
  buffer.push_back(std::move(entry));
  while (buffer.size() > cap)
    buffer.pop_front();
}

godot_autopilot::util::JsonValue build_game_timeout_receipt(const GameTimeoutReceiptInput &input) {
  const std::string op_name = input.op.empty() ? "?" : input.op;
  std::string response_summary;
  if (input.last_channel_response_age_ms >= 0) {
    response_summary =
        "responses received for this request: " +
        std::to_string(input.responses_received) + ", last channel response " +
        std::to_string(input.last_channel_response_age_ms) +
        " ms ago, pending: " + std::to_string(input.pending_count);
  } else {
    response_summary = "no gda:response received on the channel yet, pending: " +
                       std::to_string(input.pending_count);
  }

  JV error;
  if (input.awaiting_ready) {
    error = error_json(
        "game op \"" + op_name + "\" timed out after " +
        std::to_string(input.timeout_ms) + " ms (request_id " +
        std::to_string(input.request_id) +
        ") — the game process is connected but never reported gda:ready "
        "while the request was queued (no gda:ready arrived during the "
        "wait): verify the game project loads the godot-autopilot "
        "extension, then wait a moment after play and retry; " +
        response_summary);
    JV details(JV::object_tag);
    details["code"] = JV("game_ready_timeout");
    details["retryable"] = JV(true);
    details["timeout_ms"] = JV(input.timeout_ms);
    debugger_ops::SessionReadiness state;
    const auto ids = debugger_active_session_ids();
    if (!ids.empty())
      state = debugger_ops::session_readiness(ids.front());
    const char *diagnosis = debugger_ops::ready_diagnosis(state);
    details["ready_diagnosis"] = JV(diagnosis);
    details["hello_attempts"] = JV(static_cast<int64_t>(state.hello_attempts));
    details["next_action"] = JV(
        std::string(diagnosis) == "extension_not_loaded"
            ? "verify the game project loads the godot-autopilot extension"
            : "wait for scene-ready, then retry; use get_game_state session for details");
    error["structured_error"] = std::move(details);
  } else {
    error = error_json(
        "game op \"" + op_name + "\" timed out after " +
        std::to_string(input.timeout_ms) + " ms (request_id " +
        std::to_string(input.request_id) + ") — sent to " +
        std::to_string(input.session_count) + " debug session(s); " +
        response_summary +
        "; the game process may be paused or physics-frozen (query "
        "get_game_status), or the game project may not load the "
        "godot-autopilot extension. "
        "If the in-game script errored, a pending await may "
        "never complete — run get_game_log_entries to inspect "
        "the game log. The plugin cancelled the request and, when the "
        "debugger session was breaked, sent an engine continue so the "
        "game main thread is released.");
  }
  if (input.late_results.IsArray() && !input.late_results.GetArray().empty())
    error["late_results"] = input.late_results;
  if (!input.awaiting_ready && is_eval_op(op_name)) {
    JV details(JV::object_tag);
    details["code"] = JV("eval_timeout");
    details["timeout_ms"] = JV(input.timeout_ms);
    details["game_may_be_busy"] = JV(true);
    details["timeout_cause"] =
        JV(eval_timeout_cause_name(input.late_response_for_request,
                                   input.game_error_during_wait,
                                   input.last_channel_response_age_ms >= 0));
    details["note"] = JV(
        "the game process may still be occupied: the editor stopped waiting "
        "for the eval script, but a running GDScript call cannot be "
        "interrupted — the game main thread resumes only after the script "
        "returns");
    error["structured_error"] = std::move(details);
  }
  return error;
}

int64_t GameJobTable::register_job(int64_t request_id, const std::string &op,
                                    int64_t timeout_ms, int64_t now_ms) {
  if (jobs.size() >= kMaxJobs)
    return -1;
  GameJob job;
  job.job_id = next_id;
  job.request_id = request_id;
  job.op = op;
  job.started_ms = now_ms;
  job.timeout_ms = timeout_ms;
  jobs.emplace(job.job_id, std::move(job));
  return next_id++;
}

bool GameJobTable::bind_request(int64_t job_id, int64_t request_id) {
  auto it = jobs.find(job_id);
  if (it == jobs.end())
    return false;
  it->second.request_id = request_id;
  return true;
}

bool GameJobTable::release_job(int64_t job_id) {
  return jobs.erase(job_id) != 0;
}

bool GameJobTable::contains(int64_t job_id) const {
  return jobs.find(job_id) != jobs.end();
}

GameJobTable::CollectOutcome GameJobTable::collect(int64_t job_id,
                                                   int64_t now_ms) {
  CollectOutcome outcome;
  auto it = jobs.find(job_id);
  if (it == jobs.end())
    return outcome;
  outcome.found = true;
  outcome.request_id = it->second.request_id;
  outcome.expired =
      (now_ms - it->second.started_ms) > (it->second.timeout_ms + kExpiryGraceMs);
  if (outcome.expired) {
    monitor::state("game_job_expired", "degraded", true);
  }
  jobs.erase(it);
  return outcome;
}

int GameJobTable::expire_stale(int64_t now_ms) {
  int removed = 0;
  for (auto it = jobs.begin(); it != jobs.end();) {
    if ((now_ms - it->second.started_ms) >
        (it->second.timeout_ms + kExpiryGraceMs)) {
      it = jobs.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  if (removed > 0) {
    monitor::state("game_job_expired", "degraded", true);
  }
  return removed;
}

GameJobTable &game_jobs() { return g_game_jobs; }

std::mutex &game_jobs_mutex() { return g_game_jobs_mtx; }

bool needs_error_break_suppression(const std::string &op,
                                   const std::string &action) {
  return op == std::string(GDA_OP_EVAL) && action == "script";
}

void suppress_error_breaks_for_eval() {
  bool first_owner = false;
  {
    std::lock_guard<std::mutex> lock(g_error_break_mtx);
    ++g_error_break_suppression_owners;
    first_owner = g_error_break_suppression_owners == 1;
  }
  if (!first_owner) {
    LogSystem::instance().log(
        LogLevel::Debug, LogCategory::Transport,
        "ignore-error-breaks suppression already active for a game eval "
        "script; skipping duplicate send");
    return;
  }

  const bool sent = broadcast_error_break_ignore(true);
  {
    std::lock_guard<std::mutex> lock(g_error_break_mtx);
    g_error_break_ignore_active = sent;
  }
  if (sent) {
    LogSystem::instance().log(
        LogLevel::Info, LogCategory::Transport,
        "ignore-error-breaks enabled for a game eval script");
  } else {
    LogSystem::instance().log(
        LogLevel::Debug, LogCategory::Transport,
        "ignore-error-breaks not enabled: no active game debug session");
  }
}

void restore_error_breaks() {
  bool send_false = false;
  {
    std::lock_guard<std::mutex> lock(g_error_break_mtx);
    if (g_error_break_suppression_owners > 0)
      --g_error_break_suppression_owners;
    if (g_error_break_suppression_owners == 0 && g_error_break_ignore_active) {
      g_error_break_ignore_active = false;
      send_false = true;
    }
  }
  if (!send_false)
    return;

  CommandQueue *queue = current_editor_queue();
  if (queue && !queue->is_closed()) {
    if (queue->is_main_thread()) {
      send_error_break_restore();
      return;
    }
    try {
      queue->submit([]() { send_error_break_restore(); });
      return;
    } catch (const std::exception &ex) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Transport,
          "failed to schedule ignore-error-breaks restore: " +
              std::string(ex.what()));
      return;
    }
  }
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Transport,
      "ignore-error-breaks restore skipped: editor command queue unavailable "
      "— error breaks may stay disabled in the game until it restarts");
}

godot_autopilot::util::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms) {
  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it != g_pending.end())
      pending = it->second;
  }
  if (!pending) {
    return error_json("pending request not found (request_id " +
                      std::to_string(request_id) + ")");
  }

  std::unique_lock<std::mutex> lock(pending->mtx);
  bool completed =
      pending->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                           [&]() {
                             return pending->state != PendingRequest::State::Waiting;
                           });
  if (!completed) {
    std::string op_name = pending->op;
    int32_t session_id = pending->session_id;
    int32_t session_count = pending->session_count;
    bool release_error_breaks = pending->error_breaks_suppressed;
    bool awaiting_ready = pending->awaiting_ready;
    int64_t responses_received = pending->responses_received;
    int64_t error_responses_at_dispatch = pending->error_responses_at_dispatch;
    pending->error_breaks_suppressed = false;
    pending->state = PendingRequest::State::Cancelled;
    lock.unlock();

    size_t pending_count = 0;
    {
      std::lock_guard<std::mutex> pending_lock(g_pending_mtx);
      pending_count = g_pending.size();
    }
    erase_pending(request_id, pending);

    int64_t last_response_ms = g_last_response_steady_ms.load();
    schedule_timeout_recovery(session_id, request_id);
    if (release_error_breaks)
      restore_error_breaks();

    GameTimeoutReceiptInput input;
    input.op = op_name;
    input.request_id = request_id;
    input.timeout_ms = timeout_ms;
    input.session_count = session_count;
    input.awaiting_ready = awaiting_ready;
    input.responses_received = responses_received;
    input.pending_count = static_cast<int64_t>(pending_count);
    input.last_channel_response_age_ms =
        last_response_ms >= 0 ? steady_now_ms() - last_response_ms : -1;
    input.late_response_for_request = late_results_contain(request_id);
    input.game_error_during_wait =
        g_game_error_responses.load() > error_responses_at_dispatch;
    input.late_results = late_results_snapshot();
    return build_game_timeout_receipt(input);
  }
  bool release_error_breaks = pending->error_breaks_suppressed;
  pending->error_breaks_suppressed = false;
  JV response = pending->response;
  lock.unlock();
  erase_pending(request_id, pending);
  if (release_error_breaks)
    restore_error_breaks();

  if (response.Contains("error")) {
    std::string error_text = response["error"].GetString();
    if (auto *ed = response.Find("error_details")) {
      if (ed->IsString()) {
        error_text +=
            "\n(game-side error details)\n" + ed->GetString();
      }
    }
    JV error = error_json(error_text);
    if (auto *code = response.Find("code"))
      error["code"] = *code;
    if (auto *se = response.Find("structured_error"))
      error["structured_error"] = *se;
    if (auto *dg = response.Find("diagnosis"))
      error["diagnosis"] = *dg;
    if (auto *hh = response.Find("hint"))
      error["hint"] = *hh;
    if (auto *re = response.Find("runtime_error"))
      error["runtime_error"] = *re;
    if (auto *erd = response.Find("error_details"))
      error["error_details"] = *erd;
    return error;
  }
  if (auto *result_p = util::find_result_or_data(response)) {
    JV result = *result_p;
    if (pending->op == std::string(GDA_OP_STATUS) && result.IsObject()) {

      if (!result.Contains(GDA_FIELD_HEALTHY)) {
        if (auto *la = result.Find(GDA_FIELD_LAST_ACTIVITY_MS)) {
          if (la->IsInt()) {
            result[GDA_FIELD_HEALTHY] =
                JV(la->GetInt() < GDA_HEALTHY_ACTIVITY_THRESHOLD_MS);
          }
        }
      }

      bool physics_stalled = false;
      {
        std::lock_guard<std::mutex> lock(g_pending_mtx);
        if (auto *pf = result.Find("physics_frame")) {
          if (pf->IsInt()) {
            auto now = std::chrono::steady_clock::now();
            if (g_last_status_time_valid) {
              int64_t elapsed_ms =
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - g_last_status_time)
                      .count();
              bool fps_positive = false;
              if (auto *fps = result.Find("fps")) {
                if (fps->IsInt())
                  fps_positive = fps->GetInt() > 0;
                else if (fps->IsDouble())
                  fps_positive = fps->GetDouble() > 0.0;
              }
              if (pf->GetInt() == g_last_status_physics_frame &&
                  elapsed_ms >= PHYSICS_STALL_DETECT_MS && fps_positive) {
                physics_stalled = true;
              }
            }
            g_last_status_physics_frame = pf->GetInt();
            g_last_status_time = now;
            g_last_status_time_valid = true;
          }
        }
      }
      result["physics_stalled"] = JV(physics_stalled);
    }
    if (response.Find("runtime_error")) {
      JV merged = result;
      if (!merged.IsObject()) {
        JV wrapped(JV::object_tag);
        wrapped["value"] = std::move(merged);
        merged = std::move(wrapped);
      }
      if (auto *re = response.Find("runtime_error"))
        merged["runtime_error"] = *re;
      if (auto *ed = response.Find("error_details"))
        merged["error_details"] = *ed;
      if (auto *se = response.Find("structured_error"))
        merged["structured_error"] = *se;
      return carry_transport_fields(response, std::move(merged));
    }
    return carry_transport_fields(response, std::move(result));
  }
  return error_json("unexpected game response (missing result)");
}

bool wait_pending_completion(int64_t request_id, int64_t timeout_ms) {
  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it != g_pending.end())
      pending = it->second;
  }
  if (!pending) {
    return false;
  }
  std::unique_lock<std::mutex> lock(pending->mtx);
  return pending->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                              [&]() {
                                return pending->state !=
                                       PendingRequest::State::Waiting;
                              });
}

bool payload_is_pending(const godot_autopilot::util::JsonValue &payload, int64_t *out_request_id) {
  if (!payload.IsObject())
    return false;
  auto *pending_id = payload.Find("__gda_pending");
  if (!pending_id || !pending_id->IsInt())
    return false;
  if (out_request_id)
    *out_request_id = pending_id->GetInt();
  return true;
}

bool payload_is_game_job(const godot_autopilot::util::JsonValue &payload, int64_t *out_job_id) {
  if (!payload.IsObject())
    return false;
  auto *job_id = payload.Find("__gda_job");
  if (!job_id || !job_id->IsInt())
    return false;
  if (out_job_id)
    *out_job_id = job_id->GetInt();
  return true;
}

bool discard_pending(int64_t request_id, std::string &out_detail) {
  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it == g_pending.end()) {
      out_detail = "no pending request found for request_id " +
                   std::to_string(request_id);
      monitor::state("discard_pending", "degraded", true);
      return false;
    }
    pending = it->second;
    g_pending.erase(it);
  }

  bool release_error_breaks = false;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state == PendingRequest::State::Waiting)
      pending->state = PendingRequest::State::Cancelled;
    release_error_breaks = pending->error_breaks_suppressed;
    pending->error_breaks_suppressed = false;
  }
  pending->cv.notify_all();
  if (release_error_breaks)
    restore_error_breaks();

  out_detail = "discarded pending request_id " + std::to_string(request_id) +
               " (batch_execute does not await async game ops)";
  LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                            "batch_execute discarded pending request_id " +
                                std::to_string(request_id) +
                                " (async game op response will be logged as a "
                                "late game response)");
  monitor::state("discard_pending", "discarded", true);
  return true;
}

godot_autopilot::util::JsonValue start_pending_op(const std::string &op,
                                const godot_autopilot::util::JsonValue &params,
                                bool suppress_error_breaks,
                                int64_t *out_request_id, bool wait_ready,
                                 std::optional<int32_t> session_id) {
  int64_t request_id = g_next_request_id.fetch_add(1);
  JV result = dispatch_game_request(request_id, op, params, suppress_error_breaks,
                                    wait_ready, session_id);
  if (result.Contains("error"))
    return result;
  if (out_request_id)
    *out_request_id = request_id;
  return result;
}

bool try_collect_response(int64_t request_id, godot_autopilot::util::JsonValue &out_response) {
  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it == g_pending.end())
      return false;
    pending = it->second;
  }
  bool release_error_breaks = false;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->state != PendingRequest::State::Completed)
      return false;
    out_response = pending->response;
    pending->state = PendingRequest::State::Cancelled;
    release_error_breaks = pending->error_breaks_suppressed;
    pending->error_breaks_suppressed = false;
    pending->cv.notify_all();
  }
  erase_pending(request_id, pending);
  if (release_error_breaks)
    restore_error_breaks();
  return true;
}

void cancel_game_op_request(int64_t request_id) {
  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it == g_pending.end())
      return;
    pending = it->second;
  }
  int32_t session_id = -1;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    session_id = pending->session_id;
  }
  schedule_timeout_recovery(session_id, request_id);
}

void set_editor_queue(godot_autopilot::CommandQueue *q) {
  {
    std::lock_guard<std::mutex> lock(g_editor_queue_mtx);
    g_editor_queue = q;
  }
  if (!q)
    cancel_all_pending("editor command queue closed while waiting for game response");
}

bool has_editor_queue() { return current_editor_queue() != nullptr; }

bool on_editor_main_thread() {
  CommandQueue *queue = current_editor_queue();
  return queue != nullptr && queue->is_main_thread();
}

void maybe_recover_break() {
  static std::unordered_map<int32_t, int> g_auto_continue_counts;

  godot::String env = godot::OS::get_singleton()->get_environment(
      godot::String(GDA_AUTO_CONTINUE_ENV.data()));
  std::string env_value = env.utf8().ptr();

  for (int32_t id : debugger_breaked_session_ids()) {
    if (env_flag_disabled(env_value))
      continue;

    int &count = g_auto_continue_counts[id];
    if (count >= GDA_AUTO_CONTINUE_MAX) {
      if (count == GDA_AUTO_CONTINUE_MAX) {
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::System,
            "auto continue suppressed for debugger break (session " +
                std::to_string(id) + "): reached limit " +
                std::to_string(GDA_AUTO_CONTINUE_MAX) +
                " — set GDA_AUTO_CONTINUE to a larger value to allow more");
      }
      continue;
    }
    count++;
    debugger_continue_session(id);
    LogSystem::instance().log(
        LogLevel::Info, LogCategory::System,
        "auto continue issued for debugger break (session " +
            std::to_string(id) + ", count " + std::to_string(count) + ")");
  }
}

godot_autopilot::util::JsonValue handle_gda_send(const std::string &op,
                               const godot_autopilot::util::JsonValue &params, int64_t timeout_ms,
                               bool suppress_error_breaks, bool wait_ready,
                                std::optional<int32_t> session_id) {
  int64_t request_id = g_next_request_id.fetch_add(1);
  JV result = dispatch_game_request(request_id, op, params, suppress_error_breaks,
                                    wait_ready, session_id);
  if (result.Contains("error"))
    return result;
  int64_t effective_timeout_ms = timeout_ms;
  if (effective_timeout_ms > GDA_MAX_GAME_OP_TIMEOUT_MS)
    effective_timeout_ms = GDA_MAX_GAME_OP_TIMEOUT_MS;
  if (effective_timeout_ms < 1)
    effective_timeout_ms = GDA_DEFAULT_TIMEOUT_MS;
  JV r(JV::object_tag);
  r["__gda_pending"] = JV(request_id);
  r["timeout_ms"] = JV(effective_timeout_ms + RESPONSE_GRACE_MS);
  return r;
}

void run_channel_self_check(int32_t p_session_id) {
  constexpr int64_t SELF_CHECK_TIMEOUT_MS = 1500;
  constexpr size_t ERROR_PREVIEW_CHARS = 256;
  auto started = std::chrono::steady_clock::now();
  JV pending =
      handle_gda_send("status", JV(JV::object_tag), SELF_CHECK_TIMEOUT_MS,
                      false, true, p_session_id);
  if (pending.Contains("error")) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "runtime channel self-check failed to send: " +
                                  pending["error"].GetString());
    return;
  }
  auto *request_id_p = pending.Find("__gda_pending");
  if (!request_id_p || !request_id_p->IsInt()) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Tools,
        "runtime channel self-check failed to send: missing pending request id");
    return;
  }
  int64_t request_id = request_id_p->GetInt();
  int64_t wait_ms = SELF_CHECK_TIMEOUT_MS + RESPONSE_GRACE_MS;
  if (auto *timeout_p = pending.Find("timeout_ms")) {
    if (timeout_p->IsInt() && timeout_p->GetInt() > 0)
      wait_ms = timeout_p->GetInt();
  }
  try {
    std::thread([p_session_id, request_id, wait_ms, started]() {
      JV response = wait_pending_response(request_id, wait_ms);
      int64_t elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - started)
              .count();
      if (response.Contains("error")) {
        std::string error_text = response["error"].GetString();
        if (error_text.size() > ERROR_PREVIEW_CHARS)
          error_text.resize(ERROR_PREVIEW_CHARS);
        LogSystem::instance().log(
            LogLevel::Warning, LogCategory::Tools,
            "runtime channel self-check failed (session " +
                std::to_string(p_session_id) + "): " + error_text);
        return;
      }
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "runtime channel self-check ok (session " +
              std::to_string(p_session_id) + ", status round-trip " +
              std::to_string(elapsed_ms) + " ms)");
    }).detach();
  } catch (const std::exception &ex) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Tools,
        std::string("runtime channel self-check failed to start waiter: ") +
            ex.what());
  }
}

void on_session_ready(int32_t session_id) {
  struct ReplayItem {
    std::shared_ptr<PendingRequest> pending;
    int64_t request_id = 0;
    std::string op;
    std::string payload;
    std::optional<int32_t> requested_session_id;
    bool suppress_error_breaks = false;
  };
  std::vector<ReplayItem> replay_items;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    for (auto &entry : g_pending) {
      const std::shared_ptr<PendingRequest> &pending = entry.second;
      std::lock_guard<std::mutex> pending_lock(pending->mtx);
      if (!pending->awaiting_ready ||
          pending->state != PendingRequest::State::Waiting)
        continue;
      if (pending->requested_session_id.has_value() &&
          *pending->requested_session_id != session_id)
        continue;
      ReplayItem item;
      item.pending = pending;
      item.request_id = entry.first;
      item.op = pending->op;
      item.payload = pending->payload;
      item.requested_session_id = pending->requested_session_id;
      item.suppress_error_breaks = pending->suppress_error_breaks;
      replay_items.push_back(std::move(item));
    }
  }
  if (replay_items.empty())
    return;
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Transport,
      "gda:ready received (session " + std::to_string(session_id) + "): " +
          std::to_string(replay_items.size()) +
          " queued game op(s) to replay");
  const auto active_sessions = debugger_active_session_ids();
  const auto ready_sessions = debugger_request_ready_session_ids();
  for (ReplayItem &item : replay_items) {
    const SessionSelectionResult selection = select_game_session(
        ready_sessions, active_sessions, item.requested_session_id);
    if (selection.kind == SessionSelection::Ambiguous) {
      JV error = session_selection_error(
          "ambiguous_session",
          "multiple game sessions became ready; pass session_id to select one",
          ready_sessions);
      {
        std::lock_guard<std::mutex> lock(item.pending->mtx);
        item.pending->state = PendingRequest::State::Completed;
        item.pending->response = std::move(error);
      }
      item.pending->cv.notify_all();
      continue;
    }
    if (selection.kind == SessionSelection::InvalidSession) {
      JV error = session_selection_error(
          "invalid_session",
          "game session_id " + std::to_string(*item.requested_session_id) +
              " is no longer active",
          active_sessions);
      {
        std::lock_guard<std::mutex> lock(item.pending->mtx);
        item.pending->state = PendingRequest::State::Completed;
        item.pending->response = std::move(error);
      }
      item.pending->cv.notify_all();
      continue;
    }
    if (selection.kind != SessionSelection::Selected)
      continue;
    if (send_pending_to_session(item.request_id, item.op, item.payload,
                                selection.session_id,
                                item.suppress_error_breaks, item.pending)) {
      {
        std::lock_guard<std::mutex> lock(item.pending->mtx);
        item.pending->awaiting_ready = false;
      }
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Transport,
          "queued game op \"" + item.op + "\" replayed after gda:ready " +
              "(request_id " + std::to_string(item.request_id) +
              ", session " + std::to_string(selection.session_id) + ")");
      continue;
    }
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "queued game op \"" + item.op +
            "\" could not be replayed; waiting for the next gda:ready "
            "(request_id " +
            std::to_string(item.request_id) + ")");
  }
}

godot_autopilot::util::JsonValue finalize_capture_response(const godot_autopilot::util::JsonValue &pending_result,
                                         const godot_autopilot::util::JsonValue &options) {
  if (pending_result.Contains("error"))
    return pending_result;
  if (!pending_result.IsObject())
    return error_json("unexpected capture response");
  auto *path_p = pending_result.Find("path");
  if (!path_p || !path_p->IsString())
    return error_json("capture response missing path");

  CommandQueue *queue = current_editor_queue();
  if (!queue)
    return error_json("editor command queue not initialized");
  auto state = std::make_shared<MainThreadTaskState>();
  dispatch::BoundedWaitOutcome outcome = dispatch::run_bounded_on_main_thread(
      *queue, "capture file read", state,
      [state, pending_result, options](MainThreadTaskState &st) {
        st.result = read_capture_file(pending_result, options);
      },
      GDA_DISPATCH_WAIT_DEFAULT_MS);
  if (outcome.timed_out) {
    return dispatch::main_thread_timeout_result("capture file read",
                                                outcome.waited_ms,
                                                outcome.cancelled);
  }
  if (!outcome.ok)
    return error_json(outcome.error);
  return state->result;
}

void handle_game_response(const std::string &json_str) {
  g_last_response_steady_ms.store(steady_now_ms());
  maybe_recover_break();
  JV parsed = JV::Parse(json_str);
  if (!parsed.IsObject()) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Tools,
        "game response discarded: payload is not a JSON object (payload: " +
            summarize_payload(json_str) + ")");
    monitor::state("game_response", "discarded", true);
    return;
  }
  if (parsed.Contains("runtime_error")) {
    error_watermark::record_error();
    g_game_error_responses.fetch_add(1);
  }
  auto *rid = parsed.Find("request_id");
  if (!rid || !rid->IsInt()) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Tools,
        "game response discarded: missing or non-integer request_id (payload: " +
            summarize_payload(json_str) + ")");
    monitor::state("game_response", "discarded", true);
    return;
  }
  int64_t request_id = rid->GetInt();

  std::shared_ptr<PendingRequest> pending;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    auto it = g_pending.find(request_id);
    if (it != g_pending.end())
      pending = it->second;
  }
  if (!pending) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "late game response discarded (request_id " +
                                  std::to_string(request_id) +
                                  ", editor wait already timed out)");
    monitor::state("game_response_late", "discarded", true);
    record_late_result(request_id, parsed);
    return;
  }

  bool late_response = false;
  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    ++pending->responses_received;
    if (pending->state != PendingRequest::State::Waiting) {
      late_response = true;
    } else {
      pending->response = std::move(parsed);
      pending->state = PendingRequest::State::Completed;
    }
  }
  if (late_response) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "late game response discarded (request_id " +
                                  std::to_string(request_id) + ")");
    monitor::state("game_response_late", "discarded", true);
    record_late_result(request_id, parsed);
    return;
  }
  pending->cv.notify_all();
}

} // namespace runtime_ops

CommandQueue &get_editor_queue() {
  return *runtime_ops::current_editor_queue();
}

} // namespace godot_autopilot
