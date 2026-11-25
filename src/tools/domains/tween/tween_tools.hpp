#ifndef GODOT_AUTOPILOT_TWEEN_TOOLS_HPP
#define GODOT_AUTOPILOT_TWEEN_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/tween/tween_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace tween_tools {

namespace {

const std::vector<std::string> kGameTweenActions = {"create", "control",
                                                    "callback", "method",
                                                    "interval"};
const std::vector<std::string> kTweenEditorPropertyActions = {
    "property", "callback", "method", "interval"};

const std::vector<ParamSpec> kGameTweenParams = {
    {"node_path", "string", "create/callback/method: node path in the running game process, e.g. /root/Main/Player; callback and method target this node", false},
    {"property", "string", "create: property to animate, e.g. position, scale or modulate:a; must exist on the node", false},
    {"target_value", "object", "create/method: final value the property animates to (create) or the end value interpolated into the method (method), converted with the same chain as game_script action=execute script_action=set_property (e.g. {\"x\": 10, \"y\": 20} for a Vector2, a plain number for scalar properties)", false},
    {"duration", "number", "create/method/interval: tween step duration in seconds (number greater than 0)", false},
    {"trans", "integer", "create: transition type 0-10 (0=linear default, 1=sine, 2=quint, 3=quart, 4=quad, 5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, 10=back)", false},
    {"ease", "integer", "create: ease type 0-3 (0=in, 1=out, 2=in_out, 3=out_in; default 2)", false},
    {"loops", "integer", "create: loop count, an integer >= 0 (0 means infinite loop; default 1)", false},
    {"speed_scale", "number", "create: speed multiplier (number greater than 0, default: 1.0)", false},
    {"method_name", "string", "method: method on node_path that receives the interpolated value each step; must exist on the node", false},
    {"from_value", "object", "method: starting value passed to method_name, converted with the same chain as target_value", false},
    {"callback_method", "string", "callback: method on node_path invoked when the step is reached; must exist on the node", false},
    {"callback_args", "array", "callback: optional arguments bound to callback_method", false},
    {"parallel", "boolean", "create/callback/method/interval: run this step parallel to the previous step instead of after it (default: false)", false},
    {"tween_id", "integer", "control: tween handle returned by game_tween action=create", false},
    {"tween_action", "string", "control: tween control action: play, pause, stop or kill", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000)", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing (default: true)", false},
};

const std::vector<ParamSpec> kGetGameTweenListParams = {
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing; while the game is starting, the request is queued and replayed as soon as readiness arrives. Set false to fail immediately (previous behaviour). Default: true", false},
};

const std::vector<ParamSpec> kTweenEditorPropertyParams = {
    {"action", "string", "Tween step to add to the preview: 'property' (default when omitted), 'callback', 'method' or 'interval'", false},
    {"path", "string", "Anchor node path in the edited scene (e.g. 'Player/Sprite2D'); its SceneTree hosts the preview tween and callback/method resolve on this node. The response includes scene_path and scene_unsaved to confirm which scene the preview node belongs to", false},
    {"property", "string", "property: property to animate, e.g. position, scale or modulate:a; must exist on the node or the call fails fast", false},
    {"target_value", "object", "property/method: final value the property animates to (property) or the end value interpolated into the method (method), converted with the same chain as property_set (e.g. {\"x\": 10, \"y\": 20} for a Vector2, a plain number for scalar properties)", false},
    {"from_value", "object", "method: starting value passed to method_name, converted with the same chain as target_value", false},
    {"duration", "number", "property/method/interval: step duration in seconds (number greater than 0)", false},
    {"method_name", "string", "method: method on path that receives the interpolated value each step; must exist on the node", false},
    {"callback_method", "string", "callback: method on path invoked when the step is reached; must exist on the node", false},
    {"callback_args", "array", "callback: optional arguments bound to callback_method", false},
    {"parallel", "boolean", "Run this step parallel to the previous step instead of after it (default: false)", false},
    {"trans", "integer", "Transition type 0-10 (0=linear default, 1=sine, 2=quint, 3=quart, 4=quad, 5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, 10=back)", false},
    {"ease", "integer", "Ease type 0-3 (0=in, 1=out, 2=in_out, 3=out_in; default 2)", false},
    {"loops", "integer", "Loop count, an integer >= 0 (0 means infinite loop; default 1)", false},
};

