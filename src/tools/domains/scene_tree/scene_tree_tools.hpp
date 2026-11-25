#ifndef GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TREE_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/scene_tree/scene_tree_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace scene_tree_tools {

namespace {

const std::vector<std::string> kGameSceneTreeActions = {
    "call_group", "create_timer", "get_nodes_in_group", "is_paused",
    "notify_group", "reload_current_scene", "set_debug_collisions_hint",
    "set_pause"};

const std::vector<ParamSpec> kGameSceneTreeParams = {
    {"group_name", "string", "call_group/notify_group/get_nodes_in_group: name of the group in the running game (string, e.g. 'enemies')", false},
    {"method", "string", "call_group: method name to call on every group node (string, e.g. 'take_damage')", false},
    {"arguments", "array", "call_group: optional arguments array passed to the method (array, e.g. [10, 'fire'])", false},
    {"delay_sec", "number", "create_timer: timer delay in seconds (number, e.g. 1.5)", false},
    {"process_always", "boolean", "create_timer: process even while the scene tree is paused (boolean, default: true)", false},
    {"process_in_physics", "boolean", "create_timer: count down in the physics step instead of the process step (boolean, default: false)", false},
    {"notification", "integer", "notify_group: notification constant as integer (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3, NOTIFICATION_ENTER_TREE=10, NOTIFICATION_EXIT_TREE=11)", false},
    {"enabled", "boolean", "set_debug_collisions_hint: draw collision shapes while the scene runs (boolean, e.g. true)", false},
    {"paused", "boolean", "set_pause: pause state: true pauses the scene tree, false resumes (boolean)", false},
};

inline godot_autopilot::util::JsonValue scene_tree_alias_args_with_action(
    const godot_autopilot::util::JsonValue &args, const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = godot_autopilot::util::JsonValue(action);
  return out;
}

inline ToolAlias scene_tree_action_alias(const char *name,
                                         const char *canonical,
                                         const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const godot_autopilot::util::JsonValue &args) {
    return scene_tree_alias_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(ToolSpec{
      "game_scene_tree",
      "Act on the running game process's SceneTree over the runtime channel in one action tool — requires a game launched from the editor whose project loads the godot-autopilot extension, and game_runtime authorization; every action needs confirm=true. Unlike editor scene tools, this never touches the editor's own scene tree: without a running game it returns a structured error. action='call_group' calls 'method' with optional 'arguments' on every node of 'group_name' and returns ok when dispatched (call_group collects no return values). action='create_timer' creates a SceneTreeTimer for 'delay_sec' seconds with optional 'process_always' (default true) and 'process_in_physics' (default false) and returns ok with a note that the timer cannot cross processes. action='get_nodes_in_group' lists the absolute node paths of every node in 'group_name' (an empty array means no member). action='is_paused' returns whether the game SceneTree is paused as a wrapped boolean. action='notify_group' sends the integer 'notification' constant to every node in 'group_name'. action='reload_current_scene' reloads the running scene from disk and returns the error code as the result integer (ERR_UNCONFIGURED=3 when no scene runs). action='set_debug_collisions_hint' toggles collision-shape debug drawing with 'enabled'. action='set_pause' pauses or resumes the game with 'paused' (the editor itself is never paused). Deprecated aliases: call_scene_tree_group, create_scene_tree_timer, get_scene_tree_nodes_in_group, is_scene_tree_paused, notify_scene_tree_group, reload_scene_tree_current_scene, set_scene_tree_debug_collisions_hint, set_scene_tree_pause.",
      "Scene",
      {"scene", "group", "timer", "pause", "debug", "reload", "call_group",
       "call_scene_tree_group", "create_scene_tree_timer",
       "get_scene_tree_nodes_in_group", "is_scene_tree_paused",
       "notify_scene_tree_group", "reload_scene_tree_current_scene",
       "set_scene_tree_debug_collisions_hint", "set_scene_tree_pause"},
      SideEffect::GameRuntime, tool_flags::kNone,
      kGameSceneTreeParams, scene_tree_ops::handle_game_scene_tree,
      schema::build_action_schema(
          kGameSceneTreeActions, kGameSceneTreeParams,
          "Game SceneTree action: call_group|create_timer|get_nodes_in_group|is_paused|notify_group|reload_current_scene|set_debug_collisions_hint|set_pause"),
      {
          scene_tree_action_alias("call_scene_tree_group", "game_scene_tree",
                                  "call_group"),
          scene_tree_action_alias("create_scene_tree_timer",
                                  "game_scene_tree", "create_timer"),
          scene_tree_action_alias("get_scene_tree_nodes_in_group",
                                  "game_scene_tree", "get_nodes_in_group"),
          scene_tree_action_alias("is_scene_tree_paused", "game_scene_tree",
                                  "is_paused"),
          scene_tree_action_alias("notify_scene_tree_group", "game_scene_tree",
                                  "notify_group"),
          scene_tree_action_alias("reload_scene_tree_current_scene",
                                  "game_scene_tree", "reload_current_scene"),
          scene_tree_action_alias("set_scene_tree_debug_collisions_hint",
                                  "game_scene_tree",
                                  "set_debug_collisions_hint"),
          scene_tree_action_alias("set_scene_tree_pause", "game_scene_tree",
                                  "set_pause"),
      },
      {"{\"action\":\"call_group\",\"group_name\":\"enemies\",\"method\":\"take_damage\",\"arguments\":[10],\"confirm\":true}",
       "{\"action\":\"get_nodes_in_group\",\"group_name\":\"enemies\",\"confirm\":true}",
       "{\"action\":\"set_pause\",\"paused\":true,\"confirm\":true}"}}));
  return v;
}

} // namespace scene_tree_tools
} // namespace godot_autopilot

#endif
