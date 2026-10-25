#ifndef GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP
#define GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/animation_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace animation_tools {

namespace {

const std::vector<ParamSpec> kCreateSceneAnimationPlayerParams = {
    {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Root' or 'Root/Actors')", true},
    {"name", "string", "Node name for the new AnimationPlayer (string, default: AnimationPlayer)", false},
};

const std::vector<ParamSpec> kCreateAnimationParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene (e.g. 'AnimationPlayer')", true},
    {"name", "string", "Animation name to create inside the default ('') library, e.g. 'walk'; must not already exist", true},
    {"length_sec", "number", "Animation length in seconds (number, default: 1.0; must be > 0)", false},
    {"loop_mode", "integer", "Loop mode: 0 none, 1 linear loop, 2 pingpong (integer, default: 0)", false},
};

const std::vector<ParamSpec> kRemoveAnimationParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
    {"name", "string", "Animation name to remove; searched across all of the player's libraries", true},
};

const std::vector<ParamSpec> kGetAnimationListParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
};

const std::vector<ParamSpec> kCreateAnimationTrackParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
    {"anim_name", "string", "Animation name to add the track to; must already exist", true},
    {"track_type", "string", "Track kind: 'value' (animates a property) or 'method' (invokes a method per key)", true},
    {"node_path", "string", "Scene-relative path of the target node the track drives, e.g. 'Sprite2D'", true},
    {"property", "string", "Property to animate for value tracks, e.g. 'position:x' or 'modulate:a'; required when track_type is 'value', ignored for method tracks — the track path becomes <node_path>:<property>", false},
};

const std::vector<ParamSpec> kInsertAnimationKeyframeParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
    {"anim_name", "string", "Animation name containing the target track", true},
    {"track_index", "integer", "Index of the track within the animation, as returned by create_animation_track", true},
    {"time", "number", "Key time in seconds since animation start (number, >= 0)", true},
    {"value", "json", "Key value as JSON, deserialized to a Variant (e.g. 1.5 or {\"x\":0,\"y\":1}); method tracks require an object like {\"method\":\"jump\",\"args\":[42]}", true},
};

const std::vector<ParamSpec> kRemoveAnimationTrackParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
    {"anim_name", "string", "Animation name containing the track to remove", true},
    {"track_index", "integer", "Index of the track to remove; must be within [0, track_count)", true},
};

const std::vector<ParamSpec> kSetAnimationTrackBundleParams = {
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
    {"anim_name", "string", "Animation name to add the track to; must already exist", true},
    {"track_type", "string", "Track kind: 'value' (animates a property) or 'method' (invokes a method per key)", true},
    {"node_path", "string", "Scene-relative path of the target node the track drives, e.g. 'Sprite2D'", true},
    {"property", "string", "Property to animate for value tracks, e.g. 'position:x' or 'modulate:a'; required when track_type is 'value', ignored for method tracks — the track path becomes <node_path>:<property>", false},
    {"keys", "array", "Array of 1-256 key objects, each {\"time\": <number >= 0>, \"value\": <json>}; keys are inserted in ascending time order — value tracks deserialize each value to a Variant (e.g. 1.5 or {\"x\":0,\"y\":1}), method tracks require each value to be an object like {\"method\":\"jump\",\"args\":[42]}", true},
    {"overwrite", "boolean", "Duplicate key-time policy (default false): false rejects repeated times in keys, true keeps the last key at each repeated time", false},
};

