#ifndef GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/scene_tree_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace scene_tree_tools {

namespace {

const std::vector<ParamSpec> kCallSceneTreeGroupParams = {
    {"group_name", "string", "Name of the group whose nodes receive the call (string, e.g. 'enemies')", true},
    {"method", "string", "Method name to call on every group node (string, e.g. 'take_damage')", true},
    {"arguments", "array", "Optional arguments array passed to the method (array, e.g. [10, 'fire'])", false},
};

const std::vector<ParamSpec> kCreateSceneTreeTimerParams = {
    {"delay_sec", "number", "Timer delay in seconds (number, e.g. 1.5)", true},
    {"process_always", "boolean", "Process even while the scene tree is paused (boolean, default: true)", false},
    {"process_in_physics", "boolean", "Count down in the physics step instead of the process step (boolean, default: false)", false},
};

const std::vector<ParamSpec> kGetSceneTreeNodesInGroupParams = {
    {"group_name", "string", "Name of the group to look up (string, e.g. 'enemies')", true},
};

const std::vector<ParamSpec> kIsSceneTreePausedParams = {};

const std::vector<ParamSpec> kNotifySceneTreeGroupParams = {
    {"group_name", "string", "Name of the group whose nodes receive the notification (string, e.g. 'enemies')", true},
    {"notification", "integer", "Notification constant as integer (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3, NOTIFICATION_ENTER_TREE=10, NOTIFICATION_EXIT_TREE=11)", true},
};

const std::vector<ParamSpec> kReloadSceneTreeCurrentSceneParams = {};

const std::vector<ParamSpec> kSetSceneTreeDebugCollisionsHintParams = {
    {"enabled", "boolean", "Draw collision shapes while the scene runs (boolean, e.g. true)", true},
};

const std::vector<ParamSpec> kSetSceneTreePauseParams = {
    {"paused", "boolean", "Pause state: true pauses the scene tree, false resumes (boolean)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(ToolSpec{
      "call_scene_tree_group",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Call a method on every node belonging to a group in that game SceneTree. Requires 'group_name' and 'method'; optional 'arguments' (array) is passed to the call. Returns 'ok' only — call_group collects no return values, so this confirms the call was dispatched rather than reporting per-node results.",
      "Scene", {"scene", "group"}, SideEffect::GameRuntime, tool_flags::kNone,
      kCallSceneTreeGroupParams, scene_tree_ops::handle_call_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_tree_timer",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Create a SceneTreeTimer in that game SceneTree, used with await or a timeout callback for delayed game logic. Requires 'delay_sec'; optional 'process_always' (default true — runs while the game tree is paused) and 'process_in_physics' (default false). Returns 'ok' with a note that the timer was created in the game process: the SceneTreeTimer object cannot be returned across processes, so observe its effect (an await or a timeout callback) in the game instead of holding a reference.",
      "Scene", {"scene", "timer"}, SideEffect::GameRuntime, tool_flags::kNone,
      kCreateSceneTreeTimerParams, scene_tree_ops::handle_create_timer}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_tree_nodes_in_group",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. List every node in a group within that game SceneTree. Requires 'group_name'; returns an array of node paths (absolute path of each matching node). An empty array means no node is currently in the group. Membership comes from add_to_group calls and from groups saved in scenes.",
      "Scene", {"scene", "group"}, SideEffect::GameRuntime, tool_flags::kNone,
      kGetSceneTreeNodesInGroupParams, scene_tree_ops::handle_get_nodes_in_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "is_scene_tree_paused",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Check whether that game SceneTree is currently paused. Takes no parameters; returns true or false. Pausing stops processing for nodes whose process_mode is not WhenPaused. Use together with set_scene_tree_pause to confirm the state you configured. The boolean is wrapped in a 'result' field.",
      "Scene", {"scene", "pause"}, SideEffect::GameRuntime, tool_flags::kNone,
      kIsSceneTreePausedParams, scene_tree_ops::handle_is_paused}));
  v.push_back(make_spec_tool(ToolSpec{
      "notify_scene_tree_group",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Send a Godot notification to every node in a group in that game SceneTree. Requires 'group_name' and 'notification' — an integer NOTIFICATION_* constant (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3; the parameter schema lists more). Triggers the matching _notification callback on each node. Returns 'ok' when dispatched; per-node responses are not reported.",
      "Scene", {"scene", "group"}, SideEffect::GameRuntime, tool_flags::kNone,
      kNotifySceneTreeGroupParams, scene_tree_ops::handle_notify_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_scene_tree_current_scene",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Reload the scene currently running in that game process from disk. Requires a scene actively running in the game; with no current scene the game returns ERR_UNCONFIGURED (3) as the result integer. Reloading recreates the scene root, so runtime state (script state, connections, node modifications) is reset from the saved file.",
      "Scene", {"scene", "reload"}, SideEffect::GameRuntime, tool_flags::kNone,
      kReloadSceneTreeCurrentSceneParams, scene_tree_ops::handle_reload_current_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_scene_tree_debug_collisions_hint",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. Toggle collision shape debug visualization in that game SceneTree. Requires 'enabled' (bool); sets the game's SceneTree debug collisions hint so collision shapes are drawn during gameplay. The hint only affects debug drawing, not physics behavior. Shapes render only while the game is running. Returns 'ok'.",
      "Scene", {"scene", "debug"}, SideEffect::GameRuntime, tool_flags::kNone,
      kSetSceneTreeDebugCollisionsHintParams, scene_tree_ops::handle_set_debug_collisions}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_scene_tree_pause",
      "Act on the running game process's SceneTree over the runtime channel — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error, and the editor itself is never paused. Pause or unpause that game SceneTree. Requires 'paused' (bool). Only nodes whose process_mode permits WhenPaused keep processing while paused; process and physics callbacks of other nodes are not called. Query the resulting state with is_scene_tree_paused. Pass 'paused': false to resume the game. Returns result true.",
      "Scene", {"scene", "pause"}, SideEffect::GameRuntime, tool_flags::kNone,
      kSetSceneTreePauseParams, scene_tree_ops::handle_set_pause}));
  return v;
}

} // namespace scene_tree_tools
} // namespace godot_autopilot

#endif
