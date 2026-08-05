#include "game_bridge.hpp"

#include "gsd_protocol.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/readback_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <mcp/JsonValue.hpp>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_self_driving {
namespace runtime {
namespace game_bridge {

namespace {

using JV = mcp::JsonValue;

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

JV error_result(const std::string& message) {
    JV r(JV::object_tag);
    r["error"] = JV(message);
    return r;
}

JV ok_result(JV result) {
    JV r(JV::object_tag);
    r["result"] = std::move(result);
    return r;
}

godot::SceneTree* get_scene_tree() {
    auto* engine = godot::Engine::get_singleton();
    if (!engine) return nullptr;
    return godot::Object::cast_to<godot::SceneTree>(engine->get_main_loop());
}

godot::Node* resolve_node(const std::string& node_path) {
    godot::SceneTree* tree = get_scene_tree();
    if (!tree) return nullptr;
    if (node_path.empty()) return tree->get_current_scene();
    godot::NodePath path(godot::String(node_path.c_str()));
    if (auto* current = tree->get_current_scene()) {
        if (auto* node = current->get_node_or_null(path)) return node;
    }
    return tree->get_root()->get_node_or_null(path);
}

// ── 异步辅助对象 ──
// send_response 前向声明（定义在 dispatch 段），供 watcher / awaiter 回调使用
void send_response(int64_t request_id, JV body);

// 协议常量（std::string_view）转 godot::String，用于消息名等字符串参数
godot::String gsd_string(const std::string_view& sv) {
    return godot::String(std::string(sv).c_str());
}

// ── 活动时间戳与挂起操作取消注册表 ──
// 每次收到 gsd 请求刷新，供 ping / status 报告游戏进程活跃度
uint64_t g_last_activity_ms = 0;
// 异步 op（input_wait / eval await）按 request_id 注册取消回调，
// cancel op 在主线程查找并调用。全部在主线程上下文操作，无需锁。
std::unordered_map<int64_t, std::function<void()>> g_cancel_handlers;

void register_cancel_handler(int64_t request_id, std::function<void()> handler) {
    g_cancel_handlers[request_id] = std::move(handler);
}

void unregister_cancel_handler(int64_t request_id) {
    g_cancel_handlers.erase(request_id);
}

// input_wait 观测器：挂到 root，在 _physics_process 中轮询输入状态，命中或超时即响应并自毁
class GameBridgeInputWatcher : public godot::Node {
    GDCLASS(GameBridgeInputWatcher, godot::Node)

    int64_t request_id_ = 0;
    godot::StringName action_;
    int state_kind_ = 0; // 0=just_pressed 1=just_released 2=pressed
    double timeout_sec_ = 2.0;
    double elapsed_ = 0.0;
    uint64_t start_ticks_ = 0;
    bool finished_ = false;

protected:
    static void _bind_methods() {}

public:
    void setup(int64_t request_id, const godot::StringName& action, int state_kind, double timeout_sec) {
        request_id_ = request_id;
        action_ = action;
        state_kind_ = state_kind;
        timeout_sec_ = timeout_sec;
        set_process_mode(godot::Node::PROCESS_MODE_ALWAYS);
        set_physics_process(true);
        start_ticks_ = godot::Time::get_singleton()->get_ticks_msec();
    }

    // cancel op 触发的取消：不自行响应，由 cancel op 返回结果
    void cancel() {
        if (finished_) return;
        finished_ = true;
        unregister_cancel_handler(request_id_);
        queue_free();
    }

    void _physics_process(double delta) override {
        (void)delta;
        if (finished_) return;
        elapsed_ = static_cast<double>(godot::Time::get_singleton()->get_ticks_msec() - start_ticks_) / 1000.0;
        auto* input = godot::Input::get_singleton();
        bool hit = false;
        if (input) {
            if (state_kind_ == 0) hit = input->is_action_just_pressed(action_);
            else if (state_kind_ == 1) hit = input->is_action_just_released(action_);
            else hit = input->is_action_pressed(action_);
        }
        if (hit) {
            finished_ = true;
            unregister_cancel_handler(request_id_);
            JV body(JV::object_tag);
            body["result"] = JV("matched");
            body[GSD_FIELD_MATCHED_AT_PHYSICS_FRAME]
                = JV(static_cast<int64_t>(godot::Engine::get_singleton()->get_physics_frames()));
            send_response(request_id_, std::move(body));
            queue_free();
        } else if (elapsed_ >= timeout_sec_) {
            finished_ = true;
            unregister_cancel_handler(request_id_);
            const char* state_name = state_kind_ == 0 ? "just_pressed"
                : (state_kind_ == 1 ? "just_released" : "pressed");
            send_response(request_id_, error_result("timeout waiting for "
                + std::string(state_name) + " on action " + to_std(godot::String(action_))));
            queue_free();
        }
    }
};

// 物理帧计数定时释放注入器：帧数到点后按原参数补发 pressed=false 的事件，然后自毁。
// 保持模式（keep_pressed）：到期前每物理帧重发 pressed=true 并 flush，维持按下状态
class GameBridgeDelayedRelease : public godot::Node {
    GDCLASS(GameBridgeDelayedRelease, godot::Node)

    int type_ = 0; // 0=key 1=mouse_button 2=action
    godot::Key key_ = godot::KEY_NONE;
    int64_t button_index_ = 0;
    godot::Vector2 position_;
    bool has_position_ = false;
    godot::StringName action_;
    bool mode_api_ = false;
    bool keep_pressed_ = false;
    uint64_t start_physics_frame_ = 0;
    int64_t duration_frames_ = 0;

protected:
    static void _bind_methods() {}

public:
    void setup(int type, const godot::Key& key, int64_t button_index,
               const godot::Vector2& position, bool has_position,
               const godot::StringName& action, bool mode_api, double delay_sec,
               bool keep_pressed) {
        type_ = type;
        key_ = key;
        button_index_ = button_index;
        position_ = position;
        has_position_ = has_position;
        action_ = action;
        mode_api_ = mode_api;
        keep_pressed_ = keep_pressed;
        godot::SceneTree* tree = get_scene_tree();
        if (!tree) {
            memdelete(this);
            return;
        }
        tree->get_root()->add_child(this);
        start_physics_frame_ = godot::Engine::get_singleton()->get_physics_frames();
        duration_frames_ = static_cast<int64_t>(delay_sec * godot::Engine::get_singleton()->get_physics_ticks_per_second());
        set_physics_process(true);
    }

