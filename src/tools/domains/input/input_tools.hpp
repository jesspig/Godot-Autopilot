#ifndef GODOT_AUTOPILOT_INPUT_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/input/input_click_ops.hpp"
#include "tools/domains/input/input_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace input_tools {

namespace {

const std::vector<ParamSpec> kGetInputStateParams = {
    {"name", "string", "Input action name to inspect, e.g. 'ui_accept'", true},
};

const std::vector<ParamSpec> kInputActionParams = {
    {"name", "string", "Input action name, e.g. 'ui_accept'", true},
    {"strength", "number", "Analog strength 0.0 to 1.0 for action='press' (default: 1.0)", false},
};

const std::vector<ParamSpec> kInputKeyParams = {
    {"key", "string", "Key name: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP, DOWN; other values return 'invalid key'", true},
};

const std::vector<ParamSpec> kInputMouseParams = {
    {"position", "object", "Pointer position as an object with numeric x and y fields, in pixels relative to the client area of the focused window; used by action='move', 'press', 'release', 'click' and 'scroll'", false},
    {"relative", "object", "Relative movement delta since the last event for action='move', object with numeric x and y fields (default: 0, 0)", false},
    {"button", "string", "Mouse button for action='press', 'release', 'click' and 'drag': left, right or middle", false},
    {"double_click", "boolean", "Send a second press/release pair for action='click' (default: false)", false},
    {"warp", "boolean", "Warp the physical cursor onto the target position before clicking, scrolling or dragging (default: true)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
    {"direction", "string", "Wheel direction for action='scroll': up, down, left or right", false},
    {"amount", "integer", "Number of wheel detents for action='scroll', an integer from 1 to 10 (default: 1)", false},
    {"from", "object", "Drag start position for action='drag', object with numeric x and y fields", false},
    {"to", "object", "Drag end position for action='drag', object with numeric x and y fields", false},
    {"steps", "integer", "Number of interpolated motion events for action='drag', an integer from 1 to 64 (default: 8)", false},
};

const std::vector<ParamSpec> kInputTextParams = {
    {"text", "string", "Text to write into the focused control; may contain any UTF-8 text", true},
    {"submit", "boolean", "Append an Enter key press and release after the text (default: false)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kInputGamepadParams = {
    {"device", "integer", "Gamepad device index (default: 0)", false},
    {"weak", "number", "Weak motor magnitude 0.0 to 1.0 for action='start_vibration' (default: 0.5)", false},
    {"strong", "number", "Strong motor magnitude 0.0 to 1.0 for action='start_vibration' (default: 0.5)", false},
    {"duration", "number", "Vibration duration in seconds for action='start_vibration'; 0 (default) vibrates indefinitely until action='stop_vibration'", false},
};

inline std::vector<std::string> input_actions(
    std::initializer_list<const char *> actions) {
  std::vector<std::string> out;
  out.reserve(actions.size());
  for (const char *action : actions) {
    out.emplace_back(action);
  }
  return out;
}

inline std::vector<std::string> input_examples(
    std::initializer_list<const char *> examples) {
  std::vector<std::string> out;
  out.reserve(examples.size());
  for (const char *example : examples) {
    out.emplace_back(example);
  }
  return out;
}

inline ToolSpec input_action_spec(ToolSpec spec,
                                  std::initializer_list<const char *> actions,
                                  std::initializer_list<const char *> examples) {
  spec.raw_schema = schema::build_action_schema(
      input_actions(actions), spec.params,
      "Action to perform; per-action parameters are documented in the tool "
      "description");
  spec.examples = input_examples(examples);
  return spec;
}

inline mcp::JsonValue input_args_with_action(const mcp::JsonValue &args,
                                             const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue input_action_target_args(const mcp::JsonValue &args,
                                               const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *target = args.Find("action");
  if (target != nullptr) {
    out["name"] = *target;
  }
  const mcp::JsonValue *strength = args.Find("strength");
  if (strength != nullptr) {
    out["strength"] = *strength;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline ToolAlias input_alias(const char *name, const char *canonical,
                             const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return input_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(6);
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "get_input_state",
              "Read input action state in the editor process, actions: action_pressed (true while the action is held down) and action_just_pressed (true only on the single frame in which the press is injected). name is the input action, e.g. 'ui_accept'. A running game is a separate process with its own input state. Returns a boolean in result. Deprecated aliases: is_input_action_pressed, is_input_action_just_pressed.",
              "Input",
              {"input", "action", "pressed", "just_pressed",
               "is_input_action_pressed", "is_input_action_just_pressed"},
              SideEffect::None,
              tool_flags::kNone,
              kGetInputStateParams,
              input_ops::handle_get_input_state},
          {"action_pressed", "action_just_pressed"},
          {"{\"action\":\"action_pressed\",\"name\":\"ui_accept\"}",
           "{\"action\":\"action_just_pressed\",\"name\":\"ui_accept\"}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              return input_action_target_args(args, "action_pressed");
            };
            return alias;
          }(input_alias("is_input_action_pressed", "get_input_state",
                        "action_pressed")),
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              return input_action_target_args(args, "action_just_pressed");
            };
            return alias;
          }(input_alias("is_input_action_just_pressed", "get_input_state",
                        "action_just_pressed")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "input_action",
              "Press or release an input action in the editor process by injecting events through the Input singleton, actions: press (holds the action down; strength sets the analog value and defaults to 1.0) and release (the action stays pressed until released, so pair it with press). Pressing or releasing an undefined action is harmless. name is the input action, e.g. 'ui_accept'. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'. Deprecated aliases: press_input_action, release_input_action.",
              "Input",
              {"input", "action", "press", "release", "press_input_action",
               "release_input_action"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating,
              kInputActionParams,
              input_ops::handle_input_action},
          {"press", "release"},
          {"{\"action\":\"press\",\"name\":\"ui_accept\",\"strength\":1.0}",
           "{\"action\":\"release\",\"name\":\"ui_accept\"}"}),
      {
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              return input_action_target_args(args, "press");
            };
            return alias;
          }(input_alias("press_input_action", "input_action", "press")),
          [](ToolAlias alias) {
            alias.adapt_args = [](const mcp::JsonValue &args) {
              return input_action_target_args(args, "release");
            };
            return alias;
          }(input_alias("release_input_action", "input_action", "release")),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "input_key",
              "Inject raw key events into the editor process, actions: press and release. key accepts a single character A-Z or 0-9, or one of SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP and DOWN; any other value returns 'invalid key'. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'. Deprecated aliases: press_input_key, release_input_key.",
              "Input",
              {"input", "key", "press", "release", "press_input_key",
               "release_input_key"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating,
              kInputKeyParams,
              input_ops::handle_input_key},
          {"press", "release"},
          {"{\"action\":\"press\",\"key\":\"SPACE\"}",
           "{\"action\":\"release\",\"key\":\"SPACE\"}"}),
      {
          input_alias("press_input_key", "input_key", "press"),
          input_alias("release_input_key", "input_key", "release"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "input_mouse",
              "Inject mouse input into the editor process, actions: move (motion event at position with optional relative delta), press and release (button state at position; many editor controls act on the press itself, so pair press and release), click (complete sequence in one call: optional warp, motion, press, release and a second pair when double_click is true), scroll (wheel detents at position; direction up/down/left/right, amount 1-10) and drag (optional warp to from, motion, press, steps interpolated motions and release at to; steps 1-64). Positions are pixels relative to the client area of the focused window, not desktop or viewport coordinates; click, scroll and drag also accept observe=true to append a fresh editor viewport capture (the canonical tool carries the observe and capture-image flags, and may settle on the next frame). A running game is a separate process and will not receive these events. Results are {result:{ok:true}} for click/scroll/drag and {result:'ok'} for move/press/release. Deprecated aliases: move_input_mouse, press_input_mouse_button, release_input_mouse_button, click_input_mouse, scroll_input_mouse, drag_input_mouse.",
              "Input",
              {"input", "mouse", "move", "press", "release", "click",
               "scroll", "drag", "move_input_mouse",
               "press_input_mouse_button", "release_input_mouse_button",
               "click_input_mouse", "scroll_input_mouse", "drag_input_mouse"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating | tool_flags::kObserve |
                  tool_flags::kCaptureImage,
              kInputMouseParams,
              input_click_ops::handle_input_mouse},
          {"move", "press", "release", "click", "scroll", "drag"},
          {"{\"action\":\"move\",\"position\":{\"x\":200,\"y\":120}}",
           "{\"action\":\"press\",\"button\":\"left\",\"position\":{\"x\":200,\"y\":120}}",
           "{\"action\":\"release\",\"button\":\"left\",\"position\":{\"x\":200,\"y\":120}}",
           "{\"action\":\"click\",\"position\":{\"x\":200,\"y\":120}}",
           "{\"action\":\"scroll\",\"direction\":\"down\",\"position\":{\"x\":200,\"y\":120},\"amount\":2}",
           "{\"action\":\"drag\",\"from\":{\"x\":100,\"y\":100},\"to\":{\"x\":300,\"y\":200}}"}),
      {
          input_alias("move_input_mouse", "input_mouse", "move"),
          input_alias("press_input_mouse_button", "input_mouse", "press"),
          input_alias("release_input_mouse_button", "input_mouse", "release"),
          input_alias("click_input_mouse", "input_mouse", "click"),
          input_alias("scroll_input_mouse", "input_mouse", "scroll"),
          input_alias("drag_input_mouse", "input_mouse", "drag"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "input_text",
              "Write text into the control that currently has keyboard focus in the editor process by pushing the whole string as one text-input event, not by simulating individual key presses, actions: type (text is required and may contain any UTF-8 text; submit=true appends an Enter key press and release; observe=true appends a fresh editor viewport capture). Focus the target first with input_mouse action='click' or click_editor_element; typing while a dock has no focus does nothing. A running game is a separate process and will not receive this input. Returns {result:{ok:true, length}}. Deprecated alias: type_input_text.",
              "Input",
              {"input", "text", "type", "type_input_text"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating | tool_flags::kObserve |
                  tool_flags::kCaptureImage,
              kInputTextParams,
              input_click_ops::handle_input_text},
          {"type"},
          {"{\"action\":\"type\",\"text\":\"Player\",\"submit\":true}"}),
      {
          input_alias("type_input_text", "input_text", "type"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      input_action_spec(
          ToolSpec{
              "input_gamepad",
              "Control gamepad vibration on a device connected to the editor process, actions: start_vibration (device defaults to 0; weak and strong set the two motor magnitudes 0.0-1.0 and default to 0.5; duration is in seconds, 0 vibrates indefinitely) and stop_vibration (device defaults to 0). A running game is a separate process and is not affected. Returns 'result' set to 'ok'. Deprecated aliases: start_input_gamepad_vibration, stop_input_gamepad_vibration.",
              "Input",
              {"input", "gamepad", "vibration",
               "start_input_gamepad_vibration",
               "stop_input_gamepad_vibration"},
              SideEffect::ModifiesWindow,
              tool_flags::kMutating,
              kInputGamepadParams,
              input_ops::handle_input_gamepad},
          {"start_vibration", "stop_vibration"},
          {"{\"action\":\"start_vibration\",\"device\":0,\"weak\":0.5,\"strong\":0.5,\"duration\":1.0}",
           "{\"action\":\"stop_vibration\",\"device\":0}"}),
      {
          input_alias("start_input_gamepad_vibration", "input_gamepad",
                      "start_vibration"),
          input_alias("stop_input_gamepad_vibration", "input_gamepad",
                      "stop_vibration"),
      })));
  return v;
}

} // namespace input_tools
} // namespace godot_autopilot

#endif
