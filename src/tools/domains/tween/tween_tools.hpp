#ifndef GODOT_AUTOPILOT_TWEEN_TOOLS_HPP
#define GODOT_AUTOPILOT_TWEEN_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/tween/tween_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace tween_tools {

namespace {

const std::vector<std::string> kGameTweenActions = {"create", "control"};

const std::vector<ParamSpec> kGameTweenParams = {
    {"node_path", "string", "create: node path in the running game process, e.g. /root/Main/Player", false},
    {"property", "string", "create: property to animate, e.g. position, scale or modulate:a; must exist on the node", false},
    {"target_value", "object", "create: final value the property animates to, converted with the same chain as game_script action=execute script_action=set_property (e.g. {\"x\": 10, \"y\": 20} for a Vector2, a plain number for scalar properties)", false},
    {"duration", "number", "create: tween duration in seconds for one loop (number greater than 0)", false},
    {"trans", "integer", "create: transition type 0-10 (0=linear default, 1=sine, 2=quint, 3=quart, 4=quad, 5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, 10=back)", false},
    {"ease", "integer", "create: ease type 0-3 (0=in, 1=out, 2=in_out, 3=out_in; default 2)", false},
    {"loops", "integer", "create: loop count, an integer >= 0 (0 means infinite loop; default 1)", false},
    {"speed_scale", "number", "create: speed multiplier (number greater than 0, default: 1.0)", false},
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
    {"path", "string", "Node path in the edited scene (e.g. 'Player/Sprite2D'). The response includes scene_path and scene_unsaved to confirm which scene the preview node belongs to", true},
    {"property", "string", "Property to animate, e.g. position, scale or modulate:a; must exist on the node or the call fails fast", true},
    {"target_value", "object", "Final value the property animates to, converted with the same chain as property_set (e.g. {\"x\": 10, \"y\": 20} for a Vector2, a plain number for scalar properties)", true},
    {"duration", "number", "Tween duration in seconds for one loop (number greater than 0)", true},
    {"trans", "integer", "Transition type 0-10 (0=linear default, 1=sine, 2=quint, 3=quart, 4=quad, 5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, 10=back)", false},
    {"ease", "integer", "Ease type 0-3 (0=in, 1=out, 2=in_out, 3=out_in; default 2)", false},
    {"loops", "integer", "Loop count, an integer >= 0 (0 means infinite loop; default 1)", false},
};

inline mcp::JsonValue tween_alias_action(const mcp::JsonValue &args,
                                         const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue tween_alias_rename(const mcp::JsonValue &args,
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
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "game_tween",
      "Create and control property tweens in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension and confirm=true (GameRuntime). action='create' animates property from node_path to target_value over duration seconds (optional trans 0-10, ease 0-3, loops >= 0 where 0 is infinite, speed_scale) and returns the game-side tween handle; action='control' plays, pauses, stops or kills that handle (tween_id + tween_action). Read the active tweens with get_game_tween_list.",
      "Game", {"game", "runtime", "tween", "create", "control", "create_game_tween", "control_game_tween"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kGameTweenParams, tween_ops::handle_game_tween_bundle,
      schema::build_action_schema(kGameTweenActions, kGameTweenParams,
                                  "Tween action: create|control"),
      {
          ToolAlias{"create_game_tween", "game_tween",
                    [](const mcp::JsonValue &args) {
                      return tween_alias_action(args, "create");
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"control_game_tween", "game_tween",
                    [](const mcp::JsonValue &args) {
                      return tween_alias_rename(args, "action", "tween_action",
                                               "control");
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"action\":\"create\",\"node_path\":\"/root/Main/Player\",\"property\":\"position\",\"target_value\":{\"x\":10,\"y\":20},\"duration\":1.0}",
       "{\"action\":\"control\",\"tween_id\":1,\"tween_action\":\"play\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_tween_list",
      "List the active tweens in the running game process over the runtime debug channel, read-only: each entry carries the tween handle plus running and validity state for triage before game_tween action=control. Requires a game launched from the editor whose project loads the godot-autopilot extension.",
      "Game", {"game", "runtime", "tween", "list"}, SideEffect::None, tool_flags::kNone,
      kGetGameTweenListParams, tween_ops::handle_get_game_tween_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "tween_editor_property",
      "Preview a property tween on a node of the currently edited scene without running the game: pass path (node path in the edited scene) and property plus target_value (converted with the property_set chain) and duration (seconds, greater than 0). The tween is created straight on the editor SceneTree via SceneTree::create_tween and its int64 handle is kept in the editor-side TweenStore for the session; optional trans (0-10), ease (0-3) and loops (>= 0, 0 means infinite) shape the playback. Returns tween_id with scene_path and scene_unsaved so the caller can confirm which scene the preview node belongs to.",
      "Game", {"game", "editor", "tween", "preview", "property"}, SideEffect::WritesScene, tool_flags::kMutating,
      kTweenEditorPropertyParams, tween_ops::handle_tween_editor_property}));
  return v;
}

} // namespace tween_tools
} // namespace godot_autopilot

#endif