    void _physics_process(double delta) override {
        (void)delta;
        if (godot::Engine::get_singleton()->get_physics_frames() - start_physics_frame_
                >= static_cast<uint64_t>(duration_frames_)) {
            do_release();
            queue_free();
        } else if (keep_pressed_) {
            do_press();
        }
    }

private:
    void do_press() {
        auto* input = godot::Input::get_singleton();
        if (input) {
            if (type_ == 0) {
                godot::Ref<godot::InputEventKey> ev;
                ev.instantiate();
                ev->set_pressed(true);
                ev->set_keycode(key_);
                ev->set_physical_keycode(key_);
                input->parse_input_event(ev);
            } else if (type_ == 1) {
                godot::Ref<godot::InputEventMouseButton> ev;
                ev.instantiate();
                ev->set_pressed(true);
                ev->set_button_index(static_cast<godot::MouseButton>(button_index_));
                if (has_position_) {
                    ev->set_position(position_);
                    ev->set_global_position(position_);
                }
                input->parse_input_event(ev);
            } else if (mode_api_) {
                input->action_press(action_);
            } else {
                godot::Ref<godot::InputEventAction> ev;
                ev.instantiate();
                ev->set_action(action_);
                ev->set_pressed(true);
                input->parse_input_event(ev);
            }
            input->flush_buffered_events();
        }
    }

    void do_release() {
        auto* input = godot::Input::get_singleton();
        if (input) {
            if (type_ == 0) {
                godot::Ref<godot::InputEventKey> ev;
                ev.instantiate();
                ev->set_pressed(false);
                ev->set_keycode(key_);
                ev->set_physical_keycode(key_);
                input->parse_input_event(ev);
            } else if (type_ == 1) {
                godot::Ref<godot::InputEventMouseButton> ev;
                ev.instantiate();
                ev->set_pressed(false);
                ev->set_button_index(static_cast<godot::MouseButton>(button_index_));
                if (has_position_) {
                    ev->set_position(position_);
                    ev->set_global_position(position_);
                }
                input->parse_input_event(ev);
            } else if (mode_api_) {
                input->action_release(action_);
            } else {
                godot::Ref<godot::InputEventAction> ev;
                ev.instantiate();
                ev->set_action(action_);
                ev->set_pressed(false);
                input->parse_input_event(ev);
            }
        }
    }
};

// eval script 的 await 等待器：持有 GDScriptFunctionState（保活协程），
// completed 信号或超时后响应并清理
class GameBridgeEvalAwaiter : public godot::Node {
    GDCLASS(GameBridgeEvalAwaiter, godot::Node)

    int64_t request_id_ = 0;
    int64_t timeout_ms_ = 5000;
    godot::Ref<godot::RefCounted> state_ref_;
    godot::Node* target_ = nullptr;
    godot::Node* parent_ = nullptr;
    bool persist_ = false;
    bool connected_ = false;
    std::string persist_path_;
    godot::Ref<godot::SceneTreeTimer> timer_;
    bool done_ = false;

protected:
    static void _bind_methods() {
        godot::ClassDB::bind_method(godot::D_METHOD("_on_await_completed", "result"),
            &GameBridgeEvalAwaiter::_on_await_completed);
        godot::ClassDB::bind_method(godot::D_METHOD("_on_await_timeout"),
            &GameBridgeEvalAwaiter::_on_await_timeout);
    }

public:
    void setup(int64_t request_id, const godot::Ref<godot::RefCounted>& state_ref,
               godot::Node* target, godot::Node* parent, bool persist,
               const std::string& persist_path, int64_t timeout_ms) {
        request_id_ = request_id;
        timeout_ms_ = timeout_ms;
        state_ref_ = state_ref;
        target_ = target;
        parent_ = parent;
        persist_ = persist;
        persist_path_ = persist_path;
        godot::SceneTree* tree = get_scene_tree();
        if (!tree) {
            JV body = error_result("no scene tree for eval await");
            send_response(request_id_, std::move(body));
            cleanup();
            return;
        }
        tree->get_root()->add_child(this);
        register_cancel_handler(request_id_, [this] { cancel(); });
        if (state_ref_.is_valid()) {
            godot::Callable completed_cb(this, godot::StringName("_on_await_completed"));
            godot::Error err = state_ref_->connect("completed", completed_cb);
            if (err == godot::OK) connected_ = true;
        }
        // process_always=true, process_in_physics=false, ignore_time_scale=true：
        // 兜底超时不受 time_scale / 暂停影响，保证假冻结时也能按时失败
        timer_ = tree->create_timer(timeout_ms / 1000.0, true, false, true);
        if (timer_.is_valid()) {
            godot::Error err = timer_->connect("timeout",
                godot::Callable(this, godot::StringName("_on_await_timeout")));
            if (err != godot::OK && !connected_) {
                JV body = error_result("failed to arm await timeout");
                send_response(request_id_, std::move(body));
                cleanup();
            }
        }
    }

    void _on_await_completed(const godot::Variant& p_result) {
        if (done_) return;
        done_ = true;
        JV body = ok_result(VariantJson::serialize(p_result));
        if (persist_ && !persist_path_.empty()) {
            if (body["result"].IsObject()) {
                body["result"]["node_path"] = JV(persist_path_);
            } else {
                JV inner(JV::object_tag);
                inner["value"] = std::move(body["result"]);
                inner["node_path"] = JV(persist_path_);
                body = ok_result(std::move(inner));
            }
        }
        send_response(request_id_, std::move(body));
        cleanup();
    }

    void _on_await_timeout() {
        if (done_) return;
        done_ = true;
        std::string context;
        if (auto* tree = get_scene_tree()) {
            context = " (game paused: " + std::string(tree->is_paused() ? "true" : "false")
                + ", physics_frame: ";
            auto* engine = godot::Engine::get_singleton();
            context += engine ? std::to_string(engine->get_physics_frames()) : std::string("?");
            context += ")";
        }
        send_response(request_id_, error_result("await timed out after "
            + std::to_string(timeout_ms_) + " ms" + context));
        cleanup();
    }

