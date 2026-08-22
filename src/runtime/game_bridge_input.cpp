#include "game_bridge.hpp"

#include "core/config.hpp"
#include "gda_protocol.hpp"
#include "util/error_util.hpp"
#include <cctype>
#include <functional>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_action.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace godot_autopilot {
namespace runtime {
namespace game_bridge {

std::unordered_map<int64_t, std::function<void()>> g_cancel_handlers;

void register_cancel_handler(int64_t request_id,
                             std::function<void()> handler) {
  g_cancel_handlers[request_id] = std::move(handler);
}

void unregister_cancel_handler(int64_t request_id) {
  g_cancel_handlers.erase(request_id);
}

namespace {

using JV = mcp::JsonValue;

std::string inject_step(const JV &step);

class GameBridgeInputWatcher : public godot::Node {
  GDCLASS(GameBridgeInputWatcher, godot::Node)

  int64_t request_id_ = 0;
  godot::StringName action_;
  int state_kind_ = 0;
  double timeout_sec_ = 2.0;
  double elapsed_ = 0.0;
  uint64_t start_ticks_ = 0;
  bool finished_ = false;

protected:
  static void _bind_methods() {}

public:
  void setup(int64_t request_id, const godot::StringName &action,
             int state_kind, double timeout_sec) {
    request_id_ = request_id;
    action_ = action;
    state_kind_ = state_kind;
    timeout_sec_ = timeout_sec;
    set_process_mode(godot::Node::PROCESS_MODE_ALWAYS);
    set_physics_process(true);
    start_ticks_ = godot::Time::get_singleton()->get_ticks_msec();
  }

  void cancel() {
    if (finished_)
      return;
    finished_ = true;
    unregister_cancel_handler(request_id_);
    queue_free();
  }

  void _physics_process(double delta) override {
    (void)delta;
    if (finished_)
      return;
    elapsed_ =
        static_cast<double>(godot::Time::get_singleton()->get_ticks_msec() -
                            start_ticks_) /
        1000.0;
    auto *input = godot::Input::get_singleton();
    bool hit = false;
    if (input) {
      if (state_kind_ == 0)
        hit = input->is_action_just_pressed(action_);
      else if (state_kind_ == 1)
        hit = input->is_action_just_released(action_);
      else
        hit = input->is_action_pressed(action_);
    }
    if (hit) {
      finished_ = true;
      unregister_cancel_handler(request_id_);
      JV body(JV::object_tag);
      body["result"] = JV("matched");
      body[GDA_FIELD_MATCHED_AT_PHYSICS_FRAME] = JV(static_cast<int64_t>(
          godot::Engine::get_singleton()->get_physics_frames()));
      send_response(request_id_, std::move(body));
      queue_free();
    } else if (elapsed_ >= timeout_sec_) {
      finished_ = true;
      unregister_cancel_handler(request_id_);
      const char *state_name =
          state_kind_ == 0 ? "just_pressed"
                           : (state_kind_ == 1 ? "just_released" : "pressed");
      send_response(request_id_,
                    error_result("timeout waiting for " +
                                 std::string(state_name) + " on action " +
                                 util::to_std(godot::String(action_))));
      queue_free();
    }
  }
};

void dispatch_input_event(godot::Input *input, int type_code,
                          const godot::Key &key, int64_t button_index,
                          const godot::Vector2 &position, bool has_position,
                          bool pressed, const godot::StringName &action,
                          bool mode_api, bool flush) {
  if (type_code == 0) {
    godot::Ref<godot::InputEventKey> ev;
    ev.instantiate();
    ev->set_pressed(pressed);
    ev->set_keycode(key);
    ev->set_physical_keycode(key);
    input->parse_input_event(ev);
  } else if (type_code == 1) {
    godot::Ref<godot::InputEventMouseButton> ev;
    ev.instantiate();
    ev->set_pressed(pressed);
    ev->set_button_index(static_cast<godot::MouseButton>(button_index));
    if (has_position) {
      ev->set_position(position);
      ev->set_global_position(position);
    }
    input->parse_input_event(ev);
  } else if (mode_api) {
    if (pressed)
      input->action_press(action);
    else
      input->action_release(action);
  } else {
    godot::Ref<godot::InputEventAction> ev;
    ev.instantiate();
    ev->set_action(action);
    ev->set_pressed(pressed);
    input->parse_input_event(ev);
  }
  if (flush)
    input->flush_buffered_events();
}

class GameBridgeDelayedRelease : public godot::Node {
  GDCLASS(GameBridgeDelayedRelease, godot::Node)

