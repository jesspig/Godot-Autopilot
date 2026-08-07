#include "input_map_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/input_map.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>
#include <unordered_map>

namespace godot_self_driving {
namespace input_map_ops {

using JV = mcp::JsonValue;

namespace {

const std::unordered_map<std::string, int64_t> KEY_NAME_TO_CODE = {
    {"KEY_SPACE", 32},
    {"KEY_EXCLAM", 33},
    {"KEY_QUOTEDBL", 34},
    {"KEY_NUMBERSIGN", 35},
    {"KEY_DOLLAR", 36},
    {"KEY_PERCENT", 37},
    {"KEY_AMPERSAND", 38},
    {"KEY_APOSTROPHE", 39},
    {"KEY_PARENLEFT", 40},
    {"KEY_PARENRIGHT", 41},
    {"KEY_ASTERISK", 42},
    {"KEY_PLUS", 43},
    {"KEY_COMMA", 44},
    {"KEY_MINUS", 45},
    {"KEY_PERIOD", 46},
    {"KEY_SLASH", 47},
    {"KEY_0", 48},
    {"KEY_1", 49},
    {"KEY_2", 50},
    {"KEY_3", 51},
    {"KEY_4", 52},
    {"KEY_5", 53},
    {"KEY_6", 54},
    {"KEY_7", 55},
    {"KEY_8", 56},
    {"KEY_9", 57},
    {"KEY_COLON", 58},
    {"KEY_SEMICOLON", 59},
    {"KEY_LESS", 60},
    {"KEY_EQUAL", 61},
    {"KEY_GREATER", 62},
    {"KEY_QUESTION", 63},
    {"KEY_AT", 64},
    {"KEY_A", 65},
    {"KEY_B", 66},
    {"KEY_C", 67},
    {"KEY_D", 68},
    {"KEY_E", 69},
    {"KEY_F", 70},
    {"KEY_G", 71},
    {"KEY_H", 72},
    {"KEY_I", 73},
    {"KEY_J", 74},
    {"KEY_K", 75},
    {"KEY_L", 76},
    {"KEY_M", 77},
    {"KEY_N", 78},
    {"KEY_O", 79},
    {"KEY_P", 80},
    {"KEY_Q", 81},
    {"KEY_R", 82},
    {"KEY_S", 83},
    {"KEY_T", 84},
    {"KEY_U", 85},
    {"KEY_V", 86},
    {"KEY_W", 87},
    {"KEY_X", 88},
    {"KEY_Y", 89},
    {"KEY_Z", 90},
    {"KEY_BRACKETLEFT", 91},
    {"KEY_BACKSLASH", 92},
    {"KEY_BRACKETRIGHT", 93},
    {"KEY_ASCIICIRCUM", 94},
    {"KEY_UNDERSCORE", 95},
    {"KEY_QUOTELEFT", 96},
    {"KEY_BRACELEFT", 123},
    {"KEY_BAR", 124},
    {"KEY_BRACERIGHT", 125},
    {"KEY_ASCIITILDE", 126},
    {"KEY_YEN", 165},
    {"KEY_SECTION", 167},
    {"KEY_ESCAPE", 4194305},
    {"KEY_TAB", 4194306},
    {"KEY_BACKTAB", 4194307},
    {"KEY_BACKSPACE", 4194308},
    {"KEY_ENTER", 4194309},
    {"KEY_KP_ENTER", 4194310},
    {"KEY_INSERT", 4194311},
    {"KEY_DELETE", 4194312},
    {"KEY_PAUSE", 4194313},
    {"KEY_PRINT", 4194314},
    {"KEY_SYSREQ", 4194315},
    {"KEY_CLEAR", 4194316},
    {"KEY_HOME", 4194317},
    {"KEY_END", 4194318},
    {"KEY_LEFT", 4194319},
    {"KEY_UP", 4194320},
    {"KEY_RIGHT", 4194321},
    {"KEY_DOWN", 4194322},
    {"KEY_PAGEUP", 4194323},
    {"KEY_PAGEDOWN", 4194324},
    {"KEY_SHIFT", 4194325},
    {"KEY_CTRL", 4194326},
    {"KEY_META", 4194327},
    {"KEY_ALT", 4194328},
    {"KEY_CAPSLOCK", 4194329},
    {"KEY_NUMLOCK", 4194330},
    {"KEY_SCROLLLOCK", 4194331},
    {"KEY_F1", 4194332},
    {"KEY_F2", 4194333},
    {"KEY_F3", 4194334},
    {"KEY_F4", 4194335},
    {"KEY_F5", 4194336},
    {"KEY_F6", 4194337},
    {"KEY_F7", 4194338},
    {"KEY_F8", 4194339},
    {"KEY_F9", 4194340},
    {"KEY_F10", 4194341},
    {"KEY_F11", 4194342},
    {"KEY_F12", 4194343},
    {"KEY_F13", 4194344},
    {"KEY_F14", 4194345},
    {"KEY_F15", 4194346},
    {"KEY_F16", 4194347},
    {"KEY_F17", 4194348},
    {"KEY_F18", 4194349},
    {"KEY_F19", 4194350},
    {"KEY_F20", 4194351},
    {"KEY_F21", 4194352},
    {"KEY_F22", 4194353},
    {"KEY_F23", 4194354},
    {"KEY_F24", 4194355},
    {"KEY_F25", 4194356},
    {"KEY_F26", 4194357},
    {"KEY_F27", 4194358},
    {"KEY_F28", 4194359},
    {"KEY_F29", 4194360},
    {"KEY_F30", 4194361},
    {"KEY_F31", 4194362},
    {"KEY_F32", 4194363},
    {"KEY_F33", 4194364},
    {"KEY_F34", 4194365},
    {"KEY_F35", 4194366},
    {"KEY_MENU", 4194370},
    {"KEY_HYPER", 4194371},
    {"KEY_HELP", 4194373},
    {"KEY_BACK", 4194376},
    {"KEY_FORWARD", 4194377},
    {"KEY_STOP", 4194378},
    {"KEY_REFRESH", 4194379},
    {"KEY_VOLUMEDOWN", 4194380},
    {"KEY_VOLUMEMUTE", 4194381},
    {"KEY_VOLUMEUP", 4194382},
    {"KEY_MEDIAPLAY", 4194388},
    {"KEY_MEDIASTOP", 4194389},
    {"KEY_MEDIAPREVIOUS", 4194390},
    {"KEY_MEDIANEXT", 4194391},
    {"KEY_MEDIARECORD", 4194392},
    {"KEY_HOMEPAGE", 4194393},
    {"KEY_FAVORITES", 4194394},
    {"KEY_SEARCH", 4194395},
    {"KEY_STANDBY", 4194396},
    {"KEY_OPENURL", 4194397},
    {"KEY_LAUNCHMAIL", 4194398},
    {"KEY_LAUNCHMEDIA", 4194399},
    {"KEY_KP_MULTIPLY", 4194433},
    {"KEY_KP_DIVIDE", 4194434},
    {"KEY_KP_SUBTRACT", 4194435},
    {"KEY_KP_PERIOD", 4194436},
    {"KEY_KP_ADD", 4194437},
    {"KEY_KP_0", 4194438},
    {"KEY_KP_1", 4194439},
    {"KEY_KP_2", 4194440},
    {"KEY_KP_3", 4194441},
    {"KEY_KP_4", 4194442},
    {"KEY_KP_5", 4194443},
    {"KEY_KP_6", 4194444},
    {"KEY_KP_7", 4194445},
    {"KEY_KP_8", 4194446},
    {"KEY_KP_9", 4194447},
    {"KEY_UNKNOWN", 8388607},
};

void persist_action(const std::string &action_name, godot::InputMap *im,
                    godot::ProjectSettings *ps) {
  godot::StringName name(action_name.c_str());
  godot::Dictionary dict;
  dict["deadzone"] = im->action_get_deadzone(name);
  dict["events"] = im->action_get_events(name);
  ps->set_setting(godot::String(("input/" + action_name).c_str()), dict);
  ps->save();
}

} // namespace

JV handle_action_add_event(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_add_event called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *ep = args.Find("event");
  if (!ep || !ep->IsObject()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: event");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  std::string action = ap->GetString();
  std::string event_class = "InputEvent";
  bool class_explicitly_provided = false;
  auto *class_field = ep->Find("class");
  if (class_field && class_field->IsString()) {
    event_class = class_field->GetString();
    class_explicitly_provided = true;
  }
  JV event_obj = *ep;
  if (event_class == "InputEventKey") {
    for (const char *field : {"physical_keycode", "keycode"}) {
      auto *fp = event_obj.Find(field);
      if (fp && fp->IsString()) {
        auto it = KEY_NAME_TO_CODE.find(fp->GetString());
        if (it == KEY_NAME_TO_CODE.end()) {
          JV e(JV::object_tag);
          e["error"] =
              JV("unknown key name: " + fp->GetString() +
                 " — use numeric keycode (e.g. 65) or a known KEY_* name");
          return e;
        }
        event_obj[field] = JV(it->second);
      }
    }
  }
  godot::Variant event_var = VariantJson::deserialize(event_obj, event_class);
  auto event = godot::Ref<godot::InputEvent>(event_var);
  if (!class_explicitly_provided) {
    JV e(JV::object_tag);
    e["error"] =
        JV("missing required field 'class': must specify a concrete InputEvent "
           "subclass, e.g. {\"class\":\"InputEventKey\", \"keycode\":65} — use "
           "search_tools to find InputEvent* subclasses");
    return e;
  }
  if (event.is_null()) {
    JV e(JV::object_tag);
    e["error"] =
        JV("failed to deserialize InputEvent — specify a concrete class e.g. "
           "\"InputEventKey\" with field \"class\" (common: InputEventKey, "
           "InputEventMouseButton, InputEventJoypadButton, InputEventAction, "
           "InputEventShortcut)");
    return e;
  }
  im->action_add_event(godot::StringName(action.c_str()), event);
  {
    auto *ps = godot::ProjectSettings::get_singleton();
    if (ps) {
      persist_action(action, im, ps);
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_add_event completed");
  return r;
}

JV handle_action_erase_event(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_erase_event called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *ei = args.Find("event_index");
  if (!ei || !ei->IsInt()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: event_index");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
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
  {
    auto *ps = godot::ProjectSettings::get_singleton();
    if (ps) {
      persist_action(action, im, ps);
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_erase_event completed");
  return r;
}

JV handle_action_set_deadzone(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_set_deadzone called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *dp = args.Find("deadzone");
  if (!dp || !dp->IsNumber()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: deadzone");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  std::string action = ap->GetString();
  float deadzone = static_cast<float>(
      dp->IsDouble() ? dp->GetDouble() : static_cast<double>(dp->GetInt()));
  im->action_set_deadzone(godot::StringName(action.c_str()), deadzone);
  {
    auto *ps = godot::ProjectSettings::get_singleton();
    if (ps) {
      persist_action(action, im, ps);
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_action_set_deadzone completed");
  return r;
}

JV handle_add_action(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_add_action called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  std::string action = ap->GetString();
  float deadzone = 0.5f;
  auto *dz = args.Find("deadzone");
  if (dz && dz->IsNumber()) {
    deadzone = static_cast<float>(
        dz->IsDouble() ? dz->GetDouble() : static_cast<double>(dz->GetInt()));
  }
  im->add_action(godot::StringName(action.c_str()), deadzone);
  {
    auto *ps = godot::ProjectSettings::get_singleton();
    if (ps) {
      godot::Variant existing =
          ps->get_setting("input/" + godot::String(action.c_str()));
      if (existing.get_type() == godot::Variant::NIL) {
        persist_action(action, im, ps);
      }
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_add_action completed");
  return r;
}

JV handle_erase_action(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_erase_action called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  std::string action = ap->GetString();
  im->erase_action(godot::StringName(action.c_str()));
  {
    auto *ps = godot::ProjectSettings::get_singleton();
    if (ps) {

      if (ps->has_setting("input/" + godot::String(action.c_str()))) {
        ps->clear("input/" + godot::String(action.c_str()));
        ps->save();
      }
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_erase_action completed");
  return r;
}

JV handle_get_actions(const JV &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_get_actions called");
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  auto names = im->get_actions();
  JV result_arr(JV::array_tag);
  for (int i = 0; i < names.size(); i++) {
    result_arr.PushBack(JV(util::to_std(godot::String(names[i]))));
  }
  JV r(JV::object_tag);
  r["result"] = std::move(result_arr);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_get_actions completed");
  return r;
}

JV handle_has_action(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_has_action called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    JV e(JV::object_tag);
    e["error"] = JV("missing required parameter: action");
    return e;
  }
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  std::string action = ap->GetString();
  bool has = im->has_action(godot::StringName(action.c_str()));
  JV r(JV::object_tag);
  r["result"] = JV(has);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_has_action completed");
  return r;
}

JV handle_persist(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "input_map_persist called");
  auto *im = godot::InputMap::get_singleton();
  if (!im) {
    JV e(JV::object_tag);
    e["error"] = JV("InputMap not available");
    return e;
  }
  auto *ps = godot::ProjectSettings::get_singleton();
  if (!ps) {
    JV e(JV::object_tag);
    e["error"] = JV("ProjectSettings not available");
    return e;
  }
  std::vector<std::string> allowlist;
  bool has_allowlist = false;
  auto *ap = args.Find("actions");
  if (ap && !ap->IsArray()) {
    JV e(JV::object_tag);
    e["error"] = JV("invalid parameter: actions must be a JSON array of action "
                    "name strings");
    return e;
  }
  if (ap) {
    has_allowlist = true;
    const auto &arr = ap->GetArray();
    for (const auto &item : arr) {
      if (item.IsString()) {
        allowlist.push_back(item.GetString());
      }
    }
  }
  auto actions = im->get_actions();
  int persisted = 0;
  std::vector<std::string> persisted_names;
  std::vector<std::string> skipped;
  std::vector<std::string> skipped_missing;
  for (int i = 0; i < actions.size(); i++) {
    std::string name_std = util::to_std(godot::String(actions[i]));
    if (has_allowlist) {
      if (std::find(allowlist.begin(), allowlist.end(), name_std) ==
          allowlist.end()) {
        skipped.push_back(name_std);
        continue;
      }
    } else {
      if (name_std.substr(0, 3) == "ui_" ||
          name_std.find('/') != std::string::npos) {
        skipped.push_back(name_std);
        continue;
      }
    }
    godot::StringName action_name = actions[i];
    godot::Dictionary dict;
    dict["deadzone"] = im->action_get_deadzone(action_name);
    dict["events"] = im->action_get_events(action_name);
    ps->set_setting(godot::String(("input/" + name_std).c_str()), dict);
    persisted_names.push_back(name_std);
    persisted++;
  }
  if (has_allowlist) {
    for (const auto &name : allowlist) {
      if (!im->has_action(godot::StringName(name.c_str()))) {
        skipped_missing.push_back(name);
      }
    }
  }
  godot::Error err = ps->save();
  bool readback_verified = (err == godot::OK);
  std::vector<std::string> readback_failed;
  for (const auto &name : persisted_names) {
    godot::String setting_path = godot::String(("input/" + name).c_str());
    bool ok = ps->has_setting(setting_path);
    if (ok) {
      godot::Variant stored = ps->get_setting(setting_path);
      if (stored.get_type() == godot::Variant::DICTIONARY) {
        godot::Dictionary stored_dict = stored;
        ok = stored_dict.has("deadzone") && stored_dict.has("events");
      } else {
        ok = false;
      }
    }
    if (!ok) {
      readback_verified = false;
      readback_failed.push_back(name);
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Tools,
          "input_map_persist readback failed for action '" + name +
              "': setting missing or malformed at input/" + name);
    }
  }
  JV r(JV::object_tag);
  r["result"] = JV("persisted");
  r["actions_persisted"] = JV(persisted);
  r["readback_verified"] = JV(readback_verified);
  if (!readback_failed.empty()) {
    JV failed_arr(JV::array_tag);
    for (const auto &name : readback_failed) {
      failed_arr.PushBack(JV(name));
    }
    r["readback_failed"] = std::move(failed_arr);
  }
  r["note"] =
      JV("persisted actions are loaded by the game process at startup "
         "(InputMap.load_from_project_settings); the editor process InputMap "
         "does not reload project input/ settings until restart. Verify with "
         "ProjectSettings.get_setting('input/<action>'), not "
         "get_setting('input').");
  r["skipped_count"] = JV(static_cast<int64_t>(skipped.size()));
  JV skipped_arr(JV::array_tag);
  for (const auto &name : skipped) {
    skipped_arr.PushBack(JV(name));
  }
  r["skipped"] = std::move(skipped_arr);
  JV missing_arr(JV::array_tag);
  for (const auto &name : skipped_missing) {
    missing_arr.PushBack(JV(name));
  }
  r["skipped_missing"] = std::move(missing_arr);
  r["save_error"] = JV(static_cast<int>(err));
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "input_map_persist completed: " + std::to_string(persisted) +
          " actions persisted, " + std::to_string(skipped.size()) + " skipped");
  return r;
}

} // namespace input_map_ops
} // namespace godot_self_driving
