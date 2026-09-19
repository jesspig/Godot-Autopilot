#include "editor_ui_actions.hpp"
#include "core/log_system.hpp"
#include "tools/editor_ui_ops.hpp"
#include "tools/input_click_ops.hpp"
#include "util/error_util.hpp"
#include <cctype>
#include <cstdlib>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_from_window.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace editor_ui_actions {

namespace {

std::string lower_ascii(std::string text) {
  for (char &c : text) {
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + 32);
  }
  return text;
}

std::string trim_ascii(const std::string &text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end &&
         std::isspace(static_cast<unsigned char>(text[begin])) != 0)
    ++begin;
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
    --end;
  return text.substr(begin, end - begin);
}

bool read_required_string_arg(const mcp::JsonValue &args, const char *name,
                              std::string *out, std::string *error) {
  const mcp::JsonValue *value = args.Find(name);
  if (!value) {
    *error = std::string("missing required parameter: ") + name;
    return false;
  }
  if (!value->IsString()) {
    *error = std::string("invalid parameter: ") + name + " must be a string";
    return false;
  }
  *out = value->GetString();
  return true;
}

bool read_bool_arg(const mcp::JsonValue &args, const char *name,
                   bool default_value, bool *out, std::string *error) {
  *out = default_value;
  const mcp::JsonValue *value = args.Find(name);
  if (!value)
    return true;
  if (!value->IsBool()) {
    *error = std::string("invalid parameter: ") + name + " must be a boolean";
    return false;
  }
  *out = value->GetBool();
  return true;
}

bool inject_mouse_button_ex(godot::Vector2 pos, godot::MouseButton button,
                            bool pressed, bool double_click, int window_id) {
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
  if (window_id != 0)
    ev->set_window_id(window_id);

  input->parse_input_event(ev);
  return true;
}

bool inject_key_ex(godot::Key key, bool pressed, bool ctrl, bool shift,
                   bool alt, bool meta, int window_id) {
  if (!meta)
    return input_click_ops::inject_key(key, pressed, ctrl, shift, alt, window_id);

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
  ev->set_meta_pressed(true);
  if (window_id != 0)
    ev->set_window_id(window_id);

  input->parse_input_event(ev);
  return true;
}

godot::Key parse_function_key(const std::string &key_lower) {
  if (key_lower.size() < 2 || key_lower.size() > 3 || key_lower[0] != 'f')
    return godot::KEY_NONE;
  for (size_t i = 1; i < key_lower.size(); ++i) {
    if (key_lower[i] < '0' || key_lower[i] > '9')
      return godot::KEY_NONE;
  }
  const int index = std::atoi(key_lower.c_str() + 1);
  if (index < 1 || index > 12)
    return godot::KEY_NONE;
  return static_cast<godot::Key>(godot::KEY_F1 + (index - 1));
}

} // namespace