    // cancel op 触发的取消：不自行响应，由 cancel op 返回结果
    void cancel() {
        if (done_) return;
        done_ = true;
        cleanup();
    }

private:
    void cleanup() {
        unregister_cancel_handler(request_id_);
        timer_.unref();
        if (connected_ && state_ref_.is_valid()) {
            state_ref_->disconnect("completed",
                godot::Callable(this, godot::StringName("_on_await_completed")));
            connected_ = false;
        }
        state_ref_.unref();
        if (!persist_ && target_ && parent_ && target_->get_parent() == parent_) {
            parent_->remove_child(target_);
            memdelete(target_);
        }
        target_ = nullptr;
        parent_ = nullptr;
        queue_free();
    }
};

// 定时补发释放事件（pressed=false）；keep_pressed=true 时到期前保持持续按压
void schedule_release(const std::string& type, const godot::Key& key, int64_t button_index,
                      const godot::Vector2& position, bool has_position,
                      const godot::StringName& action, bool mode_api, int64_t duration_ms,
                      bool keep_pressed) {
    GameBridgeDelayedRelease* release = memnew(GameBridgeDelayedRelease);
    int type_code = (type == "key") ? 0 : (type == "mouse_button") ? 1 : 2;
    release->setup(type_code, key, button_index, position, has_position, action, mode_api,
                   duration_ms / 1000.0, keep_pressed);
}

// ── 游戏侧错误/输出环形缓冲 ──
// 引擎调试器通道（EngineDebugger::captures 表）仅在接收端分发命令，
// 发送路径（send_message/send_error/flush_output）不经过捕获回调，
// 因此注册 "error"/"output" 前缀捕获无法拦截引擎级错误/输出；
// 缓冲由 GameBridgeLogger（OS::add_logger 挂载，捕获游戏进程 print()/错误日志，
// 见下方 GameBridgeLogger 定义）与通道级错误（gsd 请求解析失败、未知 op、
// op 执行失败）共同填充，供编辑器经 gsd 通道拉取。
struct GameErrorEntry {
    int hr, min, sec, msec;
    std::string file, func;
    int line;
    std::string error, descr;
    bool is_warning;
    std::vector<std::string> stack;
};

struct GameOutputEntry {
    std::string text;
    int type;
};

constexpr size_t MAX_ERROR_BUFFER = 200;
constexpr size_t MAX_OUTPUT_BUFFER = 500;

std::mutex g_buffer_mtx;
std::vector<GameErrorEntry> g_error_buffer;
std::vector<GameOutputEntry> g_output_buffer;

void push_game_error(const std::string& file, const std::string& func, int line,
                     const std::string& error, const std::string& descr, bool is_warning,
                     std::vector<std::string> stack) {
    uint64_t time = godot::Time::get_singleton() ? godot::Time::get_singleton()->get_ticks_msec() : 0;
    std::lock_guard<std::mutex> lock(g_buffer_mtx);
    g_error_buffer.push_back({static_cast<int>(time / 3600000),
        static_cast<int>((time / 60000) % 60), static_cast<int>((time / 1000) % 60),
        static_cast<int>(time % 1000), file, func, line, error, descr, is_warning,
        std::move(stack)});
    if (g_error_buffer.size() > MAX_ERROR_BUFFER) {
        g_error_buffer.erase(g_error_buffer.begin());
    }
}

void push_game_output(const std::string& text, int type) {
    std::lock_guard<std::mutex> lock(g_buffer_mtx);
    g_output_buffer.push_back({text, type});
    if (g_output_buffer.size() > MAX_OUTPUT_BUFFER) {
        g_output_buffer.erase(g_output_buffer.begin());
    }
}

// 引擎 ErrorType 枚举（core/error/error_macros.h）：0=ERR_SCRIPT, 1=ERR_SHADER,
// 2=ERR_SYSTEM, 3=ERR_WARNING
constexpr int32_t ENGINE_ERROR_TYPE_ERR_WARNING = 3;

// 挂载到 OS::add_logger 的游戏进程日志捕获器：print()/错误日志写入缓冲
class GameBridgeLogger : public godot::Logger {
    GDCLASS(GameBridgeLogger, godot::Logger)

protected:
    static void _bind_methods() {}

public:
    void _log_error(const godot::String& p_function, const godot::String& p_file,
        int32_t p_line, const godot::String& p_code, const godot::String& p_rationale,
        bool p_editor_notify, int32_t p_error_type,
        const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>& p_script_backtraces) override {
        (void)p_editor_notify;
        (void)p_script_backtraces;
        push_game_error(to_std(p_file), to_std(p_function), p_line,
            to_std(p_code), to_std(p_rationale),
            p_error_type == ENGINE_ERROR_TYPE_ERR_WARNING, {});
    }