const std::vector<ParamSpec> kCreateSceneAnimationTreeParams = {
    {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Root')", true},
    {"name", "string", "Node name for the new AnimationTree (string, default: AnimationTree)", false},
    {"anim_player", "string", "Scene-relative AnimationPlayer path to wire into the tree's animation_player property, e.g. 'Root/AnimationPlayer'", false},
};

const std::vector<ParamSpec> kAddAnimationMachineStateParams = {
    {"animation_tree_path", "string", "Path of the AnimationTree whose state machine root receives the state", true},
    {"state_name", "string", "Unique state name within the state machine, e.g. 'idle'", true},
    {"animation", "string", "Animation name assigned to the state's AnimationNodeAnimation, e.g. 'walk'", false},
};

const std::vector<ParamSpec> kConnectAnimationStatesParams = {
    {"animation_tree_path", "string", "Path of the AnimationTree owning both states", true},
    {"from_state", "string", "Source state name of the transition", true},
    {"to_state", "string", "Target state name of the transition", true},
    {"condition", "string", "Bool parameter name gating the transition, e.g. 'is_running'; sets advance_mode AUTO with that advance_condition", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(11);
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_animation_player",
      "Create an AnimationPlayer node in the edited scene under 'parent_path' (required string, scene-relative node path, e.g. \"Root/Actors\"). Optional 'name' (string, default AnimationPlayer). Returns the new node 'path' plus an 'undo' hint; follow up with create_animation to add clips.",
      "Animation", {"animation", "player", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneAnimationPlayerParams, animation_ops::handle_create_player}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_animation",
      "Create a new Animation resource inside the default ('') library of the AnimationPlayer at 'player_path' (required string). Required 'name' (string, e.g. \"walk\"); optional 'length_sec' (number, seconds, default 1.0) and 'loop_mode' (int: 0 none, 1 linear loop, 2 pingpong; default 0). Fails if the name already exists — check get_animation_list first. Returns created metadata.",
      "Animation", {"animation", "clip", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateAnimationParams, animation_ops::handle_create_animation}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_animation",
      "Remove the animation named 'name' (required string) from the AnimationPlayer at 'player_path' (required string), searching all of its libraries. Errors if the animation does not exist. Returns 'removed' plus the owning 'library'. This tool is classified as a destructive side effect because it destroys the animation object that already exists inside the host, including all of its tracks and keyframes, and no undo path is provided.",
      "Animation", {"animation", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget,
      kRemoveAnimationParams, animation_ops::handle_remove_animation}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_animation_list",
      "List every animation of the AnimationPlayer at 'player_path' (required string) with per-animation metadata: 'length' in seconds and 'loop_mode' (0 none / 1 linear / 2 pingpong). Read-only; returns {player_path, count, animations:[{name,length,loop_mode}]}. Errors if the node is not an AnimationPlayer.",
      "Animation", {"animation", "list"}, SideEffect::None, tool_flags::kNone,
      kGetAnimationListParams, animation_ops::handle_get_list}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_animation_track",
      "Add one track to animation 'anim_name' on player 'player_path' (both required strings). Required 'track_type': \"value\" or \"method\". Required 'node_path' (scene-relative target node); for value tracks also pass 'property' (string like \"position:x\" or \"modulate:a\") — the track path becomes <node_path>:<property> and the update mode is continuous; method tracks ignore 'property'. Duplicate track for the same path+type is rejected. Returns the new 'track_index' to use with insert_animation_keyframe.",
      "Animation", {"animation", "track", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateAnimationTrackParams, animation_ops::handle_create_track}));
  v.push_back(make_spec_tool(ToolSpec{
      "insert_animation_keyframe",
      "Insert one key into track 'track_index' (int) of animation 'anim_name' on player 'player_path' at 'time' (number, seconds >= 0) — all required. 'value' (required JSON): deserialized to a Variant for value tracks (e.g. 1.5 or {\"x\":0,\"y\":1,\"z\":2} for a Vector3 property); for method tracks pass an object like {\"method\":\"jump\",\"args\":[42]}. Returns the updated key_count of the track.",
      "Animation", {"animation", "keyframe"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kInsertAnimationKeyframeParams, animation_ops::handle_insert_keyframe}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_animation_track",
      "Remove the whole track 'track_index' (required int) from animation 'anim_name' on player 'player_path' (both required strings). Index must be within [0, track_count), otherwise an error is returned. Returns removed_track_index and removed_track_path. This tool is classified as a destructive side effect because it destroys the track object that already exists inside the host, including all of its keyframes, and no undo path is provided.",
      "Animation", {"animation", "track", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget,
      kRemoveAnimationTrackParams, animation_ops::handle_remove_track}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_animation_track_bundle",
      "Create one track plus all of its keys on animation 'anim_name' of player 'player_path' in a single call (both required strings). Required 'track_type': \"value\" or \"method\". Required 'node_path' (scene-relative target node); for value tracks also pass 'property' (string like \"position:x\") — the track path becomes <node_path>:<property> with continuous update mode, method tracks ignore 'property'. Required 'keys': array of 1-256 {\"time\": <number >= 0>, \"value\": <json>} objects, inserted in ascending time order — value-track values deserialize to Variants, method-track values must be {\"method\":\"name\",\"args\":[...]} objects. Optional 'overwrite' (boolean, default false): false rejects repeated key times, true keeps the last key per repeated time. A duplicate track for the same path+type is rejected. Any key failure removes the new track so the call is all-or-nothing. Returns 'track_index', 'key_count' and sorted 'times'.",
      "Animation", {"animation", "track", "keyframe", "bundle"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetAnimationTrackBundleParams, animation_ops::handle_set_track_bundle}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_animation_tree",
      "Create an AnimationTree under required 'parent_path' in the edited scene, with an empty AnimationNodeStateMachine as its tree_root. Optional 'name' (string, default AnimationTree) and optional 'anim_player' (scene-relative AnimationPlayer path, e.g. \"Root/AnimationPlayer\") wired into the tree's animation_player property. Returns 'path', initial 'states' list and a 'next' hint pointing at add_animation_machine_state.",
      "Animation", {"animation", "tree", "state_machine", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneAnimationTreeParams, animation_ops::handle_create_tree}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_animation_machine_state",
      "Append one state to the AnimationNodeStateMachine tree_root of the AnimationTree at 'animation_tree_path' (required string). Required 'state_name' (unique string, e.g. \"idle\"); optional 'animation' (animation name playable by that tree's player, e.g. \"walk\") assigned to the underlying AnimationNodeAnimation. Errors if tree_root is not a state machine or the state name exists. Returns the full 'states' list after insertion.",
      "Animation", {"animation", "state_machine", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kAddAnimationMachineStateParams, animation_ops::handle_add_state}));
  v.push_back(make_spec_tool(ToolSpec{
      "connect_animation_states",
      "Connect two existing states of the state machine rooted in the AnimationTree at 'animation_tree_path' (required string). Required 'from_state' and 'to_state'; optional 'condition' (string naming a bool parameter, e.g. \"is_running\") switches the transition to advance_mode AUTO with that advance_condition. Duplicate transitions are rejected. Use create_scene_animation_tree + add_animation_machine_state first.",
      "Animation", {"animation", "transition", "connect"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kConnectAnimationStatesParams, animation_ops::handle_connect_states}));
  return v;
}

} // namespace animation_tools
} // namespace godot_autopilot

#endif
