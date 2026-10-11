#ifndef GODOT_AUTOPILOT_TWEEN_TOOLS_HPP
#define GODOT_AUTOPILOT_TWEEN_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/tween_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace tween_tools {

namespace {

const std::vector<ParamSpec> kCreateGameTweenParams = {
    {"node_path", "string", "Node path in the running game process, e.g. /root/Main/Player", true},
    {"property", "string", "Property to animate, e.g. position, scale or modulate:a; must exist on the node", true},
    {"target_value", "object", "Final value the property animates to, converted with the same chain as execute_game_script set_property (e.g. {\"x\": 10, \"y\": 20} for a Vector2, a plain number for scalar properties)", true},
    {"duration", "number", "Tween duration in seconds for one loop (number greater than 0)", true},
    {"trans", "integer", "Transition type 0-10 (0=linear default, 1=sine, 2=quint, 3=quart, 4=quad, 5=expo, 6=elastic, 7=cubic, 8=circ, 9=bounce, 10=back)", false},
    {"ease", "integer", "Ease type 0-3 (0=in, 1=out, 2=in_out, 3=out_in; default 2)", false},
    {"loops", "integer", "Loop count, an integer >= 0 (0 means infinite loop; default 1)", false},
    {"speed_scale", "number", "Speed multiplier (number greater than 0, default: 1.0)", false},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing; while the game is starting, the request is queued and replayed as soon as readiness arrives. Set false to fail immediately (previous behaviour). Default: true", false},
};

const std::vector<ParamSpec> kControlGameTweenParams = {
    {"tween_id", "integer", "Tween handle returned by create_game_tween", true},
    {"action", "string", "Control action: play, pause, stop or kill", true},
    {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 25000). The host waits timeout_ms + 2000 ms and that budget must stay below the 30 s HTTP transport timeout, so 25000 ms is the hard cap", false},
    {"wait_ready", "boolean", "Wait for the game process to report ready (gda:ready) before failing; while the game is starting, the request is queued and replayed as soon as readiness arrives. Set false to fail immediately (previous behaviour). Default: true", false},
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

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(ToolSpec{
      "create_game_tween",
      "Create a property tween on a node in the running game process over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. node_path selects the target node, property names the animated property (e.g. position, modulate:a) and target_value carries the final value converted with the same chain as execute_game_script set_property. duration (seconds, greater than 0) bounds one loop; optional trans (0-10, default 0 linear), ease (0-3, default 2 in_out), loops (integer >= 0, 0 means infinite, default 1) and speed_scale (greater than 0, default 1.0) shape the playback. Returns the game-side tween handle for control_game_tween.",
      "Game", {"game", "runtime", "tween", "create"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kCreateGameTweenParams, tween_ops::handle_create_game_tween}));
  v.push_back(make_spec_tool(ToolSpec{
      "control_game_tween",
      "Play, pause, stop or kill a game-side tween previously created with create_game_tween over the runtime debug channel; requires a game launched from the editor whose project loads the godot-autopilot extension. tween_id is the integer handle from creation and action is one of play, pause, stop or kill.",
      "Game", {"game", "runtime", "tween", "control"}, SideEffect::GameRuntime, tool_flags::kMutating,
      kControlGameTweenParams, tween_ops::handle_control_game_tween}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_tween_list",
      "List the active tweens in the running game process over the runtime debug channel, read-only: each entry carries the tween handle plus running and validity state for triage before control_game_tween. Requires a game launched from the editor whose project loads the godot-autopilot extension.",
      "Game", {"game", "runtime", "tween", "list"}, SideEffect::None, tool_flags::kNone,
      kGetGameTweenListParams, tween_ops::handle_get_game_tween_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "tween_editor_property",
      "Preview a property tween on a node of the currently edited scene without running the game: pass path (node path in the edited scene) and property plus target_value (converted with the property_set chain) and duration (seconds, greater than 0). The tween is created straight on the editor SceneTree via SceneTree::create_tween and its int64 handle is kept in the editor-side TweenStore for the session; optional trans (0-10), ease (0-3) and loops (>= 0, 0 means infinite) shape the playback. Returns tween_id with scene_path and scene_unsaved so the caller can confirm which scene the preview node belongs to.",
      "Game", {"game", "editor", "tween", "preview", "property"}, SideEffect::None, tool_flags::kMutating,
      kTweenEditorPropertyParams, tween_ops::handle_tween_editor_property}));
  return v;
}

} // namespace tween_tools
} // namespace godot_autopilot

#endif
