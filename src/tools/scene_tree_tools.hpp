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
      "Call a method on every node belonging to a group. Requires 'group_name' and 'method'; optional 'arguments' (array) is passed to the call. Returns 'ok' only — call_group collects no return values, so this confirms the call was dispatched rather than reporting per-node results.",
      "Scene", {"scene", "group"}, SideEffect::None, tool_flags::kNone,
      kCallSceneTreeGroupParams, scene_tree_ops::handle_call_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_tree_timer",
      "Create a SceneTreeTimer in the running scene tree, used with await or a timeout callback for delayed logic. Requires 'delay_sec'; optional 'process_always' (default true — runs while paused) and 'process_in_physics' (default false). Returns the timer as a serialized object reference (ObjectID) for later use.",
      "Scene", {"scene", "timer"}, SideEffect::None, tool_flags::kNone,
      kCreateSceneTreeTimerParams, scene_tree_ops::handle_create_timer}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_tree_nodes_in_group",
      "List every node in a group within the running scene tree. Requires 'group_name'; returns an array of node paths (absolute path of each matching node). An empty array means no node is currently in the group. Membership comes from add_to_group calls and from groups saved in scenes.",
      "Scene", {"scene", "group"}, SideEffect::None, tool_flags::kNone,
      kGetSceneTreeNodesInGroupParams, scene_tree_ops::handle_get_nodes_in_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "is_scene_tree_paused",
      "Check whether the running scene tree is currently paused. Takes no parameters; returns true or false. Pausing stops processing for nodes whose process_mode is not WhenPaused. Use together with set_scene_tree_pause to confirm the state you configured. The boolean is wrapped in a 'result' field.",
      "Scene", {"scene", "pause"}, SideEffect::None, tool_flags::kNone,
      kIsSceneTreePausedParams, scene_tree_ops::handle_is_paused}));
  v.push_back(make_spec_tool(ToolSpec{
      "notify_scene_tree_group",
      "Send a Godot notification to every node in a group. Requires 'group_name' and 'notification' — an integer NOTIFICATION_* constant (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3; the parameter schema lists more). Triggers the matching _notification callback on each node. Returns 'ok' when dispatched; per-node responses are not reported.",
      "Scene", {"scene", "group"}, SideEffect::None, tool_flags::kNone,
      kNotifySceneTreeGroupParams, scene_tree_ops::handle_notify_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_scene_tree_current_scene",
      "Reload the currently running scene from disk. Requires a scene actively running; with no current scene it returns ERR_UNCONFIGURED (3) as the result integer. Reloading recreates the scene root, so runtime state (script state, connections, node modifications) is reset from the saved file.",
      "Scene", {"scene", "reload"}, SideEffect::None, tool_flags::kNone,
      kReloadSceneTreeCurrentSceneParams, scene_tree_ops::handle_reload_current_scene}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_scene_tree_debug_collisions_hint",
      "Toggle collision shape debug visualization while the scene runs. Requires 'enabled' (bool); sets the SceneTree debug collisions hint so collision shapes are drawn during gameplay. The hint only affects debug drawing, not physics behavior. Shapes render only while the game is running. Returns 'ok'.",
      "Scene", {"scene", "debug"}, SideEffect::None, tool_flags::kNone,
      kSetSceneTreeDebugCollisionsHintParams, scene_tree_ops::handle_set_debug_collisions}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_scene_tree_pause",
      "Pause or unpause the running scene tree. Requires 'paused' (bool). Only nodes whose process_mode permits WhenPaused keep processing while paused; process and physics callbacks of other nodes are not called. Query the resulting state with is_scene_tree_paused. Pass 'paused': false to resume the game. Returns 'ok'.",
      "Scene", {"scene", "pause"}, SideEffect::None, tool_flags::kNone,
      kSetSceneTreePauseParams, scene_tree_ops::handle_set_pause}));
  return v;
}

} // namespace scene_tree_tools
} // namespace godot_autopilot

#endif