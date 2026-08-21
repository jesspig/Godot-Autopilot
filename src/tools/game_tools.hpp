#ifndef GODOT_AUTOPILOT_GAME_TOOLS_HPP
#define GODOT_AUTOPILOT_GAME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/runtime_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace game_tools {

GDA_TOOL_CLASS(GetGameStatusTool, "get_game_status",
               "Query a running game process over the runtime debug channel: engine version, current scene, node count, FPS, plus paused and physics_frame to distinguish a fake run. Requires a game launched from the editor whose project loads the godot-autopilot extension. healthy and last_activity_ms detect liveness/stall; physics_stalled indicates a frozen physics loop. An empty scene with a sharp node_count drop means a scene reload failed — stop and rerun the game.",
               "Game", std::vector<std::string>({"game", "runtime", "status", "debug"}), runtime_ops::handle_game_status, true)

GDA_TOOL_CLASS(ExecuteGameScriptTool, "execute_game_script",
               "Run code or inspect/modify state inside the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. action is 'script' (GDScript must extend Node and define func _run()), 'get_property', 'set_property' or 'call_method'. persist (default false) keeps the temporary node alive under /root/__gda_runtime and returns its path for later calls; persist_name names it. timeout_ms bounds the in-game execution; on expiry an idempotent cancel interrupts the pending await. call_method on an await method waits for the coroutine to complete (bounded by timeout_ms) and returns its final result; on timeout it returns an error.",
               "Game", std::vector<std::string>({"game", "runtime", "eval", "script", "debug"}), runtime_ops::handle_game_eval, true)

GDA_TOOL_CLASS(ReloadGameScriptsTool, "reload_game_scripts",
               "Reload GDScript files in the running game process over the runtime debug channel without restarting the game; engine soft reload keeps instance state where possible. Requires a game launched from the editor whose project loads the godot-autopilot extension. paths is a non-empty array of res:// script paths (pass multiple files in one call). The reload request is sent to the game and applied during its next idle poll; no confirmation is returned — verify with get_game_log_entries or get_game_status.",
               "Game", std::vector<std::string>({"game", "runtime", "reload", "script", "debug"}), runtime_ops::handle_game_reload_scripts, true)

GDA_TOOL_CLASS(QueueGameInputTool, "queue_game_input",
               "Simulate input inside the running game process over the runtime debug channel; unlike the editor-process input_* tools, the running game receives these events. Requires a game launched from the editor whose project loads the godot-autopilot extension. type is 'key', 'mouse_button' or 'action'; mode is 'event' (parse_input_event, default), 'api' (action_press/release, immediate) or 'hold' (event-style, kept pressed for duration_ms). Unknown parameters are ignored and reported in ignored_params. Transient states (is_action_just_pressed) are visible only in the first physics frame after injection and are lost when paused — check get_game_input_status's paused/physics_frame fields to diagnose. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead.",
               "Game", std::vector<std::string>({"game", "runtime", "input", "simulate", "debug"}), runtime_ops::handle_game_input, true)

GDA_TOOL_CLASS(CaptureGameViewportTool, "capture_game_viewport",
               "Capture the running game's viewport as a PNG image over the runtime debug channel, returned as a base64-encoded string. Requires a game launched from the editor whose project loads the godot-autopilot extension. Optional timeout_ms bounds the capture wait (default 5000, max 30000).",
               "Game", std::vector<std::string>({"game", "runtime", "capture", "screenshot", "debug"}), runtime_ops::handle_game_capture, true)

GDA_TOOL_CLASS(WaitGameInputTool, "wait_game_input",
               "Wait for a transient input state on an action in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. state is 'just_pressed' (default), 'just_released' or 'pressed'; the call returns when the state is observed in a physics frame or timeout_ms elapses (default 2000, max 30000). Optional inject sends input first in the same call, avoiding frame skew between round trips; state=just_pressed/just_released requires inject — without it the 1-frame transient window has already expired and the wait always times out. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead.",
               "Game", std::vector<std::string>({"game", "runtime", "input", "wait", "debug"}), runtime_ops::handle_game_input_wait, true)

GDA_TOOL_CLASS(GetGameInputStatusTool, "get_game_input_status",
               "Query the current input state of an action in the running game process over the runtime debug channel: pressed, just_pressed, just_released and physics_frame — use it to diagnose input injection. Requires a game launched from the editor whose project loads the godot-autopilot extension. Successful responses also include recent_engine_errors (up to 5 recent engine errors) when any exist.",
               "Game", std::vector<std::string>({"game", "runtime", "input", "status", "debug"}), runtime_ops::handle_game_input_status, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(7);
  v.push_back(std::make_unique<GetGameStatusTool>());
  v.push_back(std::make_unique<ExecuteGameScriptTool>());
  v.push_back(std::make_unique<ReloadGameScriptsTool>());
  v.push_back(std::make_unique<QueueGameInputTool>());
  v.push_back(std::make_unique<CaptureGameViewportTool>());
  v.push_back(std::make_unique<WaitGameInputTool>());
  v.push_back(std::make_unique<GetGameInputStatusTool>());
  return v;
}

} // namespace game_tools
} // namespace godot_autopilot

#endif