#ifndef GODOT_AUTOPILOT_GAME_TOOLS_HPP
#define GODOT_AUTOPILOT_GAME_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/base/runtime_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace runtime_ops {
mcp::JsonValue handle_game_input_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_game_ui_elements(const mcp::JsonValue &args);
mcp::JsonValue handle_click_game_ui_element(const mcp::JsonValue &args);
mcp::JsonValue handle_get_game_locale(const mcp::JsonValue &args);
mcp::JsonValue handle_set_game_locale(const mcp::JsonValue &args);
mcp::JsonValue handle_get_translation_list(const mcp::JsonValue &args);
mcp::JsonValue handle_translate_text(const mcp::JsonValue &args);
mcp::JsonValue handle_game_state_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_game_ui_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_game_evidence_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_game_localization_bundle(const mcp::JsonValue &args);
mcp::JsonValue handle_game_script_bundle(const mcp::JsonValue &args);
} // namespace runtime_ops
namespace game_tools {

namespace {

const std::vector<std::string> kGetGameStateActions = {"status", "session",
                                                        "input", "job"};

const std::vector<ParamSpec> kGetGameStateParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"input_action", "string", "action=input: InputMap action name to query; the response includes pressed, just_pressed, just_released, paused and physics_frame", false},
    {"job_id", "integer", "action=job: job id returned by game_script action=start_job", false},
    {"cancel", "boolean", "action=job: cancel the job instead of collecting it (default: false); an idempotent cancel reaches the game, the job record is dropped and the response reports status cancelled", false},
    {"timeout_ms", "integer", "action=input/job: timeout in milliseconds; for job, 0 = non-blocking poll (default) and a positive value waits for the game response on the transport thread, up to 25000", false},
    {"wait_ready", "boolean", "action=status/input: wait for the game process to report ready (gda:ready) before failing (default: true); set false to fail immediately", false},
};

const std::vector<std::string> kGetGameUiActions = {"elements"};

const std::vector<ParamSpec> kGetGameUiParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"max_elements", "integer", "Maximum number of Control-derived elements returned after filtering, in tree order (default: 100, max: 1000); truncated=true signals more filtered matches remain", false},
    {"filter", "string", "Case-insensitive substring matched against each element's path, type and text (default: absent/empty = no filter); only matching elements count toward max_elements and truncated", false},
    {"types", "array", "Array of 1-32 class names (exact match, e.g. \"Button\", \"Label\") used as a type allow-list; only matching elements count toward max_elements and truncated", false},
    {"visible_only", "boolean", "Keep only elements visible in the tree (visibility including ancestors; default: false keeps hidden controls too)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000)", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<std::string> kGetGameEvidenceActions = {"sample", "collect",
                                                           "validate"};

const std::vector<ParamSpec> kGetGameEvidenceParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"node_path", "string", "sample: node path in the running game process, e.g. /root/Main/Player", false},
    {"property", "string", "sample: property name to sample; must exist on the node — missing names fail fast instead of collecting nulls", false},
    {"frames", "integer", "sample: how many samples to collect, an integer from 1 to 120", false},
    {"interval_frames", "integer", "sample: sample every interval_frames+1-th process frame (integer 0-60, default: 0); frames*(interval_frames+1) must stay within 3600", false},
    {"include_status", "boolean", "collect: include the game status section, same fields as get_game_state action=status (default: true)", false},
    {"include_capture", "boolean", "collect: include the game screenshot section (default: true)", false},
    {"include_errors", "boolean", "collect: include the recent game errors section (default: true)", false},
    {"limit", "integer", "collect: maximum number of errors in the errors section, an integer from 1 to 200 (default: 20)", false},
    {"region", "object", "collect: crop the captured image to {x, y, width, height} in viewport pixels; the rect is clamped to the image", false},
    {"max_dimension", "integer", "collect: downscale the capture so its longest side is at most this many pixels (64-4096); never upscales", false},
    {"scale", "integer", "collect: integer nearest-neighbour upscale factor for the capture (1-8, default: 1), applied before max_dimension", false},
    {"max_elements", "integer", "validate: maximum number of Control elements enumerated (default: 100); truncated=true signals more remain", false},
    {"ignore_paths", "array", "validate: up to 50 exact or '/'-suffix paths suppressing known-good entries", false},
    {"ignore_classes", "array", "validate: up to 20 class names suppressing known-good entries; subclasses match, e.g. Container covers layout-only containers", false},
    {"min_area", "number", "validate: visible areas below this size report tiny_area info (default: 4.0)", false},
    {"bounds_margin", "number", "validate: pixel tolerance around the viewport visible rect (default: 2.0)", false},
    {"occlude_ratio", "number", "validate: fraction 0.5-1.0 of an element area covered by a later visible control before possibly_occluded warns (default: 0.98); controls are compared in effective z_index order, each control's z_index accumulated up its ancestors while z_as_relative holds, and a control with mouse_filter=IGNORE never counts as an occluder", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000)", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<std::string> kGameScriptActions = {"execute", "start_job"};

