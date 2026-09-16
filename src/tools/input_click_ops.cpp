#include "input_click_ops.hpp"
#include "core/log_system.hpp"
#include "tools/capture_ops.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_from_window.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_with_modifiers.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <cmath>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace input_click_ops {

godot::Key parse_key_name(const std::string &key_str) {
  if (key_str.empty())
    return godot::KEY_NONE;

  if (key_str.size() == 1) {
    char c = key_str[0];
    if (c >= 'a' && c <= 'z')
      c -= 32;
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      return static_cast<godot::Key>(c);
    }
  }

  std::string u = key_str;
  for (auto &c : u) {
    if (c >= 'a' && c <= 'z')
      c -= 32;
  }

  if (u == "SPACE")
    return godot::KEY_SPACE;
  if (u == "ENTER")
    return godot::KEY_ENTER;
  if (u == "ESCAPE")
    return godot::KEY_ESCAPE;
  if (u == "SHIFT")
    return godot::KEY_SHIFT;
  if (u == "CONTROL" || u == "CTRL")
    return godot::KEY_CTRL;
  if (u == "ALT")
    return godot::KEY_ALT;
  if (u == "TAB")
    return godot::KEY_TAB;
  if (u == "BACKSPACE")
    return godot::KEY_BACKSPACE;
  if (u == "DELETE")
    return godot::KEY_DELETE;
  if (u == "LEFT")
    return godot::KEY_LEFT;
  if (u == "RIGHT")
    return godot::KEY_RIGHT;
  if (u == "UP")
    return godot::KEY_UP;
  if (u == "DOWN")
    return godot::KEY_DOWN;

  return godot::KEY_NONE;
}

godot::MouseButton parse_mouse_button_name(const std::string &button_str) {
  std::string u = button_str;
  for (auto &c : u) {
    if (c >= 'a' && c <= 'z')
      c -= 32;
  }

  if (u == "LEFT")
    return godot::MOUSE_BUTTON_LEFT;
  if (u == "RIGHT")
    return godot::MOUSE_BUTTON_RIGHT;
  if (u == "MIDDLE")
    return godot::MOUSE_BUTTON_MIDDLE;

  return godot::MOUSE_BUTTON_NONE;
}

bool warp_to(godot::Vector2 pos) {
  auto *ds = godot::DisplayServer::get_singleton();
  if (!ds)
    return false;
  ds->warp_mouse(godot::Vector2i(static_cast<int32_t>(pos.x),
                                 static_cast<int32_t>(pos.y)));
  return true;
}

bool inject_mouse_motion(godot::Vector2 pos, godot::Vector2 relative,
                         int window_id) {
  auto *input = godot::Input::get_singleton();
  if (!input)
    return false;

  godot::Ref<godot::InputEventMouseMotion> ev;
  ev.instantiate();
  ev->set_position(pos);
  ev->set_global_position(pos);
  ev->set_relative(relative);
  if (window_id != 0)
    ev->set_window_id(window_id);

  input->parse_input_event(ev);
  return true;
}

namespace {

bool inject_mouse_button_impl(godot::Vector2 pos, godot::MouseButton button,
                              bool pressed, bool double_click) {
  auto *input = godot::Input::get_singleton();
  if (!input)
    return false;

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(pressed);
  ev->set_button_index(button);
  ev->set_position(pos);
  ev->set_global_position(pos);
  ev->set_double_click(double_click);

  input->parse_input_event(ev);
  return true;
}

} // namespace

bool inject_mouse_button(godot::Vector2 pos, godot::MouseButton button,
                         bool pressed, int window_id) {
  if (window_id == 0)
    return inject_mouse_button_impl(pos, button, pressed, false);
  auto *input = godot::Input::get_singleton();
  if (!input)
    return false;

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(pressed);
  ev->set_button_index(button);
  ev->set_position(pos);
  ev->set_global_position(pos);
  ev->set_double_click(false);
  ev->set_window_id(window_id);

  input->parse_input_event(ev);
  return true;
}

