#ifndef GODOT_AUTOPILOT_INPUT_TOOLS_HPP
#define GODOT_AUTOPILOT_INPUT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/input_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace input_tools {

GDA_TOOL_CLASS(PressInputActionTool, "press_input_action",
               "Press an input action in the editor process by injecting a press event through the Input singleton. 'strength' sets the analog value and defaults to 1.0. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "action", "press"}), input_ops::handle_action_press, false)

GDA_TOOL_CLASS(ReleaseInputActionTool, "release_input_action",
               "Release a previously pressed input action in the editor process. The action stays pressed until released, so pair this with press_input_action. Pressing or releasing an undefined action is harmless. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "action", "release"}), input_ops::handle_action_release, false)

GDA_TOOL_CLASS(IsInputActionPressedTool, "is_input_action_pressed",
               "Check whether an input action is currently held down in the editor process. This reflects continuous state: it stays true on every frame while the action is pressed, until release_input_action is called. A running game is a separate process with its own input state. Returns a boolean in 'result'.",
               "Input", std::vector<std::string>({"input", "action", "pressed"}), input_ops::handle_is_action_pressed, false)

GDA_TOOL_CLASS(IsInputActionJustPressedTool, "is_input_action_just_pressed",
               "Check whether an input action was just pressed in the editor process. The result is transient: it is true only on the single frame in which the press is injected and false on the next frame, unlike is_input_action_pressed which stays true while held. A running game is a separate process. Returns a boolean in 'result'.",
               "Input", std::vector<std::string>({"input", "action", "just_pressed"}), input_ops::handle_is_action_just_pressed, false)

GDA_TOOL_CLASS(PressInputKeyTool, "press_input_key",
               "Inject a key press event into the editor process. 'key' accepts a single character A-Z or 0-9, or one of the named keys SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP and DOWN; any other value returns 'invalid key'. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "key", "press"}), input_ops::handle_key_press, true)

GDA_TOOL_CLASS(ReleaseInputKeyTool, "release_input_key",
               "Inject a key release event into the editor process, matching the restricted key set of press_input_key: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP and DOWN. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "key", "release"}), input_ops::handle_key_release, true)

GDA_TOOL_CLASS(MoveInputMouseTool, "move_input_mouse",
               "Inject a mouse motion event into the editor process at the given position. 'position' is an object with numeric x and y fields, in pixels relative to the client area of the focused window — the editor main window while it has focus — so (0, 0) means that window's top-left corner, not a desktop/screen position and not the edited viewport (get_display_mouse_position reports screen coordinates instead). To drive editor docks and other UI, pass main-window client-area coordinates: warp_display_mouse to the target first, then inject, or estimate them from the window size. Optional 'relative' sets the movement delta since the last event and defaults to zero. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "mouse", "move"}), input_ops::handle_mouse_move, false)

GDA_TOOL_CLASS(PressInputMouseButtonTool, "press_input_mouse_button",
               "Inject a mouse button press into the editor process. 'button' accepts only left, right or middle. Optional 'position' sets where the press occurs, in pixels relative to the client area of the focused window (default (0, 0), that window's top-left corner — not a desktop/screen position); to click editor docks and UI, pass main-window client-area coordinates, e.g. warp_display_mouse to the target first, then inject. Many controls react to the press event itself, so the press usually takes effect immediately; release_input_mouse_button is optional but recommended to pair press and release. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "mouse", "press"}), input_ops::handle_mouse_button_press, false)

GDA_TOOL_CLASS(ReleaseInputMouseButtonTool, "release_input_mouse_button",
               "Inject a mouse button release into the editor process. 'button' accepts only left, right or middle, matching press_input_mouse_button. Optional 'position' sets where the release occurs, in pixels relative to the client area of the focused window (default (0, 0), that window's top-left corner — not a desktop/screen position); use the same client-area coordinates as the matching press. Editor controls commonly act on the press event itself, so a release alone often has no effect; call it after press_input_mouse_button to keep press and release paired. A running game is a separate process and will not receive these events. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "mouse", "release"}), input_ops::handle_mouse_button_release, false)

GDA_TOOL_CLASS(StartInputGamepadVibrationTool, "start_input_gamepad_vibration",
               "Start vibration on a gamepad connected to the editor process. 'device' is the gamepad index and defaults to 0; 'weak' and 'strong' set the two motor magnitudes from 0.0 to 1.0 and default to 0.5. 'duration' is in seconds and defaults to 0, which vibrates indefinitely until stop_input_gamepad_vibration is called. A running game is a separate process and is not affected.",
               "Input", std::vector<std::string>({"input", "gamepad", "vibration"}), input_ops::handle_gamepad_vibration_start, true)

GDA_TOOL_CLASS(StopInputGamepadVibrationTool, "stop_input_gamepad_vibration",
               "Stop vibration on a gamepad connected to the editor process, ending any vibration started with start_input_gamepad_vibration. 'device' is the gamepad index and defaults to 0. A running game is a separate process and is not affected. Returns 'result' set to 'ok'.",
               "Input", std::vector<std::string>({"input", "gamepad", "vibration"}), input_ops::handle_gamepad_vibration_stop, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(11);
  v.push_back(std::make_unique<PressInputActionTool>());
  v.push_back(std::make_unique<ReleaseInputActionTool>());
  v.push_back(std::make_unique<IsInputActionPressedTool>());
  v.push_back(std::make_unique<IsInputActionJustPressedTool>());
  v.push_back(std::make_unique<PressInputKeyTool>());
  v.push_back(std::make_unique<ReleaseInputKeyTool>());
  v.push_back(std::make_unique<MoveInputMouseTool>());
  v.push_back(std::make_unique<PressInputMouseButtonTool>());
  v.push_back(std::make_unique<ReleaseInputMouseButtonTool>());
  v.push_back(std::make_unique<StartInputGamepadVibrationTool>());
  v.push_back(std::make_unique<StopInputGamepadVibrationTool>());
  return v;
}

} // namespace input_tools
} // namespace godot_autopilot

#endif