    void _log_message(const godot::String& p_message, bool p_error) override {
        if (p_error) {
            push_game_error("", "", 0, to_std(p_message), "", false, {});
        } else {
            push_game_output(to_std(p_message), 0);
        }
    }
};

// ── op: status ──
JV op_status() {
    JV r(JV::object_tag);
    auto* engine = godot::Engine::get_singleton();
    if (engine) {
        godot::Dictionary version_info = engine->get_version_info();
        if (version_info.has("string")) {
            r["version"] = JV(to_std(godot::String(version_info["string"])));
        }
        r["fps"] = JV(engine->get_frames_per_second());
        r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
    }
    r[GSD_FIELD_LAST_ACTIVITY_MS] = JV(static_cast<int64_t>(g_last_activity_ms));
    if (auto* tree = get_scene_tree()) {
        r["paused"] = JV(tree->is_paused());
        r["node_count"] = JV(static_cast<int64_t>(tree->get_node_count()));
        godot::Node* current = tree->get_current_scene();
        if (current) {
            godot::String scene_path = current->get_scene_file_path();
            if (scene_path.is_empty()) scene_path = godot::String(current->get_name());
            r["scene"] = JV(to_std(scene_path));
        } else {
            r["scene"] = JV("");
        }
    }
    return ok_result(std::move(r));
}

// ── op: eval (script / get_property / set_property / call_method) ──
JV op_eval_script(const JV& params, int64_t request_id) {
    auto* code_p = params.Find("source_code");
    if (!code_p || !code_p->IsString()) {
        return error_result("eval script requires source_code");
    }
    std::string source = code_p->GetString();

    godot::Ref<godot::GDScript> script;
    script.instantiate();
    if (script.is_null()) return error_result("failed to create GDScript instance");

    script->set_source_code(godot::String(source.c_str()));
    godot::Error parse_err = script->reload();
    if (parse_err != godot::OK) {
        return error_result("GDScript compilation failed (ERR code "
            + std::to_string(static_cast<int>(parse_err)) + ")");
    }

    bool persist = false;
    if (auto* persist_p = params.Find("persist")) {
        if (persist_p->IsBool()) persist = persist_p->GetBool();
    }
    std::string persist_name;
    if (auto* name_p = params.Find("persist_name")) {
        if (name_p->IsString()) persist_name = name_p->GetString();
    }
    if (persist && persist_name.empty()) {
        persist_name = "eval_" + std::to_string(request_id);
    }
    int64_t await_timeout_ms = 5000;
    if (auto* tp = params.Find("timeout_ms")) {
        if (tp->IsInt() && tp->GetInt() > 0) await_timeout_ms = tp->GetInt();
    }

    godot::SceneTree* tree = get_scene_tree();
    if (!tree) return error_result("no scene tree");
    godot::Node* root = tree->get_root();
    if (!root) return error_result("no root node");

    godot::Node* parent = root;
    if (persist) {
        godot::Node* container = root->get_node_or_null(godot::NodePath("/root/__gsd_runtime"));
        if (!container) {
            container = memnew(godot::Node);
            container->set_name("__gsd_runtime");
            root->add_child(container);
        }
        if (container->get_node_or_null(godot::NodePath(godot::String(persist_name.c_str())))) {
            return error_result("persist node already exists: /root/__gsd_runtime/" + persist_name);
        }
        parent = container;
    }

    godot::Node* temp_node = memnew(godot::Node);
    if (persist) temp_node->set_name(godot::StringName(persist_name.c_str()));
    temp_node->set_script(godot::Variant(script));
    parent->add_child(temp_node);

    godot::StringName run_fn("_run");
    if (!temp_node->has_method(run_fn)) {
        parent->remove_child(temp_node);
        memdelete(temp_node);
        return error_result("function _run not found in compiled script — script must extend Node and define func _run()");
    }

    godot::Variant result = temp_node->call(run_fn);

    godot::Object* state_obj = nullptr;
    if (result.get_type() == godot::Variant::OBJECT) {
        state_obj = godot::Object::cast_to<godot::Object>(result);
    }
    bool is_await = state_obj && state_obj->get_class() == "GDScriptFunctionState";

    if (is_await) {
        std::string persist_path;
        if (persist) persist_path = "/root/__gsd_runtime/" + persist_name;
        GameBridgeEvalAwaiter* awaiter = memnew(GameBridgeEvalAwaiter);
        awaiter->setup(request_id, godot::Ref<godot::RefCounted>(result),
            temp_node, parent, persist, persist_path, await_timeout_ms);
        return JV(); // 异步：不立即响应，由 awaiter 在 completed / timeout 时 send_response
    }

    if (!persist) {
        parent->remove_child(temp_node);
        memdelete(temp_node);
    }

    JV body = ok_result(VariantJson::serialize(result));
    if (persist) {
        if (body["result"].IsObject()) {
            body["result"]["node_path"] = JV("/root/__gsd_runtime/" + persist_name);
        } else {
            JV inner(JV::object_tag);
            inner["value"] = std::move(body["result"]);
            inner["node_path"] = JV("/root/__gsd_runtime/" + persist_name);
            body = ok_result(std::move(inner));
        }
    }
    return body;
}

JV op_eval_get_property(const JV& params) {
    auto* path_p = params.Find("node_path");
    auto* prop_p = params.Find("property");
    if (!path_p || !path_p->IsString()) return error_result("eval get_property requires node_path");
    if (!prop_p || !prop_p->IsString()) return error_result("eval get_property requires property");
    godot::Node* node = resolve_node(path_p->GetString());
    if (!node) return error_result("node not found: " + path_p->GetString());
    godot::Variant value = node->get(godot::StringName(prop_p->GetString().c_str()));
    return ok_result(VariantJson::serialize(value));
}

godot::Dictionary find_property_info(godot::Node* node, const std::string& prop_name) {
    godot::TypedArray<godot::Dictionary> props = node->get_property_list();
    for (int64_t i = 0; i < props.size(); i++) {
        godot::Dictionary dict = props[i];
        if (dict.has("name") && to_std(dict["name"].operator godot::String()) == prop_name) {
            return dict;
        }
    }
    return godot::Dictionary();
}

JV op_eval_set_property(const JV& params) {
    auto* path_p = params.Find("node_path");
    auto* prop_p = params.Find("property");
    auto* value_p = params.Find("value");
    if (!path_p || !path_p->IsString()) return error_result("eval set_property requires node_path");
    if (!prop_p || !prop_p->IsString()) return error_result("eval set_property requires property");
    if (!value_p) return error_result("eval set_property requires value");
    godot::Node* node = resolve_node(path_p->GetString());
    if (!node) return error_result("node not found: " + path_p->GetString());
    std::string prop_name = prop_p->GetString();

    godot::Dictionary prop_info = find_property_info(node, prop_name);
    if (prop_info.is_empty()) {
        return error_result("property not found: " + prop_name + " on " + path_p->GetString());
    }

    std::string type_hint;
    if (prop_info.has("type")) {
        int type_id = static_cast<int>(prop_info["type"]);
        int hint_val = 0;
        if (prop_info.has("hint")) {
            hint_val = static_cast<int>(prop_info["hint"]);
        }
        bool is_object_type = static_cast<godot::Variant::Type>(type_id) == godot::Variant::OBJECT;
        bool is_resource_hint = hint_val == godot::PROPERTY_HINT_RESOURCE_TYPE;
        if ((is_object_type || is_resource_hint) && prop_info.has("hint_string")) {
            std::string hint_str = to_std(prop_info["hint_string"].operator godot::String());
            if (!hint_str.empty()) {
                type_hint = hint_str;
            }
        }
        if (type_hint.empty()) {
            type_hint = to_std(godot::Variant::get_type_name(static_cast<godot::Variant::Type>(type_id)));
        }
    }

    godot::StringName prop_name_sn(prop_name.c_str());
    godot::Variant value = VariantJson::deserialize(*value_p, type_hint);
    godot::Variant old_val = node->get(prop_name_sn);
    node->set(prop_name_sn, value);
    godot::Variant new_val = node->get(prop_name_sn);

    std::string readback_detail;
    util::ReadbackStatus readback = util::check_readback(value, old_val, new_val, readback_detail);
    if (readback == util::ReadbackStatus::REJECTED) {
        return util::error_detail(
            "property rejected: '" + prop_name + "' on " + path_p->GetString(),
            path_p->GetString(),
            "readback equals set value",
            "property may not exist, be read-only, or require a type hint");
    }
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    if (readback == util::ReadbackStatus::CONVERTED) {
        r["warning"] = JV("set applied; " + readback_detail);
    }
    return r;
}

JV op_eval_call_method(const JV& params) {
    auto* path_p = params.Find("node_path");
    auto* method_p = params.Find("method");
    if (!path_p || !path_p->IsString()) return error_result("eval call_method requires node_path");
    if (!method_p || !method_p->IsString()) return error_result("eval call_method requires method");
    godot::Node* node = resolve_node(path_p->GetString());
    if (!node) return error_result("node not found: " + path_p->GetString());
    godot::Array call_args;
    if (auto* args_p = params.Find("args")) {
        if (args_p->IsArray()) {
            for (const auto& arg : args_p->GetArray()) {
                call_args.push_back(VariantJson::deserialize(arg));
            }
        }
    }
    godot::StringName method_sn(method_p->GetString().c_str());
    if (!node->has_method(method_sn)) {
        return error_result("method not found: '" + method_p->GetString() + "' on " + path_p->GetString()
            + " — expected a built-in or script method of that node; use get_property_list or the node's script to list available methods");
    }
    godot::Variant result = node->callv(method_sn, call_args);
    return ok_result(VariantJson::serialize(result));
}

JV op_eval(const JV& params, int64_t request_id) {
    auto* action_p = params.Find("action");
    if (!action_p || !action_p->IsString()) {
        return error_result("eval requires action (script|get_property|set_property|call_method)");
    }
    std::string action = action_p->GetString();
    if (action == "script") return op_eval_script(params, request_id);
    if (action == "get_property") return op_eval_get_property(params);
    if (action == "set_property") return op_eval_set_property(params);
    if (action == "call_method") return op_eval_call_method(params);
    return error_result("unknown eval action: " + action);
}

// ── op: input (key / mouse_button / action) ──
godot::Key parse_keycode(const JV& keycode) {
    auto parse_int_key = [](int64_t value) -> godot::Key {
        if (value > 0 && value <= 0xFFFFFF) return static_cast<godot::Key>(value);
        return godot::KEY_NONE;
    };
    if (keycode.IsInt()) return parse_int_key(keycode.GetInt());
    if (!keycode.IsString()) return godot::KEY_NONE;

    std::string s = keycode.GetString();
    if (s.empty()) return godot::KEY_NONE;

    bool all_digits = true;
    for (char c : s) {
        if (c < '0' || c > '9') { all_digits = false; break; }
    }
    if (all_digits) return parse_int_key(std::stoll(s));

    if (s.size() == 1) {
        char c = s[0];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            return static_cast<godot::Key>(c);
        }
        return godot::KEY_NONE;
    }