bool inject_wheel(godot::Vector2 pos, godot::MouseButton wheel, int amount,
                  int window_id) {
  auto *input = godot::Input::get_singleton();
  if (!input)
    return false;

  godot::Ref<godot::InputEventMouseButton> press_ev;
  press_ev.instantiate();
  press_ev->set_pressed(true);
  press_ev->set_button_index(wheel);
  press_ev->set_position(pos);
  press_ev->set_global_position(pos);
  press_ev->set_factor(static_cast<float>(amount));
  if (window_id != 0)
    press_ev->set_window_id(window_id);
  input->parse_input_event(press_ev);

  godot::Ref<godot::InputEventMouseButton> release_ev;
  release_ev.instantiate();
  release_ev->set_pressed(false);
  release_ev->set_button_index(wheel);
  release_ev->set_position(pos);
  release_ev->set_global_position(pos);
  release_ev->set_factor(static_cast<float>(amount));
  if (window_id != 0)
    release_ev->set_window_id(window_id);
  input->parse_input_event(release_ev);

  return true;
}

bool inject_key(godot::Key key, bool pressed, bool ctrl, bool shift, bool alt,
                int window_id) {
  auto *input = godot::Input::get_singleton();
  if (!input)
    return false;

  godot::Ref<godot::InputEventKey> ev;
  ev.instantiate();
  ev->set_pressed(pressed);
  ev->set_keycode(key);
  ev->set_ctrl_pressed(ctrl);
  ev->set_shift_pressed(shift);
  ev->set_alt_pressed(alt);
  if (window_id != 0)
    ev->set_window_id(window_id);

  input->parse_input_event(ev);
  return true;
}

namespace {

bool extract_vec2(const mcp::JsonValue &obj, godot::Vector2 &out) {
  auto *xp = obj.Find("x");
  auto *yp = obj.Find("y");
  if (!xp || !xp->IsNumber())
    return false;
  if (!yp || !yp->IsNumber())
    return false;
  const double x =
      xp->IsInt() ? static_cast<double>(xp->GetInt()) : xp->GetDouble();
  const double y =
      yp->IsInt() ? static_cast<double>(yp->GetInt()) : yp->GetDouble();
  out = godot::Vector2(static_cast<godot::real_t>(x),
                       static_cast<godot::real_t>(y));
  return true;
}

bool read_bool_arg(const mcp::JsonValue &args, const char *name,
                   bool default_value, bool &out, std::string &error) {
  out = default_value;
  auto *p = args.Find(name);
  if (!p)
    return true;
  if (!p->IsBool()) {
    error = std::string("invalid parameter: ") + name + " must be a boolean";
    return false;
  }
  out = p->GetBool();
  return true;
}

bool read_int_arg(const mcp::JsonValue &args, const char *name,
                  int default_value, int min_value, int max_value, int &out,
                  std::string &error) {
  out = default_value;
  auto *p = args.Find(name);
  if (!p)
    return true;
  if (!p->IsNumber()) {
    error = std::string("invalid parameter: ") + name + " must be an integer";
    return false;
  }
  const double raw =
      p->IsInt() ? static_cast<double>(p->GetInt()) : p->GetDouble();
  if (raw < min_value || raw > max_value || raw != std::floor(raw)) {
    error = std::string("invalid ") + name + ": must be an integer between " +
            std::to_string(min_value) + " and " + std::to_string(max_value);
    return false;
  }
  out = static_cast<int>(raw);
  return true;
}

bool read_button_arg(const mcp::JsonValue &args, godot::MouseButton &out,
                     std::string &error) {
  out = godot::MOUSE_BUTTON_LEFT;
  auto *p = args.Find("button");
  if (!p)
    return true;
  if (!p->IsString()) {
    error = "invalid parameter: button must be a string";
    return false;
  }
  const std::string button_str = p->GetString();
  out = parse_mouse_button_name(button_str);
  if (out == godot::MOUSE_BUTTON_NONE) {
    error = "invalid mouse button: " + button_str;
    return false;
  }
  return true;
}

void merge_observe_result(mcp::JsonValue &inner) {
  mcp::JsonValue capture = capture_ops::handle_capture_viewport(
      mcp::JsonValue::Parse(R"({"target":"editor"})"));
  const mcp::JsonValue *capture_result = capture.Find("result");
  if (capture_result && capture_result->IsObject()) {
    const char *keys[] = {"data", "format", "width", "height", "path"};
    for (const char *key : keys) {
      if (const mcp::JsonValue *value = capture_result->Find(key)) {
        inner[key] = *value;
      }
    }
    return;
  }
  const mcp::JsonValue *error = capture.Find("error");
  if (error && error->IsString()) {
    inner["observe_error"] = *error;
  } else {
    inner["observe_error"] = mcp::JsonValue("capture failed");
  }
}

} // namespace