  int type_ = 0;
  godot::Key key_ = godot::KEY_NONE;
  int64_t button_index_ = 0;
  godot::Vector2 position_;
  bool has_position_ = false;
  godot::StringName action_;
  bool mode_api_ = false;
  bool keep_pressed_ = false;
  uint64_t start_physics_frame_ = 0;
  int64_t duration_frames_ = 0;

protected:
  static void _bind_methods() {}

public:
  void setup(int type, const godot::Key &key, int64_t button_index,
             const godot::Vector2 &position, bool has_position,
             const godot::StringName &action, bool mode_api, double delay_sec,
             bool keep_pressed) {
    type_ = type;
    key_ = key;
    button_index_ = button_index;
    position_ = position;
    has_position_ = has_position;
    action_ = action;
    mode_api_ = mode_api;
    keep_pressed_ = keep_pressed;
    godot::SceneTree *tree = get_scene_tree();
    if (!tree) {
      memdelete(this);
      return;
    }
    tree->get_root()->add_child(this);
    start_physics_frame_ = godot::Engine::get_singleton()->get_physics_frames();
    duration_frames_ = static_cast<int64_t>(
        delay_sec *
        godot::Engine::get_singleton()->get_physics_ticks_per_second());
    set_physics_process(true);
  }

  void _physics_process(double delta) override {
    (void)delta;
    if (godot::Engine::get_singleton()->get_physics_frames() -
            start_physics_frame_ >=
        static_cast<uint64_t>(duration_frames_)) {
      do_release();
      queue_free();
    } else if (keep_pressed_) {
      do_press();
    }
  }

private:
  void do_press() {
    auto *input = godot::Input::get_singleton();
    if (input) {
      dispatch_input_event(input, type_, key_, button_index_, position_,
                           has_position_, true, action_, mode_api_, true);
    }
  }

  void do_release() {
    auto *input = godot::Input::get_singleton();
    if (input) {
      dispatch_input_event(input, type_, key_, button_index_, position_,
                           has_position_, false, action_, mode_api_, false);
    }
  }
};

void schedule_release(const std::string &type, const godot::Key &key,
                      int64_t button_index, const godot::Vector2 &position,
                      bool has_position, const godot::StringName &action,
                      bool mode_api, int64_t duration_ms, bool keep_pressed) {
  GameBridgeDelayedRelease *release = memnew(GameBridgeDelayedRelease);
  int type_code = (type == "key") ? 0 : (type == "mouse_button") ? 1 : 2;
  release->setup(type_code, key, button_index, position, has_position, action,
                 mode_api, duration_ms / 1000.0, keep_pressed);
}

class GameBridgeInputSequence : public godot::Node {
  GDCLASS(GameBridgeInputSequence, godot::Node)

  std::vector<JV> steps_;
  size_t index_ = 0;
  godot::Ref<godot::SceneTreeTimer> timer_;

protected:
  static void _bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("_on_step_timeout"),
                                &GameBridgeInputSequence::_on_step_timeout);
  }

public:
  void setup(std::vector<JV> steps) {
    steps_ = std::move(steps);
    godot::SceneTree *tree = get_scene_tree();
    if (!tree) {
      memdelete(this);
      return;
    }
    tree->get_root()->add_child(this);
    run_step();
  }

