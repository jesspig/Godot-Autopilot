#ifndef GODOT_AUTOPILOT_GAME_TOOLS_HPP
#define GODOT_AUTOPILOT_GAME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/runtime_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace runtime_ops {
mcp::JsonValue handle_sequence_game_inputs(const mcp::JsonValue &args);
mcp::JsonValue handle_game_ui_elements(const mcp::JsonValue &args);
mcp::JsonValue handle_click_game_ui_element(const mcp::JsonValue &args);
} // namespace runtime_ops
namespace game_tools {

GDA_TOOL_CLASS(GetGameStatusTool, "get_game_status",
               "Query a running game process over the runtime debug channel: engine version, current scene, node count, FPS, plus paused and physics_frame to distinguish a fake run. Requires a game launched from the editor whose project loads the godot-autopilot extension. healthy and last_activity_ms detect liveness/stall; physics_stalled indicates a frozen physics loop. An empty scene with a sharp node_count drop means a scene reload failed — stop and rerun the game.",
               "Game", std::vector<std::string>({"game", "runtime", "status", "debug"}), runtime_ops::handle_game_status, true)

GDA_TOOL_CLASS_SIDE(ExecuteGameScriptTool, "execute_game_script",
               "Run code or inspect/modify state inside the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. action is 'script' (GDScript must extend Node and define func _run()), 'get_property', 'set_property' or 'call_method'. persist (default false) keeps the temporary node alive under /root/__gda_runtime and returns its path for later calls; persist_name names it. timeout_ms bounds the in-game execution (default 5000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout); on expiry an idempotent cancel interrupts the pending await and, when the debugger session was breaked, the plugin sends an engine continue so the game is released. call_method on an await method waits for the coroutine to complete (bounded by timeout_ms) and returns its final result; on timeout it returns an error.",
               "Game", std::vector<std::string>({"game", "runtime", "eval", "script", "debug"}), runtime_ops::handle_game_eval, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS_SIDE(StartGameJobTool, "start_game_job",
               "Submit a game op for asynchronous execution inside the running game process over the runtime debug channel and return immediately with a job_id instead of waiting for the result; submit without waiting, poll with get_game_job. Use it for long-running scripts that would exceed the 25 s per-call timeout budget. op is currently 'eval' only and params carries that op's parameters verbatim (same fields as execute_game_script, e.g. {\"action\":\"script\",\"source_code\":\"...\"}); timeout_ms (default 5000, max 25000) bounds the in-game execution and the job expiry check, not the submit call. The job table holds at most 16 jobs and entries expire timeout_ms + 2000 ms after start. Requires a game launched from the editor whose project loads the godot-autopilot extension and the game_runtime authorization gate.",
               "Game", std::vector<std::string>({"game", "runtime", "job", "async", "eval", "debug"}), runtime_ops::handle_game_job_start, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS(GetGameJobTool, "get_game_job",
               "Poll a game job previously submitted with start_game_job over the runtime debug channel: status pending means the result is not ready yet, done carries the game's response verbatim including its own error fields, expired means the job exceeded timeout_ms + 2000 ms after start and was dropped, cancelled means cancel=true tore the request down (an idempotent engine cancel is sent to the game). Collecting, expiring and cancelling each release the eval error-break suppression. timeout_ms (default 0 = non-blocking poll, max 25000) bounds an internal wait loop that re-checks the job every few milliseconds.",
               "Game", std::vector<std::string>({"game", "runtime", "job", "async", "poll", "debug"}), runtime_ops::handle_game_job_get, true)

GDA_TOOL_CLASS_SIDE(ReloadGameScriptsTool, "reload_game_scripts",
               "Reload GDScript files in the running game process over the runtime debug channel without restarting the game; engine soft reload keeps instance state where possible. Requires a game launched from the editor whose project loads the godot-autopilot extension. paths is a non-empty array of res:// script paths (pass multiple files in one call). The reload request is sent to the game and applied during its next idle poll; no confirmation is returned — verify with get_game_log_entries or get_game_status. Workflow after editing a script file: call reload_game_scripts first, then reload_current_scene (or retry the failed operation) — reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).",
                "Game", std::vector<std::string>({"game", "runtime", "reload", "script", "debug"}), runtime_ops::handle_game_reload_scripts, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS_SIDE(QueueGameInputTool, "queue_game_input",
               "Simulate input inside the running game process over the runtime debug channel; unlike the editor-process input_* tools, the running game receives these events. Requires a game launched from the editor whose project loads the godot-autopilot extension. type is 'key', 'mouse_button', 'wheel', 'mouse_motion' or 'action'; mode is 'event' (parse_input_event, default), 'api' (action_press/release, immediate) or 'hold' (event-style, kept pressed for duration_ms). wheel scrolls the mouse wheel: give direction ('up', 'down', 'left', 'right', case-insensitive) or button_index (4=up, 5=down, 6=left, 7=right), optional amount (integer 1-10, default 1) and optional position {x,y}; the wheel button is injected pressed then released with factor=amount. mouse_motion moves the pointer: position {x,y} is required and optional relative {x,y} sets the movement delta. Unknown parameters are rejected with an error. Transient states (is_action_just_pressed) are visible only in the first physics frame after injection and are lost when paused — check get_game_input_status's paused/physics_frame fields to diagnose. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead.",
               "Game", std::vector<std::string>({"game", "runtime", "input", "simulate", "debug"}), runtime_ops::handle_game_input, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS(CaptureGameViewportTool, "capture_game_viewport",
               "Capture the running game's viewport as a PNG image over the runtime debug channel, returned as a base64-encoded string. Requires a game launched from the editor whose project loads the godot-autopilot extension. Optional timeout_ms bounds the capture wait (default 5000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout); when after_frames/when delay the capture it also bounds the in-game wait and the game answers with a structured error (code when_timeout, plus frames_waited) on expiry. Optional after_frames (integer >= 0) waits that many rendered frames and optional when (a GDScript expression re-evaluated every rendered frame with the current scene as base instance) holds the capture until it is true - together they capture short-lived feedback such as a 2 s banner or a 0.7 s death screen without slowing the game down: act first, then capture with a condition. A malformed expression fails immediately with a structured error (code when_parse_error) carrying the Expression error text; evaluation errors keep the last message in the timeout error. Optional region {x, y, width, height} crops the captured image (clamped to the image; an empty intersection fails with region_out_of_bounds) and optional max_dimension (64-4096) downscales the output so its longest side is at most that many pixels; it never upscales. Optional scale (integer 1-8, default 1) nearest-neighbour upscales the output after the crop and before max_dimension, so max_dimension still caps the longest side when both are given and width/height report the final size. The game always writes the PNG to the OS cache as the transport mechanism before the editor reads it back and returns it inline. With annotate=true a numbered box is drawn for every Control element found in the game scene tree and the game-side result adds annotated=true plus an elements array {id,path,type,text,position,size} in image pixels, so UI elements can be located visually and then clicked with click_game_ui_element using the same row's path or text/text_index. id is a per-capture sequence number (1-based, unstable across captures) and cannot be passed to click_game_ui_element. Optional annotate_nodes (array of 1-50 game node paths) draws numbered blue boxes for scene nodes with per-item ok/error in node_elements plus node_truncated on budget truncation.",
               "Game", std::vector<std::string>({"game", "runtime", "capture", "screenshot", "debug"}), runtime_ops::handle_game_capture, true)

GDA_TOOL_CLASS_SIDE(WaitGameInputTool, "wait_game_input",
               "Wait for a transient input state on an action in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. state is 'just_pressed' (default), 'just_released' or 'pressed'; the call returns when the state is observed in a physics frame or timeout_ms elapses (default 2000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout). Optional inject sends input first in the same call, avoiding frame skew between round trips; state=just_pressed/just_released requires inject — without it the 1-frame transient window has already expired and the wait always times out. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead.",
                "Game", std::vector<std::string>({"game", "runtime", "input", "wait", "debug"}), runtime_ops::handle_game_input_wait, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS(GetGameInputStatusTool, "get_game_input_status",
               "Query the current input state of an action in the running game process over the runtime debug channel: pressed, just_pressed, just_released and physics_frame — use it to diagnose input injection. Requires a game launched from the editor whose project loads the godot-autopilot extension. Responses never include recent_engine_errors; for engine errors use get_debugger_errors (running game) or get_debugger_log (editor process).",
               "Game", std::vector<std::string>({"game", "runtime", "input", "status", "debug"}), runtime_ops::handle_game_input_status, true)

GDA_TOOL_CLASS_SIDE(SequenceGameInputsTool, "sequence_game_inputs",
               "Schedule a timeline of input events inside the running game process over the runtime debug channel, each fired on an exact physics frame offset (at_frame, relative to sequence start; 0 = immediately) — unlike repeated queue_game_input round trips this avoids cross-call latency jitter for playtest automation. Requires a game launched from the editor whose project loads the godot-autopilot extension. Each inputs item needs kind ('key'|'mouse_button'|'mouse_motion'|'wheel'|'action'), at_frame (non-negative integer; out-of-order entries are allowed and fire when their frame arrives) plus the queue_game_input fields for that kind: keycode for key, button_index and optional position {x,y} for mouse_button, position {x,y} for mouse_motion, direction ('up'|'down'|'left'|'right') or button_index 4-7 with optional amount 1-10 and position {x,y} for wheel, action for action; optional pressed (default true), duration_ms (auto-release) and mode ('event'|'api'|'hold'). Max 256 items. Example: [{\"kind\":\"key\",\"keycode\":\"X\",\"at_frame\":0},{\"kind\":\"mouse_button\",\"button_index\":1,\"position\":{\"x\":120,\"y\":80},\"at_frame\":5},{\"kind\":\"action\",\"action\":\"jump\",\"at_frame\":15}] presses X now, left-clicks at (120,80) on frame 5 and triggers jump on frame 15. The call resolves when every item has fired ({completed:true, executed:n}) or timeout_ms elapses ({completed:false, executed:n}); timeout_ms defaults to max(at_frame)*33+2000, max 25000 (the host waits timeout_ms + 2000 ms, below the 30 s transport timeout).",
                "Game", std::vector<std::string>({"game", "runtime", "input", "sequence", "timeline", "debug"}), runtime_ops::handle_sequence_game_inputs, true, ::godot_autopilot::SideEffect::GameRuntime)

GDA_TOOL_CLASS(GetGameUiElementsTool, "get_game_ui_elements",
               "Enumerate interactive UI nodes (every Control-derived node) in the running game's scene tree over the runtime debug channel — reconnaissance before click automation: locate buttons/panels here, then click them with click_game_ui_element using path (preferred) or text/text_index, or reference them from execute_game_script using path. global_rect is in viewport/canvas coordinates and only click_game_ui_element converts it to window coordinates before injection — do not feed global_rect directly to queue_game_input or sequence_game_inputs. Requires a game launched from the editor whose project loads the godot-autopilot extension. Returns per element: path (absolute node path), type (class name), visible (visibility including ancestors), text (when the control has a text property) and global_rect {position,size} in viewport coordinates. Elements come in tree order capped by max_elements (default 100, max 1000); truncated=true signals more remain — raise the cap or target a subtree via execute_game_script.",
               "Game", std::vector<std::string>({"game", "runtime", "ui", "tree", "elements", "debug"}), runtime_ops::handle_game_ui_elements, true)

GDA_TOOL_CLASS_SIDE(ClickGameUiElementTool, "click_game_ui_element",
               "Click a UI element in the running game process by node path or visible text over the runtime debug channel: the game enumerates its Control tree, resolves path (exact match first, otherwise a node path ending with '/'+path) or text (exact match over visible elements first, then substring) and injects a mouse_button press/release pair at the element's global_rect centre through the same path as queue_game_input. Requires a game launched from the editor whose project loads the godot-autopilot extension and the game_runtime authorization gate. path (from get_game_ui_elements) or text (visible control text) selects the element — pass exactly one of them, path wins when both are given; text matches visible elements exactly first, then by substring, and text_index (default 0) picks the nth match when several match. The annotate id from capture_game_viewport is a per-capture sequence number and is not clickable — use the same row's path or its text instead; button_index (1=left default, 2=right, 3=middle) and double_click (default false, adds a second press/release pair) are optional. max_elements caps the enumeration (default 1000, max 1000); timeout_ms defaults to 5000 (max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout). One call performs both steps (enumerate + inject) and resolves with {\"result\":{\"ok\":true,\"path\":...,\"position\":{x,y},\"clicks\":n}}; when the path is not found the error lists the element count and candidate paths.",
               "Game", std::vector<std::string>({"game", "runtime", "input", "click", "ui", "debug"}), runtime_ops::handle_click_game_ui_element, true, ::godot_autopilot::SideEffect::GameRuntime)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(12);
  v.push_back(std::make_unique<GetGameStatusTool>());
  v.push_back(std::make_unique<ExecuteGameScriptTool>());
  v.push_back(std::make_unique<StartGameJobTool>());
  v.push_back(std::make_unique<GetGameJobTool>());
  v.push_back(std::make_unique<ReloadGameScriptsTool>());
  v.push_back(std::make_unique<QueueGameInputTool>());
  v.push_back(std::make_unique<CaptureGameViewportTool>());
  v.push_back(std::make_unique<WaitGameInputTool>());
  v.push_back(std::make_unique<GetGameInputStatusTool>());
  v.push_back(std::make_unique<SequenceGameInputsTool>());
  v.push_back(std::make_unique<GetGameUiElementsTool>());
  v.push_back(std::make_unique<ClickGameUiElementTool>());
  return v;
}

} // namespace game_tools
} // namespace godot_autopilot

#endif
