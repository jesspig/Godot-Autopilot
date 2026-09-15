#include "display_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace display_ops {

namespace {

using JV = mcp::JsonValue;

int window_id_from_args(const JV &args) {
  auto *wp = args.Find("window_id");
  if (wp && wp->IsInt()) {
    return wp->GetInt();
  }
  return 0;
}

JV vec2i_to_json(const godot::Vector2i &value) {
  JV o(JV::object_tag);
  o["x"] = JV(static_cast<int64_t>(value.x));
  o["y"] = JV(static_cast<int64_t>(value.y));
  return o;
}

} // namespace

JV handle_window_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_display_window called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  if (!ds->has_feature(godot::DisplayServer::FEATURE_SUBWINDOWS)) {
    return util::error_json("subwindows not supported on this platform");
  }
  int mode = 0;
  auto *mp = args.Find("mode");
  if (mp && mp->IsInt()) {
    mode = mp->GetInt();
  }
  int rx = 0, ry = 0, rw = 800, rh = 600;
  auto *rp = args.Find("rect");
  if (rp && rp->IsObject()) {
    auto *xp = rp->Find("x");
    if (xp && xp->IsInt())
      rx = xp->GetInt();
    auto *yp = rp->Find("y");
    if (yp && yp->IsInt())
      ry = yp->GetInt();
    auto *wp = rp->Find("w");
    if (wp && wp->IsInt())
      rw = wp->GetInt();
    auto *hp = rp->Find("h");
    if (hp && hp->IsInt())
      rh = hp->GetInt();
  }
  auto *window = memnew(godot::Window);
  window->set_position(godot::Vector2i(rx, ry));
  window->set_size(godot::Vector2i(rw, rh));
  window->set_mode(static_cast<godot::Window::Mode>(mode));
  window->set_visible(true);
  int64_t window_id = window->get_window_id();
  JV r(JV::object_tag);
  r["result"] = JV(window_id);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "create_display_window completed");
  return r;
}

JV handle_window_delete(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "delete_display_window called");
  auto *wp = args.Find("window_id");
  if (!wp || !wp->IsInt()) {
    return util::error_json("missing required parameter: window_id");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int64_t window_id = wp->GetInt();
  uint64_t instance_id = ds->window_get_attached_instance_id(window_id);
  if (instance_id == 0) {
    return util::error_json("no window found with the given id");
  }
  auto *obj = godot::UtilityFunctions::instance_from_id(
      static_cast<int64_t>(instance_id));
  auto *window = godot::Object::cast_to<godot::Window>(obj);
  if (!window) {
    return util::error_json("object is not a Window");
  }
  window->queue_free();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "delete_display_window completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_get_rect(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_window_rect called");
  auto *wp = args.Find("window_id");
  if (wp && !wp->IsInt()) {
    return util::error_json("invalid parameter: window_id must be an integer");
  }
  int wid = window_id_from_args(args);
  if (wid < 0) {
    return util::error_json("invalid window_id: " + std::to_string(wid));
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int screen = ds->window_get_current_screen(wid);
  JV result(JV::object_tag);
  result["window_id"] = JV(static_cast<int64_t>(wid));
  result["position"] = vec2i_to_json(ds->window_get_position(wid));
  result["size"] = vec2i_to_json(ds->window_get_size(wid));
  result["decorated_position"] =
      vec2i_to_json(ds->window_get_position_with_decorations(wid));
  result["decorated_size"] =
      vec2i_to_json(ds->window_get_size_with_decorations(wid));
  result["screen"] = JV(static_cast<int64_t>(screen));
  result["screen_scale"] =
      JV(static_cast<double>(ds->screen_get_scale(screen)));
  result["screen_position"] = vec2i_to_json(ds->screen_get_position(screen));
  result["screen_size"] = vec2i_to_json(ds->screen_get_size(screen));
  result["note"] = JV(
      "position and size describe the window's client area in virtual desktop "
      "coordinates; the input injection tools (warp_display_mouse, "
      "move_input_mouse, press_input_mouse_button) take client-area-relative "
      "coordinates, so subtract position from a desktop point to target it");
  JV r(JV::object_tag);
  r["result"] = std::move(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_display_window_rect completed");
  return r;
}

JV handle_window_move_to_foreground(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_display_window_to_foreground called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_move_to_foreground(wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "move_display_window_to_foreground completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_request_attention(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "request_display_window_attention called");
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_request_attention(wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "request_display_window_attention completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_set_flag(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_flag called");
  auto *fp = args.Find("flag");
  if (!fp || !fp->IsInt()) {
    return util::error_json("missing required parameter: flag");
  }
  auto *ep = args.Find("enabled");
  if (!ep || !ep->IsBool()) {
    return util::error_json("missing required parameter: enabled");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_set_flag(
      static_cast<godot::DisplayServer::WindowFlags>(fp->GetInt()),
      ep->GetBool(), wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_flag completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_set_mode(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_mode called");
  auto *mp = args.Find("mode");
  if (!mp || !mp->IsInt()) {
    return util::error_json("missing required parameter: mode");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_set_mode(
      static_cast<godot::DisplayServer::WindowMode>(mp->GetInt()), wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_mode completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_set_position(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_position called");
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
  int wid = window_id_from_args(args);
  ds->window_set_position(godot::Vector2i(xp->GetInt(), yp->GetInt()), wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_position completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_set_size(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_size called");
  auto *wp = args.Find("width");
  if (!wp || !wp->IsInt()) {
    return util::error_json("missing required parameter: width");
  }
  auto *hp = args.Find("height");
  if (!hp || !hp->IsInt()) {
    return util::error_json("missing required parameter: height");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_set_size(godot::Vector2i(wp->GetInt(), hp->GetInt()), wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_size completed");
  return util::ok_result(JV("ok"));
}

JV handle_window_set_title(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_title called");
  auto *tp = args.Find("title");
  if (!tp || !tp->IsString()) {
    return util::error_json("missing required parameter: title");
  }
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds) {
    return util::error_json("DisplayServer not available");
  }
  int wid = window_id_from_args(args);
  ds->window_set_title(godot::String(tp->GetString().c_str()), wid);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_display_window_title completed");
  return util::ok_result(JV("ok"));
}

} // namespace display_ops
} // namespace godot_autopilot
