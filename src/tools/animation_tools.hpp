#ifndef GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP
#define GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/animation_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace animation_tools {

GDA_TOOL_CLASS(CreateSceneAnimationPlayerTool, "create_scene_animation_player",
               "Create an AnimationPlayer node in the edited scene under 'parent_path' (required string, scene-relative node path, e.g. \"Root/Actors\"). Optional 'name' (string, default AnimationPlayer). Returns the new node 'path' plus an 'undo' hint; follow up with create_animation to add clips.",
               "Animation", std::vector<std::string>({"animation", "player", "create"}), animation_ops::handle_create_player, true)

GDA_TOOL_CLASS(CreateAnimationTool, "create_animation",
               "Create a new Animation resource inside the default ('') library of the AnimationPlayer at 'player_path' (required string). Required 'name' (string, e.g. \"walk\"); optional 'length_sec' (number, seconds, default 1.0) and 'loop_mode' (int: 0 none, 1 linear loop, 2 pingpong; default 0). Fails if the name already exists — check get_animation_list first. Returns created metadata.",
               "Animation", std::vector<std::string>({"animation", "clip", "create"}), animation_ops::handle_create_animation, true)

GDA_TOOL_CLASS(RemoveAnimationTool, "remove_animation",
               "Remove the animation named 'name' (required string) from the AnimationPlayer at 'player_path' (required string), searching all of its libraries. Errors if the animation does not exist. Returns 'removed' plus the owning 'library'.",
               "Animation", std::vector<std::string>({"animation", "delete"}), animation_ops::handle_remove_animation, true)

GDA_TOOL_CLASS(GetAnimationListTool, "get_animation_list",
               "List every animation of the AnimationPlayer at 'player_path' (required string) with per-animation metadata: 'length' in seconds and 'loop_mode' (0 none / 1 linear / 2 pingpong). Read-only; returns {player_path, count, animations:[{name,length,loop_mode}]}. Errors if the node is not an AnimationPlayer.",
               "Animation", std::vector<std::string>({"animation", "list"}), animation_ops::handle_get_list, true)

GDA_TOOL_CLASS(CreateAnimationTrackTool, "create_animation_track",
               "Add one track to animation 'anim_name' on player 'player_path' (both required strings). Required 'track_type': \"value\" or \"method\". Required 'node_path' (scene-relative target node); for value tracks also pass 'property' (string like \"position:x\" or \"modulate:a\") — the track path becomes <node_path>:<property> and the update mode is continuous; method tracks ignore 'property'. Duplicate track for the same path+type is rejected. Returns the new 'track_index' to use with insert_animation_keyframe.",
               "Animation", std::vector<std::string>({"animation", "track", "create"}), animation_ops::handle_create_track, true)

GDA_TOOL_CLASS(InsertAnimationKeyframeTool, "insert_animation_keyframe",
               "Insert one key into track 'track_index' (int) of animation 'anim_name' on player 'player_path' at 'time' (number, seconds >= 0) — all required. 'value' (required JSON): deserialized to a Variant for value tracks (e.g. 1.5 or {\"x\":0,\"y\":1,\"z\":2} for a Vector3 property); for method tracks pass an object like {\"method\":\"jump\",\"args\":[42]}. Returns the updated key_count of the track.",
               "Animation", std::vector<std::string>({"animation", "keyframe"}), animation_ops::handle_insert_keyframe, true)

GDA_TOOL_CLASS(RemoveAnimationTrackTool, "remove_animation_track",
               "Remove the whole track 'track_index' (required int) from animation 'anim_name' on player 'player_path' (both required strings). Index must be within [0, track_count), otherwise an error is returned. Returns removed_track_index and removed_track_path.",
               "Animation", std::vector<std::string>({"animation", "track", "delete"}), animation_ops::handle_remove_track, true)

GDA_TOOL_CLASS(CreateSceneAnimationTreeTool, "create_scene_animation_tree",
               "Create an AnimationTree under required 'parent_path' in the edited scene, with an empty AnimationNodeStateMachine as its tree_root. Optional 'name' (string, default AnimationTree) and optional 'anim_player' (scene-relative AnimationPlayer path, e.g. \"Root/AnimationPlayer\") wired into the tree's animation_player property. Returns 'path', initial 'states' list and a 'next' hint pointing at add_animation_machine_state.",
               "Animation", std::vector<std::string>({"animation", "tree", "state_machine", "create"}), animation_ops::handle_create_tree, true)

GDA_TOOL_CLASS(AddAnimationMachineStateTool, "add_animation_machine_state",
               "Append one state to the AnimationNodeStateMachine tree_root of the AnimationTree at 'animation_tree_path' (required string). Required 'state_name' (unique string, e.g. \"idle\"); optional 'animation' (animation name playable by that tree's player, e.g. \"walk\") assigned to the underlying AnimationNodeAnimation. Errors if tree_root is not a state machine or the state name exists. Returns the full 'states' list after insertion.",
               "Animation", std::vector<std::string>({"animation", "state_machine", "create"}), animation_ops::handle_add_state, true)

GDA_TOOL_CLASS(ConnectAnimationStatesTool, "connect_animation_states",
               "Connect two existing states of the state machine rooted in the AnimationTree at 'animation_tree_path' (required string). Required 'from_state' and 'to_state'; optional 'condition' (string naming a bool parameter, e.g. \"is_running\") switches the transition to advance_mode AUTO with that advance_condition. Duplicate transitions are rejected. Use create_scene_animation_tree + add_animation_machine_state first.",
               "Animation", std::vector<std::string>({"animation", "transition", "connect"}), animation_ops::handle_connect_states, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(std::make_unique<CreateSceneAnimationPlayerTool>());
  v.push_back(std::make_unique<CreateAnimationTool>());
  v.push_back(std::make_unique<RemoveAnimationTool>());
  v.push_back(std::make_unique<GetAnimationListTool>());
  v.push_back(std::make_unique<CreateAnimationTrackTool>());
  v.push_back(std::make_unique<InsertAnimationKeyframeTool>());
  v.push_back(std::make_unique<RemoveAnimationTrackTool>());
  v.push_back(std::make_unique<CreateSceneAnimationTreeTool>());
  v.push_back(std::make_unique<AddAnimationMachineStateTool>());
  v.push_back(std::make_unique<ConnectAnimationStatesTool>());
  return v;
}

} // namespace animation_tools
} // namespace godot_autopilot

#endif