  void _on_step_timeout() {
    timer_.unref();
    run_step();
  }

private:
  void run_step() {
    if (index_ >= steps_.size()) {
      queue_free();
      return;
    }
    const JV &step = steps_[index_];
    int64_t delay_ms = 0;
    if (auto *d = step.Find("duration_ms")) {
      if (d->IsInt() && d->GetInt() > 0)
        delay_ms = d->GetInt();
    }
    std::string step_err = inject_step(step);
    if (!step_err.empty()) {

      push_game_error("game_bridge.cpp", "op_input sequence", 0,
                      "sequence step " + std::to_string(index_ + 1) +
                          " failed: " + step_err,
                      "", false, {});
    }
    index_++;
    if (index_ >= steps_.size()) {
      queue_free();
      return;
    }
    godot::SceneTree *tree = get_scene_tree();
    if (!tree) {
      queue_free();
      return;
    }

    timer_ = tree->create_timer(static_cast<double>(delay_ms) / 1000.0, true,
                                false, true);
    if (timer_.is_valid()) {
      godot::Error err = timer_->connect(
          "timeout",
          godot::Callable(this, godot::StringName("_on_step_timeout")));
      if (err != godot::OK)
        queue_free();
    } else {
      queue_free();
    }
  }
};

struct KeyCodeEntry {
  const char *name;
  godot::Key key;
};

constexpr KeyCodeEntry kKeyCodeNameTable[] = {
    {"SPACE", godot::KEY_SPACE},
    {"ENTER", godot::KEY_ENTER},
    {"RETURN", godot::KEY_ENTER},
    {"ESCAPE", godot::KEY_ESCAPE},
    {"TAB", godot::KEY_TAB},
    {"BACKSPACE", godot::KEY_BACKSPACE},
    {"INSERT", godot::KEY_INSERT},
    {"DELETE", godot::KEY_DELETE},
    {"PAUSE", godot::KEY_PAUSE},
    {"PRINT", godot::KEY_PRINT},
    {"CLEAR", godot::KEY_CLEAR},
    {"SHIFT", godot::KEY_SHIFT},
    {"CTRL", godot::KEY_CTRL},
    {"CONTROL", godot::KEY_CTRL},
    {"META", godot::KEY_META},
    {"ALT", godot::KEY_ALT},
    {"CAPSLOCK", godot::KEY_CAPSLOCK},
    {"NUMLOCK", godot::KEY_NUMLOCK},
    {"SCROLLLOCK", godot::KEY_SCROLLLOCK},
    {"MENU", godot::KEY_MENU},
    {"HOME", godot::KEY_HOME},
    {"END", godot::KEY_END},
    {"LEFT", godot::KEY_LEFT},
    {"RIGHT", godot::KEY_RIGHT},
    {"UP", godot::KEY_UP},
    {"DOWN", godot::KEY_DOWN},
    {"PAGEUP", godot::KEY_PAGEUP},
    {"PAGEDOWN", godot::KEY_PAGEDOWN},
    {"F1", godot::KEY_F1},
    {"F2", godot::KEY_F2},
    {"F3", godot::KEY_F3},
    {"F4", godot::KEY_F4},
    {"F5", godot::KEY_F5},
    {"F6", godot::KEY_F6},
    {"F7", godot::KEY_F7},
    {"F8", godot::KEY_F8},
    {"F9", godot::KEY_F9},
    {"F10", godot::KEY_F10},
    {"F11", godot::KEY_F11},
    {"F12", godot::KEY_F12},
    {"MINUS", godot::KEY_MINUS},
    {"EQUAL", godot::KEY_EQUAL},
    {"BRACKETLEFT", godot::KEY_BRACKETLEFT},
    {"BRACKETRIGHT", godot::KEY_BRACKETRIGHT},
    {"BACKSLASH", godot::KEY_BACKSLASH},
    {"SEMICOLON", godot::KEY_SEMICOLON},
    {"APOSTROPHE", godot::KEY_APOSTROPHE},
    {"COMMA", godot::KEY_COMMA},
    {"PERIOD", godot::KEY_PERIOD},
    {"SLASH", godot::KEY_SLASH},
    {"QUOTELEFT", godot::KEY_QUOTELEFT},
    {"KP_ENTER", godot::KEY_KP_ENTER},
    {"KP_ADD", godot::KEY_KP_ADD},
    {"KP_SUBTRACT", godot::KEY_KP_SUBTRACT},
    {"KP_MULTIPLY", godot::KEY_KP_MULTIPLY},
    {"KP_DIVIDE", godot::KEY_KP_DIVIDE},
    {"KP_PERIOD", godot::KEY_KP_PERIOD},
};

char ascii_upper(char c) {
  return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

godot::Key parse_keycode(const JV &keycode) {
  auto parse_int_key = [](int64_t value) -> godot::Key {
    if (value > 0 && value <= 0xFFFFFF)
      return static_cast<godot::Key>(value);
    return godot::KEY_NONE;
  };
  if (keycode.IsInt())
    return parse_int_key(keycode.GetInt());
  if (!keycode.IsString())
    return godot::KEY_NONE;

  std::string s = keycode.GetString();
  if (s.empty())
    return godot::KEY_NONE;

  bool all_digits = true;
  for (char c : s) {
    if (c < '0' || c > '9') {
      all_digits = false;
      break;
    }
  }
  if (all_digits)
    return parse_int_key(std::stoll(s));

  if (s.size() == 1) {
    char c = ascii_upper(s[0]);
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      return static_cast<godot::Key>(c);
    }
    return godot::KEY_NONE;
  }

  std::string u = s;
  for (auto &c : u)
    c = ascii_upper(c);
  if (u.rfind("KEY_", 0) == 0)
    u = u.substr(4);

  for (const auto &entry : kKeyCodeNameTable) {
    if (u == entry.name)
      return entry.key;
  }
  return godot::KEY_NONE;
}

bool extract_position(const JV &params, godot::Vector2 &out) {
  auto *pos_p = params.Find("position");
  if (!pos_p || !pos_p->IsObject())
    return false;
  auto *x = pos_p->Find("x");
  auto *y = pos_p->Find("y");
  if (!x || !y || !x->IsNumber() || !y->IsNumber())
    return false;
  double px = x->IsInt() ? static_cast<double>(x->GetInt()) : x->GetDouble();
  double py = y->IsInt() ? static_cast<double>(y->GetInt()) : y->GetDouble();
  out = godot::Vector2(px, py);
  return true;
}

bool param_pressed(const JV &params, bool default_value) {
  if (auto *p = params.Find("pressed")) {
    if (p->IsBool())
      return p->GetBool();
  }
  return default_value;
}

std::string inject_step(const JV &step) {
  auto *type_p = step.Find("type");
  if (!type_p || !type_p->IsString()) {
    return "input requires type (key|mouse_button|action)";
  }
  std::string type = type_p->GetString();

  auto *input = godot::Input::get_singleton();
  if (!input)
    return "Input singleton not available";

  std::string mode = "event";
  if (auto *mode_p = step.Find("mode")) {
    if (mode_p->IsString())
      mode = mode_p->GetString();
  }

  bool mode_api = (mode == "api");
  bool keep_pressed = (mode == "hold");
  int64_t duration_ms = 0;
  if (auto *dur_p = step.Find("duration_ms")) {
    if (dur_p->IsInt() && dur_p->GetInt() > 0)
      duration_ms = dur_p->GetInt();
  }
  bool pressed = param_pressed(step, true);

  std::string release_type;
  godot::Key key = godot::KEY_NONE;
  int64_t button_index = 0;
  godot::Vector2 pos;
  bool has_pos = false;
  godot::StringName action;

  if (type == "key") {
    auto *kc = step.Find("keycode");
    if (!kc || !(kc->IsInt() || kc->IsString())) {
      return "input key requires keycode (numeric key code or key name)";
    }
    key = parse_keycode(*kc);
    if (key == godot::KEY_NONE) {
      return "invalid keycode: " +
             (kc->IsString() ? kc->GetString() : std::to_string(kc->GetInt()));
    }
    if (pressed) {
      dispatch_input_event(input, 0, key, 0, pos, has_pos, false,
                           godot::StringName(), false, true);
    }
    dispatch_input_event(input, 0, key, 0, pos, has_pos, pressed,
                         godot::StringName(), false, true);
    release_type = "key";
  } else if (type == "mouse_button") {
    auto *bi = step.Find("button_index");
    if (!bi || !bi->IsInt()) {
      return "input mouse_button requires button_index (integer)";
    }
    button_index = bi->GetInt();
    if (extract_position(step, pos))
      has_pos = true;
    if (pressed) {
      dispatch_input_event(input, 1, key, button_index, pos, has_pos, false,
                           godot::StringName(), false, true);
    }
    dispatch_input_event(input, 1, key, button_index, pos, has_pos, pressed,
                         godot::StringName(), false, true);
    release_type = "mouse_button";
  } else if (type == "action") {
    auto *act = step.Find("action");
    if (!act || !act->IsString()) {
      return "input action requires action (string)";
    }
    action = godot::StringName(act->GetString().c_str());
    if (pressed) {

      input->action_release(action);
    }
    dispatch_input_event(input, 2, key, button_index, pos, has_pos, pressed,
                         action, mode_api, mode_api ? false : pressed);
    release_type = "action";
  } else {
    return "unknown input type: " + type;
  }

  if (pressed && duration_ms > 0 && !release_type.empty()) {
    schedule_release(release_type, key, button_index, pos, has_pos, action,
                     mode_api, duration_ms, keep_pressed);
  }
  return "";
}

JV op_input_sequence(const JV &seq_value) {
  if (!seq_value.IsArray()) {
    return error_result("input sequence must be an array");
  }
  const auto &items = seq_value.GetArray();
  if (items.empty()) {
    return error_result("input sequence must not be empty");
  }
  std::vector<JV> steps;
  steps.reserve(items.size());
  for (const JV &item : items) {
    if (!item.IsObject()) {
      return error_result("input sequence item must be an object with "
                          "type/keycode or type/action fields");
    }
    steps.push_back(item);
  }
  GameBridgeInputSequence *seq = memnew(GameBridgeInputSequence);
  seq->setup(std::move(steps));
  JV inner(JV::object_tag);
  inner["sequence"] = JV("scheduled");
  inner["steps"] = JV(static_cast<int64_t>(items.size()));
  return ok_result(std::move(inner));
}

} // namespace

