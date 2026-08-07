#include "input_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_self_driving {
namespace input_ops {

namespace {

godot::Key parse_key(const std::string &key_str) {
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

godot::MouseButton parse_mouse_button(const std::string &button_str) {
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

bool extract_vec2(const mcp::JsonValue &obj, double &x, double &y) {
  auto *xp = obj.Find("x");
  auto *yp = obj.Find("y");
  if (!xp || !xp->IsNumber())
    return false;
  if (!yp || !yp->IsNumber())
    return false;
  x = xp->IsInt() ? static_cast<double>(xp->GetInt()) : xp->GetDouble();
  y = yp->IsInt() ? static_cast<double>(yp->GetInt()) : yp->GetDouble();
  return true;
}

} // namespace

mcp::JsonValue handle_action_press(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_action_press called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: action");
    return e;
  }

  float strength = 1.0f;
  auto *strength_p = args.Find("strength");
  if (strength_p && strength_p->IsNumber()) {
    strength = static_cast<float>(
        strength_p->IsInt() ? static_cast<double>(strength_p->GetInt())
                            : strength_p->GetDouble());
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  input->action_press(godot::StringName(action_str.c_str()), strength);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_action_release(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_action_release called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  input->action_release(godot::StringName(action_str.c_str()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_is_action_pressed(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_is_action_pressed called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  bool result = input->is_action_pressed(godot::StringName(action_str.c_str()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(result);
  return r;
}

mcp::JsonValue handle_is_action_just_pressed(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_is_action_just_pressed called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  bool result =
      input->is_action_just_pressed(godot::StringName(action_str.c_str()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(result);
  return r;
}

mcp::JsonValue handle_key_press(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_key_press called");

  auto *key_p = args.Find("key");
  if (!key_p || !key_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: key");
    return e;
  }

  std::string key_str = key_p->GetString();
  godot::Key k = parse_key(key_str);
  if (k == godot::KEY_NONE) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid key: " + key_str);
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventKey> ev;
  ev.instantiate();
  ev->set_pressed(true);
  ev->set_keycode(k);

  input->parse_input_event(ev);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_key_release(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_key_release called");

  auto *key_p = args.Find("key");
  if (!key_p || !key_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: key");
    return e;
  }

  std::string key_str = key_p->GetString();
  godot::Key k = parse_key(key_str);
  if (k == godot::KEY_NONE) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid key: " + key_str);
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventKey> ev;
  ev.instantiate();
  ev->set_pressed(false);
  ev->set_keycode(k);

  input->parse_input_event(ev);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_mouse_move(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_mouse_move called");

  auto *pos_p = args.Find("position");
  if (!pos_p || !pos_p->IsObject()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: position");
    return e;
  }

  double px = 0.0, py = 0.0;
  if (!extract_vec2(*pos_p, px, py)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("position must have numeric x and y fields");
    return e;
  }

  double rx = 0.0, ry = 0.0;
  auto *rel_p = args.Find("relative");
  if (rel_p && rel_p->IsObject()) {
    extract_vec2(*rel_p, rx, ry);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseMotion> ev;
  ev.instantiate();
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));
  ev->set_relative(godot::Vector2(rx, ry));

  input->parse_input_event(ev);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_mouse_button_press(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_mouse_button_press called");

  auto *button_p = args.Find("button");
  if (!button_p || !button_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: button");
    return e;
  }

  std::string button_str = button_p->GetString();
  godot::MouseButton mb = parse_mouse_button(button_str);
  if (mb == godot::MOUSE_BUTTON_NONE) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid mouse button: " + button_str);
    return e;
  }

  double px = 0.0, py = 0.0;
  auto *pos_p = args.Find("position");
  if (pos_p && pos_p->IsObject()) {
    extract_vec2(*pos_p, px, py);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(true);
  ev->set_button_index(mb);
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));

  input->parse_input_event(ev);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_mouse_button_release(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_mouse_button_release called");

  auto *button_p = args.Find("button");
  if (!button_p || !button_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: button");
    return e;
  }

  std::string button_str = button_p->GetString();
  godot::MouseButton mb = parse_mouse_button(button_str);
  if (mb == godot::MOUSE_BUTTON_NONE) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("invalid mouse button: " + button_str);
    return e;
  }

  double px = 0.0, py = 0.0;
  auto *pos_p = args.Find("position");
  if (pos_p && pos_p->IsObject()) {
    extract_vec2(*pos_p, px, py);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(false);
  ev->set_button_index(mb);
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));

  input->parse_input_event(ev);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  return r;
}

mcp::JsonValue handle_gamepad_simulate(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "input_gamepad_simulate called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: action");
    return e;
  }

  std::string action_str = action_p->GetString();
  std::string u = action_str;
  for (auto &c : u) {
    if (c >= 'a' && c <= 'z')
      c -= 32;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("Input singleton not available");
    return e;
  }

  if (u == "STOP") {
    int device = 0;
    auto *dev_p = args.Find("device");
    if (dev_p && dev_p->IsInt())
      device = dev_p->GetInt();

    input->stop_joy_vibration(device);

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    return r;
  }

  if (u == "VIBRATE") {
    int device = 0;
    auto *dev_p = args.Find("device");
    if (dev_p && dev_p->IsInt())
      device = dev_p->GetInt();

    float weak = 0.5f;
    auto *weak_p = args.Find("weak_magnitude");
    if (weak_p && weak_p->IsNumber())
      weak = static_cast<float>(weak_p->IsInt()
                                    ? static_cast<double>(weak_p->GetInt())
                                    : weak_p->GetDouble());

    float strong = 0.5f;
    auto *strong_p = args.Find("strong_magnitude");
    if (strong_p && strong_p->IsNumber())
      strong = static_cast<float>(strong_p->IsInt()
                                      ? static_cast<double>(strong_p->GetInt())
                                      : strong_p->GetDouble());

    float duration = 0.0f;
    auto *dur_p = args.Find("duration");
    if (dur_p && dur_p->IsNumber())
      duration = static_cast<float>(dur_p->IsInt()
                                        ? static_cast<double>(dur_p->GetInt())
                                        : dur_p->GetDouble());

    input->start_joy_vibration(device, weak, strong, duration);

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("ok");
    return r;
  }

  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue("unknown gamepad action: " + action_str);
  return e;
}

} // namespace input_ops
} // namespace godot_self_driving
