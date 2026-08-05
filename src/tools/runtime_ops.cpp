#include "runtime_ops.hpp"

#include "core/command_queue.hpp"
#include "core/log_system.hpp"
#include "runtime/gsd_protocol.hpp"
#include "tools/capture_ops.hpp"
#include "tools/debugger_ops.hpp"
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_self_driving {
namespace runtime_ops {

namespace {

using JV = mcp::JsonValue;

constexpr int64_t DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t MAX_TIMEOUT_MS = 30000;
constexpr int64_t RESPONSE_GRACE_MS = 2000;
constexpr int64_t HEALTHY_ACTIVITY_THRESHOLD_MS = 3000;

bool env_flag_disabled(const std::string& value) {
    return value == "0" || value == "false";
}

JV error_json(const std::string& message) {
    JV e(JV::object_tag);
    e["error"] = JV(message);
    return e;
}

int64_t extract_timeout(const JV& args) {
    int64_t timeout = DEFAULT_TIMEOUT_MS;
    if (auto* tp = args.Find("timeout_ms")) {
        if (tp->IsInt() && tp->GetInt() > 0) timeout = tp->GetInt();
    }
    if (timeout > MAX_TIMEOUT_MS) timeout = MAX_TIMEOUT_MS;
    return timeout;
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

bool is_session_ready(godot::DebugCapturePlugin* plugin, int32_t session_id) {
    return std::find(plugin->ready_session_ids_.begin(), plugin->ready_session_ids_.end(),
               session_id) != plugin->ready_session_ids_.end();
}

// Runs on the editor main thread (via CommandQueue). Registers a pending slot
// and broadcasts "gsd:request" to every active debug session that reported ready.
JV send_request(int64_t request_id, const std::string& op, const JV& params) {
    auto* plugin = ::godot::DebugCapturePlugin::get_instance();
    if (!plugin) return error_json("debugger capture plugin not initialized");

    int32_t used_session_id = -1;
    std::vector<godot::Ref<godot::EditorDebuggerSession>> active_sessions;
    for (int32_t id : plugin->session_ids_) {
        auto session = plugin->get_session(id);
        if (!(session.is_valid() && session->is_active())) continue;
        if (!is_session_ready(plugin, id)) continue;
        active_sessions.push_back(session);
        if (used_session_id < 0) used_session_id = id;
    }
    if (active_sessions.empty()) {
        return error_json("game not ready: the game process has not reported gsd ready yet — wait a moment after play, or verify the game project loads the godot-self-driving extension");
    }

    auto pending = std::make_shared<PendingRequest>();
    pending->op = op;
    pending->session_id = used_session_id;
    {
        std::lock_guard<std::mutex> lock(g_pending_mtx);
        g_pending[request_id] = pending;
    }

    JV payload(JV::object_tag);
    payload[GSD_FIELD_REQUEST_ID] = JV(request_id);
    payload[GSD_FIELD_OP] = JV(op);
    payload[GSD_FIELD_PARAMS] = params;
    godot::Array arr;
    arr.push_back(godot::String(payload.Dump().c_str()));
    for (auto& session : active_sessions) {
        session->send_message(godot::String(std::string(GSD_MSG_REQUEST).c_str()), arr);
    }

    JV r(JV::object_tag);
    r["request_id"] = JV(request_id);
    r["result"] = JV("sent");
    return r;
}

// Blocks the calling (HTTP) thread until the game responds or the timeout hits.
JV wait_for_response(int64_t request_id, int64_t timeout_ms) {
    std::shared_ptr<PendingRequest> pending;
    {
        std::lock_guard<std::mutex> lock(g_pending_mtx);
        auto it = g_pending.find(request_id);
        if (it != g_pending.end()) pending = it->second;
    }
    if (!pending) {
        return error_json("pending request not found (request_id " + std::to_string(request_id) + ")");
    }

    std::unique_lock<std::mutex> lock(pending->mtx);
    bool completed = pending->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [&]() { return pending->done; });
    if (!completed) {
        std::string op_name = pending->op.empty() ? "?" : pending->op;
        int32_t session_id = pending->session_id;
        {
            std::lock_guard<std::mutex> gl(g_pending_mtx);
            g_pending.erase(request_id);
        }
        if (session_id >= 0) {
            get_editor_queue().submit([session_id, request_id]() {
                auto* plugin = ::godot::DebugCapturePlugin::get_instance();
                if (!plugin) return;
                auto session = plugin->get_session(session_id);
                if (!(session.is_valid() && session->is_active())) return;
                JV cancel_body(JV::object_tag);
                cancel_body[GSD_FIELD_REQUEST_ID] = JV(request_id);
                cancel_body[GSD_FIELD_OP] = JV(std::string(GSD_OP_CANCEL));
                cancel_body[GSD_FIELD_PARAMS] = JV(JV::object_tag);
                godot::Array arr;
                arr.push_back(godot::String(cancel_body.Dump().c_str()));
                session->send_message(godot::String(std::string(GSD_MSG_REQUEST).c_str()), arr);
            });
        }
        return error_json("game op \"" + op_name + "\" timed out after " + std::to_string(timeout_ms) + " ms (request_id " + std::to_string(request_id) + ") — the request was sent to the active debug session(s) but no response arrived; the game process may be paused or physics-frozen (query game_status), or the game project may not load the godot-self-driving extension");
    }
    {
        std::lock_guard<std::mutex> gl(g_pending_mtx);
        g_pending.erase(request_id);
    }

    JV response = pending->response;
    if (response.Contains("error")) {
        return error_json(response["error"].GetString());
    }
    if (auto* result_p = response.Find(GSD_FIELD_RESULT)) {
        JV result = *result_p;
        if (pending->op == std::string(GSD_OP_STATUS) && result.IsObject()) {
            if (auto* la = result.Find(GSD_FIELD_LAST_ACTIVITY_MS)) {
                if (la->IsInt()) {
                    result[GSD_FIELD_HEALTHY] = JV(la->GetInt() < HEALTHY_ACTIVITY_THRESHOLD_MS);
                }
            }
        }
        return result;
    }
    return error_json("unexpected game response (missing result)");
}

} // namespace

// Called on the editor main thread each time a game message arrives. If the
// game is stuck in a debugger error-break, issue an engine "continue" command
// (bounded per session by GSD_AUTO_CONTINUE_MAX) so physics resumes.
void maybe_recover_break() {
    static std::unordered_map<int32_t, int> g_auto_continue_counts;
    auto* plugin = ::godot::DebugCapturePlugin::get_instance();
    if (!plugin) return;

    godot::String env = godot::OS::get_singleton()->get_environment(
        godot::String(GSD_AUTO_CONTINUE_ENV.data()));
    std::string env_value = env.utf8().ptr();

    for (int32_t id : plugin->session_ids_) {
        auto session = plugin->get_session(id);
        if (!(session.is_valid() && session->is_active() && session->is_breaked())) continue;
        if (env_flag_disabled(env_value)) continue;

        int& count = g_auto_continue_counts[id];
        if (count >= GSD_AUTO_CONTINUE_MAX) {
            if (count == GSD_AUTO_CONTINUE_MAX) {
                LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                    "auto continue suppressed for debugger break (session " + std::to_string(id) +
                    "): reached limit " + std::to_string(GSD_AUTO_CONTINUE_MAX) +
                    " — set GSD_AUTO_CONTINUE to a larger value to allow more");
            }
            continue;
        }
        count++;
        session->send_message(godot::String("continue"), godot::Array());
        LogSystem::instance().log(LogLevel::Info, LogCategory::System,
            "auto continue issued for debugger break (session " + std::to_string(id) +
            ", count " + std::to_string(count) + ")");
    }
}

mcp::JsonValue handle_gsd_send(const std::string& op, const mcp::JsonValue& params, int64_t timeout_ms) {
    int64_t request_id = g_next_request_id.fetch_add(1);
    JV result;
    try {
        if (get_editor_queue().is_main_thread()) {
            result = send_request(request_id, op, params);
        } else {
            result = get_editor_queue().submit(
                [&]() { return send_request(request_id, op, params); }).get();
        }
    } catch (const std::exception& ex) {
        return error_json(std::string("failed to submit request to main thread: ") + ex.what());
    }
    if (result.Contains("error")) return result;
    JV r(JV::object_tag);
    r["__gsd_pending"] = JV(request_id);
    r["timeout_ms"] = JV(timeout_ms + RESPONSE_GRACE_MS);
    return r;
}

mcp::JsonValue wait_pending_response(int64_t request_id, int64_t timeout_ms) {
    return wait_for_response(request_id, timeout_ms);
}

mcp::JsonValue finalize_capture_response(const mcp::JsonValue& pending_result) {
    if (pending_result.Contains("error")) return pending_result;
    if (!pending_result.IsObject()) return error_json("unexpected capture response");
    auto* path_p = pending_result.Find("path");
    if (!path_p || !path_p->IsString()) return error_json("capture response missing path");

    try {
        return get_editor_queue().submit([pending_result]() -> JV {
            std::string path = pending_result["path"].GetString();
            godot::Ref<godot::FileAccess> file =
                godot::FileAccess::open(godot::String(path.c_str()), godot::FileAccess::READ);
            if (file.is_null() || !file->is_open()) {
                return error_json("failed to read captured file: " + path);
            }
            uint64_t length = file->get_length();
            godot::PackedByteArray bytes = file->get_buffer(static_cast<int64_t>(length));
            file->close();
            if (bytes.size() <= 0) {
                return error_json("captured file is empty: " + path);
            }
            std::string b64 = capture_ops::base64_encode(bytes.ptrw(), static_cast<size_t>(bytes.size()));

            JV r(JV::object_tag);
            r["data"] = JV(b64);
            r["format"] = JV("png");
            if (auto* w = pending_result.Find("width")) r["width"] = *w;
            if (auto* h = pending_result.Find("height")) r["height"] = *h;
            r["path"] = JV(path);
            LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_capture completed");
            return r;
        }).get();
    } catch (const std::exception& ex) {
        return error_json(std::string("failed to read captured file on main thread: ") + ex.what());
    }
}

namespace {

void copy_optional(const JV& from, JV& to, const char* key) {
    if (auto* v = from.Find(key)) to[key] = *v;
}

constexpr const char* INPUT_PARAM_WHITELIST[] = {
    "type", "keycode", "pressed", "button_index", "position",
    "action", "duration_ms", "mode", "timeout_ms",
};

bool is_input_param_allowed(const std::string& key) {
    for (const char* allowed : INPUT_PARAM_WHITELIST) {
        if (key == allowed) return true;
    }
    return false;
}

} // namespace

void handle_game_response(const std::string& json_str) {
    maybe_recover_break();
    JV parsed = JV::Parse(json_str);
    if (!parsed.IsObject()) return;
    auto* rid = parsed.Find("request_id");
    if (!rid || !rid->IsInt()) return;
    int64_t request_id = rid->GetInt();

    std::shared_ptr<PendingRequest> pending;
    {
        std::lock_guard<std::mutex> lock(g_pending_mtx);
        auto it = g_pending.find(request_id);
        if (it != g_pending.end()) pending = it->second;
    }
    if (!pending) {
        LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
            "late game response discarded (request_id " + std::to_string(request_id) + ", editor wait already timed out)");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(pending->mtx);
        if (pending->done) return;
        pending->response = std::move(parsed);
        pending->done = true;
    }
    pending->cv.notify_all();
}