mcp::JsonValue handle_click_editor_element(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "click_editor_element called");

  std::string error;
  std::string path;
  if (!read_required_string_arg(args, "path", &path, &error))
    return util::error_json(error);

  editor_ui_ops::Element element;
  if (!editor_ui_ops::find_element_by_path(path, &element)) {
    return util::error_json(
        "editor UI element not found: " + path +
        " (use get_editor_ui_elements or hit_test_editor_point to list valid "
        "element paths)");
  }

  std::string button_name = "left";
  if (const mcp::JsonValue *button_p = args.Find("button")) {
    if (!button_p->IsString())
      return util::error_json("invalid parameter: button must be a string");
    button_name = button_p->GetString();
  }
  const godot::MouseButton button =
      input_click_ops::parse_mouse_button_name(button_name);
  if (button == godot::MOUSE_BUTTON_NONE)
    return util::error_json("invalid mouse button: " + button_name);

  bool double_click = false;
  if (!read_bool_arg(args, "double_click", false, &double_click, &error))
    return util::error_json(error);
  bool warp = true;
  if (!read_bool_arg(args, "warp", true, &warp, &error))
    return util::error_json(error);
  bool observe = false;
  if (!read_bool_arg(args, "observe", false, &observe, &error))
    return util::error_json(error);
  (void)observe;

  const godot::Vector2 pos(
      static_cast<godot::real_t>(element.x + element.w / 2.0),
      static_cast<godot::real_t>(element.y + element.h / 2.0));
  const int window_id = element.window_id;

  if (warp && !input_click_ops::warp_to(pos))
    return util::error_json("DisplayServer not available");

  if (!input_click_ops::inject_mouse_motion(pos, godot::Vector2(), window_id))
    return util::error_json("Input singleton not available");
  if (!inject_mouse_button_ex(pos, button, true, false, window_id))
    return util::error_json("Input singleton not available");
  if (!inject_mouse_button_ex(pos, button, false, false, window_id))
    return util::error_json("Input singleton not available");
  if (double_click) {
    if (!inject_mouse_button_ex(pos, button, true, true, window_id))
      return util::error_json("Input singleton not available");
    if (!inject_mouse_button_ex(pos, button, false, false, window_id))
      return util::error_json("Input singleton not available");
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  inner["path"] = mcp::JsonValue(element.path);
  inner["type"] = mcp::JsonValue(element.type);
  inner["button"] = mcp::JsonValue(lower_ascii(button_name));
  inner["double_click"] = mcp::JsonValue(double_click);
  inner["window_id"] = mcp::JsonValue(window_id);
  mcp::JsonValue position(mcp::JsonValue::object_tag);
  position["x"] = mcp::JsonValue(static_cast<double>(pos.x));
  position["y"] = mcp::JsonValue(static_cast<double>(pos.y));
  inner["position"] = std::move(position);
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_type_editor_element_text(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "type_editor_element_text called");

  std::string error;
  std::string path;
  if (!read_required_string_arg(args, "path", &path, &error))
    return util::error_json(error);
  std::string text;
  if (!read_required_string_arg(args, "text", &text, &error))
    return util::error_json(error);

  bool submit = false;
  if (!read_bool_arg(args, "submit", false, &submit, &error))
    return util::error_json(error);
  bool observe = false;
  if (!read_bool_arg(args, "observe", false, &observe, &error))
    return util::error_json(error);
  (void)observe;

  editor_ui_ops::Element element;
  if (!editor_ui_ops::find_element_by_path(path, &element)) {
    return util::error_json(
        "editor UI element not found: " + path +
        " (use get_editor_ui_elements or hit_test_editor_point to list valid "
        "element paths)");
  }
  godot::Control *control = editor_ui_ops::find_control_by_path(path);
  if (!control)
    return util::error_json("editor UI control not found: " + path);

  const bool focused = control->has_focus();
  if (!focused)
    control->grab_focus();

  godot::Viewport *viewport = control->get_viewport();
  if (!viewport)
    return util::error_json("editor control viewport not available");

  const godot::String text_str = godot::String::utf8(text.c_str());
  viewport->push_text_input(text_str);

  if (submit) {
    if (!input_click_ops::inject_key(godot::KEY_ENTER, true, false, false,
                                     false, element.window_id))
      return util::error_json("Input singleton not available");
    if (!input_click_ops::inject_key(godot::KEY_ENTER, false, false, false,
                                     false, element.window_id))
      return util::error_json("Input singleton not available");
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  inner["path"] = mcp::JsonValue(path);
  inner["length"] = mcp::JsonValue(text_str.length());
  inner["focused"] = mcp::JsonValue(focused);
  inner["submit"] = mcp::JsonValue(submit);
  return util::ok_result(std::move(inner));
}

mcp::JsonValue handle_run_editor_shortcut(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "run_editor_shortcut called");

  std::string error;
  std::string shortcut;
  if (!read_required_string_arg(args, "shortcut", &shortcut, &error))
    return util::error_json(error);

  std::vector<std::string> parts;
  std::string current;
  for (char c : shortcut) {
    if (c == '+') {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  parts.push_back(current);
  for (std::string &part : parts)
    part = trim_ascii(part);

  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool meta = false;
  for (size_t i = 0; i + 1 < parts.size(); ++i) {
    const std::string modifier = lower_ascii(parts[i]);
    if (modifier.empty())
      return util::error_json("invalid shortcut: empty modifier in '" +
                              shortcut + "'");
    if (modifier == "ctrl" || modifier == "control")
      ctrl = true;
    else if (modifier == "shift")
      shift = true;
    else if (modifier == "alt")
      alt = true;
    else if (modifier == "meta" || modifier == "super")
      meta = true;
    else
      return util::error_json("invalid shortcut modifier: " + parts[i]);
  }

  const std::string main_key = parts.back();
  if (main_key.empty())
    return util::error_json("invalid shortcut: missing key in '" + shortcut +
                            "'");
  godot::Key key = input_click_ops::parse_key_name(main_key);
  if (key == godot::KEY_NONE)
    key = parse_function_key(lower_ascii(main_key));
  if (key == godot::KEY_NONE)
    return util::error_json("invalid shortcut key: " + main_key);

  if (!inject_key_ex(key, true, ctrl, shift, alt, meta, 0))
    return util::error_json("Input singleton not available");
  if (!inject_key_ex(key, false, ctrl, shift, alt, meta, 0))
    return util::error_json("Input singleton not available");

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["ok"] = mcp::JsonValue(true);
  inner["shortcut"] = mcp::JsonValue(shortcut);
  inner["key"] = mcp::JsonValue(static_cast<int>(key));
  inner["ctrl"] = mcp::JsonValue(ctrl);
  inner["shift"] = mcp::JsonValue(shift);
  inner["alt"] = mcp::JsonValue(alt);
  inner["meta"] = mcp::JsonValue(meta);
  return util::ok_result(std::move(inner));
}

} // namespace editor_ui_actions
} // namespace godot_autopilot
