#include "display_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>

namespace godot_self_driving {
namespace display_ops {

namespace {

using JV = mcp::JsonValue;

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

JV error_json(const std::string& msg) {
    JV e(JV::object_tag);
    e["error"] = JV(msg);
    return e;
}

JV ok_json() {
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    return r;
}

int screen_from_args(const JV& args) {
    auto* sp = args.Find("screen");
    if (sp && sp->IsInt()) {
        return sp->GetInt();
    }
    return 0;
}

int window_id_from_args(const JV& args) {
    auto* wp = args.Find("window_id");
    if (wp && wp->IsInt()) {
        return wp->GetInt();
    }
    return 0;
}

} // namespace

JV handle_clipboard_get(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_clipboard_get called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    godot::String text = ds->clipboard_get();
    JV r(JV::object_tag);
    r["result"] = JV(to_std(text));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_clipboard_get completed");
    return r;
}

JV handle_clipboard_set(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_clipboard_set called");
    auto* tp = args.Find("text");
    if (!tp || !tp->IsString()) {
        return error_json("missing required parameter: text");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    ds->clipboard_set(godot::String(tp->GetString().c_str()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_clipboard_set completed");
    return ok_json();
}

JV handle_dialog_show(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_dialog_show called");
    auto* tp = args.Find("title");
    if (!tp || !tp->IsString()) {
        return error_json("missing required parameter: title");
    }
    auto* dp = args.Find("description");
    if (!dp || !dp->IsString()) {
        return error_json("missing required parameter: description");
    }
    auto* bp = args.Find("buttons");
    if (!bp || !bp->IsArray()) {
        return error_json("missing required parameter: buttons");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    godot::PackedStringArray buttons;
    for (const auto& b : bp->GetArray()) {
        if (b.IsString()) {
            buttons.push_back(godot::String(b.GetString().c_str()));
        }
    }
    godot::Error err = ds->dialog_show(
        godot::String(tp->GetString().c_str()),
        godot::String(dp->GetString().c_str()),
        buttons,
        godot::Callable()
    );
    if (err != godot::OK) {
        return error_json("dialog_show failed with error: " + std::to_string(static_cast<int>(err)));
    }
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_dialog_show completed");
    return ok_json();
}

JV handle_mouse_get_position(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_get_position called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    godot::Vector2i pos = ds->mouse_get_position();
    JV r(JV::object_tag);
    JV pos_obj(JV::object_tag);
    pos_obj["x"] = JV(static_cast<int64_t>(pos.x));
    pos_obj["y"] = JV(static_cast<int64_t>(pos.y));
    r["result"] = std::move(pos_obj);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_get_position completed");
    return r;
}

JV handle_mouse_set_mode(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_set_mode called");
    auto* mp = args.Find("mode");
    if (!mp || !mp->IsInt()) {
        return error_json("missing required parameter: mode");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int mode = mp->GetInt();
    if (mode < 0 || mode > 4) {
        return error_json("invalid mouse mode: " + std::to_string(mode));
    }
    ds->mouse_set_mode(static_cast<godot::DisplayServer::MouseMode>(mode));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_set_mode completed");
    return ok_json();
}

JV handle_mouse_warp(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_warp called");
    auto* xp = args.Find("x");
    if (!xp || !xp->IsInt()) {
        return error_json("missing required parameter: x");
    }
    auto* yp = args.Find("y");
    if (!yp || !yp->IsInt()) {
        return error_json("missing required parameter: y");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    ds->warp_mouse(godot::Vector2i(xp->GetInt(), yp->GetInt()));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_mouse_warp completed");
    return ok_json();
}

JV handle_screen_capture(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_capture called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int screen = screen_from_args(args);
    godot::Ref<godot::Image> image = ds->screen_get_image(screen);
    if (image.is_null()) {
        return error_json("failed to capture screen image");
    }
    auto serialized = VariantJson::serialize(godot::Variant(image));
    JV r(JV::object_tag);
    r["result"] = std::move(serialized);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_capture completed");
    return r;
}

JV handle_screen_get_count(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_count called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int count = ds->get_screen_count();
    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(count));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_count completed");
    return r;
}

JV handle_screen_get_dpi(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_dpi called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int screen = screen_from_args(args);
    int dpi = ds->screen_get_dpi(screen);
    JV r(JV::object_tag);
    r["result"] = JV(static_cast<int64_t>(dpi));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_dpi completed");
    return r;
}

JV handle_screen_get_position(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_position called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int screen = screen_from_args(args);
    godot::Vector2i pos = ds->screen_get_position(screen);
    JV r(JV::object_tag);
    JV pos_obj(JV::object_tag);
    pos_obj["x"] = JV(static_cast<int64_t>(pos.x));
    pos_obj["y"] = JV(static_cast<int64_t>(pos.y));
    r["result"] = std::move(pos_obj);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_position completed");
    return r;
}

JV handle_screen_get_refresh_rate(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_refresh_rate called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int screen = screen_from_args(args);
    float rate = ds->screen_get_refresh_rate(screen);
    JV r(JV::object_tag);
    r["result"] = JV(static_cast<double>(rate));
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_refresh_rate completed");
    return r;
}

JV handle_screen_get_size(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_size called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int screen = screen_from_args(args);
    godot::Vector2i size = ds->screen_get_size(screen);
    JV r(JV::object_tag);
    JV size_obj(JV::object_tag);
    size_obj["x"] = JV(static_cast<int64_t>(size.x));
    size_obj["y"] = JV(static_cast<int64_t>(size.y));
    r["result"] = std::move(size_obj);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_screen_get_size completed");
    return r;
}

JV handle_tts_get_voices(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_get_voices called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    godot::TypedArray<godot::Dictionary> voices = ds->tts_get_voices();
    auto serialized = VariantJson::serialize(godot::Variant(voices));
    JV r(JV::object_tag);
    r["result"] = std::move(serialized);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_get_voices completed");
    return r;
}

JV handle_tts_speak(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_speak called");
    auto* tp = args.Find("text");
    if (!tp || !tp->IsString()) {
        return error_json("missing required parameter: text");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    std::string text = tp->GetString();
    std::string voice;
    auto* vp = args.Find("voice");
    if (vp && vp->IsString()) {
        voice = vp->GetString();
    }
    int volume = 50;
    auto* volp = args.Find("volume");
    if (volp && volp->IsInt()) {
        volume = volp->GetInt();
    }
    float pitch = 1.0f;
    auto* pp = args.Find("pitch");
    if (pp && pp->IsNumber()) {
        pitch = static_cast<float>(pp->IsDouble() ? pp->GetDouble() : static_cast<double>(pp->GetInt()));
    }
    float rate = 1.0f;
    auto* rp = args.Find("rate");
    if (rp && rp->IsNumber()) {
        rate = static_cast<float>(rp->IsDouble() ? rp->GetDouble() : static_cast<double>(rp->GetInt()));
    }
    ds->tts_speak(
        godot::String(text.c_str()),
        godot::String(voice.c_str()),
        volume,
        pitch,
        rate
    );
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_speak completed");
    return ok_json();
}

JV handle_tts_stop(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_stop called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    ds->tts_stop();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_tts_stop completed");
    return ok_json();
}

JV handle_window_create(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_create called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    if (!ds->has_feature(godot::DisplayServer::FEATURE_SUBWINDOWS)) {
        return error_json("subwindows not supported on this platform");
    }
    int mode = 0;
    auto* mp = args.Find("mode");
    if (mp && mp->IsInt()) {
        mode = mp->GetInt();
    }
    int rx = 0, ry = 0, rw = 800, rh = 600;
    auto* rp = args.Find("rect");
    if (rp && rp->IsObject()) {
        auto* xp = rp->Find("x");
        if (xp && xp->IsInt()) rx = xp->GetInt();
        auto* yp = rp->Find("y");
        if (yp && yp->IsInt()) ry = yp->GetInt();
        auto* wp = rp->Find("w");
        if (wp && wp->IsInt()) rw = wp->GetInt();
        auto* hp = rp->Find("h");
        if (hp && hp->IsInt()) rh = hp->GetInt();
    }
    auto* window = memnew(godot::Window);
    window->set_position(godot::Vector2i(rx, ry));
    window->set_size(godot::Vector2i(rw, rh));
    window->set_mode(static_cast<godot::Window::Mode>(mode));
    window->set_visible(true);
    int64_t window_id = window->get_window_id();
    JV r(JV::object_tag);
    r["result"] = JV(window_id);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_create completed");
    return r;
}

JV handle_window_delete(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_delete called");
    auto* wp = args.Find("window_id");
    if (!wp || !wp->IsInt()) {
        return error_json("missing required parameter: window_id");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int64_t window_id = wp->GetInt();
    uint64_t instance_id = ds->window_get_attached_instance_id(window_id);
    if (instance_id == 0) {
        return error_json("no window found with the given id");
    }
    auto* obj = godot::UtilityFunctions::instance_from_id(static_cast<int64_t>(instance_id));
    auto* window = godot::Object::cast_to<godot::Window>(obj);
    if (!window) {
        return error_json("object is not a Window");
    }
    window->queue_free();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_delete completed");
    return ok_json();
}

JV handle_window_move_to_foreground(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_move_to_foreground called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_move_to_foreground(wid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_move_to_foreground completed");
    return ok_json();
}

JV handle_window_request_attention(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_request_attention called");
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_request_attention(wid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_request_attention completed");
    return ok_json();
}

JV handle_window_set_flag(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_flag called");
    auto* fp = args.Find("flag");
    if (!fp || !fp->IsInt()) {
        return error_json("missing required parameter: flag");
    }
    auto* ep = args.Find("enabled");
    if (!ep || !ep->IsBool()) {
        return error_json("missing required parameter: enabled");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_set_flag(
        static_cast<godot::DisplayServer::WindowFlags>(fp->GetInt()),
        ep->GetBool(),
        wid
    );
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_flag completed");
    return ok_json();
}

JV handle_window_set_mode(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_mode called");
    auto* mp = args.Find("mode");
    if (!mp || !mp->IsInt()) {
        return error_json("missing required parameter: mode");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_set_mode(
        static_cast<godot::DisplayServer::WindowMode>(mp->GetInt()),
        wid
    );
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_mode completed");
    return ok_json();
}

JV handle_window_set_position(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_position called");
    auto* xp = args.Find("x");
    if (!xp || !xp->IsInt()) {
        return error_json("missing required parameter: x");
    }
    auto* yp = args.Find("y");
    if (!yp || !yp->IsInt()) {
        return error_json("missing required parameter: y");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_set_position(godot::Vector2i(xp->GetInt(), yp->GetInt()), wid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_position completed");
    return ok_json();
}

JV handle_window_set_size(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_size called");
    auto* wp = args.Find("width");
    if (!wp || !wp->IsInt()) {
        return error_json("missing required parameter: width");
    }
    auto* hp = args.Find("height");
    if (!hp || !hp->IsInt()) {
        return error_json("missing required parameter: height");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_set_size(godot::Vector2i(wp->GetInt(), hp->GetInt()), wid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_size completed");
    return ok_json();
}

JV handle_window_set_title(const JV& args) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_title called");
    auto* tp = args.Find("title");
    if (!tp || !tp->IsString()) {
        return error_json("missing required parameter: title");
    }
    auto* ds = godot::DisplayServer::get_singleton();
    if (!ds) {
        return error_json("DisplayServer not available");
    }
    int wid = window_id_from_args(args);
    ds->window_set_title(godot::String(tp->GetString().c_str()), wid);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "display_window_set_title completed");
    return ok_json();
}

} // namespace display_ops
} // namespace godot_self_driving
