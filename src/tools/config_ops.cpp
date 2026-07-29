#include "config_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <string>

namespace godot_self_driving {
namespace config_ops {

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

} // namespace

mcp::JsonValue handle_project_settings_get(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_get called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* ps = godot::ProjectSettings::get_singleton();
    if (!ps) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ProjectSettings not available");
        return e;
    }
    std::string name = np->GetString();
    godot::Variant default_var;
    auto* dp = args.Find("default");
    if (dp && !dp->IsNull()) {
        default_var = VariantJson::deserialize(*dp);
    }
    godot::Variant result = ps->get_setting(godot::String(name.c_str()), default_var);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_get completed");
    return r;
}

mcp::JsonValue handle_project_settings_set(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_set called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* vp = args.Find("value");
    if (!vp) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: value");
        return e;
    }
    auto* ps = godot::ProjectSettings::get_singleton();
    if (!ps) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ProjectSettings not available");
        return e;
    }
    std::string name = np->GetString();
    godot::Variant value;
    if (vp->IsObject()) {
        auto* type_field = vp->Find("type");
        auto* value_field = vp->Find("value");
        if (type_field && type_field->IsString() && value_field) {
            value = VariantJson::deserialize(*value_field, type_field->GetString());
        } else {
            value = VariantJson::deserialize(*vp);
        }
    } else {
        value = VariantJson::deserialize(*vp);
    }
    ps->set_setting(godot::String(name.c_str()), value);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_set completed");
    return r;
}

mcp::JsonValue handle_project_settings_has(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_has called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* ps = godot::ProjectSettings::get_singleton();
    if (!ps) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ProjectSettings not available");
        return e;
    }
    std::string name = np->GetString();
    bool has = ps->has_setting(godot::String(name.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(has);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_has completed");
    return r;
}

mcp::JsonValue handle_project_settings_save(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_save called");
    auto* ps = godot::ProjectSettings::get_singleton();
    if (!ps) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ProjectSettings not available");
        return e;
    }
    godot::Error err = ps->save();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "project_settings_save completed");
    return r;
}

mcp::JsonValue handle_engine_get_version(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_version called");
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    godot::Dictionary info = engine->get_version_info();
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    if (info.has("major")) {
        j["major"] = mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["major"])));
    }
    if (info.has("minor")) {
        j["minor"] = mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["minor"])));
    }
    if (info.has("patch")) {
        j["patch"] = mcp::JsonValue(static_cast<int64_t>(static_cast<int>(info["patch"])));
    }
    if (info.has("string")) {
        godot::String s = info["string"];
        j["string"] = mcp::JsonValue(to_std(s));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_version completed");
    return r;
}

mcp::JsonValue handle_engine_get_fps(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_fps called");
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    double fps = engine->get_frames_per_second();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(fps);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_fps completed");
    return r;
}

mcp::JsonValue handle_engine_get_frames_drawn(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_frames_drawn called");
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    uint64_t count = engine->get_frames_drawn();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(count));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_frames_drawn completed");
    return r;
}

mcp::JsonValue handle_engine_set_time_scale(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_set_time_scale called");
    auto* sp = args.Find("scale");
    if (!sp || !sp->IsNumber()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: scale (number)");
        return e;
    }
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    double scale = sp->IsDouble() ? sp->GetDouble() : static_cast<double>(sp->GetInt());
    engine->set_time_scale(scale);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_set_time_scale completed");
    return r;
}

mcp::JsonValue handle_engine_get_time_scale(const mcp::JsonValue&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_time_scale called");
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    double scale = engine->get_time_scale();
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(scale);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_get_time_scale completed");
    return r;
}

mcp::JsonValue handle_engine_set_max_fps(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_set_max_fps called");
    auto* fp = args.Find("fps");
    if (!fp || !fp->IsInt()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: fps (integer)");
        return e;
    }
    auto* engine = godot::Engine::get_singleton();
    if (!engine) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("Engine not available");
        return e;
    }
    int64_t fps = fp->GetInt();
    engine->set_max_fps(static_cast<int32_t>(fps));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "engine_set_max_fps completed");
    return r;
}

mcp::JsonValue handle_editor_settings_get(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_get called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorInterface not available");
        return e;
    }
    auto es = editor->get_editor_settings();
    if (es.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorSettings not available");
        return e;
    }
    std::string name = np->GetString();
    godot::Variant result = es->get_setting(godot::String(name.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = VariantJson::serialize(result);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_get completed");
    return r;
}

mcp::JsonValue handle_editor_settings_set(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_set called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* vp = args.Find("value");
    if (!vp) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: value");
        return e;
    }
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorInterface not available");
        return e;
    }
    auto es = editor->get_editor_settings();
    if (es.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorSettings not available");
        return e;
    }
    std::string name = np->GetString();
    godot::Variant value = VariantJson::deserialize(*vp);
    es->set_setting(godot::String(name.c_str()), value);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_set completed");
    return r;
}

mcp::JsonValue handle_editor_settings_has(const mcp::JsonValue& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_has called");
    auto* np = args.Find("name");
    if (!np || !np->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: name");
        return e;
    }
    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorInterface not available");
        return e;
    }
    auto es = editor->get_editor_settings();
    if (es.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorSettings not available");
        return e;
    }
    std::string name = np->GetString();
    bool has = es->has_setting(godot::String(name.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(has);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "editor_settings_has completed");
    return r;
}

} // namespace config_ops
} // namespace godot_self_driving