JV op_input(const JV &params, int64_t request_id) {
  (void)request_id;
  if (auto *seq_p = params.Find("sequence")) {

    return op_input_sequence(*seq_p);
  }
  auto *type_p = params.Find("type");
  if (!type_p || !type_p->IsString()) {
    return error_result("input requires type (key|mouse_button|action)");
  }
  std::string step_err = inject_step(params);
  if (!step_err.empty())
    return error_result(step_err);

  bool pressed = param_pressed(params, true);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  if (pressed) {
    auto *engine = godot::Engine::get_singleton();
    if (engine) {
      r["injected_at_physics_frame"] =
          JV(static_cast<int64_t>(engine->get_physics_frames()));
      r["expected_visible_frame"] =
          JV(static_cast<int64_t>(engine->get_physics_frames() + 1));

      r[GDA_FIELD_PARSED_PHYSICS_FRAME] =
          JV(static_cast<int64_t>(engine->get_physics_frames()));
      r[GDA_FIELD_PARSED_PROCESS_FRAME] =
          JV(static_cast<int64_t>(engine->get_process_frames()));
    }
    if (auto *tree = get_scene_tree()) {
      if (tree->is_paused()) {
        r["warning"] = JV("game paused: transient input edge will not be "
                          "consumed by physics callbacks while paused");
      }
    }
  }
  return r;
}

