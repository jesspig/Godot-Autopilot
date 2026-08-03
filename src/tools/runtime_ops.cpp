#include "runtime_ops.hpp"

#include "core/command_queue.hpp"
#include "core/log_system.hpp"
#include "tools/capture_ops.hpp"
#include "tools/debugger_ops.hpp"
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace godot_self_driving {
namespace runtime_ops {

namespace {

using JV = mcp::JsonValue;

constexpr int64_t DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t MAX_TIMEOUT_MS = 30000;
constexpr int64_t RESPONSE_GRACE_MS = 2000;

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
};

std::mutex g_pending_mtx;
std::map<int64_t, std::shared_ptr<PendingRequest>> g_pending;

// Runs on the editor main thread (via CommandQueue). Registers a pending slot
// and broadcasts "gsd:request" to every active debug session.
JV send_request(int64_t request_id, const std::string& op, const JV& params) {
    auto* plugin = ::godot::DebugCapturePlugin::get_instance();
    if (!plugin) return error_json("debugger capture plugin not initialized");

    std::vector<godot::Ref<godot::EditorDebuggerSession>> active_sessions;
    for (int32_t id : plugin->session_ids_) {
        auto session = plugin->get_session(id);
        if (session.is_valid() && session->is_active()) {
            active_sessions.push_back(session);
        }
    }
    if (active_sessions.empty()) {
        return error_json("no active debug session — start the game with editor_play_current_scene");
    }

    auto pending = std::make_shared<PendingRequest>();
    pending->op = op;
    {
        std::lock_guard<std::mutex> lock(g_pending_mtx);
        g_pending[request_id] = pending;
    }

    JV payload(JV::object_tag);
    payload["request_id"] = JV(request_id);
    payload["op"] = JV(op);
    payload["params"] = params;
    godot::Array arr;
    arr.push_back(godot::String(payload.Dump().c_str()));
    for (auto& session : active_sessions) {
        session->send_message(godot::String("gsd:request"), arr);
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
        std::lock_guard<std::mutex> gl(g_pending_mtx);
        g_pending.erase(request_id);
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
    if (auto* result_p = response.Find("result")) {
        return *result_p;
    }
    return error_json("unexpected game response (missing result)");
}

} // namespace

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
