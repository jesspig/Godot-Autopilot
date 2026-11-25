#include "tools/domains/input/input_ops.hpp"
#include "core/observability/log_system.hpp"
#include "tools/domains/input_map/input_map_ops.hpp"
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_autopilot {
namespace input_ops {

namespace {

godot::Key parse_key(const std::string &key_str) {
  return input_map_ops::resolve_input_key(key_str);
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

bool extract_vec2(const godot_autopilot::util::JsonValue &obj, double &x, double &y) {
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

godot_autopilot::util::JsonValue handle_action_press(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "press_input_action called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
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
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  input->action_press(godot::StringName(action_str.c_str()), strength);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_action_release(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "release_input_action called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  input->action_release(godot::StringName(action_str.c_str()));

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_is_action_pressed(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "is_input_action_pressed called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  bool result = input->is_action_pressed(godot::StringName(action_str.c_str()));

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue(result);
  return r;
}

godot_autopilot::util::JsonValue handle_is_action_just_pressed(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "is_input_action_just_pressed called");

  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: action");
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  std::string action_str = action_p->GetString();
  bool result =
      input->is_action_just_pressed(godot::StringName(action_str.c_str()));

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue(result);
  return r;
}

godot_autopilot::util::JsonValue handle_key_press(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "press_input_key called");

  auto *key_p = args.Find("key");
  if (!key_p || !key_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: key");
    return e;
  }

  std::string key_str = key_p->GetString();
  godot::Key k = parse_key(key_str);
  if (k == godot::KEY_NONE) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("invalid key: " + key_str);
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventKey> ev;
  ev.instantiate();
  ev->set_pressed(true);
  ev->set_keycode(k);

  input->parse_input_event(ev);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_key_release(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "release_input_key called");

  auto *key_p = args.Find("key");
  if (!key_p || !key_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: key");
    return e;
  }

  std::string key_str = key_p->GetString();
  godot::Key k = parse_key(key_str);
  if (k == godot::KEY_NONE) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("invalid key: " + key_str);
    return e;
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventKey> ev;
  ev.instantiate();
  ev->set_pressed(false);
  ev->set_keycode(k);

  input->parse_input_event(ev);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_mouse_move(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "move_input_mouse called");

  auto *pos_p = args.Find("position");
  if (!pos_p || !pos_p->IsObject()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: position");
    return e;
  }

  double px = 0.0, py = 0.0;
  if (!extract_vec2(*pos_p, px, py)) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("position must have numeric x and y fields");
    return e;
  }

  double rx = 0.0, ry = 0.0;
  auto *rel_p = args.Find("relative");
  if (rel_p && rel_p->IsObject()) {
    extract_vec2(*rel_p, rx, ry);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseMotion> ev;
  ev.instantiate();
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));
  ev->set_relative(godot::Vector2(rx, ry));

  input->parse_input_event(ev);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_mouse_button_press(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "press_input_mouse_button called");

  auto *button_p = args.Find("button");
  if (!button_p || !button_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: button");
    return e;
  }

  std::string button_str = button_p->GetString();
  godot::MouseButton mb = parse_mouse_button(button_str);
  if (mb == godot::MOUSE_BUTTON_NONE) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("invalid mouse button: " + button_str);
    return e;
  }

  double px = 0.0, py = 0.0;
  auto *pos_p = args.Find("position");
  if (pos_p && pos_p->IsObject()) {
    extract_vec2(*pos_p, px, py);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(true);
  ev->set_button_index(mb);
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));

  input->parse_input_event(ev);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_mouse_button_release(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "release_input_mouse_button called");

  auto *button_p = args.Find("button");
  if (!button_p || !button_p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: button");
    return e;
  }

  std::string button_str = button_p->GetString();
  godot::MouseButton mb = parse_mouse_button(button_str);
  if (mb == godot::MOUSE_BUTTON_NONE) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("invalid mouse button: " + button_str);
    return e;
  }

  double px = 0.0, py = 0.0;
  auto *pos_p = args.Find("position");
  if (pos_p && pos_p->IsObject()) {
    extract_vec2(*pos_p, px, py);
  }

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  godot::Ref<godot::InputEventMouseButton> ev;
  ev.instantiate();
  ev->set_pressed(false);
  ev->set_button_index(mb);
  ev->set_position(godot::Vector2(px, py));
  ev->set_global_position(godot::Vector2(px, py));

  input->parse_input_event(ev);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_gamepad_vibration_start(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "start_input_gamepad_vibration called");

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  int device = 0;
  auto *dev_p = args.Find("device");
  if (dev_p && dev_p->IsInt())
    device = dev_p->GetInt();

  float weak = 0.5f;
  auto *weak_p = args.Find("weak");
  if (weak_p && weak_p->IsNumber())
    weak = static_cast<float>(weak_p->IsInt()
                                  ? static_cast<double>(weak_p->GetInt())
                                  : weak_p->GetDouble());

  float strong = 0.5f;
  auto *strong_p = args.Find("strong");
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

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

godot_autopilot::util::JsonValue handle_gamepad_vibration_stop(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "stop_input_gamepad_vibration called");

  auto *input = godot::Input::get_singleton();
  if (!input) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("Input singleton not available");
    return e;
  }

  int device = 0;
  auto *dev_p = args.Find("device");
  if (dev_p && dev_p->IsInt())
    device = dev_p->GetInt();

  input->stop_joy_vibration(device);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("ok");
  return r;
}