    std::string u = s;
    for (auto& c : u) {
        if (c >= 'a' && c <= 'z') c -= 32;
    }
    if (u.rfind("KEY_", 0) == 0) u = u.substr(4);

    static const std::unordered_map<std::string, godot::Key> name_map = {
        {"SPACE", godot::KEY_SPACE}, {"ENTER", godot::KEY_ENTER}, {"RETURN", godot::KEY_ENTER},
        {"ESCAPE", godot::KEY_ESCAPE}, {"TAB", godot::KEY_TAB}, {"BACKSPACE", godot::KEY_BACKSPACE},
        {"INSERT", godot::KEY_INSERT}, {"DELETE", godot::KEY_DELETE}, {"PAUSE", godot::KEY_PAUSE},
        {"PRINT", godot::KEY_PRINT}, {"CLEAR", godot::KEY_CLEAR}, {"SHIFT", godot::KEY_SHIFT},
        {"CTRL", godot::KEY_CTRL}, {"CONTROL", godot::KEY_CTRL}, {"META", godot::KEY_META},
        {"ALT", godot::KEY_ALT}, {"CAPSLOCK", godot::KEY_CAPSLOCK}, {"NUMLOCK", godot::KEY_NUMLOCK},
        {"SCROLLLOCK", godot::KEY_SCROLLLOCK}, {"MENU", godot::KEY_MENU}, {"HOME", godot::KEY_HOME},
        {"END", godot::KEY_END}, {"LEFT", godot::KEY_LEFT}, {"RIGHT", godot::KEY_RIGHT},
        {"UP", godot::KEY_UP}, {"DOWN", godot::KEY_DOWN}, {"PAGEUP", godot::KEY_PAGEUP},
        {"PAGEDOWN", godot::KEY_PAGEDOWN},
        {"F1", godot::KEY_F1}, {"F2", godot::KEY_F2}, {"F3", godot::KEY_F3}, {"F4", godot::KEY_F4},
        {"F5", godot::KEY_F5}, {"F6", godot::KEY_F6}, {"F7", godot::KEY_F7}, {"F8", godot::KEY_F8},
        {"F9", godot::KEY_F9}, {"F10", godot::KEY_F10}, {"F11", godot::KEY_F11}, {"F12", godot::KEY_F12},
        {"MINUS", godot::KEY_MINUS}, {"EQUAL", godot::KEY_EQUAL}, {"BRACKETLEFT", godot::KEY_BRACKETLEFT},
        {"BRACKETRIGHT", godot::KEY_BRACKETRIGHT}, {"BACKSLASH", godot::KEY_BACKSLASH},
        {"SEMICOLON", godot::KEY_SEMICOLON}, {"APOSTROPHE", godot::KEY_APOSTROPHE},
        {"COMMA", godot::KEY_COMMA}, {"PERIOD", godot::KEY_PERIOD}, {"SLASH", godot::KEY_SLASH},
        {"QUOTELEFT", godot::KEY_QUOTELEFT},
        {"KP_ENTER", godot::KEY_KP_ENTER}, {"KP_ADD", godot::KEY_KP_ADD},
        {"KP_SUBTRACT", godot::KEY_KP_SUBTRACT}, {"KP_MULTIPLY", godot::KEY_KP_MULTIPLY},
        {"KP_DIVIDE", godot::KEY_KP_DIVIDE}, {"KP_PERIOD", godot::KEY_KP_PERIOD},
    };
    auto it = name_map.find(u);
    if (it == name_map.end()) return godot::KEY_NONE;
    return it->second;
}

bool extract_position(const JV& params, godot::Vector2& out) {
    auto* pos_p = params.Find("position");
    if (!pos_p || !pos_p->IsObject()) return false;
    auto* x = pos_p->Find("x");
    auto* y = pos_p->Find("y");
    if (!x || !y || !x->IsNumber() || !y->IsNumber()) return false;
    double px = x->IsInt() ? static_cast<double>(x->GetInt()) : x->GetDouble();
    double py = y->IsInt() ? static_cast<double>(y->GetInt()) : y->GetDouble();
    out = godot::Vector2(px, py);
    return true;
}

bool param_pressed(const JV& params, bool default_value) {
    if (auto* p = params.Find("pressed")) {
        if (p->IsBool()) return p->GetBool();
    }
    return default_value;
}

JV op_input(const JV& params, int64_t request_id) {
    (void)request_id; // 预留：hold 注入可经 cancel op 中断
    auto* type_p = params.Find("type");
    if (!type_p || !type_p->IsString()) {
        return error_result("input requires type (key|mouse_button|action)");
    }
    std::string type = type_p->GetString();

    auto* input = godot::Input::get_singleton();
    if (!input) return error_result("Input singleton not available");

    std::string mode = "event";
    if (auto* mode_p = params.Find("mode")) {
        if (mode_p->IsString()) mode = mode_p->GetString();
    }
    // hold 与 event 都走事件注入（api 走 Input API）；hold 在 duration 内保持持续按压
    bool mode_api = (mode == "api");
    bool keep_pressed = (mode == "hold");
    int64_t duration_ms = 0;
    if (auto* dur_p = params.Find("duration_ms")) {
        if (dur_p->IsInt() && dur_p->GetInt() > 0) duration_ms = dur_p->GetInt();
    }
    bool pressed = param_pressed(params, true);

    std::string release_type;
    godot::Key key = godot::KEY_NONE;
    int64_t button_index = 0;
    godot::Vector2 pos;
    bool has_pos = false;
    godot::StringName action;

    if (type == "key") {
        auto* kc = params.Find("keycode");
        if (!kc || !(kc->IsInt() || kc->IsString())) {
            return error_result("input key requires keycode (numeric key code or key name)");
        }
        key = parse_keycode(*kc);
        if (key == godot::KEY_NONE) {
            return error_result("invalid keycode: " + (kc->IsString() ? kc->GetString() : std::to_string(kc->GetInt())));
        }
        if (pressed) {
            godot::Ref<godot::InputEventKey> release_ev;
            release_ev.instantiate();
            release_ev->set_pressed(false);
            release_ev->set_keycode(key);
            release_ev->set_physical_keycode(key);
            input->parse_input_event(release_ev);
            input->flush_buffered_events();
        }
        godot::Ref<godot::InputEventKey> ev;
        ev.instantiate();
        ev->set_pressed(pressed);
        ev->set_keycode(key);
        ev->set_physical_keycode(key);
        input->parse_input_event(ev);
        input->flush_buffered_events();
        release_type = "key";
    } else if (type == "mouse_button") {
        auto* bi = params.Find("button_index");
        if (!bi || !bi->IsInt()) {
            return error_result("input mouse_button requires button_index (integer)");
        }
        button_index = bi->GetInt();
        if (extract_position(params, pos)) has_pos = true;
        if (pressed) {
            godot::Ref<godot::InputEventMouseButton> release_ev;
            release_ev.instantiate();
            release_ev->set_pressed(false);
            release_ev->set_button_index(static_cast<godot::MouseButton>(button_index));
            if (has_pos) {
                release_ev->set_position(pos);
                release_ev->set_global_position(pos);
            }
            input->parse_input_event(release_ev);
            input->flush_buffered_events();
        }
        godot::Ref<godot::InputEventMouseButton> ev;
        ev.instantiate();
        ev->set_pressed(pressed);
        ev->set_button_index(static_cast<godot::MouseButton>(button_index));
        if (has_pos) {
            ev->set_position(pos);
            ev->set_global_position(pos);
        }
        input->parse_input_event(ev);
        input->flush_buffered_events();
        release_type = "mouse_button";
    } else if (type == "action") {
        auto* act = params.Find("action");
        if (!act || !act->IsString()) {
            return error_result("input action requires action (string)");
        }
        action = godot::StringName(act->GetString().c_str());
        if (pressed) {
            // 先强制 release 清掉残留按下状态，保证本次注入产生新的 false→true 沿
            input->action_release(action);
        }
        if (mode_api) {
            if (pressed) input->action_press(action);
            else input->action_release(action);
        } else {
            godot::Ref<godot::InputEventAction> ev;
            ev.instantiate();
            ev->set_action(action);
            ev->set_pressed(pressed);
            input->parse_input_event(ev);
            if (pressed) input->flush_buffered_events();
        }
        release_type = "action";
    } else {
        return error_result("unknown input type: " + type);
    }

    if (pressed && duration_ms > 0 && !release_type.empty()) {
        schedule_release(release_type, key, button_index, pos, has_pos, action, mode_api,
                         duration_ms, keep_pressed);
    }

    JV r(JV::object_tag);
    r["result"] = JV("ok");
    if (pressed) {
        auto* engine = godot::Engine::get_singleton();
        if (engine) {
            r["injected_at_physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
            r["expected_visible_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames() + 1));
            // flush 后的当前帧，供编辑器回读校验注入边沿是否已被消费
            r[GSD_FIELD_PARSED_PHYSICS_FRAME] = JV(static_cast<int64_t>(engine->get_physics_frames()));
            r[GSD_FIELD_PARSED_PROCESS_FRAME] = JV(static_cast<int64_t>(engine->get_process_frames()));
        }
        if (auto* tree = get_scene_tree()) {
            if (tree->is_paused()) {
                r["warning"] = JV("game paused: transient input edge will not be consumed by physics callbacks while paused");
            }
        }
    }
    return r;
}

// ── op: ping（进程健康检测） ──
JV op_ping() {
    JV r(JV::object_tag);
    auto* engine = godot::Engine::get_singleton();
    if (engine) {
        r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
        r["process_frame"] = JV(static_cast<int64_t>(engine->get_process_frames()));
    }
    r[GSD_FIELD_LAST_ACTIVITY_MS] = JV(static_cast<int64_t>(g_last_activity_ms));
    return r;
}

// ── op: cancel（取消挂起的异步 op） ──
JV op_cancel(const JV& params) {
    auto* rid_p = params.Find(GSD_FIELD_REQUEST_ID);
    if (!rid_p || !rid_p->IsInt()) {
        return error_result("cancel requires request_id (integer)");
    }
    int64_t target = rid_p->GetInt();
    JV r(JV::object_tag);
    auto it = g_cancel_handlers.find(target);
    if (it == g_cancel_handlers.end()) {
        r[GSD_FIELD_CANCELLED] = JV(false);
        r["reason"] = JV("no pending operation for request_id " + std::to_string(target));
        return r;
    }
    // 先取出回调并注销，再调用；回调内部 cleanup 的注销为幂等操作
    std::function<void()> handler = std::move(it->second);
    g_cancel_handlers.erase(it);
    handler();
    r[GSD_FIELD_CANCELLED] = JV(true);
    return r;
}

// ── op: input_wait（异步，命中或超时后由 watcher 自行响应） ──
JV op_input_wait(const JV& params, int64_t request_id) {
    auto* act = params.Find("action");
    if (!act || !act->IsString()) {
        return error_result("input_wait requires action (string)");
    }
    std::string state = "just_pressed";
    if (auto* state_p = params.Find("state")) {
        if (state_p->IsString()) state = state_p->GetString();
    }
    int state_kind;
    if (state == "just_pressed") state_kind = 0;
    else if (state == "just_released") state_kind = 1;
    else if (state == "pressed") state_kind = 2;
    else return error_result("input_wait state must be just_pressed|just_released|pressed");

    int64_t timeout_ms = 2000;
    if (auto* tp = params.Find("timeout_ms")) {
        if (tp->IsInt() && tp->GetInt() > 0) timeout_ms = tp->GetInt();
    }
    if (timeout_ms > 30000) timeout_ms = 30000;

    godot::SceneTree* tree = get_scene_tree();
    if (!tree) return error_result("no scene tree");
    godot::Node* root = tree->get_root();
    if (!root) return error_result("no root node");

    if (auto* inject_p = params.Find("inject")) {
        if (inject_p->IsObject()) {
            JV inj_result = op_input(*inject_p, request_id);
            if (inj_result.Contains("error")) return inj_result;
        }
    }

    GameBridgeInputWatcher* watcher = memnew(GameBridgeInputWatcher);
    watcher->setup(request_id, godot::StringName(act->GetString().c_str()), state_kind,
                   timeout_ms / 1000.0);
    root->add_child(watcher);
    register_cancel_handler(request_id, [watcher] { watcher->cancel(); });
    return JV(); // 异步：不立即响应，由 watcher 在命中 / 超时 send_response
}

// ── op: input_status（同步查询当前输入状态） ──
JV op_input_status(const JV& params) {
    auto* act = params.Find("action");
    if (!act || !act->IsString()) {
        return error_result("input_status requires action (string)");
    }
    auto* input = godot::Input::get_singleton();
    if (!input) return error_result("Input singleton not available");
    auto* engine = godot::Engine::get_singleton();
    if (!engine) return error_result("Engine singleton not available");
    godot::StringName action(act->GetString().c_str());
    JV r(JV::object_tag);
    r["pressed"] = JV(input->is_action_pressed(action));
    r["just_pressed"] = JV(input->is_action_just_pressed(action));
    r["just_released"] = JV(input->is_action_just_released(action));
    r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
    r["paused"] = JV(get_scene_tree() ? get_scene_tree()->is_paused() : false);
    return ok_result(std::move(r));
}

// ── op: capture ──
JV op_capture(int64_t request_id) {
    godot::SceneTree* tree = get_scene_tree();
    if (!tree) return error_result("no scene tree");
    godot::Window* root = tree->get_root();
    if (!root) return error_result("no root window");
    godot::Ref<godot::Image> image = root->get_texture()->get_image();
    if (image.is_null() || image->is_empty()) {
        return error_result("failed to read viewport texture");
    }
    std::string path = to_std(godot::OS::get_singleton()->get_cache_dir())
        + "/gsd_capture_" + std::to_string(request_id) + ".png";
    godot::Error save_err = image->save_png(godot::String(path.c_str()));
    if (save_err != godot::OK) {
        return error_result("save_png failed (ERR code " + std::to_string(static_cast<int>(save_err))
            + ") at " + path);
    }
    JV r(JV::object_tag);
    r["path"] = JV(path);
    r["width"] = JV(static_cast<int64_t>(image->get_width()));
    r["height"] = JV(static_cast<int64_t>(image->get_height()));
    return ok_result(std::move(r));
}

// ── op: get_errors（读取游戏侧错误缓冲） ──
JV op_get_errors(const JV& params) {
    int64_t limit = 50;
    if (auto* l = params.Find("limit")) {
        if (l->IsInt() && l->GetInt() > 0) limit = l->GetInt();
    }
    JV arr(JV::array_tag);
    {
        std::lock_guard<std::mutex> lock(g_buffer_mtx);
        size_t start = (static_cast<size_t>(limit) >= g_error_buffer.size())
            ? 0 : g_error_buffer.size() - static_cast<size_t>(limit);
        for (size_t i = start; i < g_error_buffer.size(); i++) {
            const GameErrorEntry& e = g_error_buffer[i];
            JV item(JV::object_tag);
            char time_buf[16];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d.%03d",
                e.hr, e.min, e.sec, e.msec);
            item["time"] = JV(std::string(time_buf));
            item["file"] = JV(e.file);
            item["func"] = JV(e.func);
            item["line"] = JV(static_cast<int64_t>(e.line));
            item["error"] = JV(e.error);
            item["descr"] = JV(e.descr);
            item["is_warning"] = JV(e.is_warning);
            JV stack(JV::array_tag);
            for (const std::string& f : e.stack) stack.PushBack(JV(f));
            item["stack"] = std::move(stack);
            arr.PushBack(std::move(item));
        }
    }
    return ok_result(std::move(arr));
}

// ── op: get_output（读取游戏侧输出缓冲） ──
JV op_get_output(const JV& params) {
    int64_t limit = 200;
    if (auto* l = params.Find("limit")) {
        if (l->IsInt() && l->GetInt() > 0) limit = l->GetInt();
    }
    JV arr(JV::array_tag);
    {
        std::lock_guard<std::mutex> lock(g_buffer_mtx);
        size_t start = (static_cast<size_t>(limit) >= g_output_buffer.size())
            ? 0 : g_output_buffer.size() - static_cast<size_t>(limit);
        for (size_t i = start; i < g_output_buffer.size(); i++) {
            JV item(JV::object_tag);
            item["type"] = JV(static_cast<int64_t>(g_output_buffer[i].type));
            item["text"] = JV(g_output_buffer[i].text);
            arr.PushBack(std::move(item));
        }
    }
    return ok_result(std::move(arr));
}

// ── op: get_tree（遍历 SceneTree root，文本化） ──
constexpr int MAX_TREE_DEPTH = 64;
constexpr int64_t MAX_TREE_NODES = 2000;

struct TreeWalkState {
    std::string out;
    int64_t count = 0;
    bool truncated = false;
};

void walk_tree(godot::Node* node, int depth, TreeWalkState& state) {
    if (state.truncated) return;
    if (depth > MAX_TREE_DEPTH) {
        state.out += std::string(static_cast<size_t>(depth) * 2, ' ')
            + "(max depth " + std::to_string(MAX_TREE_DEPTH) + ")\n";
        return;
    }
    if (state.count >= MAX_TREE_NODES) {
        state.truncated = true;
        return;
    }
    state.out += std::string(static_cast<size_t>(depth) * 2, ' ')
        + to_std(godot::String(node->get_name()))
        + " (" + to_std(godot::String(node->get_class())) + ")\n";
    state.count++;
    int64_t child_count = node->get_child_count();
    for (int64_t i = 0; i < child_count; i++) {
        walk_tree(node->get_child(i), depth + 1, state);
    }
}

JV op_get_tree() {
    godot::SceneTree* tree = get_scene_tree();
    if (!tree) return error_result("no scene tree");
    godot::Node* root = tree->get_root();
    if (!root) return error_result("no root node");
    TreeWalkState state;
    walk_tree(root, 0, state);
    if (state.truncated) {
        state.out += "(tree truncated at " + std::to_string(MAX_TREE_NODES) + " nodes)\n";
    }
    return ok_result(JV(state.out));
}

// ── dispatch & response ──
void send_response(int64_t request_id, JV body) {
    body["request_id"] = JV(request_id);
    godot::Array payload;
    payload.push_back(godot::String(body.Dump().c_str()));
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->send_message(gsd_string(GSD_MSG_RESPONSE), payload);
    }
}

class GameBridgeListener : public godot::RefCounted {
    GDCLASS(GameBridgeListener, godot::RefCounted)

protected:
    static void _bind_methods() {
        godot::ClassDB::bind_method(godot::D_METHOD("on_gsd_message"), &GameBridgeListener::on_gsd_message);
    }

public:
    bool on_gsd_message(const godot::String& p_message, const godot::Array& p_data) {
        (void)p_message;
        g_last_activity_ms = godot::Time::get_singleton() ? godot::Time::get_singleton()->get_ticks_msec() : 0;
        if (p_data.size() < 1) return true;
        std::string req_str = to_std(godot::String(p_data[0]));
        JV request = JV::Parse(req_str);
        if (!request.IsObject()) {
            push_game_error("game_bridge.cpp", "on_gsd_message", 0,
                "malformed gsd request", req_str.substr(0, 200), false, {});
            push_game_output("malformed gsd request: " + req_str.substr(0, 200), 1);
            return true;
        }

        int64_t request_id = 0;
        if (auto* rid = request.Find(GSD_FIELD_REQUEST_ID)) {
            if (rid->IsInt()) request_id = rid->GetInt();
        }
        std::string op;
        if (auto* op_p = request.Find(GSD_FIELD_OP)) {
            if (op_p->IsString()) op = op_p->GetString();
        }
        JV params(JV::object_tag);
        if (auto* params_p = request.Find(GSD_FIELD_PARAMS)) {
            if (params_p->IsObject()) params = *params_p;
        }

        JV body;
        if (op == GSD_OP_STATUS) body = op_status();
        else if (op == GSD_OP_PING) body = op_ping();
        else if (op == GSD_OP_CANCEL) body = op_cancel(params);
        else if (op == GSD_OP_EVAL) body = op_eval(params, request_id);
        else if (op == GSD_OP_INPUT) body = op_input(params, request_id);
        else if (op == GSD_OP_INPUT_WAIT) body = op_input_wait(params, request_id);
        else if (op == GSD_OP_INPUT_STATUS) body = op_input_status(params);
        else if (op == GSD_OP_CAPTURE) body = op_capture(request_id);
        else if (op == GSD_OP_GET_ERRORS) body = op_get_errors(params);
        else if (op == GSD_OP_GET_OUTPUT) body = op_get_output(params);
        else if (op == GSD_OP_GET_TREE) body = op_get_tree();
        else body = error_result("unknown op: " + op);

        // 通道级错误主动写入缓冲（引擎发送端不经过捕获回调，见缓冲说明）
        if (body.Contains("error")) {
            std::string err_text = body["error"].GetString();
            push_game_error("game_bridge.cpp", "on_gsd_message", 0,
                "op '" + op + "' failed: " + err_text, "", false, {});
            push_game_output("op " + op + " error: " + err_text, 1);
        }

        // 异步 op（input_wait / eval await）返回 null，由 watcher / awaiter 自行响应
        if (!body.IsNull()) {
            body["ok"] = JV(!body.Contains("error"));
            send_response(request_id, std::move(body));
        }
        return true;
    }
};

godot::Ref<GameBridgeListener> g_listener;
godot::Ref<GameBridgeLogger> g_logger;
bool g_registered = false;

} // namespace

void register_listener() {
    if (g_registered) return;
    static bool class_registered = false;
    if (!class_registered) {
        godot::ClassDB::register_class<GameBridgeListener>();
        godot::ClassDB::register_class<GameBridgeInputWatcher>();
        godot::ClassDB::register_class<GameBridgeDelayedRelease>();
        godot::ClassDB::register_class<GameBridgeEvalAwaiter>();
        godot::ClassDB::register_class<GameBridgeLogger>();
        class_registered = true;
    }
    if (g_listener.is_null()) {
        g_listener.instantiate();
    }
    if (g_logger.is_null()) {
        g_logger.instantiate();
    }
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->register_message_capture(godot::StringName(gsd_string(GSD_PREFIX)),
            godot::Callable(g_listener.ptr(), godot::StringName("on_gsd_message")));
        if (auto* os = godot::OS::get_singleton()) {
            os->add_logger(g_logger);
        }
        g_registered = true;
        // 就绪握手：告知编辑器本进程已注册 gsd 通道、可接收请求
        {
            JV body(JV::object_tag);
            body[GSD_FIELD_READY] = JV(true);
            body[GSD_FIELD_LAST_ACTIVITY_MS] = JV(static_cast<int64_t>(g_last_activity_ms));
            godot::Array payload;
            payload.push_back(godot::String(body.Dump().c_str()));
            dbg->send_message(gsd_string(GSD_MSG_READY), payload);
        }
        LogSystem::instance().log(LogLevel::Info, LogCategory::System,
            "Game bridge listener registered (gsd message capture + game logger)");
    }
}

void unregister_listener() {
    if (!g_registered) return;
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->unregister_message_capture(godot::StringName(gsd_string(GSD_PREFIX)));
    }
    if (auto* os = godot::OS::get_singleton()) {
        os->remove_logger(g_logger);
    }
    g_logger.unref();
    g_listener.unref();
    g_registered = false;
    LogSystem::instance().log(LogLevel::Info, LogCategory::System,
        "Game bridge listener unregistered");
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_self_driving