mcp::JsonValue handle_click_mouse(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "click_input_mouse called");

  auto *pos_p = args.Find("position");
  if (!pos_p || !pos_p->IsObject())
    return util::error_json("missing required parameter: position");
  godot::Vector2 pos;
  if (!extract_vec2(*pos_p, pos))
    return util::error_json("position must have numeric x and y fields");

  std::string error;
  godot::MouseButton button = godot::MOUSE_BUTTON_LEFT;
  if (!read_button_arg(args, button, error))
    return util::error_json(error);

  bool double_click = false;
  if (!read_bool_arg(args, "double_click", false, double_click, error))
    return util::error_json(error);

  bool warp = true;
  if (!read_bool_arg(args, "warp", true, warp, error))
    return util::error_json(error);

  bool observe = false;
  if (!read_bool_arg(args, "observe", false, observe, error))
    return util::error_json(error);

  if (warp && !warp_to(pos))
    return util::error_json("DisplayServer not available");

  if (!inject_mouse_motion(pos, godot::Vector2()))
    return util::error_json("Input singleton not available");
  if (!inject_mouse_button(pos, button, true))
    return util::error_json("Input singleton not available");
  if (!inject_mouse_button(pos, button, false))
    return util::error_json("Input singleton not available");
  if (double_click) {
    if (!inject_mouse_button_impl(pos, button, true, true))
      return util::error_json("Input singleton not available");
    if (!inject_mouse_button_impl(pos, button, false, false))
      return util::error_json("Input singleton not available");
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  if (observe)
    merge_observe_result(inner);
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_scroll_mouse(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "scroll_input_mouse called");

  auto *dir_p = args.Find("direction");
  if (!dir_p || !dir_p->IsString())
    return util::error_json("missing required parameter: direction");
  std::string direction = dir_p->GetString();
  for (auto &c : direction) {
    if (c >= 'A' && c <= 'Z')
      c += 32;
  }
  godot::MouseButton wheel = godot::MOUSE_BUTTON_NONE;
  if (direction == "up")
    wheel = godot::MOUSE_BUTTON_WHEEL_UP;
  else if (direction == "down")
    wheel = godot::MOUSE_BUTTON_WHEEL_DOWN;
  else if (direction == "left")
    wheel = godot::MOUSE_BUTTON_WHEEL_LEFT;
  else if (direction == "right")
    wheel = godot::MOUSE_BUTTON_WHEEL_RIGHT;
  else
    return util::error_json("invalid scroll direction: " + direction);

  auto *pos_p = args.Find("position");
  if (!pos_p || !pos_p->IsObject())
    return util::error_json("missing required parameter: position");
  godot::Vector2 pos;
  if (!extract_vec2(*pos_p, pos))
    return util::error_json("position must have numeric x and y fields");

  std::string error;
  int amount = 1;
  if (!read_int_arg(args, "amount", 1, 1, 10, amount, error))
    return util::error_json(error);

  bool warp = true;
  if (!read_bool_arg(args, "warp", true, warp, error))
    return util::error_json(error);

  bool observe = false;
  if (!read_bool_arg(args, "observe", false, observe, error))
    return util::error_json(error);

  if (warp && !warp_to(pos))
    return util::error_json("DisplayServer not available");

  if (!inject_mouse_motion(pos, godot::Vector2()))
    return util::error_json("Input singleton not available");
  if (!inject_wheel(pos, wheel, amount))
    return util::error_json("Input singleton not available");

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  if (observe)
    merge_observe_result(inner);
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_drag_mouse(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "drag_input_mouse called");

  auto *from_p = args.Find("from");
  if (!from_p || !from_p->IsObject())
    return util::error_json("missing required parameter: from");
  godot::Vector2 from;
  if (!extract_vec2(*from_p, from))
    return util::error_json("from must have numeric x and y fields");

  auto *to_p = args.Find("to");
  if (!to_p || !to_p->IsObject())
    return util::error_json("missing required parameter: to");
  godot::Vector2 to;
  if (!extract_vec2(*to_p, to))
    return util::error_json("to must have numeric x and y fields");

  std::string error;
  godot::MouseButton button = godot::MOUSE_BUTTON_LEFT;
  if (!read_button_arg(args, button, error))
    return util::error_json(error);

  int steps = 8;
  if (!read_int_arg(args, "steps", 8, 1, 64, steps, error))
    return util::error_json(error);

  bool warp = true;
  if (!read_bool_arg(args, "warp", true, warp, error))
    return util::error_json(error);

  bool observe = false;
  if (!read_bool_arg(args, "observe", false, observe, error))
    return util::error_json(error);

  if (warp && !warp_to(from))
    return util::error_json("DisplayServer not available");

  if (!inject_mouse_motion(from, godot::Vector2()))
    return util::error_json("Input singleton not available");
  if (!inject_mouse_button(from, button, true))
    return util::error_json("Input singleton not available");

  const godot::Vector2 delta =
      (to - from) / static_cast<godot::real_t>(steps);
  for (int i = 1; i <= steps; ++i) {
    const godot::Vector2 point = from.lerp(
        to, static_cast<godot::real_t>(i) / static_cast<godot::real_t>(steps));
    if (!inject_mouse_motion(point, delta))
      return util::error_json("Input singleton not available");
  }

  if (!inject_mouse_button(to, button, false))
    return util::error_json("Input singleton not available");

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  if (observe)
    merge_observe_result(inner);
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_type_text(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "type_input_text called");

  auto *text_p = args.Find("text");
  if (!text_p || !text_p->IsString())
    return util::error_json("missing required parameter: text");
  const std::string text = text_p->GetString();

  std::string error;
  bool submit = false;
  if (!read_bool_arg(args, "submit", false, submit, error))
    return util::error_json(error);

  bool observe = false;
  if (!read_bool_arg(args, "observe", false, observe, error))
    return util::error_json(error);

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return util::error_json("EditorInterface not available");
  godot::Control *base_control = editor->get_base_control();
  if (!base_control)
    return util::error_json("editor base control not available");
  godot::Viewport *viewport = base_control->get_viewport();
  if (!viewport)
    return util::error_json("editor viewport not available");
  if (submit && !godot::Input::get_singleton())
    return util::error_json("Input singleton not available");

  const godot::String text_str = godot::String::utf8(text.c_str());
  viewport->push_text_input(text_str);

  if (submit) {
    if (!inject_key(godot::KEY_ENTER, true, false, false, false))
      return util::error_json("Input singleton not available");
    if (!inject_key(godot::KEY_ENTER, false, false, false, false))
      return util::error_json("Input singleton not available");
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  inner["length"] = mcp::JsonValue(text_str.length());
  if (observe)
    merge_observe_result(inner);
  return util::ok_result(std::move(inner));
}

} // namespace input_click_ops
} // namespace godot_autopilot