const std::vector<ParamSpec> kGameScriptParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"script_action", "string", "execute: inner operation to run in the game process: 'script' (execute arbitrary GDScript), 'get_property', 'set_property' or 'call_method'", false},
    {"node_path", "string", "execute: node path for get_property/set_property/call_method (omit to use the current scene root)", false},
    {"property", "string", "execute: property name for get_property/set_property", false},
    {"value", "json", "execute: value to set for set_property", false},
    {"method", "string", "execute: method name for call_method; await methods are awaited to completion (bounded by timeout_ms) and return their final result", false},
    {"args", "array", "execute: arguments for call_method", false},
    {"source_code", "string", "execute: GDScript source for script_action='script' — a complete script defining func _run() (used as-is), or bare statements auto-wrapped into @tool extends Node + func _run() (an explicit return is needed to produce a value); the wrapper node runs under /root, outside the current scene, so reach scene nodes with absolute paths (/root/...) or ../", false},
    {"persist", "boolean", "execute: keep the script's temporary node alive after the call (default: false removes and frees it, including on timeout/cancel); with true the node is stored under /root/__gda_runtime and its path is returned in node_path for later get_property/call_method use", false},
    {"persist_name", "string", "execute: node name under /root/__gda_runtime when persist=true (default: auto-generated)", false},
    {"assert", "string", "execute: optional sandboxed assertion expression (max 1024 chars) evaluated with the eval result bound as value, e.g. value > 0; coroutine results cannot be asserted — omit assert for awaitable calls", false},
    {"timeout_ms", "integer", "Timeout in milliseconds: bounds the in-game execution for execute (default 5000) or the in-game run and job expiry for start_job (default 5000); max 25000", false},
    {"op", "string", "start_job: wrapped game op to submit for asynchronous execution; currently only 'eval' is supported", false},
    {"params", "object", "start_job: parameters object passed to the wrapped op verbatim — same fields as game_script action=execute, e.g. {\"action\":\"script\",\"source_code\":\"...\"}", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<std::string> kGameInputActions = {"queue", "wait",
                                                     "sequence"};

const std::vector<ParamSpec> kGameInputParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"type", "string", "queue: input event type: 'key', 'mouse_button', 'wheel', 'mouse_motion' or 'action'", false},
    {"keycode", "string", "queue: key for type='key': numeric key code (e.g. 65) or key name (e.g. 'A', 'space', 'KEY_LEFT')", false},
    {"pressed", "boolean", "queue: pressed state (default: true)", false},
    {"button_index", "integer", "queue: mouse button index 1=left, 2=right, 3=middle for type='mouse_button'; for type='wheel' a wheel button index (4=up, 5=down, 6=left, 7=right)", false},
    {"position", "object", "queue: mouse position {x, y} in window pixels with top-left origin for mouse_button, wheel or mouse_motion; pass viewport coordinates only with coord_space='viewport'", false},
    {"coord_space", "string", "queue/sequence: coordinate space for positions: 'window' (default) or 'viewport' (converted to window via the root viewport screen_transform before injection)", false},
    {"direction", "string", "queue: wheel direction 'up', 'down', 'left' or 'right' (case-insensitive); ignored when button_index is given", false},
    {"amount", "integer", "queue: wheel scroll amount (integer 1-10, default: 1)", false},
    {"relative", "object", "queue: pointer movement delta {x, y} for type='mouse_motion' (default: zero vector)", false},
    {"input_action", "string", "queue/wait: InputMap action name; required by wait, used by queue for type='action'", false},
    {"duration_ms", "integer", "queue: hold duration in ms — the input auto-releases when it expires (0/absent = no auto-release); mode='hold' re-presses every physics frame until release, so read transient state back within this window", false},
    {"click", "boolean", "queue: paired click helper for type='mouse_button' (default: false); the release follows 2 physics frames after the press so BaseButton controls observe both edges", false},
    {"double_click", "boolean", "queue: inject a second press/release pair after the first for type='mouse_button' (default: false)", false},
    {"mode", "string", "queue: injection mode 'event' (default), 'api' or 'hold'", false},
    {"state", "string", "wait: transient state to wait for: 'just_pressed' (default), 'just_released' or 'pressed'", false},
    {"inject", "object", "wait: inject an input before waiting, in the same call (fields mirror queue: type/action/keycode/pressed/mode/duration_ms/button_index/position/direction/amount/relative/click/double_click); required for just_pressed/just_released", false},
    {"inputs", "array", "sequence: timeline items, each {kind, at_frame, ...queue fields}; max 256 items, each fires on its physics-frame offset", false},
    {"timeout_ms", "integer", "Timeout in milliseconds (queue/wait default 5000, max 25000; sequence defaults to max(at_frame)*33+2000)", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<std::string> kGetGameLocalizationActions = {
    "locale", "list", "translate"};

const std::vector<ParamSpec> kGetGameLocalizationParams = {
    {"key", "string", "translate: source message key to translate under the current game locale; unknown keys fail instead of echoing the key", false},
};

const std::vector<ParamSpec> kClickGameUiElementParams = {
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
    {"path", "string", "Node path of the UI element to click, as returned by get_game_ui action=elements (e.g. '/root/Main/Menu/StartButton'); an exact match wins, otherwise a path ending with '/'+path is used. Alternatively pass text/text_index instead — path wins when both are given; the annotate id from capture_editor_viewport is a per-capture sequence number and is not clickable", false},
    {"text", "string", "Visible text of the UI element to click (exact match over visible elements first, then substring); alternative to path — path wins when both are given", false},
    {"text_index", "integer", "Selects the nth visible text match (0-based, default: 0); without it an ambiguous text fails listing candidates", false},
    {"button_index", "integer", "Mouse button to click: 1=left (default), 2=right, 3=middle", false},
    {"double_click", "boolean", "Inject a second press/release pair (default: false)", false},
    {"strict_hit", "boolean", "Turn a hit mismatch into an error (default: false). The game reports hit_mismatch when the hovered control is neither the resolved element nor one of its descendants; strict_hit=false still succeeds and only reports hit_mismatch plus warning, strict_hit=true fails the call with code click_occluded", false},
    {"max_elements", "integer", "Maximum number of Control elements enumerated while resolving path (default: 1000, max: 1000)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000)", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<ParamSpec> kReloadGameScriptsParams = {
    {"paths", "array", "Non-empty array of res:// GDScript file paths to reload in the running game", true},
    {"session_id", "integer", "Target game debugger session ID; required when multiple sessions are ready; omitted selects the only ready session", false},
};

const std::vector<ParamSpec> kSetGameLocaleParams = {
    {"locale", "string", "Game locale to activate in en or en_US format (e.g. en_US, zh_CN, en); other formats are rejected without changing state", true},
};

inline mcp::JsonValue game_alias_action(const mcp::JsonValue &args,
                                        const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue game_alias_rename(const mcp::JsonValue &args,
                                        const char *from, const char *to,
                                        const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  auto *value = args.Find(from);
  if (value != nullptr) {
    out[to] = *value;
  }
  if (out.IsObject()) {
    out.GetObject().erase(from);
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(9);
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_state",
      "Read running-game state over the runtime debug channel in one action tool. action='status' returns engine version, current scene, node count, FPS, paused and physics_frame plus healthy/last_activity_ms; action='session' returns the editor-side game session summary (playing, active_sessions, ready_sessions, scene_ready_sessions, breaked_sessions) without contacting the game; action='input' queries one InputMap action (input_action required) and returns pressed/just_pressed/just_released plus paused and physics_frame; action='job' polls a job started with game_script action=start_job (job_id, optional cancel, timeout_ms where 0 polls without waiting). Read-only; no confirmation is required. Requires a game launched from the editor whose project loads the godot-autopilot extension for status/input.",
      "Game", {"game", "runtime", "state", "status", "session", "input", "job", "debug", "get_game_status", "get_game_input_status", "get_game_job"}, SideEffect::None, tool_flags::kLongBlocking,
      kGetGameStateParams, runtime_ops::handle_game_state_bundle,
      schema::build_action_schema(kGetGameStateActions, kGetGameStateParams,
                                  "Game state to read: status|session|input|job"),
      {
          ToolAlias{"get_game_status", "get_game_state",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "status");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_game_input_status", "get_game_state",
                    [](const mcp::JsonValue &args) {
                      return game_alias_rename(args, "action", "input_action",
                                               "input");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_game_job", "get_game_state",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "job");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"status\"}", "{\"action\":\"input\",\"input_action\":\"ui_accept\"}",
       "{\"action\":\"job\",\"job_id\":1,\"timeout_ms\":2000}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_ui",
      "Enumerate interactive UI nodes (every Control-derived node) in the running game's scene tree over the runtime debug channel before click automation. action='elements' returns per element: path, type, visible, text (when the control has a text property), mouse_filter (STOP, PASS or IGNORE — IGNORE controls do not receive mouse button events, so they cannot be clicked), z_index (the control's own CanvasItem z_index; combine it with ancestors' z_index and z_as_relative to reason about draw order), global_rect in viewport coordinates, window_rect, window_center and window_approx. Optional filter (case-insensitive over path/type/text), types (1-32 exact class names) and visible_only narrow the enumeration before max_elements caps it; truncated=true signals more matches. Read-only; use click_game_ui_element with the returned path or text to act.",
      "Game", {"game", "runtime", "ui", "tree", "elements", "get_game_ui_elements"}, SideEffect::None, tool_flags::kNone,
      kGetGameUiParams, runtime_ops::handle_game_ui_bundle,
      schema::build_action_schema(kGetGameUiActions, kGetGameUiParams,
                                  "Game UI data to read: elements"),
      {
          ToolAlias{"get_game_ui_elements", "get_game_ui",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "elements");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"elements\",\"filter\":\"Start\",\"visible_only\":true}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_evidence",
      "Collect read-only evidence from the running game over the runtime debug channel in one action tool. action='sample' samples node_path/property over frames process frames (optional interval_frames); action='collect' bundles game status, a screenshot and recent errors in one round trip (optional include_status/include_capture/include_errors, limit, region, max_dimension, scale); action='validate' scans the Control tree for zero-size, out-of-view and possibly-occluded controls (optional max_elements, ignore_paths, ignore_classes, min_area, bounds_margin, occlude_ratio); controls are compared in effective z_index order and a control with mouse_filter=IGNORE never counts as an occluder, possibly-occluded entries carry occluder_path and occluder_mouse_filter, and the response adds possibly_occluded_count as a scalar total. Each section fails in isolation; no automatic verdict and no confirmation. Requires a game launched from the editor whose project loads the godot-autopilot extension.",
      "Game", {"game", "runtime", "evidence", "sample", "collect", "validate", "debug", "sample_game_property", "collect_game_evidence", "validate_game_ui_layout"}, SideEffect::None, tool_flags::kNone,
      kGetGameEvidenceParams, runtime_ops::handle_game_evidence_bundle,
      schema::build_action_schema(kGetGameEvidenceActions,
                                  kGetGameEvidenceParams,
                                  "Evidence action: sample|collect|validate"),
      {
          ToolAlias{"sample_game_property", "get_game_evidence",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "sample");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"collect_game_evidence", "get_game_evidence",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "collect");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"validate_game_ui_layout", "get_game_evidence",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "validate");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"sample\",\"node_path\":\"/root/Main/Player\",\"property\":\"position\",\"frames\":10}",
       "{\"action\":\"collect\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "game_script",
      "Run code or inspect/modify state inside the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). action='execute' runs script_action: 'script' (complete GDScript defining func _run() or bare statements auto-wrapped; an explicit return is needed for a value), 'get_property', 'set_property' or 'call_method' on node_path. Execution context for 'script': the host wraps bare statements into @tool extends Node + func _run(), attaches the script to a temporary Node and adds it directly under /root (the root window), outside the current scene; relative get_node paths resolve from that temporary node (its parent is /root), so reach scene nodes with absolute paths such as /root/<SceneRoot>/Player or ../. After the call — including timeout and cancel — the node is removed and freed unless persist=true, which keeps it under /root/__gda_runtime (named persist_name, default eval_<request_id>; a duplicate name errors) and returns its node_path. assert attaches a sandboxed expression verdict. After a (force) restart, wait for the scene to be ready before the first call: wait_ready=true (default) queues the request until the game reports gda:ready with scene_ready. action='start_job' submits op='eval' with params (same fields as execute) for asynchronous execution and returns a job_id immediately; poll it with get_game_state action=job. timeout_ms bounds the in-game execution (default 5000, max 25000).",
      "Game", {"game", "runtime", "eval", "script", "job", "async", "debug", "execute_game_script", "start_game_job"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kGameScriptParams, runtime_ops::handle_game_script_bundle,
      schema::build_action_schema(kGameScriptActions, kGameScriptParams,
                                  "Script action: execute|start_job"),
      {
          ToolAlias{"execute_game_script", "game_script",
                    [](const mcp::JsonValue &args) {
                      return game_alias_rename(args, "action", "script_action",
                                               "execute");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"start_game_job", "game_script",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "start_job");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"execute\",\"script_action\":\"script\",\"source_code\":\"return 1\"}",
       "{\"action\":\"start_job\",\"op\":\"eval\",\"params\":{\"action\":\"script\",\"source_code\":\"return 1\"}}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "game_input",
      "Simulate input inside the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). action='queue' injects one event (type is key/mouse_button/wheel/mouse_motion/action; mode event/api/hold; optional click pairing, wheel direction/amount, coord_space). mode='hold' re-presses the input every physics frame and auto-releases it when duration_ms expires: a readback (get_game_state action=input) must land inside that hold window to observe pressed=true — after the auto-release the transient state is gone, while persistent effects such as accumulated movement remain; mode='event' with duration_ms=0 (or absent) presses without scheduling any release. Injection uses Input.parse_input_event and does not route events to _unhandled_input; poll Input.is_action_pressed in _process instead. action='wait' waits for a transient InputMap state on input_action (optional inject runs the input in the same call); action='sequence' schedules a timeline of up to 256 items on exact physics frames. Transient states are visible only in the next physics frame; use get_game_state action=input to diagnose. Input.is_action_just_pressed is true only on the single physics frame the injected event is processed and false again afterwards; a cross-process readback after a queue/inject call lands in a later frame and misses that one-frame transient state. To capture just_pressed, use action='wait' with state='just_pressed' on input_action (it returns within the same frame the state becomes true) or read get_game_state action=input in that same frame. Recommended pattern: issue the wait with state='just_pressed' (optionally with an inline inject in the same call) and sample/assert just_pressed there — do not queue the event and then make a separate get_game_state call, which races the one-frame window.",
      "Game", {"game", "runtime", "input", "queue", "wait", "sequence", "simulate", "debug", "queue_game_input", "wait_game_input", "sequence_game_inputs"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kGameInputParams, runtime_ops::handle_game_input_bundle,
      schema::build_action_schema(kGameInputActions, kGameInputParams,
                                  "Input action: queue|wait|sequence"),
      {
          ToolAlias{"queue_game_input", "game_input",
                    [](const mcp::JsonValue &args) {
                      return game_alias_rename(args, "action", "input_action",
                                               "queue");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"wait_game_input", "game_input",
                    [](const mcp::JsonValue &args) {
                      return game_alias_rename(args, "action", "input_action",
                                               "wait");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"sequence_game_inputs", "game_input",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "sequence");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"queue\",\"type\":\"key\",\"keycode\":\"A\"}",
       "{\"action\":\"wait\",\"input_action\":\"ui_accept\",\"state\":\"just_pressed\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "click_game_ui_element",
      "Click a UI element in the running game process by node path or visible text over the runtime debug channel: the game enumerates its Control tree, resolves path (exact match first, otherwise a node path ending with '/'+path) or text (exact match over visible elements first, then substring) and injects a same-coordinate mouse_motion (mode 'event') first to refresh the game's hover state, then a mouse_button press/release pair at the element's global_rect centre through the same path as game_input action=queue. Requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). path (from get_game_ui) or text selects the element — pass one of them, path wins when both are given; text matches visible elements exactly first, then by substring, and text_index picks the nth match when several match. button_index (1=left default, 2=right, 3=middle) and double_click (default false, adds a second press/release pair) are optional. After injecting, the game compares the viewport hovered control with the resolved element: hit_path is the hovered control's node path (empty when nothing is hovered) and hit_mismatch is true when the hovered control is neither the resolved element nor one of its descendants. A mismatch is not an error by default: the call succeeds, the result carries hit_path and hit_mismatch, and warning explains the mismatch. strict_hit (default false) instead fails the call with structured_error.code click_occluded (carrying target_path, hit_path and target_mouse_filter). max_elements caps the enumeration (default 1000, max 1000); timeout_ms defaults to 5000 (max 25000).",
      "Game", {"game", "runtime", "input", "click", "ui", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kClickGameUiElementParams, runtime_ops::handle_click_game_ui_element}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_game_scripts",
      "Reload GDScript files in the running game process over the runtime debug channel without restarting the game; engine soft reload keeps instance state where possible. Requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). paths is a non-empty array of res:// script paths (pass multiple files in one call). The reload request is sent to the game and applied during its next idle poll; no confirmation is returned — verify with get_game_log_entries or get_game_state action=status. Workflow after editing a script file: call reload_game_scripts first, then reload_current_scene (or retry the failed operation) — reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).",
      "Game", {"game", "runtime", "reload", "script", "debug"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kReloadGameScriptsParams, runtime_ops::handle_game_reload_scripts}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_game_locale",
      "Set the game locale on the TranslationServer singleton. Requires locale in en or en_US format (e.g. en_US, zh_CN); other formats are rejected without changing state. Returns result ok plus the applied locale. Requires confirm=true (GameRuntime); read the current locale with get_game_localization action=locale.",
      "Game", {"game", "locale", "i18n"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kSetGameLocaleParams, runtime_ops::handle_set_game_locale}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_localization",
      "Read game localization state in one action tool. action='locale' returns the current TranslationServer locale (e.g. en_US); action='list' returns the locales with loaded translations (same set as get_loaded_locales); action='translate' translates key under the current locale and returns the text plus key and locale (unknown keys fail instead of echoing). All actions are read-only; switch the locale with set_game_locale.",
      "Game", {"game", "locale", "i18n", "translation", "get_game_locale", "get_translation_list", "translate_text"}, SideEffect::None, tool_flags::kNone,
      kGetGameLocalizationParams, runtime_ops::handle_game_localization_bundle,
      schema::build_action_schema(kGetGameLocalizationActions,
                                  kGetGameLocalizationParams,
                                  "Localization data to read: locale|list|translate"),
      {
          ToolAlias{"get_game_locale", "get_game_localization",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "locale");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_translation_list", "get_game_localization",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "list");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"translate_text", "get_game_localization",
                    [](const mcp::JsonValue &args) {
                      return game_alias_action(args, "translate");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"locale\"}", "{\"action\":\"translate\",\"key\":\"HELLO\"}"}}));
  return v;
}

} // namespace game_tools
} // namespace godot_autopilot

#endif
