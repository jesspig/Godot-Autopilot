#include "game_bridge.hpp"

#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/gd_script.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <unordered_map>

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
    }
    if (auto* tree = get_scene_tree()) {
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
JV op_eval_script(const JV& params) {
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

    godot::Node* temp_node = memnew(godot::Node);
    temp_node->set_script(godot::Variant(script));

    bool attached = false;
    if (auto* tree = get_scene_tree()) {
        tree->get_root()->add_child(temp_node);
        attached = true;
    }

    godot::StringName run_fn("_run");
    if (!temp_node->has_method(run_fn)) {
        if (attached && temp_node->get_parent()) {
            temp_node->get_parent()->remove_child(temp_node);
        }
        memdelete(temp_node);
        return error_result("function _run not found in compiled script — script must extend Node and define func _run()");
    }

    godot::Variant result = temp_node->call(run_fn);

    if (attached && temp_node->get_parent()) {
        temp_node->get_parent()->remove_child(temp_node);
    }
    memdelete(temp_node);

    return ok_result(VariantJson::serialize(result));
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

JV op_eval_set_property(const JV& params) {
    auto* path_p = params.Find("node_path");
    auto* prop_p = params.Find("property");
    auto* value_p = params.Find("value");
    if (!path_p || !path_p->IsString()) return error_result("eval set_property requires node_path");
    if (!prop_p || !prop_p->IsString()) return error_result("eval set_property requires property");
    if (!value_p) return error_result("eval set_property requires value");
    godot::Node* node = resolve_node(path_p->GetString());
    if (!node) return error_result("node not found: " + path_p->GetString());
    node->set(godot::StringName(prop_p->GetString().c_str()), VariantJson::deserialize(*value_p));
    JV r(JV::object_tag);
    r["result"] = JV("ok");
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
    godot::Variant result = node->callv(godot::StringName(method_p->GetString().c_str()), call_args);
    return ok_result(VariantJson::serialize(result));
}

JV op_eval(const JV& params) {
    auto* action_p = params.Find("action");
    if (!action_p || !action_p->IsString()) {
        return error_result("eval requires action (script|get_property|set_property|call_method)");
    }
    std::string action = action_p->GetString();
    if (action == "script") return op_eval_script(params);
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

JV op_input(const JV& params) {
    auto* type_p = params.Find("type");
    if (!type_p || !type_p->IsString()) {
        return error_result("input requires type (key|mouse_button|action)");
    }
    std::string type = type_p->GetString();

    auto* input = godot::Input::get_singleton();
    if (!input) return error_result("Input singleton not available");

    if (type == "key") {
        auto* kc = params.Find("keycode");
        if (!kc || !(kc->IsInt() || kc->IsString())) {
            return error_result("input key requires keycode (numeric key code or key name)");
        }
        godot::Key key = parse_keycode(*kc);
        if (key == godot::KEY_NONE) {
            return error_result("invalid keycode: " + (kc->IsString() ? kc->GetString() : std::to_string(kc->GetInt())));
        }
        godot::Ref<godot::InputEventKey> ev;
        ev.instantiate();
        ev->set_pressed(param_pressed(params, true));
        ev->set_keycode(key);
        ev->set_physical_keycode(key);
        input->parse_input_event(ev);
    } else if (type == "mouse_button") {
        auto* bi = params.Find("button_index");
        if (!bi || !bi->IsInt()) {
            return error_result("input mouse_button requires button_index (integer)");
        }
        godot::Ref<godot::InputEventMouseButton> ev;
        ev.instantiate();
        ev->set_pressed(param_pressed(params, true));
        ev->set_button_index(static_cast<godot::MouseButton>(bi->GetInt()));
        godot::Vector2 pos;
        if (extract_position(params, pos)) {
            ev->set_position(pos);
            ev->set_global_position(pos);
        }
        input->parse_input_event(ev);
    } else if (type == "action") {
        auto* act = params.Find("action");
        if (!act || !act->IsString()) {
            return error_result("input action requires action (string)");
        }
        godot::Ref<godot::InputEventAction> ev;
        ev.instantiate();
        ev->set_action(godot::StringName(act->GetString().c_str()));
        ev->set_pressed(param_pressed(params, true));
        input->parse_input_event(ev);
    } else {
        return error_result("unknown input type: " + type);
    }

    JV r(JV::object_tag);
    r["result"] = JV("ok");
    return r;
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

// ── dispatch & response ──
void send_response(int64_t request_id, JV body) {
    body["request_id"] = JV(request_id);
    godot::Array payload;
    payload.push_back(godot::String(body.Dump().c_str()));
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->send_message(godot::String("gsd:response"), payload);
    }
}

class GameBridgeListener : public godot::RefCounted {
    GDCLASS(GameBridgeListener, godot::RefCounted)

protected:
    static void _bind_methods() {
        godot::ClassDB::bind_method(godot::D_METHOD("on_gsd_message"), &GameBridgeListener::on_gsd_message);
    }

public:
    void on_gsd_message(const godot::Array& p_data) {
        if (p_data.size() < 1) return;
        std::string req_str = to_std(godot::String(p_data[0]));
        JV request = JV::Parse(req_str);
        if (!request.IsObject()) return;

        int64_t request_id = 0;
        if (auto* rid = request.Find("request_id")) {
            if (rid->IsInt()) request_id = rid->GetInt();
        }
        std::string op;
        if (auto* op_p = request.Find("op")) {
            if (op_p->IsString()) op = op_p->GetString();
        }
        JV params(JV::object_tag);
        if (auto* params_p = request.Find("params")) {
            if (params_p->IsObject()) params = *params_p;
        }

        JV body;
        if (op == "status") body = op_status();
        else if (op == "eval") body = op_eval(params);
        else if (op == "input") body = op_input(params);
        else if (op == "capture") body = op_capture(request_id);
        else body = error_result("unknown op: " + op);

        body["ok"] = JV(!body.Contains("error"));
        send_response(request_id, std::move(body));
    }
};

godot::Ref<GameBridgeListener> g_listener;
bool g_registered = false;

} // namespace

void register_listener() {
    if (g_registered) return;
    static bool class_registered = false;
    if (!class_registered) {
        godot::ClassDB::register_class<GameBridgeListener>();
        class_registered = true;
    }
    if (g_listener.is_null()) {
        g_listener.instantiate();
    }
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->register_message_capture(godot::StringName("gsd"),
            godot::Callable(g_listener.ptr(), godot::StringName("on_gsd_message")));
        g_registered = true;
        LogSystem::instance().log(LogLevel::Info, LogCategory::System,
            "Game bridge listener registered (gsd message capture)");
    }
}

void unregister_listener() {
    if (!g_registered) return;
    if (auto* dbg = godot::EngineDebugger::get_singleton()) {
        dbg->unregister_message_capture(godot::StringName("gsd"));
    }
    g_listener.unref();
    g_registered = false;
    LogSystem::instance().log(LogLevel::Info, LogCategory::System,
        "Game bridge listener unregistered");
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_self_driving
