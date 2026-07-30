#include "input_map_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <string>

namespace godot_self_driving {
namespace input_map_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

} // namespace

JV handle_action_add_event(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_add_event called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* ep = args.Find("event");
    if (!ep || !ep->IsObject()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: event");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    std::string event_class = "InputEvent";
    bool class_explicitly_provided = false;
    auto* class_field = ep->Find("class");
    if (class_field && class_field->IsString()) {
        event_class = class_field->GetString();
        class_explicitly_provided = true;
    }
    godot::Variant event_var = VariantJson::deserialize(*ep, event_class);
    auto event = godot::Ref<godot::InputEvent>(event_var);
    if (!class_explicitly_provided) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required field 'class': must specify a concrete InputEvent subclass, e.g. {\"class\":\"InputEventKey\", \"keycode\":65} — use search_tools to find InputEvent* subclasses");
        return e;
    }
    if (event.is_null()) {
        JV e(JV::object_tag);
        e["error"] = JV("failed to deserialize InputEvent — specify a concrete class e.g. \"InputEventKey\" with field \"class\" (common: InputEventKey, InputEventMouseButton, InputEventJoypadButton, InputEventAction, InputEventShortcut)");
        return e;
    }
    im->action_add_event(godot::StringName(action.c_str()), event);
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_add_event completed");
    return r;
}

JV handle_action_erase_event(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_erase_event called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* ei = args.Find("event_index");
    if (!ei || !ei->IsInt()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: event_index");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    int index = ei->GetInt();
    auto events = im->action_get_events(godot::StringName(action.c_str()));
    if (index < 0 || index >= events.size()) {
        JV e(JV::object_tag);
        e["error"] = JV("event_index out of range: " + std::to_string(index));
        return e;
    }
    auto event = events[index];
    if (event.get_type() == godot::Variant::NIL) {
        JV e(JV::object_tag);
        e["error"] = JV("event at index is null");
        return e;
    }
    im->action_erase_event(godot::StringName(action.c_str()), event);
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_erase_event completed");
    return r;
}

JV handle_action_set_deadzone(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_set_deadzone called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* dp = args.Find("deadzone");
    if (!dp || !dp->IsNumber()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: deadzone");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    float deadzone = static_cast<float>(dp->IsDouble() ? dp->GetDouble() : static_cast<double>(dp->GetInt()));
    im->action_set_deadzone(godot::StringName(action.c_str()), deadzone);
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_action_set_deadzone completed");
    return r;
}

JV handle_add_action(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_add_action called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    float deadzone = 0.5f;
    auto* dz = args.Find("deadzone");
    if (dz && dz->IsNumber()) {
        deadzone = static_cast<float>(dz->IsDouble() ? dz->GetDouble() : static_cast<double>(dz->GetInt()));
    }
    im->add_action(godot::StringName(action.c_str()), deadzone);
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_add_action completed");
    return r;
}

JV handle_erase_action(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_erase_action called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    im->erase_action(godot::StringName(action.c_str()));
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_erase_action completed");
    return r;
}

JV handle_get_actions(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_get_actions called");
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    auto names = im->get_actions();
    JV result_arr(JV::array_tag);
    for (int i = 0; i < names.size(); i++) {
        result_arr.PushBack(JV(to_std(godot::String(names[i]))));
    }
    JV r(JV::object_tag);
    r["result"] = std::move(result_arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_get_actions completed");
    return r;
}

JV handle_has_action(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_has_action called");
    auto* ap = args.Find("action");
    if (!ap || !ap->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: action");
        return e;
    }
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    std::string action = ap->GetString();
    bool has = im->has_action(godot::StringName(action.c_str()));
    JV r(JV::object_tag);
    r["result"] = JV(has);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_has_action completed");
    return r;
}

JV handle_persist(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_persist called");
    auto* im = godot::InputMap::get_singleton();
    if (!im) {
        JV e(JV::object_tag);
        e["error"] = JV("InputMap not available");
        return e;
    }
    auto* ps = godot::ProjectSettings::get_singleton();
    if (!ps) {
        JV e(JV::object_tag);
        e["error"] = JV("ProjectSettings not available");
        return e;
    }
    auto actions = im->get_actions();
    int persisted = 0;
    for (int i = 0; i < actions.size(); i++) {
        godot::StringName action_name = actions[i];
        std::string name_std = to_std(godot::String(action_name));
        if (name_std.substr(0, 3) == "ui_") continue;
        std::string prefix = "input/" + name_std;
        float deadzone = im->action_get_deadzone(action_name);
        ps->set_setting(godot::String((prefix + "/deadzone").c_str()), deadzone);
        auto events = im->action_get_events(action_name);
        ps->set_setting(godot::String((prefix + "/events").c_str()), events);
        persisted++;
    }
    godot::Error err = ps->save();
    JV r(JV::object_tag);
    r["result"] = JV("persisted");
    r["actions_persisted"] = JV(persisted);
    r["save_error"] = JV(static_cast<int>(err));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "input_map_persist completed: " + std::to_string(persisted) + " actions persisted");
    return r;
}

} // namespace input_map_ops
} // namespace godot_self_driving
