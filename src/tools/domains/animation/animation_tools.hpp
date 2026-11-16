#ifndef GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP
#define GODOT_AUTOPILOT_ANIMATION_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/animation/animation_ops.hpp"
#include <tools/base/tool_spec.hpp>

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
    {"action", "string", "Operation to run: 'list' (default) returns every animation, 'inspect' reads one animation's tracks and keyframes, 'library' lists or edits the AnimationLibrary containers", false},
    {"anim_name", "string", "inspect: animation name to read, library-qualified as '<library>/<name>' when it lives outside the default library", false},
    {"time", "number", "inspect: sample time in seconds used to interpolate every value track (number, >= 0; default 0.0)", false},
    {"max_tracks", "integer", "inspect: maximum number of tracks returned (integer, default 64; 0 or -1 means unlimited; capped at 4096)", false},
    {"max_keys", "integer", "inspect: maximum number of keys returned per track (integer, default 128; 0 or -1 means unlimited; capped at 4096)", false},
    {"library_action", "string", "library: list|add|remove|rename — list reports every library, add creates an empty library, remove deletes an empty named library, rename renames a named library", false},
    {"library", "string", "library: library name used by add/remove/rename; \"\" is the default library, which add may create but remove and rename reject", false},
    {"new_name", "string", "library with library_action=rename: new library name; must not exist yet", false},
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

const std::vector<ParamSpec> kAnimationRuntimeParams = {
    {"action", "string", "Operation to run: play|pause|stop|seek|play_backwards|set_speed|queue|current|get_parameter|set_parameter", false},
    {"player_path", "string", "Path of the AnimationPlayer node in the edited scene; required by every player action", false},
    {"animation", "string", "play/play_backwards/queue: animation name to play or enqueue; omit to play the assigned animation; unknown names error with the available animations", false},
    {"position", "number", "seek: target position in seconds since animation start (number, >= 0); the returned position is the post-seek value", false},
    {"update", "boolean", "seek: apply the seek immediately to the animated nodes (boolean, default false)", false},
    {"keep_state", "boolean", "stop: keep the current playback state instead of resetting it (boolean, default false)", false},
    {"speed_scale", "number", "set_speed: new playback speed multiplier on the player (number, 1.0 normal, negative reverses)", false},
    {"animation_tree_path", "string", "get_parameter/set_parameter: path of the AnimationTree node whose parameters/... properties are read or written", false},
    {"path", "string", "get_parameter/set_parameter: parameter property path, must start with 'parameters/' (e.g. 'parameters/conditions/is_running')", false},
    {"value", "json", "set_parameter: JSON value written to the parameter (numbers, strings, booleans and vectors as objects); the old value is returned alongside the new one", false},
};

const std::vector<ParamSpec> kAnimationTreeParams = {
    {"action", "string", "Operation to run: create|inspect|add_node|remove_node|rename_node|connect|remove_transition|set_transition", false},
    {"parent_path", "string", "create: parent node path in the edited scene (e.g. 'Root')", false},
    {"name", "string", "create: node name for the new AnimationTree (string, default AnimationTree)", false},
    {"anim_player", "string", "create: scene-relative AnimationPlayer path wired into the tree's animation_player property, e.g. 'Root/AnimationPlayer'", false},
    {"animation_tree_path", "string", "inspect/add_node/remove_node/rename_node/connect/remove_transition/set_transition: path of the AnimationTree in the edited scene", false},
    {"state_name", "string", "add_node/remove_node/rename_node: name of the state machine node, e.g. 'idle'", false},
    {"new_name", "string", "rename_node: new unique state name; '/' and ':' are rejected by the engine, 'Start' and 'End' are reserved", false},
    {"animation", "string", "add_node: animation name playable by the tree's player, assigned to the underlying AnimationNodeAnimation, e.g. 'walk'", false},
    {"from_state", "string", "connect/remove_transition/set_transition: source state name of the transition", false},
    {"to_state", "string", "connect/remove_transition/set_transition: target state name of the transition", false},
    {"condition", "string", "connect: bool parameter name gating the transition, e.g. 'is_running'; sets advance_mode AUTO with that advance_condition", false},
    {"priority", "integer", "connect/set_transition: transition priority (integer, default 0); higher values are evaluated first", false},
    {"switch_mode", "string", "connect/set_transition: switch timing 'immediate'|'sync'|'at_end' (or 0,1,2); default immediate", false},
    {"advance_mode", "string", "connect/set_transition: advance mode 'disabled'|'enabled'|'auto' (or 0,1,2); a non-empty advance_condition defaults it to auto", false},
    {"advance_condition", "string", "set_transition: bool parameter name that gates the transition; pass \"\" to clear it", false},
    {"xfade", "number", "connect/set_transition: cross-fade time in seconds (number, >= 0)", false},
};

