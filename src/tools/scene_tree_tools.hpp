#ifndef GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/scene_tree_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace scene_tree_tools {

GDA_TOOL_CLASS(CallSceneTreeGroupTool, "call_scene_tree_group",
               "Call a method on every node belonging to a group. Requires 'group_name' and 'method'; optional 'arguments' (array) is passed to the call. Returns 'ok' only — call_group collects no return values, so this confirms the call was dispatched rather than reporting per-node results.",
               "Scene", std::vector<std::string>({"scene", "group"}), scene_tree_ops::handle_call_group, true)

GDA_TOOL_CLASS(CreateSceneTreeTimerTool, "create_scene_tree_timer",
               "Create a SceneTreeTimer in the running scene tree, used with await or a timeout callback for delayed logic. Requires 'delay_sec'; optional 'process_always' (default true — runs while paused) and 'process_in_physics' (default false). Returns the timer as a serialized object reference (ObjectID) for later use.",
               "Scene", std::vector<std::string>({"scene", "timer"}), scene_tree_ops::handle_create_timer, true)

GDA_TOOL_CLASS(GetSceneTreeNodesInGroupTool, "get_scene_tree_nodes_in_group",
               "List every node in a group within the running scene tree. Requires 'group_name'; returns an array of node paths (absolute path of each matching node). An empty array means no node is currently in the group. Membership comes from add_to_group calls and from groups saved in scenes.",
               "Scene", std::vector<std::string>({"scene", "group"}), scene_tree_ops::handle_get_nodes_in_group, true)

GDA_TOOL_CLASS(IsSceneTreePausedTool, "is_scene_tree_paused",
               "Check whether the running scene tree is currently paused. Takes no parameters; returns true or false. Pausing stops processing for nodes whose process_mode is not WhenPaused. Use together with set_scene_tree_pause to confirm the state you configured. The boolean is wrapped in a 'result' field.",
               "Scene", std::vector<std::string>({"scene", "pause"}), scene_tree_ops::handle_is_paused, true)

GDA_TOOL_CLASS(NotifySceneTreeGroupTool, "notify_scene_tree_group",
               "Send a Godot notification to every node in a group. Requires 'group_name' and 'notification' — an integer NOTIFICATION_* constant (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3; the parameter schema lists more). Triggers the matching _notification callback on each node. Returns 'ok' when dispatched; per-node responses are not reported.",
               "Scene", std::vector<std::string>({"scene", "group"}), scene_tree_ops::handle_notify_group, true)

GDA_TOOL_CLASS(ReloadSceneTreeCurrentSceneTool, "reload_scene_tree_current_scene",
               "Reload the currently running scene from disk. Requires a scene actively running; with no current scene it returns ERR_UNCONFIGURED (3) as the result integer. Reloading recreates the scene root, so runtime state (script state, connections, node modifications) is reset from the saved file.",
               "Scene", std::vector<std::string>({"scene", "reload"}), scene_tree_ops::handle_reload_current_scene, true)

GDA_TOOL_CLASS(SetSceneTreeDebugCollisionsHintTool, "set_scene_tree_debug_collisions_hint",
               "Toggle collision shape debug visualization while the scene runs. Requires 'enabled' (bool); sets the SceneTree debug collisions hint so collision shapes are drawn during gameplay. The hint only affects debug drawing, not physics behavior. Shapes render only while the game is running. Returns 'ok'.",
               "Scene", std::vector<std::string>({"scene", "debug"}), scene_tree_ops::handle_set_debug_collisions, true)

GDA_TOOL_CLASS(SetSceneTreePauseTool, "set_scene_tree_pause",
               "Pause or unpause the running scene tree. Requires 'paused' (bool). Only nodes whose process_mode permits WhenPaused keep processing while paused; process and physics callbacks of other nodes are not called. Query the resulting state with is_scene_tree_paused. Pass 'paused': false to resume the game. Returns 'ok'.",
               "Scene", std::vector<std::string>({"scene", "pause"}), scene_tree_ops::handle_set_pause, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(std::make_unique<CallSceneTreeGroupTool>());
  v.push_back(std::make_unique<CreateSceneTreeTimerTool>());
  v.push_back(std::make_unique<GetSceneTreeNodesInGroupTool>());
  v.push_back(std::make_unique<IsSceneTreePausedTool>());
  v.push_back(std::make_unique<NotifySceneTreeGroupTool>());
  v.push_back(std::make_unique<ReloadSceneTreeCurrentSceneTool>());
  v.push_back(std::make_unique<SetSceneTreeDebugCollisionsHintTool>());
  v.push_back(std::make_unique<SetSceneTreePauseTool>());
  return v;
}

} // namespace scene_tree_tools
} // namespace godot_autopilot

#endif