inline godot_autopilot::util::JsonValue tween_alias_action(const godot_autopilot::util::JsonValue &args,
                                         const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = godot_autopilot::util::JsonValue(action);
  return out;
}

inline godot_autopilot::util::JsonValue tween_alias_rename(const godot_autopilot::util::JsonValue &args,
                                         const char *from, const char *to,
                                         const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
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
  out["action"] = godot_autopilot::util::JsonValue(action);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "game_tween",
      "Create and control tween steps in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). action='create' animates property from node_path to target_value over duration seconds (optional trans 0-10, ease 0-3, loops >= 0 where 0 is infinite, speed_scale); action='callback' calls callback_method on node_path (optional callback_args bound as literals); action='method' interpolates from_value to target_value over duration and passes each value to method_name on node_path; action='interval' inserts a wait of duration seconds; every step accepts parallel to run alongside the previous step and returns the game-side tween handle; action='control' plays, pauses, stops or kills that handle (tween_id + tween_action). Callback and method steps only accept method names that must exist on the node — no source code is evaluated. Read the active tweens with get_game_tween_list.",
      "Game", {"game", "runtime", "tween", "create", "control", "callback",
               "method", "interval", "parallel", "create_game_tween",
               "control_game_tween"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kGameTweenParams, tween_ops::handle_game_tween_bundle,
      schema::build_action_schema(kGameTweenActions, kGameTweenParams,
                                  "Tween action: create|control|callback|method|interval"),
      {
          ToolAlias{"create_game_tween", "game_tween",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return tween_alias_action(args, "create");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"control_game_tween", "game_tween",
                    [](const godot_autopilot::util::JsonValue &args) {
                      return tween_alias_rename(args, "action", "tween_action",
                                               "control");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"create\",\"node_path\":\"/root/Main/Player\",\"property\":\"position\",\"target_value\":{\"x\":10,\"y\":20},\"duration\":1.0}",
       "{\"action\":\"callback\",\"node_path\":\"/root/Main/Player\",\"callback_method\":\"queue_free\"}",
       "{\"action\":\"interval\",\"duration\":0.5,\"parallel\":true}",
       "{\"action\":\"control\",\"tween_id\":1,\"tween_action\":\"play\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_tween_list",
      "List the active tweens in the running game process over the runtime debug channel, read-only: each entry carries the tween handle plus running and validity state for triage before game_tween action=control. Requires a game launched from the editor whose project loads the godot-autopilot extension.",
      "Game", {"game", "runtime", "tween", "list"}, SideEffect::None, tool_flags::kNone,
      kGetGameTweenListParams, tween_ops::handle_get_game_tween_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "tween_editor_property",
      "Preview a tween step on a node of the currently edited scene without running the game: pass path (anchor node path in the edited scene) plus the fields named by action. action='property' (default when omitted) animates property to target_value (converted with the property_set chain) over duration seconds; action='callback' calls callback_method on the node with optional callback_args bound as literal values; action='method' interpolates from_value to target_value over duration and passes each value to method_name; action='interval' inserts a wait of duration seconds. Callback and method only accept method names that exist on the node (no source code is evaluated), keeping the preview limited to the property/method whitelist. The tween is created straight on the editor SceneTree via SceneTree::create_tween and its int64 handle is kept in the editor-side TweenStore for the session; optional trans (0-10), ease (0-3), loops (>= 0, 0 means infinite) and parallel shape the playback. Returns action, tween_id, path and running with scene_path and scene_unsaved so the caller can confirm which scene the preview node belongs to.",
      "Game", {"game", "editor", "tween", "preview", "property", "callback",
                "method", "interval", "parallel"}, SideEffect::WritesScene, tool_flags::kMutating,
      kTweenEditorPropertyParams, tween_ops::handle_tween_editor_property,
      schema::build_action_schema(
          kTweenEditorPropertyActions, kTweenEditorPropertyParams,
          "Preview step: property|callback|method|interval"),
      {},
      {"{\"action\":\"property\",\"path\":\"Player/Sprite2D\",\"property\":\"position\",\"target_value\":{\"x\":10,\"y\":20},\"duration\":1.0}",
       "{\"action\":\"callback\",\"path\":\"Player\",\"callback_method\":\"queue_free\"}",
       "{\"action\":\"interval\",\"path\":\"Player\",\"duration\":0.5,\"parallel\":true}"}}));
  return v;
}

} // namespace tween_tools
} // namespace godot_autopilot

#endif