namespace {

godot_autopilot::util::JsonValue input_action_error(const std::string &message) {
  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
  error["error"] = godot_autopilot::util::JsonValue(message);
  return error;
}

bool read_input_action(const godot_autopilot::util::JsonValue &args, std::string &out,
                       godot_autopilot::util::JsonValue &error) {
  const godot_autopilot::util::JsonValue *action = args.Find("action");
  if (action == nullptr) {
    error = input_action_error("missing required parameter: action");
    return false;
  }
  if (!action->IsString()) {
    error = input_action_error("invalid parameter: action must be a string");
    return false;
  }
  out = action->GetString();
  return true;
}

godot_autopilot::util::JsonValue legacy_input_args(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out = args.IsObject()
                           ? args
                           : godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
  out.GetObject().erase("action");
  const godot_autopilot::util::JsonValue *name = args.Find("name");
  if (name != nullptr) {
    out["action"] = *name;
  }
  return out;
}

godot_autopilot::util::JsonValue strip_input_action(const godot_autopilot::util::JsonValue &args) {
  godot_autopilot::util::JsonValue out = args.IsObject()
                           ? args
                           : godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
  out.GetObject().erase("action");
  return out;
}

} // namespace

godot_autopilot::util::JsonValue handle_get_input_state(const godot_autopilot::util::JsonValue &args) {
  std::string action;
  godot_autopilot::util::JsonValue error;
  if (!read_input_action(args, action, error)) {
    return error;
  }
  const godot_autopilot::util::JsonValue legacy = legacy_input_args(args);
  if (action == "action_pressed") {
    return handle_is_action_pressed(legacy);
  }
  if (action == "action_just_pressed") {
    return handle_is_action_just_pressed(legacy);
  }
  return input_action_error(
      "unknown action '" + action +
      "' for get_input_state (expected action_pressed, action_just_pressed)");
}

godot_autopilot::util::JsonValue handle_input_action(const godot_autopilot::util::JsonValue &args) {
  std::string action;
  godot_autopilot::util::JsonValue error;
  if (!read_input_action(args, action, error)) {
    return error;
  }
  const godot_autopilot::util::JsonValue legacy = legacy_input_args(args);
  if (action == "press") {
    return handle_action_press(legacy);
  }
  if (action == "release") {
    return handle_action_release(legacy);
  }
  return input_action_error(
      "unknown action '" + action +
      "' for input_action (expected press, release)");
}

godot_autopilot::util::JsonValue handle_input_key(const godot_autopilot::util::JsonValue &args) {
  std::string action;
  godot_autopilot::util::JsonValue error;
  if (!read_input_action(args, action, error)) {
    return error;
  }
  const godot_autopilot::util::JsonValue legacy = strip_input_action(args);
  if (action == "press") {
    return handle_key_press(legacy);
  }
  if (action == "release") {
    return handle_key_release(legacy);
  }
  return input_action_error(
      "unknown action '" + action +
      "' for input_key (expected press, release)");
}

godot_autopilot::util::JsonValue handle_input_gamepad(const godot_autopilot::util::JsonValue &args) {
  std::string action;
  godot_autopilot::util::JsonValue error;
  if (!read_input_action(args, action, error)) {
    return error;
  }
  const godot_autopilot::util::JsonValue legacy = strip_input_action(args);
  if (action == "start_vibration") {
    return handle_gamepad_vibration_start(legacy);
  }
  if (action == "stop_vibration") {
    return handle_gamepad_vibration_stop(legacy);
  }
  return input_action_error(
      "unknown action '" + action +
      "' for input_gamepad (expected start_vibration, stop_vibration)");
}

} // namespace input_ops
} // namespace godot_autopilot