JV op_input_wait(const JV &params, int64_t request_id) {
  auto *act = params.Find("action");
  if (!act || !act->IsString()) {
    return error_result("input_wait requires action (string)");
  }
  std::string state = "just_pressed";
  if (auto *state_p = params.Find("state")) {
    if (state_p->IsString())
      state = state_p->GetString();
  }
  int state_kind;
  if (state == "just_pressed")
    state_kind = 0;
  else if (state == "just_released")
    state_kind = 1;
  else if (state == "pressed")
    state_kind = 2;
  else
    return error_result(
        "input_wait state must be just_pressed|just_released|pressed");

  int64_t timeout_ms = 2000;
  if (auto *tp = params.Find("timeout_ms")) {
    if (tp->IsInt() && tp->GetInt() > 0)
      timeout_ms = tp->GetInt();
  }
  if (timeout_ms > GDA_MAX_TIMEOUT_MS)
    timeout_ms = GDA_MAX_TIMEOUT_MS;

  godot::SceneTree *tree = get_scene_tree();
  if (!tree)
    return error_result("no scene tree");
  godot::Node *root = tree->get_root();
  if (!root)
    return error_result("no root node");

  if (auto *inject_p = params.Find("inject")) {
    if (inject_p->IsObject()) {
      JV inj_result = op_input(*inject_p, request_id);
      if (inj_result.Contains("error"))
        return inj_result;
    }
  }

  GameBridgeInputWatcher *watcher = memnew(GameBridgeInputWatcher);
  watcher->setup(request_id, godot::StringName(act->GetString().c_str()),
                 state_kind, timeout_ms / 1000.0);
  root->add_child(watcher);
  register_cancel_handler(request_id, [watcher] { watcher->cancel(); });
  return JV();
}

JV op_input_status(const JV &params) {
  auto *act = params.Find("action");
  if (!act || !act->IsString()) {
    return error_result("input_status requires action (string)");
  }
  auto *input = godot::Input::get_singleton();
  if (!input)
    return error_result("Input singleton not available");
  auto *engine = godot::Engine::get_singleton();
  if (!engine)
    return error_result("Engine singleton not available");
  godot::StringName action(act->GetString().c_str());
  JV r(JV::object_tag);
  r["pressed"] = JV(input->is_action_pressed(action));
  r["just_pressed"] = JV(input->is_action_just_pressed(action));
  r["just_released"] = JV(input->is_action_just_released(action));
  r["physics_frame"] = JV(static_cast<int64_t>(engine->get_physics_frames()));
  r["paused"] = JV(get_scene_tree() ? get_scene_tree()->is_paused() : false);
  return ok_result(std::move(r));
}

void register_input_bridge_classes() {
  godot::ClassDB::register_class<GameBridgeInputWatcher>();
  godot::ClassDB::register_class<GameBridgeDelayedRelease>();
  godot::ClassDB::register_class<GameBridgeInputSequence>();
}

} // namespace game_bridge
} // namespace runtime
} // namespace godot_autopilot
