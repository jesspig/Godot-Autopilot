#include "display_ops.hpp"
#include "capture_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <string>

namespace godot_autopilot {
namespace display_ops {

namespace {

using JV = mcp::JsonValue;

JV ok_json() {
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

int screen_from_args(const JV &args) {
  auto *sp = args.Find("screen");
  if (sp && sp->IsInt()) {
    return sp->GetInt();
  }
  return 0;
}

} // namespace

JV handle_clipboard_get(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_clipboard called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  godot::String text = ds->clipboard_get();
  JV r(JV::object_tag);
  r["result"] = JV(util::to_std(text));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_clipboard completed");
  return r;
}

JV handle_clipboard_set(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_clipboard called");
  auto *tp = args.Find("text");
  if (!tp || !tp->IsString()) {
    return util::error_json("missing required parameter: text");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  ds->clipboard_set(godot::String(tp->GetString().c_str()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_clipboard completed");
  return ok_json();
}

JV handle_dialog_show(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "show_display_dialog called");
  auto *tp = args.Find("title");
  if (!tp || !tp->IsString()) {
    return util::error_json("missing required parameter: title");
  }
  auto *dp = args.Find("description");
  if (!dp || !dp->IsString()) {
    return util::error_json("missing required parameter: description");
  }
  auto *bp = args.Find("buttons");
  if (!bp || !bp->IsArray()) {
    return util::error_json("missing required parameter: buttons");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  godot::PackedStringArray buttons;
  for (const auto &b : bp->GetArray()) {
    if (b.IsString()) {
      buttons.push_back(godot::String(b.GetString().c_str()));
    }
  }
  godot::Error err = ds->dialog_show(godot::String(tp->GetString().c_str()),
                                     godot::String(dp->GetString().c_str()),
                                     buttons, godot::Callable());
  if (err != godot::OK) {
    return util::error_json("dialog_show failed with error: " +
                      std::to_string(static_cast<int>(err)));
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "show_display_dialog completed");
  return ok_json();
}

JV handle_mouse_get_position(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_mouse_position called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  godot::Vector2i pos = ds->mouse_get_position();
  JV r(JV::object_tag);
  JV pos_obj(JV::object_tag);
  pos_obj["x"] = JV(static_cast<int64_t>(pos.x));
  pos_obj["y"] = JV(static_cast<int64_t>(pos.y));
  r["result"] = std::move(pos_obj);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_mouse_position completed");
  return r;
}

JV handle_mouse_set_mode(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_mouse_mode called");
  auto *mp = args.Find("mode");
  if (!mp || !mp->IsInt()) {
    return util::error_json("missing required parameter: mode");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int mode = mp->GetInt();
  if (mode < 0 || mode > 4) {
    return util::error_json("invalid mouse mode: " + std::to_string(mode));
  }
  ds->mouse_set_mode(static_cast<godot::DisplayServer::MouseMode>(mode));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_mouse_mode completed");
  return ok_json();
}

JV handle_mouse_warp(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "warp_display_mouse called");
  auto *xp = args.Find("x");
  if (!xp || !xp->IsInt()) {
    return util::error_json("missing required parameter: x");
  }
  auto *yp = args.Find("y");
  if (!yp || !yp->IsInt()) {
    return util::error_json("missing required parameter: y");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  ds->warp_mouse(godot::Vector2i(xp->GetInt(), yp->GetInt()));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "warp_display_mouse completed");
  return ok_json();
}

JV handle_screen_capture(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_display_screen called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = screen_from_args(args);
  godot::Ref<godot::Image> image = ds->screen_get_image(screen);
  if (image.is_null()) {
    return util::error_json("failed to capture screen image");
  }
  godot::PackedByteArray png_buffer = image->save_png_to_buffer();
  if (png_buffer.size() == 0) {
    auto serialized = VariantJson::serialize(godot::Variant(image));
    JV r(JV::object_tag);
    r["result"] = std::move(serialized);
    r["warning"] =
        JV("PNG encoding failed; returned raw serialized image data");
    LogSystem::instance().log(
        LogLevel::Info, LogCategory::Tools,
        "capture_display_screen completed (png fallback)");
    return r;
  }
  std::string b64 = capture_ops::base64_encode(
      png_buffer.ptr(), static_cast<size_t>(png_buffer.size()));
  JV inner(JV::object_tag);
  inner["mime"] = JV("image/png");
  inner["format"] = JV("png");
  inner["data"] = JV(b64);
  JV r(JV::object_tag);
  r["result"] = std::move(inner);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "capture_display_screen completed");
  return r;
}

JV handle_screen_get_count(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_count called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int count = ds->get_screen_count();
  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(count));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_count completed");
  return r;
}

JV handle_screen_get_dpi(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_dpi called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = screen_from_args(args);
  int dpi = ds->screen_get_dpi(screen);
  JV r(JV::object_tag);
  r["result"] = JV(static_cast<int64_t>(dpi));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_dpi completed");
  return r;
}

JV handle_screen_get_position(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_position called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = screen_from_args(args);
  godot::Vector2i pos = ds->screen_get_position(screen);
  JV r(JV::object_tag);
  JV pos_obj(JV::object_tag);
  pos_obj["x"] = JV(static_cast<int64_t>(pos.x));
  pos_obj["y"] = JV(static_cast<int64_t>(pos.y));
  r["result"] = std::move(pos_obj);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_position completed");
  return r;
}

JV handle_screen_get_refresh_rate(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_refresh_rate called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = screen_from_args(args);
  float rate = ds->screen_get_refresh_rate(screen);
  JV r(JV::object_tag);
  r["result"] = JV(static_cast<double>(rate));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_refresh_rate completed");
  return r;
}

JV handle_screen_get_size(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_size called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = screen_from_args(args);
  godot::Vector2i size = ds->screen_get_size(screen);
  JV r(JV::object_tag);
  JV size_obj(JV::object_tag);
  size_obj["x"] = JV(static_cast<int64_t>(size.x));
  size_obj["y"] = JV(static_cast<int64_t>(size.y));
  r["result"] = std::move(size_obj);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_screen_size completed");
  return r;
}

JV handle_tts_get_voices(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_tts_voices called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  godot::TypedArray<godot::Dictionary> voices = ds->tts_get_voices();
  auto serialized = VariantJson::serialize(godot::Variant(voices));
  JV r(JV::object_tag);
  r["result"] = std::move(serialized);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_tts_voices completed");
  return r;
}

JV handle_tts_speak(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "speak_display_tts called");
  auto *tp = args.Find("text");
  if (!tp || !tp->IsString()) {
    return util::error_json("missing required parameter: text");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  std::string text = tp->GetString();
  std::string voice;
  auto *vp = args.Find("voice");
  if (vp && vp->IsString()) {
    voice = vp->GetString();
  }
  int volume = 50;
  auto *volp = args.Find("volume");
  if (volp && volp->IsInt()) {
    volume = volp->GetInt();
  }
  float pitch = 1.0f;
  auto *pp = args.Find("pitch");
  if (pp && pp->IsNumber()) {
    pitch = static_cast<float>(
        pp->IsDouble() ? pp->GetDouble() : static_cast<double>(pp->GetInt()));
  }
  float rate = 1.0f;
  auto *rp = args.Find("rate");
  if (rp && rp->IsNumber()) {
    rate = static_cast<float>(
        rp->IsDouble() ? rp->GetDouble() : static_cast<double>(rp->GetInt()));
  }
  ds->tts_speak(godot::String(text.c_str()), godot::String(voice.c_str()),
                volume, pitch, rate);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "speak_display_tts completed");
  return ok_json();
}

JV handle_tts_stop(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_display_tts called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  ds->tts_stop();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "stop_display_tts completed");
  return ok_json();
}

} // namespace display_ops
} // namespace godot_autopilot