mcp::JsonValue handle_game_status(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_status called");
    JV params(JV::object_tag);
    return handle_gsd_send("status", params, extract_timeout(args));
}

mcp::JsonValue handle_game_eval(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_eval called");
    auto* action_p = args.Find("action");
    if (!action_p || !action_p->IsString()) {
        return error_json("missing required parameter: action (script|get_property|set_property|call_method)");
    }
    JV params(JV::object_tag);
    params["action"] = *action_p;
    copy_optional(args, params, "node_path");
    copy_optional(args, params, "property");
    copy_optional(args, params, "value");
    copy_optional(args, params, "method");
    copy_optional(args, params, "args");
    copy_optional(args, params, "source_code");
    copy_optional(args, params, "persist");
    copy_optional(args, params, "persist_name");
    copy_optional(args, params, "timeout_ms");
    return handle_gsd_send("eval", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_input called");
    auto* type_p = args.Find("type");
    if (!type_p || !type_p->IsString()) {
        return error_json("missing required parameter: type (key|mouse_button|action)");
    }
    JV params(JV::object_tag);
    params["type"] = *type_p;
    copy_optional(args, params, "keycode");
    copy_optional(args, params, "pressed");
    copy_optional(args, params, "button_index");
    copy_optional(args, params, "position");
    copy_optional(args, params, "action");
    copy_optional(args, params, "duration_ms");
    copy_optional(args, params, "mode");

    std::vector<std::string> ignored;
    if (args.IsObject()) {
        for (const auto& entry : args.GetObject()) {
            if (!is_input_param_allowed(entry.first)) {
                ignored.push_back(entry.first);
            }
        }
    }

    JV result = handle_gsd_send("input", params, extract_timeout(args));
    if (!result.Contains("error") && !ignored.empty()) {
        JV ignored_arr(JV::array_tag);
        for (const auto& key : ignored) ignored_arr.PushBack(JV(key));
        result["ignored_params"] = std::move(ignored_arr);
        std::string warning = "ignored unknown parameters: ";
        for (size_t i = 0; i < ignored.size(); i++) {
            if (i > 0) warning += ", ";
            warning += ignored[i];
        }
        result["warning"] = JV(warning);
    }
    return result;
}

mcp::JsonValue handle_game_input_wait(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_input_wait called");
    auto* action_p = args.Find("action");
    if (!action_p || !action_p->IsString()) {
        return error_json("missing required parameter: action");
    }
    JV params(JV::object_tag);
    params["action"] = *action_p;
    copy_optional(args, params, "state");
    copy_optional(args, params, "inject");
    copy_optional(args, params, "timeout_ms");
    return handle_gsd_send("input_wait", params, extract_timeout(args));
}

mcp::JsonValue handle_game_input_status(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_input_status called");
    auto* action_p = args.Find("action");
    if (!action_p || !action_p->IsString()) {
        return error_json("missing required parameter: action");
    }
    JV params(JV::object_tag);
    params["action"] = *action_p;
    JV result = handle_gsd_send("input_status", params, extract_timeout(args));
    if (!result.Contains("error")) {
        std::string recent_errors = debugger_ops::capture_get_errors_text(5);
        if (!recent_errors.empty()) {
            result["recent_engine_errors"] = JV(recent_errors);
        }
    }
    return result;
}

mcp::JsonValue handle_game_capture(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "game_capture called");
    JV params(JV::object_tag);
    return handle_gsd_send("capture", params, extract_timeout(args));
}

} // namespace runtime_ops
} // namespace godot_self_driving