const std::vector<std::string> kAnimationRuntimeActions = {
    "play", "pause", "stop", "seek", "play_backwards", "set_speed", "queue",
    "current", "get_parameter", "set_parameter"};

const std::vector<std::string> kAnimationTreeActions = {
    "create", "inspect", "add_node", "remove_node", "rename_node", "connect",
    "remove_transition", "set_transition"};

inline mcp::JsonValue animation_optional_action_schema(
    const std::vector<ParamSpec> &params,
    std::initializer_list<const char *> actions, const char *description) {
  mcp::JsonValue raw = schema::build_schema(params);
  mcp::JsonValue action(mcp::JsonValue::object_tag);
  action["type"] = mcp::JsonValue("string");
  action["description"] = mcp::JsonValue(description);
  mcp::JsonValue values(mcp::JsonValue::array_tag);
  for (const char *value : actions)
    values.PushBack(mcp::JsonValue(value));
  action["enum"] = std::move(values);
  mcp::JsonValue *properties = raw.Find("properties");
  if (properties != nullptr && properties->IsObject())
    (*properties)["action"] = std::move(action);
  return raw;
}

inline mcp::JsonValue animation_alias_args_with_action(
    const mcp::JsonValue &args, const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject())
    out = args;
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline ToolAlias animation_action_alias(const char *name, const char *canonical,
                                        const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return animation_alias_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_animation_player",
      "Create an AnimationPlayer node in the edited scene under 'parent_path' (required string, scene-relative node path, e.g. \"Root/Actors\"). Optional 'name' (string, default AnimationPlayer). Returns the new node 'path' plus an 'undo' hint; follow up with create_animation to add clips.",
      "Animation", {"animation", "player", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneAnimationPlayerParams, animation_ops::handle_create_player}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_animation",
      "Create a new Animation resource inside the default ('') library of the AnimationPlayer at 'player_path' (required string). Required 'name' (string, e.g. \"walk\"); optional 'length_sec' (number, seconds, default 1.0) and 'loop_mode' (int: 0 none, 1 linear loop, 2 pingpong; default 0). Fails if the name already exists — check get_animation_list first. Returns created metadata.",
      "Animation", {"animation", "clip", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateAnimationParams, animation_ops::handle_create_animation}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_animation",
      "Remove the animation named 'name' (required string) from the AnimationPlayer at 'player_path' (required string), searching all of its libraries. Errors if the animation does not exist. Returns 'removed' plus the owning 'library'. This tool is classified as a destructive side effect because it destroys the animation object that already exists inside the host, including all of its tracks and keyframes, and no undo path is provided.",
      "Animation", {"animation", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget,
      kRemoveAnimationParams, animation_ops::handle_remove_animation}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_animation_list",
      "Read animations of the AnimationPlayer at 'player_path' (required string) in one action tool. action='list' (default) returns {player_path, count, animations:[{name,length,loop_mode}]}. action='inspect' returns one animation named 'anim_name' with length, loop_mode, track_count, returned_track_count, truncated, sample time and tracks:[{index,type,type_id,path,key_count,returned_key_count,truncated,keys:[{time,value}],interpolated_value?}]: every value track also reports value_track_interpolate at 'time' (default 0.0), 'max_tracks' (default 64) and 'max_keys' (default 128, per track) cap the payload, and 0 or -1 means unlimited. action='library' manages AnimationLibrary containers through 'library_action': list, add, remove (empty libraries only) or rename; 'library' names the target (\"\" is the default library, which add may create but remove and rename reject) and rename takes 'new_name'. Library results carry library_count and libraries:[{name,default,animation_count,animations}]. list and inspect are read-only; library add/remove/rename are WritesScene. Errors if the node is not an AnimationPlayer.",
      "Animation", {"animation", "list", "inspect", "library", "libraries",
                    "tracks", "keyframes", "keys", "interpolate"},
      SideEffect::WritesScene, tool_flags::kMutating,
      kGetAnimationListParams, animation_ops::handle_get_list,
      animation_optional_action_schema(
          kGetAnimationListParams, {"list", "inspect", "library"},
          "Animation read/edit action: list (default)|inspect|library"),
      {},
      {"{\"action\":\"list\",\"player_path\":\"AnimationPlayer\"}",
       "{\"action\":\"inspect\",\"player_path\":\"AnimationPlayer\",\"anim_name\":\"walk\",\"time\":0.5,\"max_tracks\":16,\"max_keys\":32}",
       "{\"action\":\"library\",\"player_path\":\"AnimationPlayer\",\"library_action\":\"add\",\"library\":\"combat\"}",
       "{\"action\":\"library\",\"player_path\":\"AnimationPlayer\",\"library_action\":\"rename\",\"library\":\"combat\",\"new_name\":\"battle\"}",
       "{\"action\":\"library\",\"player_path\":\"AnimationPlayer\",\"library_action\":\"list\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_animation_track",
      "Add one track to animation 'anim_name' on player 'player_path' (both required strings). Required 'track_type': \"value\" or \"method\". Required 'node_path' (scene-relative target node); for value tracks also pass 'property' (string like \"position:x\" or \"modulate:a\") — the track path becomes <node_path>:<property> and the update mode is continuous; method tracks ignore 'property'. Duplicate track for the same path+type is rejected. Returns the new 'track_index' to use with insert_animation_keyframe.",
      "Animation", {"animation", "track", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateAnimationTrackParams, animation_ops::handle_create_track}));
  v.push_back(make_spec_tool(ToolSpec{
      "insert_animation_keyframe",
      "Insert one key into track 'track_index' (int) of animation 'anim_name' on player 'player_path' at 'time' (number, seconds >= 0) — all required. 'value' (required JSON): deserialized to a Variant for value tracks (e.g. 1.5 or {\"x\":0,\"y\":1,\"z\":2} for a Vector3 property); for method tracks pass an object like {\"method\":\"jump\",\"args\":[42]}. Returns the updated key_count of the track.",
      "Animation", {"animation", "keyframe"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kInsertAnimationKeyframeParams, animation_ops::handle_insert_keyframe}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_animation_track",
      "Remove the whole track 'track_index' (required int) from animation 'anim_name' on player 'player_path' (both required strings). Index must be within [0, track_count), otherwise an error is returned. Returns removed_track_index and removed_track_path. This tool is classified as a destructive side effect because it destroys the track object that already exists inside the host, including all of its keyframes, and no undo path is provided.",
      "Animation", {"animation", "track", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget,
      kRemoveAnimationTrackParams, animation_ops::handle_remove_track}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_animation_track_bundle",
      "Create one track plus all of its keys on animation 'anim_name' of player 'player_path' in a single call (both required strings). Required 'track_type': \"value\" or \"method\". Required 'node_path' (scene-relative target node); for value tracks also pass 'property' (string like \"position:x\") — the track path becomes <node_path>:<property> with continuous update mode, method tracks ignore 'property'. Required 'keys': array of 1-256 {\"time\": <number >= 0>, \"value\": <json>} objects, inserted in ascending time order — value-track values deserialize to Variants, method-track values must be {\"method\":\"name\",\"args\":[...]} objects. Optional 'overwrite' (boolean, default false): false rejects repeated key times, true keeps the last key per repeated time. A duplicate track for the same path+type is rejected. Any key failure removes the new track so the call is all-or-nothing. Returns 'track_index', 'key_count' and sorted 'times'.",
      "Animation", {"animation", "track", "keyframe", "bundle"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetAnimationTrackBundleParams, animation_ops::handle_set_track_bundle}));
  v.push_back(make_spec_tool(ToolSpec{
      "animation_runtime",
      "Drive an AnimationPlayer or read/write AnimationTree parameters at editor time in one action tool; nothing here is saved to the scene file. action='play' plays 'animation' (or the assigned animation when omitted), action='play_backwards' plays it in reverse, action='pause' pauses, action='stop' stops (optional 'keep_state'), action='seek' jumps to 'position' seconds (optional 'update' applies it to the animated nodes immediately), action='set_speed' sets 'speed_scale' and action='queue' enqueues 'animation' after the current one; every player action requires 'player_path' and returns current_animation, position, length, playing, speed_scale and queue (plus animation/queued/old_speed_scale where relevant). action='get_parameter' and action='set_parameter' operate on the AnimationTree at 'animation_tree_path': 'path' must start with 'parameters/' (e.g. parameters/conditions/is_running) and is read and written through Object.get/set, unknown paths error with the available parameters, get returns 'value', set requires 'value' and returns 'old', 'new' and 'changed'. ModifiesWindow state only; use get_animation_list action=inspect and animation_tree action=inspect to verify scene content.",
      "Animation", {"animation", "player", "runtime", "play", "pause", "stop",
                    "seek", "speed", "queue", "parameter", "conditions"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kAnimationRuntimeParams, animation_ops::handle_runtime,
      schema::build_action_schema(
          kAnimationRuntimeActions, kAnimationRuntimeParams,
          "Runtime action: play|pause|stop|seek|play_backwards|set_speed|queue|current|get_parameter|set_parameter"),
      {},
      {"{\"action\":\"play\",\"player_path\":\"AnimationPlayer\",\"animation\":\"walk\"}",
       "{\"action\":\"seek\",\"player_path\":\"AnimationPlayer\",\"position\":0.5,\"update\":true}",
       "{\"action\":\"set_speed\",\"player_path\":\"AnimationPlayer\",\"speed_scale\":2}",
       "{\"action\":\"queue\",\"player_path\":\"AnimationPlayer\",\"animation\":\"run\"}",
       "{\"action\":\"get_parameter\",\"animation_tree_path\":\"AnimationTree\",\"path\":\"parameters/conditions/is_running\"}",
       "{\"action\":\"set_parameter\",\"animation_tree_path\":\"AnimationTree\",\"path\":\"parameters/conditions/is_running\",\"value\":true}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "animation_tree",
      "Create and edit an AnimationTree in the edited scene in one action tool. action='create' adds the node under 'parent_path' with an empty AnimationNodeStateMachine tree_root, optional 'name' (default AnimationTree) and optional 'anim_player' wired into its animation_player property. action='inspect' reports tree_root, animation_player, node_count, nodes:[{name,type,animation?}] and transition_count, transitions:[{index,from,to,priority,switch_mode,switch_mode_name,advance_mode,advance_mode_name,advance_condition,xfade}], all resolved through the AnimationNodeStateMachine tree_root. action='add_node' adds state 'state_name' (optional 'animation' assigned to its AnimationNodeAnimation); action='remove_node' deletes a state and its transitions; action='rename_node' renames 'state_name' to 'new_name' and updates transition endpoints. action='connect' adds a transition 'from_state'->'to_state' with optional 'condition', 'priority', 'switch_mode', 'advance_mode' and 'xfade'. action='set_transition' updates those parameters on an existing transition, where 'advance_condition' \"\" clears the condition and a non-empty condition defaults advance_mode to auto unless 'advance_mode' is given. action='remove_transition' deletes the transition 'from_state'->'to_state'. Every action requires 'animation_tree_path' except create; inspect is read-only, all other actions are WritesScene and mark the scene modified. Unknown states and transitions error with the available candidates. Deprecated aliases: create_scene_animation_tree (action=create), add_animation_machine_state (action=add_node), connect_animation_states (action=connect).",
      "Animation", {"animation", "tree", "state_machine", "create", "inspect",
                    "add_node", "remove_node", "rename_node", "connect",
                    "remove_transition", "set_transition", "transition",
                    "priority", "switch_mode", "advance_mode",
                    "advance_condition", "xfade",
                    "create_scene_animation_tree",
                    "add_animation_machine_state", "connect_animation_states"},
      SideEffect::WritesScene,
      tool_flags::kMutating | tool_flags::kSceneTarget,
      kAnimationTreeParams, animation_ops::handle_tree,
      schema::build_action_schema(
          kAnimationTreeActions, kAnimationTreeParams,
          "Tree action: create|inspect|add_node|remove_node|rename_node|connect|remove_transition|set_transition"),
      {
          animation_action_alias("create_scene_animation_tree", "animation_tree",
                                 "create"),
          animation_action_alias("add_animation_machine_state", "animation_tree",
                                 "add_node"),
          animation_action_alias("connect_animation_states", "animation_tree",
                                 "connect"),
      },
      {"{\"action\":\"create\",\"parent_path\":\"Root\",\"anim_player\":\"Root/AnimationPlayer\"}",
       "{\"action\":\"inspect\",\"animation_tree_path\":\"Root/AnimationTree\"}",
       "{\"action\":\"add_node\",\"animation_tree_path\":\"Root/AnimationTree\",\"state_name\":\"idle\",\"animation\":\"idle\"}",
       "{\"action\":\"connect\",\"animation_tree_path\":\"Root/AnimationTree\",\"from_state\":\"idle\",\"to_state\":\"run\",\"condition\":\"is_running\"}",
       "{\"action\":\"set_transition\",\"animation_tree_path\":\"Root/AnimationTree\",\"from_state\":\"idle\",\"to_state\":\"run\",\"advance_condition\":\"is_running\",\"advance_mode\":\"auto\",\"priority\":2,\"switch_mode\":\"at_end\",\"xfade\":0.2}",
       "{\"action\":\"rename_node\",\"animation_tree_path\":\"Root/AnimationTree\",\"state_name\":\"run\",\"new_name\":\"sprint\"}",
       "{\"action\":\"remove_transition\",\"animation_tree_path\":\"Root/AnimationTree\",\"from_state\":\"idle\",\"to_state\":\"run\"}"}}));
  return v;
}

} // namespace animation_tools
} // namespace godot_autopilot

#endif
