#ifndef GODOT_AUTOPILOT_INPUT_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/input_click_ops.hpp"
#include "tools/input_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace input_tools {

namespace {

const std::vector<ParamSpec> kPressInputActionParams = {
    {"action", "string", "Input action name to press, e.g. 'ui_accept'", true},
    {"strength", "number", "Analog strength 0.0 to 1.0 (default: 1.0)", false},
};

const std::vector<ParamSpec> kReleaseInputActionParams = {
    {"action", "string", "Input action name to release, e.g. 'ui_accept'", true},
};

const std::vector<ParamSpec> kIsInputActionPressedParams = {
    {"action", "string", "Input action name to check, e.g. 'ui_accept'; true while the action is held down", true},
};

const std::vector<ParamSpec> kIsInputActionJustPressedParams = {
    {"action", "string", "Input action name to check, e.g. 'ui_accept'; true only on the frame in which the press is injected", true},
};

const std::vector<ParamSpec> kPressInputKeyParams = {
    {"key", "string", "Key to press: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP, DOWN; other values return 'invalid key'", true},
};

const std::vector<ParamSpec> kReleaseInputKeyParams = {
    {"key", "string", "Key to release: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP, DOWN; other values return 'invalid key'", true},
};

const std::vector<ParamSpec> kMoveInputMouseParams = {
    {"position", "object", "Mouse position, object with numeric x and y fields", true},
    {"relative", "object", "Relative movement delta since the last event, object with numeric x and y fields (default: 0, 0)", false},
};

const std::vector<ParamSpec> kPressInputMouseButtonParams = {
    {"button", "string", "Mouse button to press: left, right or middle only", true},
    {"position", "object", "Position of the press, object with numeric x and y fields (default: 0, 0)", false},
};

const std::vector<ParamSpec> kReleaseInputMouseButtonParams = {
    {"button", "string", "Mouse button to release: left, right or middle only", true},
    {"position", "object", "Position of the release, object with numeric x and y fields (default: 0, 0)", false},
};

const std::vector<ParamSpec> kClickInputMouseParams = {
    {"position", "object", "Click position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
    {"button", "string", "Mouse button to click: left, right or middle (default: left)", false},
    {"double_click", "boolean", "Send a second press/release pair to form a double click (default: false)", false},
    {"warp", "boolean", "Warp the physical cursor onto the position before clicking (default: true)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kScrollInputMouseParams = {
    {"direction", "string", "Wheel direction: up, down, left or right", true},
    {"position", "object", "Pointer position the wheel event is routed to, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
    {"amount", "integer", "Number of wheel detents, an integer from 1 to 10 (default: 1)", false},
    {"warp", "boolean", "Warp the physical cursor onto the position first (default: true)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kDragInputMouseParams = {
    {"from", "object", "Drag start position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
    {"to", "object", "Drag end position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
    {"button", "string", "Mouse button to drag with: left, right or middle (default: left)", false},
    {"steps", "integer", "Number of interpolated motion events between from and to, an integer from 1 to 64 (default: 8)", false},
    {"warp", "boolean", "Warp the physical cursor onto the start position first (default: true)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kTypeInputTextParams = {
    {"text", "string", "Text to write into the focused control; may contain any UTF-8 text", true},
    {"submit", "boolean", "Append an Enter key press and release after the text (default: false)", false},
    {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
};

const std::vector<ParamSpec> kStartInputGamepadVibrationParams = {
    {"device", "integer", "Gamepad device index (default: 0)", true},
    {"weak", "number", "Weak motor magnitude 0.0 to 1.0 (default: 0.5)", true},
    {"strong", "number", "Strong motor magnitude 0.0 to 1.0 (default: 0.5)", true},
    {"duration", "number", "Vibration duration in seconds; 0 (default) vibrates indefinitely until stop_input_gamepad_vibration", false},
};

const std::vector<ParamSpec> kStopInputGamepadVibrationParams = {
    {"device", "integer", "Gamepad device index (default: 0)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(make_spec_tool(ToolSpec{
      "press_input_action",
      "Press an input action in the editor process by injecting a press event through the Input singleton. 'strength' sets the analog value and defaults to 1.0. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "action", "press"}, SideEffect::None, tool_flags::kNone,
      kPressInputActionParams, input_ops::handle_action_press}));
  v.push_back(make_spec_tool(ToolSpec{
      "release_input_action",
      "Release a previously pressed input action in the editor process. The action stays pressed until released, so pair this with press_input_action. Pressing or releasing an undefined action is harmless. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "action", "release"}, SideEffect::None, tool_flags::kNone,
      kReleaseInputActionParams, input_ops::handle_action_release}));
  v.push_back(make_spec_tool(ToolSpec{
      "is_input_action_pressed",
      "Check whether an input action is currently held down in the editor process. This reflects continuous state: it stays true on every frame while the action is pressed, until release_input_action is called. A running game is a separate process with its own input state. Returns a boolean in 'result'.",
      "Input", {"input", "action", "pressed"}, SideEffect::None, tool_flags::kNone,
      kIsInputActionPressedParams, input_ops::handle_is_action_pressed}));
  v.push_back(make_spec_tool(ToolSpec{
      "is_input_action_just_pressed",
      "Check whether an input action was just pressed in the editor process. The result is transient: it is true only on the single frame in which the press is injected and false on the next frame, unlike is_input_action_pressed which stays true while held. A running game is a separate process. Returns a boolean in 'result'.",
      "Input", {"input", "action", "just_pressed"}, SideEffect::None, tool_flags::kNone,
      kIsInputActionJustPressedParams, input_ops::handle_is_action_just_pressed}));
  v.push_back(make_spec_tool(ToolSpec{
      "press_input_key",
      "Inject a key press event into the editor process. 'key' accepts a single character A-Z or 0-9, or one of the named keys SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP and DOWN; any other value returns 'invalid key'. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "key", "press"}, SideEffect::None, tool_flags::kNone,
      kPressInputKeyParams, input_ops::handle_key_press}));
  v.push_back(make_spec_tool(ToolSpec{
      "release_input_key",
      "Inject a key release event into the editor process, matching the restricted key set of press_input_key: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP and DOWN. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "key", "release"}, SideEffect::None, tool_flags::kNone,
      kReleaseInputKeyParams, input_ops::handle_key_release}));
  v.push_back(make_spec_tool(ToolSpec{
      "move_input_mouse",
      "Inject a mouse motion event into the editor process at the given position. 'position' is an object with numeric x and y fields, in pixels relative to the client area of the focused window — the editor main window while it has focus — so (0, 0) means that window's top-left corner, not a desktop/screen position and not the edited viewport (get_display_mouse_position reports screen coordinates instead). To drive editor docks and other UI, pass main-window client-area coordinates: warp_display_mouse to the target first, then inject, or estimate them from the window size. Optional 'relative' sets the movement delta since the last event and defaults to zero. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "mouse", "move"}, SideEffect::None, tool_flags::kNone,
      kMoveInputMouseParams, input_ops::handle_mouse_move}));
  v.push_back(make_spec_tool(ToolSpec{
      "press_input_mouse_button",
      "Inject a mouse button press into the editor process. 'button' accepts only left, right or middle. Optional 'position' sets where the press occurs, in pixels relative to the client area of the focused window (default (0, 0), that window's top-left corner — not a desktop/screen position); to click editor docks and UI, pass main-window client-area coordinates, e.g. warp_display_mouse to the target first, then inject. Many controls react to the press event itself, so the press usually takes effect immediately; release_input_mouse_button is optional but recommended to pair press and release. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "mouse", "press"}, SideEffect::None, tool_flags::kNone,
      kPressInputMouseButtonParams, input_ops::handle_mouse_button_press}));
  v.push_back(make_spec_tool(ToolSpec{
      "release_input_mouse_button",
      "Inject a mouse button release into the editor process. 'button' accepts only left, right or middle, matching press_input_mouse_button. Optional 'position' sets where the release occurs, in pixels relative to the client area of the focused window (default (0, 0), that window's top-left corner — not a desktop/screen position); use the same client-area coordinates as the matching press. Editor controls commonly act on the press event itself, so a release alone often has no effect; call it after press_input_mouse_button to keep press and release paired. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
      "Input", {"input", "mouse", "release"}, SideEffect::None, tool_flags::kNone,
      kReleaseInputMouseButtonParams, input_ops::handle_mouse_button_release}));
  v.push_back(make_spec_tool(ToolSpec{
      "click_input_mouse",
      "Click a point in the editor process by injecting a complete mouse sequence in one call: optional warp, a motion event, a press and a release, plus a second press/release pair when 'double_click' is true. 'position' is an object with numeric x and y fields, in pixels relative to the client area of the focused window — the editor main window while it has focus — so (0, 0) is that window's top-left corner, not a desktop/screen position and not the edited viewport. Prefer this composite over hand-chaining move_input_mouse, press_input_mouse_button and release_input_mouse_button; for editor UI elements prefer click_editor_element, which resolves the element to coordinates itself. 'button' accepts left, right or middle and defaults to left; 'warp' (default true) moves the physical cursor onto the target first. Controls that react to hover and drags depend on the physical mouse, so the effect may only settle on the next frame — re-observe before concluding that the click did nothing. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. A running game is a separate process and will not receive these events. Returns result with ok true.",
      "Input", {"input", "mouse", "click"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage, kClickInputMouseParams,
      input_click_ops::handle_click_mouse}));
  v.push_back(make_spec_tool(ToolSpec{
      "scroll_input_mouse",
      "Scroll the mouse wheel at a point in the editor process. 'direction' is required and accepts up, down, left or right; 'amount' is the number of wheel detents, an integer from 1 to 10 defaulting to 1, and out-of-range values return an error. 'position' is required because the wheel event is routed to the control hit at that point: an object with numeric x and y fields, in pixels relative to the client area of the focused window — the editor main window while it has focus — not a desktop/screen position. 'warp' (default true) moves the physical cursor onto that point first. Scroll containers depend on the physical mouse and advance over the next frame, so re-observe before concluding that the scroll did nothing. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. A running game is a separate process and will not receive these events. Returns result with ok true.",
      "Input", {"input", "mouse", "scroll"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage, kScrollInputMouseParams,
      input_click_ops::handle_scroll_mouse}));
  v.push_back(make_spec_tool(ToolSpec{
      "drag_input_mouse",
      "Drag a point to another point in the editor process by injecting a complete mouse sequence in one call: optional warp to 'from', a motion event, a button press, 'steps' interpolated motion events and a release at 'to'. 'from' and 'to' are objects with numeric x and y fields, in pixels relative to the client area of the focused window — the editor main window while it has focus — not desktop/screen positions. 'button' accepts left, right or middle and defaults to left; 'steps' is the number of interpolated motion events, an integer from 1 to 64 defaulting to 8, and out-of-range values return an error, with the per-step relative delta derived from 'from' and 'to'; 'warp' (default true) moves the physical cursor onto 'from' first. Drag-and-drop depends on the physical mouse and the interpolated path, so the drop may only settle on the next frame — re-observe before concluding that the drag failed. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. A running game is a separate process and will not receive these events. Returns result with ok true.",
      "Input", {"input", "mouse", "drag"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage, kDragInputMouseParams,
      input_click_ops::handle_drag_mouse}));
  v.push_back(make_spec_tool(ToolSpec{
      "type_input_text",
      "Write text into the control that currently has keyboard focus in the editor process by pushing the whole string as one text-input event, not by simulating individual key presses. Focus the target first with click_input_mouse or click_editor_element; typing while a dock has no focus does nothing. 'text' is required and may contain any UTF-8 text; 'submit' (default false) appends an Enter key press and release, e.g. to confirm a LineEdit. Optional 'observe' (default false) appends a fresh editor viewport capture to the result. A running game is a separate process and will not receive this input. Returns result with ok true and 'length', the number of characters written.",
      "Input", {"input", "text", "type"}, SideEffect::ModifiesWindow,
      tool_flags::kMutating | tool_flags::kObserve | tool_flags::kCaptureImage, kTypeInputTextParams,
      input_click_ops::handle_type_text}));
  v.push_back(make_spec_tool(ToolSpec{
      "start_input_gamepad_vibration",
      "Start vibration on a gamepad connected to the editor process. 'device' is the gamepad index and defaults to 0; 'weak' and 'strong' set the two motor magnitudes from 0.0 to 1.0 and default to 0.5. 'duration' is in seconds and defaults to 0, which vibrates indefinitely until stop_input_gamepad_vibration is called. A running game is a separate process and is not affected.",
      "Input", {"input", "gamepad", "vibration"}, SideEffect::None, tool_flags::kNone,
      kStartInputGamepadVibrationParams, input_ops::handle_gamepad_vibration_start}));
  v.push_back(make_spec_tool(ToolSpec{
      "stop_input_gamepad_vibration",
      "Stop vibration on a gamepad connected to the editor process, ending any vibration started with start_input_gamepad_vibration. 'device' is the gamepad index and defaults to 0. A running game is a separate process and is not affected. Returns 'result' set to 'ok'.",
      "Input", {"input", "gamepad", "vibration"}, SideEffect::None, tool_flags::kNone,
      kStopInputGamepadVibrationParams, input_ops::handle_gamepad_vibration_stop}));
  return v;
}

} // namespace input_tools
} // namespace godot_autopilot

#endif
