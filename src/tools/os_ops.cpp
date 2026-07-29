#include "os_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/time.hpp>
#include <string>

namespace godot_self_driving {
namespace os_ops {

using JV = mcp::JsonValue;

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

godot::String join_psa(const godot::PackedStringArray& psa) {
    godot::String result;
    for (int i = 0; i < psa.size(); ++i) {
        if (i > 0) result += "\n";
        result += psa[i];
    }
    return result;
}

} // namespace

JV handle_os_alert(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_alert called");

    auto* text_p = args.Find("text");
    if (!text_p || !text_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: text");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string text = text_p->GetString();
    std::string title = "Alert!";
    auto* title_p = args.Find("title");
    if (title_p && title_p->IsString()) {
        title = title_p->GetString();
    }

    os->alert(godot::String(text.c_str()), godot::String(title.c_str()));

    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_alert completed");
    return r;
}

JV handle_os_create_process(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_create_process called");

    auto* path_p = args.Find("path");
    if (!path_p || !path_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: path");
        return e;
    }

    auto* args_p = args.Find("arguments");
    if (!args_p || !args_p->IsArray()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: arguments (array)");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string path = path_p->GetString();
    const auto& json_args = args_p->GetArray();
    godot::PackedStringArray ps_args;
    for (const auto& a : json_args) {
        if (a.IsString()) {
            ps_args.append(godot::String(a.GetString().c_str()));
        }
    }

    int32_t pid = os->create_process(godot::String(path.c_str()), ps_args);

    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(pid));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_create_process completed");
    return r;
}

JV handle_os_execute(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_execute called");

    auto* path_p = args.Find("path");
    if (!path_p || !path_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: path");
        return e;
    }

    auto* args_p = args.Find("arguments");
    if (!args_p || !args_p->IsArray()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: arguments (array)");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string path = path_p->GetString();
    const auto& json_args = args_p->GetArray();
    godot::PackedStringArray ps_args;
    for (const auto& a : json_args) {
        if (a.IsString()) {
            ps_args.append(godot::String(a.GetString().c_str()));
        }
    }

    bool capture_output = false;
    auto* output_p = args.Find("output");
    if (output_p && output_p->IsBool()) {
        capture_output = output_p->GetBool();
    }

    godot::Dictionary d;
    if (capture_output) {
        godot::Array output_arr;
        output_arr.append(godot::PackedStringArray());
        int32_t exit_code = os->execute(
            godot::String(path.c_str()), ps_args, output_arr, true);

        d["exit_code"] = static_cast<int64_t>(exit_code);

        if (output_arr.size() > 0) {
            godot::PackedStringArray out_lines = output_arr[0];
            d["stdout"] = join_psa(out_lines);
        } else {
            d["stdout"] = godot::String();
        }

        if (output_arr.size() > 1) {
            godot::PackedStringArray err_lines = output_arr[1];
            d["stderr"] = join_psa(err_lines);
        } else {
            d["stderr"] = godot::String();
        }
    } else {
        godot::Array empty_arr;
        int32_t exit_code = os->execute(
            godot::String(path.c_str()), ps_args, empty_arr, false);
        d["exit_code"] = static_cast<int64_t>(exit_code);
        d["stdout"] = godot::String();
        d["stderr"] = godot::String();
    }

    JV r(JV::object_tag);
    r["result"] = VariantJson::serialize(d);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_execute completed");
    return r;
}

JV handle_os_get_datetime(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_datetime called");

    auto* time = godot::Time::get_singleton();
    if (!time) {
        JV e(JV::object_tag);
        e["error"] = JV("Time singleton not available");
        return e;
    }

    bool utc = false;
    auto* utc_p = args.Find("utc");
    if (utc_p && utc_p->IsBool()) {
        utc = utc_p->GetBool();
    }

    godot::Dictionary dt = time->get_datetime_dict_from_system(utc);

    JV r(JV::object_tag);
    r["result"] = VariantJson::serialize(dt);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_datetime completed");
    return r;
}

JV handle_os_get_environment(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_environment called");

    auto* var_p = args.Find("variable");
    if (!var_p || !var_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: variable");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string variable = var_p->GetString();
    godot::String value = os->get_environment(godot::String(variable.c_str()));

    JV r(JV::object_tag);
    r["result"] = JV(to_std(value));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_environment completed");
    return r;
}

JV handle_os_get_locale(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_locale called");

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    godot::String locale = os->get_locale();

    JV r(JV::object_tag);
    r["result"] = JV(to_std(locale));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_locale completed");
    return r;
}

JV handle_os_get_system_fonts(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_system_fonts called");

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    godot::PackedStringArray fonts = os->get_system_fonts();
    JV arr(JV::array_tag);
    for (int i = 0; i < fonts.size(); ++i) {
        arr.PushBack(JV(to_std(fonts[i])));
    }

    JV r(JV::object_tag);
    r["result"] = std::move(arr);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_system_fonts completed");
    return r;
}

JV handle_os_get_system_info(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_system_info called");

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    JV info(JV::object_tag);
    info["name"] = JV(to_std(os->get_name()));
    info["version"] = JV(to_std(os->get_version()));
    info["processor_count"] = JV(static_cast<int64_t>(os->get_processor_count()));
    info["processor_name"] = JV(to_std(os->get_processor_name()));

    JV r(JV::object_tag);
    r["result"] = std::move(info);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_system_info completed");
    return r;
}

JV handle_os_get_unique_id(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_unique_id called");

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    godot::String uid = os->get_unique_id();

    JV r(JV::object_tag);
    r["result"] = JV(to_std(uid));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_unique_id completed");
    return r;
}

JV handle_os_get_unix_time(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_unix_time called");

    auto* time = godot::Time::get_singleton();
    if (!time) {
        JV e(JV::object_tag);
        e["error"] = JV("Time singleton not available");
        return e;
    }

    double unix_time = time->get_unix_time_from_system();

    JV r(JV::object_tag);
    r["result"] = JV(unix_time);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_unix_time completed");
    return r;
}

JV handle_os_get_user_data_dir(const JV&) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_user_data_dir called");

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    godot::String dir = os->get_user_data_dir();

    JV r(JV::object_tag);
    r["result"] = JV(to_std(dir));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_get_user_data_dir completed");
    return r;
}

JV handle_os_kill(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_kill called");

    auto* pid_p = args.Find("pid");
    if (!pid_p || !pid_p->IsInt()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: pid (integer)");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    int32_t pid = static_cast<int32_t>(pid_p->GetInt());
    godot::Error err = os->kill(pid);

    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(err));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_kill completed");
    return r;
}

JV handle_os_move_to_trash(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_move_to_trash called");

    auto* path_p = args.Find("path");
    if (!path_p || !path_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: path");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string path = path_p->GetString();
    godot::Error err = os->move_to_trash(godot::String(path.c_str()));

    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(err));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_move_to_trash completed");
    return r;
}

JV handle_os_set_environment(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_set_environment called");

    auto* var_p = args.Find("variable");
    if (!var_p || !var_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: variable");
        return e;
    }

    auto* val_p = args.Find("value");
    if (!val_p || !val_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: value");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string variable = var_p->GetString();
    std::string value = val_p->GetString();
    os->set_environment(godot::String(variable.c_str()), godot::String(value.c_str()));

    JV r(JV::object_tag);
    r["result"] = JV("ok");
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_set_environment completed");
    return r;
}

JV handle_os_shell_open(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_shell_open called");

    auto* uri_p = args.Find("uri");
    if (!uri_p || !uri_p->IsString()) {
        JV e(JV::object_tag);
        e["error"] = JV("missing required parameter: uri");
        return e;
    }

    auto* os = godot::OS::get_singleton();
    if (!os) {
        JV e(JV::object_tag);
        e["error"] = JV("OS singleton not available");
        return e;
    }

    std::string uri = uri_p->GetString();
    godot::Error err = os->shell_open(godot::String(uri.c_str()));

    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(err));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "os_shell_open completed");
    return r;
}

} // namespace os_ops
} // namespace godot_self_driving
