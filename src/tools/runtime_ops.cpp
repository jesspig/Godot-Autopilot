#include "runtime_ops.hpp"

#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "core/log_system.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/capture_ops.hpp"
#include "tools/debugger_access.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace runtime_ops {

using godot_autopilot::util::error_json;

namespace {

using JV = mcp::JsonValue;

CommandQueue *g_editor_queue = nullptr;

constexpr int64_t RESPONSE_GRACE_MS = 2000;
constexpr int64_t PHYSICS_STALL_DETECT_MS = 1000;

int64_t g_last_status_physics_frame = -1;
std::chrono::steady_clock::time_point g_last_status_time;
bool g_last_status_time_valid = false;

bool env_flag_disabled(const std::string &value) {
  return value == "0" || value == "false";
}

std::atomic<int64_t> g_next_request_id{1};

struct PendingRequest {
  std::mutex mtx;
  std::condition_variable cv;
  bool done = false;
  JV response;
  std::string op;
  int32_t session_id = -1;
};

std::mutex g_pending_mtx;
std::map<int64_t, std::shared_ptr<PendingRequest>> g_pending;

JV send_request(int64_t request_id, const std::string &op, const JV &params) {
  if (!debugger_capture_initialized())
    return error_json("debugger capture plugin not initialized");

  JV payload(JV::object_tag);
  payload[GDA_FIELD_REQUEST_ID] = JV(request_id);
  payload[GDA_FIELD_OP] = JV(op);
  payload[GDA_FIELD_PARAMS] = params;

  int32_t used_session_id = -1;
  if (!debugger_broadcast_request(payload.Dump(), &used_session_id)) {
    return error_json("game not ready: the game process has not reported gda "
                      "ready yet — wait a moment after play, or verify the "
                      "game project loads the godot-autopilot extension");
  }

  auto pending = std::make_shared<PendingRequest>();
  pending->op = op;
  pending->session_id = used_session_id;
  {
    std::lock_guard<std::mutex> lock(g_pending_mtx);
    g_pending[request_id] = pending;
  }

  JV r(JV::object_tag);
  r["request_id"] = JV(request_id);
  r["result"] = JV("sent");
  return r;
}

JV wait_for_response(int64_t request_id, int64_t timeout_ms) {
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
                           [&]() { return pending->done; });
  if (!completed) {
    std::string op_name = pending->op.empty() ? "?" : pending->op;
    int32_t session_id = pending->session_id;
    {
      std::lock_guard<std::mutex> gl(g_pending_mtx);
      g_pending.erase(request_id);
    }
    if (session_id >= 0 && has_editor_queue()) {
      get_editor_queue().submit([session_id, request_id]() {
        debugger_send_cancel(session_id, request_id);
      });
    }
    return error_json("game op \"" + op_name + "\" timed out after " +
                      std::to_string(timeout_ms) + " ms (request_id " +
                      std::to_string(request_id) +
                      ") — the request was sent to the active debug session(s) "
                      "but no response arrived; the game process may be paused "
                      "or physics-frozen (query game_status), or the game "
                      "project may not load the godot-autopilot extension");
  }
  {
    std::lock_guard<std::mutex> gl(g_pending_mtx);
    g_pending.erase(request_id);
  }

  JV response = pending->response;
  if (response.Contains("error")) {
    return error_json(response["error"].GetString());
  }
  if (auto *result_p = response.Find(GDA_FIELD_RESULT)) {
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
    return result;
  }
  return error_json("unexpected game response (missing result)");
}

} // namespace

void set_editor_queue(godot_autopilot::CommandQueue *q) {
  g_editor_queue = q;
}

bool has_editor_queue() { return g_editor_queue != nullptr; }

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

mcp::JsonValue handle_gda_send(const std::string &op,
                               const mcp::JsonValue &params,
                               int64_t timeout_ms) {
  int64_t request_id = g_next_request_id.fetch_add(1);
  JV result;
  try {
    if (!has_editor_queue())
      return error_json("editor command queue not initialized");
    if (get_editor_queue().is_main_thread()) {
      result = send_request(request_id, op, params);
    } else {
      result =
          get_editor_queue()
              .submit([&]() { return send_request(request_id, op, params); })
              .get();
    }
  } catch (const std::exception &ex) {
    return error_json(std::string("failed to submit request to main thread: ") +
                      ex.what());
  }
  if (result.Contains("error"))
    return result;
  JV r(JV::object_tag);
  r["__gda_pending"] = JV(request_id);
  r["timeout_ms"] = JV(timeout_ms + RESPONSE_GRACE_MS);
  return r;
}

mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms) {
  return wait_for_response(request_id, timeout_ms);
}

mcp::JsonValue finalize_capture_response(const mcp::JsonValue &pending_result) {
  if (pending_result.Contains("error"))
    return pending_result;
  if (!pending_result.IsObject())
    return error_json("unexpected capture response");
  auto *path_p = pending_result.Find("path");
  if (!path_p || !path_p->IsString())
    return error_json("capture response missing path");

  try {
    if (!has_editor_queue())
      return error_json("editor command queue not initialized");
    return get_editor_queue()
        .submit([pending_result]() -> JV {
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
          std::string b64 = capture_ops::base64_encode(
              bytes.ptrw(), static_cast<size_t>(bytes.size()));

          JV r(JV::object_tag);
          r["data"] = JV(b64);
          r["format"] = JV("png");
          if (auto *w = pending_result.Find("width"))
            r["width"] = *w;
          if (auto *h = pending_result.Find("height"))
            r["height"] = *h;
          r["path"] = JV(path);
          LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                                    "game_capture completed");
          return r;
        })
        .get();
  } catch (const std::exception &ex) {
    return error_json(
        std::string("failed to read captured file on main thread: ") +
        ex.what());
  }
}

void handle_game_response(const std::string &json_str) {
  maybe_recover_break();
  JV parsed = JV::Parse(json_str);
  if (!parsed.IsObject())
    return;
  auto *rid = parsed.Find("request_id");
  if (!rid || !rid->IsInt())
    return;
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
    return;
  }

  {
    std::lock_guard<std::mutex> lock(pending->mtx);
    if (pending->done)
      return;
    pending->response = std::move(parsed);
    pending->done = true;
  }
  pending->cv.notify_all();
}

} // namespace runtime_ops

CommandQueue &get_editor_queue() { return *runtime_ops::g_editor_queue; }

} // namespace godot_autopilot
