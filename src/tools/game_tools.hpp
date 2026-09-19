#ifndef GODOT_AUTOPILOT_GAME_TOOLS_HPP
#define GODOT_AUTOPILOT_GAME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/runtime_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace runtime_ops {
mcp::JsonValue handle_sequence_game_inputs(const mcp::JsonValue &args);
mcp::JsonValue handle_game_ui_elements(const mcp::JsonValue &args);
mcp::JsonValue handle_click_game_ui_element(const mcp::JsonValue &args);
} // namespace runtime_ops
namespace game_tools {

namespace {

const std::vector<ParamSpec> kSampleGamePropertyParams = {
    {"node_path", "string", "Node path in the running game process, e.g. /root/Main/Player", true},
    {"property", "string", "Property name to sample; must exist on the node — missing names fail fast instead of collecting nulls", true},
    {"frames", "integer", "How many samples to collect, an integer from 1 to 120", true},
    {"interval_frames", "integer", "Sample every interval_frames+1-th process frame (integer 0-60, default: 0); frames*(interval_frames+1) must stay within 3600", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000); on expiry the call fails with code sample_timeout plus the samples collected so far, and a node freed mid-run fails with code sample_node_freed. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
};

const std::vector<ParamSpec> kCollectGameEvidenceParams = {
    {"include_status", "boolean", "Include the game status section, same fields as get_game_status (default: true)", false},
    {"include_capture", "boolean", "Include the game screenshot section, same semantics as capture_game_viewport immediate capture (default: true)", false},
    {"include_errors", "boolean", "Include the recent game errors section, same list as get_debugger_errors (default: true)", false},
    {"limit", "integer", "Maximum number of errors in the errors section, an integer from 1 to 200 (default: 20)", false},
    {"region", "object", "Crop the captured image to {x, y, width, height} in viewport pixels; the rect is clamped to the image", false},
    {"max_dimension", "integer", "Downscale the capture so its longest side is at most this many pixels (64-4096); never upscales", false},
    {"scale", "integer", "Integer nearest-neighbour upscale factor for the capture (1-8, default: 1), applied before max_dimension", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
};

const std::vector<ParamSpec> kValidateGameUiLayoutParams = {
    {"max_elements", "integer", "Maximum number of Control elements enumerated (default: 100); truncated=true signals more remain", false},
    {"ignore_paths", "array", "Up to 50 exact or '/'-suffix paths suppressing known-good entries", false},
    {"ignore_classes", "array", "Up to 20 class names suppressing known-good entries; subclasses match, e.g. Container covers layout-only containers", false},
    {"min_area", "number", "Visible areas below this size report tiny_area info (default: 4.0)", false},
    {"bounds_margin", "number", "Pixel tolerance around the viewport visible rect (default: 2.0)", false},
    {"occlude_ratio", "number", "Fraction 0.5-1.0 of an element area covered by a later visible control before possibly_occluded warns (default: 0.98); occlusion is a document-order heuristic ignoring canvas_layer and z_index", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
};

const std::vector<ParamSpec> kGetGameStatusParams = {};

const std::vector<ParamSpec> kExecuteGameScriptParams = {
    {"action", "string", "Action to run in the game process: 'script' (execute arbitrary GDScript), 'get_property', 'set_property' or 'call_method'", true},
    {"node_path", "string", "Node path for get_property/set_property/call_method (omit to use the current scene root)", false},
    {"property", "string", "Property name for get_property/set_property", false},
    {"value", "object", "Value to set for set_property", false},
    {"method", "string", "Method name for call_method; await methods are awaited to completion (bounded by timeout_ms) and return their final result", false},
    {"args", "array", "Arguments for call_method", false},
    {"source_code", "string", "GDScript source for action='script' — must extend Node and define func _run()", false},
    {"persist", "boolean", "Keep the script's temporary node alive after the call (default: false); the node is stored under /root/__gda_runtime and its path is returned in node_path for later get_property/call_method use", false},
    {"persist_name", "string", "Node name under /root/__gda_runtime when persist=true (default: auto-generated)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000); on expiry an idempotent cancel interrupts the pending in-game await, and the plugin broadcasts an engine continue when the debugger session was breaked. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
    {"assert", "string", "Optional sandboxed assertion expression (max 1024 chars) evaluated with the eval result bound as value, e.g. value > 0; the response carries assert {pass, expression} plus actual. Expression grammar only with no scene access, sharing the op timeout; coroutine results cannot be asserted — omit assert for awaitable calls", false},
};

const std::vector<ParamSpec> kStartGameJobParams = {
    {"op", "string", "Wrapped game op to submit for asynchronous execution; currently only 'eval' (the execute_game_script op) is supported", true},
    {"params", "object", "Parameters object passed to the wrapped op verbatim — same fields as execute_game_script, e.g. {\"action\":\"script\",\"source_code\":\"...\"}", true},
    {"timeout_ms", "integer", "In-game execution budget in milliseconds (default: 5000, maximum: 25000); the call returns immediately with a job_id and does not wait, so this bounds the in-game run and the job expiry check (expiry at timeout_ms + 2000 ms) instead of the transport wait. The job table holds at most 16 entries — collect finished jobs with get_game_job to free slots. Same budget chain as execute_game_script: values above 25000 are rejected, keeping every host wait below the 30 s HTTP transport timeout", false},
};

const std::vector<ParamSpec> kGetGameJobParams = {
    {"job_id", "integer", "Job id returned by start_game_job", true},
    {"cancel", "boolean", "Cancel the job instead of collecting it (default: false); an idempotent cancel reaches the game, the job record is dropped and the response reports status cancelled", false},
    {"timeout_ms", "integer", "Poll wait in milliseconds (default: 0 = non-blocking poll that returns status pending immediately, maximum: 25000); with a positive value the call re-checks the job every few milliseconds until the result arrives, the job expires or the budget ends", false},
};

const std::vector<ParamSpec> kReloadGameScriptsParams = {
    {"paths", "array", "Non-empty array of res:// GDScript file paths to reload in the running game", true},
};

const std::vector<ParamSpec> kQueueGameInputParams = {
    {"type", "string", "Input event type: 'key' (keyboard), 'mouse_button' (mouse click), 'wheel' (mouse wheel scroll), 'mouse_motion' (pointer move) or 'action' (named InputMap action)", true},
    {"keycode", "string", "Key for type='key': numeric key code (e.g. 65) or key name (e.g. 'A', 'space', 'KEY_LEFT')", false},
    {"pressed", "boolean", "Pressed state (default: true)", false},
    {"button_index", "integer", "Mouse button index: 1=left, 2=right, 3=middle for type='mouse_button'; for type='wheel' a wheel button index (4=up, 5=down, 6=left, 7=right) used instead of direction", false},
    {"position", "object", "Mouse position {x, y} for type='mouse_button', 'wheel' or 'mouse_motion'", false},
    {"direction", "string", "Wheel direction for type='wheel': 'up', 'down', 'left' or 'right' (case-insensitive); ignored when button_index is given", false},
    {"amount", "integer", "Wheel scroll amount for type='wheel' (integer 1-10, default: 1); the wheel button is injected pressed then released with this factor", false},
    {"relative", "object", "Pointer movement delta {x, y} for type='mouse_motion' (default: zero vector)", false},
    {"action", "string", "Action name for type='action' (e.g. 'ui_accept'); transient states (is_action_just_pressed) are only visible in the next physics frame — use wait_game_input or get_game_input_status to observe effects. Transient states are not visible in _process (render frame)", false},
    {"duration_ms", "integer", "Hold duration: auto-release the input after this many ms (0/absent = no auto-release); only meaningful when pressed=true and ignored by type='wheel'", false},
    {"mode", "string", "Injection mode: 'event' (default, via Input.parse_input_event), 'api' (via Input.action_press/action_release, immediate) or 'hold' (event-style injection that stays pressed for duration_ms)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
};

const std::vector<ParamSpec> kCaptureGameViewportParams = {
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000); the capture is returned as a base64-encoded PNG; when after_frames/when delay the capture it also bounds the in-game wait and the game answers with a structured error (code when_timeout, plus frames_waited) on expiry. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
    {"region", "object", "Crop the captured image to {x, y, width, height} in viewport pixels (width/height must be positive); the rect is clamped to the image and an empty intersection fails with region_out_of_bounds", false},
    {"max_dimension", "integer", "Downscale the output image so its longest side is at most this many pixels (64-4096); never upscales (default: no scaling). Applied last: region crop first, then scale, then this cap — when both are given max_dimension wins and width/height report the final size", false},
    {"scale", "integer", "Integer nearest-neighbour upscale factor (1-8, default: 1) applied after the region crop and before max_dimension; useful for reading small HUD text from a cropped area. The game always writes the PNG to the OS cache directory as the transport mechanism before it is read back and returned inline; the editor target instead returns the image directly and only writes a file when save=true", false},
    {"after_frames", "integer", "Wait this many rendered frames before capturing (integer >= 0, default: 0 = capture immediately); combined with when, the frame budget must be spent first and the capture fires on the first frame where both hold. Use it for short-lived events (a 2 s banner, a 0.7 s death screen) without slowing the game down: act first, then capture. timeout_ms bounds the wait and a never-met condition returns a structured error (code when_timeout) instead of hanging", false},
    {"when", "string", "GDScript expression evaluated once per rendered frame inside the game process; the capture fires when it is true (empty string = no condition). It runs with the current scene as the base instance (falling back to the tree root when no scene is set), so node lookups such as get_node(\"HUD/MessageLabel\").text != \"\" work directly. A malformed expression fails immediately with a structured error (code when_parse_error) carrying the Expression error text; evaluation errors keep the last message in the timeout error instead of failing the call", false},
    {"annotate", "boolean", "Draw a numbered box for every Control element on the captured image (default: false); the game-side result also carries annotated=true plus an elements array {id,path,type,text,position,size} in image pixels; id is a per-capture sequence number — click with the same row's path or text/text_index, never with id", false},
    {"annotate_nodes", "array", "Draw numbered blue boxes for the listed game scene nodes (1-50 absolute paths, exact match first then '/'+path suffix match); per-item failures carry ok:false plus error and candidates without failing the call; result adds node_elements[{id,path,type,ok,position,size,visible,behind,error}] in final-image pixels plus node_truncated when the 200-box budget truncates node marks", false},
    {"annotate_nodes_max", "integer", "Optional self limit 1-50 for annotate_nodes; the call fails when annotate_nodes is longer than this value", false},
};

const std::vector<ParamSpec> kWaitGameInputParams = {
    {"action", "string", "Action name to wait on", true},
    {"state", "string", "Transient state to wait for: 'just_pressed' (default), 'just_released' or 'pressed'", false},
    {"inject", "object", "Inject an input before waiting, in the same call (fields mirror queue_game_input: type/action/keycode/pressed/mode/duration_ms/button_index/position/direction/amount/relative) — eliminates the inject-then-observe cross-roundtrip frame gap; required for state=just_pressed/just_released, without inject the transient window (1 physics frame) has already expired and the wait will always time out. Transient states are not visible in _process (render frame)", false},
    {"timeout_ms", "integer", "Wait timeout in milliseconds (default: 2000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
};

const std::vector<ParamSpec> kGetGameInputStatusParams = {
    {"action", "string", "Action name to query; the response includes paused and physics_frame so transient input consumption can be diagnosed when the game is paused", true},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000); responses never include recent_engine_errors — use get_debugger_errors for running-game errors or get_debugger_log for editor-process errors. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
};

const std::vector<ParamSpec> kSequenceGameInputsParams = {
    {"inputs", "array", "Timeline items, each an object: kind ('key'|'mouse_button'|'mouse_motion'|'wheel'|'action'), at_frame (integer physics-frame offset from sequence start, 0 = immediately; out-of-order allowed, fires when its frame arrives) plus the queue_game_input fields for that kind — keycode for key, button_index and optional position {x,y} for mouse_button, position {x,y} for mouse_motion, direction ('up'|'down'|'left'|'right') or button_index 4-7 with optional amount 1-10 and position {x,y} for wheel, action for action; optional pressed (default true), duration_ms (auto-release), mode ('event'|'api'|'hold'). Max 256 items. Example: [{\"kind\":\"key\",\"keycode\":\"X\",\"at_frame\":0},{\"kind\":\"mouse_button\",\"button_index\":1,\"position\":{\"x\":120,\"y\":80},\"at_frame\":5},{\"kind\":\"action\",\"action\":\"jump\",\"at_frame\":15}]", true},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: max(at_frame)*33+2000, max: 25000); the call resolves with {completed:true, executed:n} once every item has fired, or {completed:false, executed:n} on expiry listing how many items fired before the deadline. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long sequences into shorter ones", false},
};

const std::vector<ParamSpec> kGetGameUiElementsParams = {
    {"max_elements", "integer", "Maximum number of Control-derived elements returned in tree order (default: 100, max: 1000); truncated=true signals more remain — raise the cap or narrow via execute_game_script", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
};

const std::vector<ParamSpec> kClickGameUiElementParams = {
    {"path", "string", "Node path of the UI element to click, as returned by get_game_ui_elements (e.g. '/root/Main/Menu/StartButton'); an exact match wins, otherwise a path ending with '/'+path is used. Alternatively pass text/text_index instead — path wins when both are given; the annotate id from capture_game_viewport is a per-capture sequence number and is not clickable", false},
    {"text", "string", "Visible text of the UI element to click (exact match over visible elements first, then substring); alternative to path — path wins when both are given", false},
    {"text_index", "integer", "Selects the nth visible text match (0-based, default: 0); without it an ambiguous text fails listing candidates", false},
    {"button_index", "integer", "Mouse button to click: 1=left (default), 2=right, 3=middle", false},
    {"double_click", "boolean", "Inject a second press/release pair (default: false)", false},
    {"max_elements", "integer", "Maximum number of Control elements enumerated while resolving path (default: 1000, max: 1000)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000); the game performs both the enumeration and the injection before answering. The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap — split long work into shorter calls", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(make_spec_tool(ToolSpec{
      "sample_game_property",
      "Sample a game node property over N process frames in the running game process over the runtime debug channel, read-only: returns the serialized value per sample. Requires a game launched from the editor whose project loads the godot-autopilot extension. node_path and property select the target (property must exist — missing names fail fast instead of collecting nulls); frames (integer 1-120) sets how many samples to collect and interval_frames (integer 0-60, default 0) spaces them (every interval_frames+1-th process frame is sampled, keeping main-thread cost O(1) per frame); frames*(interval_frames+1) must stay within 3600. timeout_ms (default 5000, max 25000) bounds the whole sampling run — on expiry the call fails explicitly with code sample_timeout plus the samples collected so far; a node freed mid-run fails with code sample_node_freed. No polling loop on the caller side is needed.",
      "Game", {"game", "runtime", "sample", "property", "debug"}, SideEffect::None, tool_flags::kNone,
      kSampleGamePropertyParams, runtime_ops::handle_game_sample_property}));
  v.push_back(make_spec_tool(ToolSpec{
      "collect_game_evidence",
      "Collect a read-only evidence bundle from the running game process over the runtime debug channel in one round trip, without any automatic verdict: game status (same fields as get_game_status, with healthy/last_activity measured at request time), game screenshot (same semantics as capture_game_viewport immediate capture) and recent game errors (same list as get_debugger_errors). Requires a game launched from the editor whose project loads the godot-autopilot extension. Each section fails in isolation with an error object instead of failing the call; pass include_status/include_capture/include_errors=false to skip a section. Optional limit (integer 1-200, default 20) caps the errors section; region/max_dimension/scale mirror the capture parameters. Prefer this over three separate calls when the three observations must belong to the same moment; for a verdict, compare the sections yourself.",
      "Game", {"game", "runtime", "evidence", "status", "debug"}, SideEffect::None, tool_flags::kNone,
      kCollectGameEvidenceParams, runtime_ops::handle_game_collect_evidence}));
  v.push_back(make_spec_tool(ToolSpec{
      "validate_game_ui_layout",
      "Scan the running game's Control tree for layout defects, read-only: zero-size visible controls, controls fully outside the viewport visible rect and possibly-occluded controls, each reported as {kind,severity,path,type,rect,detail}. Requires a game launched from the editor whose project loads the godot-autopilot extension. Invisible controls are always skipped; ignore_paths (up to 50 exact or '/'-suffix paths) and ignore_classes (up to 20 class names, subclasses match, e.g. 'Container' covers layout-only containers) suppress known-good entries. Thresholds guard against false positives: min_area (default 4.0, smaller visible areas report tiny_area info), bounds_margin (default 2.0 px tolerance around the viewport), occlude_ratio (0.5-1.0, default 0.98 — fraction of an element's area covered by a later visible control before possibly_occluded warns; occlusion is a document-order heuristic that ignores canvas_layer/z_index). max_elements caps the enumeration (default 100, max 1000); truncated=true signals more remain. No automatic verdict — triage the issues yourself.",
      "Game", {"game", "runtime", "ui", "layout", "validate", "debug"}, SideEffect::None, tool_flags::kNone,
      kValidateGameUiLayoutParams, runtime_ops::handle_game_validate_ui_layout}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_status",
      "Query a running game process over the runtime debug channel: engine version, current scene, node count, FPS, plus paused and physics_frame to distinguish a fake run. Requires a game launched from the editor whose project loads the godot-autopilot extension. healthy and last_activity_ms detect liveness/stall; physics_stalled indicates a frozen physics loop. An empty scene with a sharp node_count drop means a scene reload failed — stop and rerun the game.",
      "Game", {"game", "runtime", "status", "debug"}, SideEffect::None, tool_flags::kNone,
      kGetGameStatusParams, runtime_ops::handle_game_status}));
  v.push_back(make_spec_tool(ToolSpec{
      "execute_game_script",
      "Run code or inspect/modify state inside the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. action is 'script' (GDScript must extend Node and define func _run()), 'get_property', 'set_property' or 'call_method'. persist (default false) keeps the temporary node alive under /root/__gda_runtime and returns its path for later calls; persist_name names it. timeout_ms bounds the in-game execution (default 5000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout); on expiry an idempotent cancel interrupts the pending await and, when the debugger session was breaked, the plugin sends an engine continue so the game is released. call_method on an await method waits for the coroutine to complete (bounded by timeout_ms) and returns its final result; on timeout it returns an error. Optional 'assert' (max 1024 chars) evaluates a sandboxed GDScript expression with the eval result bound as `value` (e.g. \"value > 0\") and attaches {assert:{pass,expression},actual}: Expression grammar only with no scene access, sharing the op timeout; coroutine (async) results cannot be asserted and fail explicitly — omit assert for awaitable calls.",
      "Game", {"game", "runtime", "eval", "script", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kExecuteGameScriptParams, runtime_ops::handle_game_eval}));
  v.push_back(make_spec_tool(ToolSpec{
      "start_game_job",
      "Submit a game op for asynchronous execution inside the running game process over the runtime debug channel and return immediately with a job_id instead of waiting for the result; submit without waiting, poll with get_game_job. Use it for long-running scripts that would exceed the 25 s per-call timeout budget. op is currently 'eval' only and params carries that op's parameters verbatim (same fields as execute_game_script, e.g. {\"action\":\"script\",\"source_code\":\"...\"}); timeout_ms (default 5000, max 25000) bounds the in-game execution and the job expiry check, not the submit call. The job table holds at most 16 jobs and entries expire timeout_ms + 2000 ms after start. Requires a game launched from the editor whose project loads the godot-autopilot extension and the game_runtime authorization gate.",
      "Game", {"game", "runtime", "job", "async", "eval", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kStartGameJobParams, runtime_ops::handle_game_job_start}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_job",
      "Poll a game job previously submitted with start_game_job over the runtime debug channel: status pending means the result is not ready yet, done carries the game's response verbatim including its own error fields, expired means the job exceeded timeout_ms + 2000 ms after start and was dropped, cancelled means cancel=true tore the request down (an idempotent engine cancel is sent to the game). Collecting, expiring and cancelling each release the eval error-break suppression. timeout_ms (default 0 = non-blocking poll, max 25000) bounds an internal wait loop that re-checks the job every few milliseconds.",
      "Game", {"game", "runtime", "job", "async", "poll", "debug"}, SideEffect::None, tool_flags::kNone,
      kGetGameJobParams, runtime_ops::handle_game_job_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_game_scripts",
      "Reload GDScript files in the running game process over the runtime debug channel without restarting the game; engine soft reload keeps instance state where possible. Requires a game launched from the editor whose project loads the godot-autopilot extension. paths is a non-empty array of res:// script paths (pass multiple files in one call). The reload request is sent to the game and applied during its next idle poll; no confirmation is returned — verify with get_game_log_entries or get_game_status. Workflow after editing a script file: call reload_game_scripts first, then reload_current_scene (or retry the failed operation) — reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).",
      "Game", {"game", "runtime", "reload", "script", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kReloadGameScriptsParams, runtime_ops::handle_game_reload_scripts}));
  v.push_back(make_spec_tool(ToolSpec{
      "queue_game_input",
      "Simulate input inside the running game process over the runtime debug channel; unlike the editor-process input_* tools, the running game receives these events. Requires a game launched from the editor whose project loads the godot-autopilot extension. type is 'key', 'mouse_button', 'wheel', 'mouse_motion' or 'action'; mode is 'event' (parse_input_event, default), 'api' (action_press/release, immediate) or 'hold' (event-style, kept pressed for duration_ms). wheel scrolls the mouse wheel: give direction ('up', 'down', 'left', 'right', case-insensitive) or button_index (4=up, 5=down, 6=left, 7=right), optional amount (integer 1-10, default 1) and optional position {x,y}; the wheel button is injected pressed then released with factor=amount. mouse_motion moves the pointer: position {x,y} is required and optional relative {x,y} sets the movement delta. Unknown parameters are rejected with an error. Transient states (is_action_just_pressed) are visible only in the first physics frame after injection and are lost when paused — check get_game_input_status's paused/physics_frame fields to diagnose. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead. 同帧内连续 press+release 会使 just_pressed 与 just_released 同时为真，边沿观察需跨物理帧（sequence_game_inputs 按帧排布可规避）。hold 模式逐帧注入有固定开销，长按慎用时长。动作与同名物理键位共存时，事件释放只清自身槽位不等价全局释放；如需全局释放用 api 模式.",
      "Game", {"game", "runtime", "input", "simulate", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kQueueGameInputParams, runtime_ops::handle_game_input}));
  v.push_back(make_spec_tool(ToolSpec{
      "capture_game_viewport",
      "Capture the running game's viewport as a PNG image over the runtime debug channel, returned as a base64-encoded string. Requires a game launched from the editor whose project loads the godot-autopilot extension. Optional timeout_ms bounds the capture wait (default 5000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout); when after_frames/when delay the capture it also bounds the in-game wait and the game answers with a structured error (code when_timeout, plus frames_waited) on expiry. Optional after_frames (integer >= 0) waits that many rendered frames and optional when (a GDScript expression re-evaluated every rendered frame with the current scene as base instance) holds the capture until it is true - together they capture short-lived feedback such as a 2 s banner or a 0.7 s death screen without slowing the game down: act first, then capture with a condition. A malformed expression fails immediately with a structured error (code when_parse_error) carrying the Expression error text; evaluation errors keep the last message in the timeout error. Optional region {x, y, width, height} crops the captured image (clamped to the image; an empty intersection fails with region_out_of_bounds) and optional max_dimension (64-4096) downscales the output so its longest side is at most that many pixels; it never upscales. Optional scale (integer 1-8, default 1) nearest-neighbour upscales the output after the crop and before max_dimension, so max_dimension still caps the longest side when both are given and width/height report the final size. The game always writes the PNG to the OS cache as the transport mechanism before the editor reads it back and returns it inline. With annotate=true a numbered box is drawn for every Control element found in the game scene tree and the game-side result adds annotated=true plus an elements array {id,path,type,text,position,size} in image pixels, so UI elements can be located visually and then clicked with click_game_ui_element using the same row's path or text/text_index. id is a per-capture sequence number (1-based, unstable across captures) and cannot be passed to click_game_ui_element. Optional annotate_nodes (array of 1-50 game node paths) draws numbered blue boxes for scene nodes with per-item ok/error in node_elements plus node_truncated on budget truncation.",
      "Game", {"game", "runtime", "capture", "screenshot", "debug"}, SideEffect::None, tool_flags::kNone | tool_flags::kCaptureImage,
      kCaptureGameViewportParams, runtime_ops::handle_game_capture}));
  v.push_back(make_spec_tool(ToolSpec{
      "wait_game_input",
      "Wait for a transient input state on an action in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. state is 'just_pressed' (default), 'just_released' or 'pressed'; the call returns when the state is observed in a physics frame or timeout_ms elapses (default 2000, max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout). Optional inject sends input first in the same call, avoiding frame skew between round trips; state=just_pressed/just_released requires inject — without it the 1-frame transient window has already expired and the wait always times out. Transient states are tracked against physics frames only: is_action_just_pressed polled in _process (render frame) will not observe injected input — poll in _physics_process instead.",
      "Game", {"game", "runtime", "input", "wait", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kWaitGameInputParams, runtime_ops::handle_game_input_wait}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_input_status",
      "Query the current input state of an action in the running game process over the runtime debug channel: pressed, just_pressed, just_released and physics_frame — use it to diagnose input injection. Requires a game launched from the editor whose project loads the godot-autopilot extension. Responses never include recent_engine_errors; for engine errors use get_debugger_errors (running game) or get_debugger_log (editor process).",
      "Game", {"game", "runtime", "input", "status", "debug"}, SideEffect::None, tool_flags::kNone,
      kGetGameInputStatusParams, runtime_ops::handle_game_input_status}));
  v.push_back(make_spec_tool(ToolSpec{
      "sequence_game_inputs",
      "Schedule a timeline of input events inside the running game process over the runtime debug channel, each fired on an exact physics frame offset (at_frame, relative to sequence start; 0 = immediately) — unlike repeated queue_game_input round trips this avoids cross-call latency jitter for playtest automation. Requires a game launched from the editor whose project loads the godot-autopilot extension. Each inputs item needs kind ('key'|'mouse_button'|'mouse_motion'|'wheel'|'action'), at_frame (non-negative integer; out-of-order entries are allowed and fire when their frame arrives) plus the queue_game_input fields for that kind: keycode for key, button_index and optional position {x,y} for mouse_button, position {x,y} for mouse_motion, direction ('up'|'down'|'left'|'right') or button_index 4-7 with optional amount 1-10 and position {x,y} for wheel, action for action; optional pressed (default true), duration_ms (auto-release) and mode ('event'|'api'|'hold'). Max 256 items. Example: [{\"kind\":\"key\",\"keycode\":\"X\",\"at_frame\":0},{\"kind\":\"mouse_button\",\"button_index\":1,\"position\":{\"x\":120,\"y\":80},\"at_frame\":5},{\"kind\":\"action\",\"action\":\"jump\",\"at_frame\":15}] presses X now, left-clicks at (120,80) on frame 5 and triggers jump on frame 15. The call resolves when every item has fired ({completed:true, executed:n}) or timeout_ms elapses ({completed:false, executed:n}); timeout_ms defaults to max(at_frame)*33+2000, max 25000 (the host waits timeout_ms + 2000 ms, below the 30 s transport timeout).",
      "Game", {"game", "runtime", "input", "sequence", "timeline", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kSequenceGameInputsParams, runtime_ops::handle_sequence_game_inputs}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_ui_elements",
      "Enumerate interactive UI nodes (every Control-derived node) in the running game's scene tree over the runtime debug channel — reconnaissance before click automation: locate buttons/panels here, then click them with click_game_ui_element using path (preferred) or text/text_index, or reference them from execute_game_script using path. global_rect is in viewport/canvas coordinates and only click_game_ui_element converts it to window coordinates before injection — do not feed global_rect directly to queue_game_input or sequence_game_inputs. Requires a game launched from the editor whose project loads the godot-autopilot extension. Returns per element: path (absolute node path), type (class name), visible (visibility including ancestors), text (when the control has a text property) and global_rect {position,size} in viewport coordinates. Elements come in tree order capped by max_elements (default 100, max 1000); truncated=true signals more remain — raise the cap or target a subtree via execute_game_script.",
      "Game", {"game", "runtime", "ui", "tree", "elements", "debug"}, SideEffect::None, tool_flags::kNone,
      kGetGameUiElementsParams, runtime_ops::handle_game_ui_elements}));
  v.push_back(make_spec_tool(ToolSpec{
      "click_game_ui_element",
      "Click a UI element in the running game process by node path or visible text over the runtime debug channel: the game enumerates its Control tree, resolves path (exact match first, otherwise a node path ending with '/'+path) or text (exact match over visible elements first, then substring) and injects a mouse_button press/release pair at the element's global_rect centre through the same path as queue_game_input. Requires a game launched from the editor whose project loads the godot-autopilot extension and the game_runtime authorization gate. path (from get_game_ui_elements) or text (visible control text) selects the element — pass exactly one of them, path wins when both are given; text matches visible elements exactly first, then by substring, and text_index (default 0) picks the nth match when several match. The annotate id from capture_game_viewport is a per-capture sequence number and is not clickable — use the same row's path or its text instead; button_index (1=left default, 2=right, 3=middle) and double_click (default false, adds a second press/release pair) are optional. max_elements caps the enumeration (default 1000, max 1000); timeout_ms defaults to 5000 (max 25000; the host waits timeout_ms + 2000 ms, below the 30 s transport timeout). One call performs both steps (enumerate + inject) and resolves with {\"result\":{\"ok\":true,\"path\":...,\"position\":{x,y},\"clicks\":n}}; when the path is not found the error lists the element count and candidate paths.",
      "Game", {"game", "runtime", "input", "click", "ui", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kClickGameUiElementParams, runtime_ops::handle_click_game_ui_element}));
  return v;
}

} // namespace game_tools
} // namespace godot_autopilot

#endif
