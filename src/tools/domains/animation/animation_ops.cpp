#include "tools/domains/animation/animation_ops.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "core/scene_dirty_tracker.hpp"
#include "util/error_util.hpp"
#include "util/input_guards.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/animation_mixer.hpp>
#include <godot_cpp/classes/animation_node_animation.hpp>
#include <godot_cpp/classes/animation_node_state_machine.hpp>
#include <godot_cpp/classes/animation_node_state_machine_transition.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/animation_tree.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>

namespace godot_autopilot {
namespace animation_ops {

namespace {

constexpr double DEFAULT_ANIMATION_LENGTH = 1.0;
constexpr int64_t LOOP_MODE_MAX = 2;
constexpr const char *DEFAULT_PLAYER_NAME = "AnimationPlayer";
constexpr const char *DEFAULT_TREE_NAME = "AnimationTree";
constexpr const char *PARAMETER_PREFIX = "parameters/";
constexpr int64_t DEFAULT_MAX_TRACKS = 64;
constexpr int64_t DEFAULT_MAX_KEYS = 128;
constexpr int64_t MAX_TRACKS_LIMIT = 4096;
constexpr int64_t MAX_KEYS_LIMIT = 4096;
constexpr std::size_t CANDIDATE_LIMIT = 8;

bool read_string_arg(const mcp::JsonValue &args, const char *key,
                     std::string &out) {
  auto *v = args.Find(key);
  if (v && v->IsString()) {
    out = v->GetString();
    return true;
  }
  return false;
}

bool require_string_arg(const mcp::JsonValue &args, const char *key,
                        std::string &out) {
  return util::require_nonempty_string_arg(args, key, out);
}

double read_number_arg(const mcp::JsonValue &args, const char *key,
                       double fallback) {
  auto *v = args.Find(key);
  if (!v || !v->IsNumber())
    return fallback;
  return v->IsInt() ? static_cast<double>(v->GetInt()) : v->GetDouble();
}

std::string relative_scene_path(godot::Node *node, godot::Node *scene_root) {
  std::string abs = util::to_std(node->get_path());
  if (!scene_root) {
    auto *parent = node->get_parent();
    if (parent) {
      std::string abs_parent = util::to_std(parent->get_path());
      if (abs.find(abs_parent + "/") == 0)
        return abs.substr(abs_parent.size() + 1);
    }
    return abs;
  }
  std::string root_pref = util::to_std(scene_root->get_path());
  if (abs == root_pref)
    return util::to_std(node->get_name());
  if (abs.find(root_pref + "/") == 0)
    return abs.substr(root_pref.size() + 1);
  return abs;
}

godot::AnimationPlayer *resolve_player(const std::string &player_path,
                                       std::string &msg) {
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(player_path, scene_root, &hint);
  if (!node) {
    msg = "node not found: " + player_path + " — " + hint;
    return nullptr;
  }
  auto *player = godot::Object::cast_to<godot::AnimationPlayer>(node);
  if (!player) {
    msg = "node is not an AnimationPlayer: " + player_path + " (found " +
          util::to_std(node->get_class()) + ")";
    return nullptr;
  }
  return player;
}

godot::AnimationTree *resolve_tree(const std::string &tree_path,
                                   std::string &msg) {
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  godot::Node *node =
      util::resolve_scene_node(tree_path, scene_root, &hint);
  if (!node) {
    msg = "node not found: " + tree_path + " — " + hint;
    return nullptr;
  }
  auto *tree = godot::Object::cast_to<godot::AnimationTree>(node);
  if (!tree) {
    msg = "node is not an AnimationTree: " + tree_path + " (found " +
          util::to_std(node->get_class()) + ")";
    return nullptr;
  }
  return tree;
}

godot::AnimationNodeStateMachine *
resolve_state_machine(godot::AnimationTree *tree) {
  return godot::Object::cast_to<godot::AnimationNodeStateMachine>(
      tree->get_tree_root().ptr());
}

godot::Ref<godot::AnimationLibrary>
ensure_default_library(godot::AnimationMixer *mixer, std::string &msg) {
  godot::StringName default_lib("");
  if (mixer->has_animation_library(default_lib))
    return mixer->get_animation_library(default_lib);
  godot::Ref<godot::AnimationLibrary> lib;
  lib.instantiate();
  if (mixer->add_animation_library(default_lib, lib) != godot::Error::OK) {
    msg = "failed to create the default animation library on node " +
          util::to_std(godot::String(mixer->get_path()));
    return godot::Ref<godot::AnimationLibrary>();
  }
  return lib;
}

bool read_int_arg(const mcp::JsonValue &args, const char *key, int64_t &out) {
  auto *v = args.Find(key);
  if (!v)
    return false;
  if (v->IsInt()) {
    out = v->GetInt();
    return true;
  }
  if (v->IsDouble()) {
    out = static_cast<int64_t>(v->GetDouble());
    return true;
  }
  return false;
}

std::string join_limited(const std::vector<std::string> &names) {
  std::string out;
  for (std::size_t i = 0; i < names.size() && i < CANDIDATE_LIMIT; i++) {
    if (!out.empty())
      out += ", ";
    out += names[i];
  }
  if (names.size() > CANDIDATE_LIMIT)
    out += ", ...";
  return out;
}

mcp::JsonValue names_json(const std::vector<std::string> &names) {
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (std::size_t i = 0; i < names.size() && i < CANDIDATE_LIMIT; i++)
    arr.PushBack(mcp::JsonValue(names[i]));
  return arr;
}

std::vector<std::string> animation_names(godot::AnimationPlayer *player) {
  std::vector<std::string> names;
  godot::PackedStringArray list = player->get_animation_list();
  for (int i = 0; i < list.size(); i++)
    names.push_back(util::to_std(list[i]));
  return names;
}

std::string unknown_animation_hint(godot::AnimationPlayer *player) {
  std::vector<std::string> names = animation_names(player);
  if (names.empty())
    return " — the player has no animations yet (create_animation first)";
  return " — available animations: " + join_limited(names);
}

std::string track_type_name(godot::Animation::TrackType type) {
  switch (type) {
    case godot::Animation::TYPE_VALUE:
      return "value";
    case godot::Animation::TYPE_POSITION_3D:
      return "position_3d";
    case godot::Animation::TYPE_ROTATION_3D:
      return "rotation_3d";
    case godot::Animation::TYPE_SCALE_3D:
      return "scale_3d";
    case godot::Animation::TYPE_BLEND_SHAPE:
      return "blend_shape";
    case godot::Animation::TYPE_METHOD:
      return "method";
    case godot::Animation::TYPE_BEZIER:
      return "bezier";
    case godot::Animation::TYPE_AUDIO:
      return "audio";
    case godot::Animation::TYPE_ANIMATION:
      return "animation";
  }
  return "unknown";
}

mcp::JsonValue parameter_value_json(const godot::Variant &value) {
  if (value.get_type() == godot::Variant::OBJECT) {
    godot::Object *object = value.operator godot::Object *();
    mcp::JsonValue out(mcp::JsonValue::object_tag);
    out["type"] = mcp::JsonValue("object");
    if (object) {
      out["class"] = mcp::JsonValue(util::to_std(object->get_class()));
      out["object_id"] =
          mcp::JsonValue(static_cast<int64_t>(object->get_instance_id()));
    } else {
      out["class"] = mcp::JsonValue();
    }
    return out;
  }
  return VariantJson::serialize(value);
}

mcp::JsonValue player_state(godot::AnimationPlayer *player) {
  mcp::JsonValue state(mcp::JsonValue::object_tag);
  state["current_animation"] = mcp::JsonValue(
      util::to_std(godot::String(player->get_current_animation())));
  state["position"] = mcp::JsonValue(player->get_current_animation_position());
  state["length"] = mcp::JsonValue(player->get_current_animation_length());
  state["playing"] = mcp::JsonValue(player->is_playing());
  state["speed_scale"] =
      mcp::JsonValue(static_cast<double>(player->get_speed_scale()));
  mcp::JsonValue queue(mcp::JsonValue::array_tag);
  godot::TypedArray<godot::StringName> queued = player->get_queue();
  for (int i = 0; i < queued.size(); i++)
    queue.PushBack(mcp::JsonValue(util::to_std(godot::String(queued[i]))));
  state["queue"] = std::move(queue);
  return state;
}

mcp::JsonValue player_result(const std::string &player_path,
                             const std::string &action,
                             godot::AnimationPlayer *player) {
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["action"] = mcp::JsonValue(action);
  mcp::JsonValue state = player_state(player);
  for (const auto &entry : state.GetObject())
    inner[entry.first] = entry.second;
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

std::vector<std::string> state_names(
    godot::AnimationNodeStateMachine *state_machine) {
  std::vector<std::string> names;
  godot::TypedArray<godot::StringName> list = state_machine->get_node_list();
  for (int i = 0; i < list.size(); i++)
    names.push_back(util::to_std(godot::String(list[i])));
  return names;
}

std::string unknown_state_hint(godot::AnimationNodeStateMachine *state_machine) {
  std::vector<std::string> names = state_names(state_machine);
  if (names.empty())
    return " — the state machine has no states yet (animation_tree action="
           "add_node first)";
  return " — available states: " + join_limited(names);
}

std::string transition_label(
    godot::AnimationNodeStateMachine *state_machine, int32_t index) {
  return util::to_std(godot::String(
             state_machine->get_transition_from(index))) +
         " -> " +
         util::to_std(godot::String(state_machine->get_transition_to(index)));
}

std::vector<std::string> transition_names(
    godot::AnimationNodeStateMachine *state_machine) {
  std::vector<std::string> names;
  for (int32_t i = 0; i < state_machine->get_transition_count(); i++)
    names.push_back(transition_label(state_machine, i));
  return names;
}

std::string switch_mode_name(int32_t mode) {
  switch (mode) {
    case godot::AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE:
      return "immediate";
    case godot::AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC:
      return "sync";
    case godot::AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END:
      return "at_end";
  }
  return "unknown";
}

std::string advance_mode_name(int32_t mode) {
  switch (mode) {
    case godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED:
      return "disabled";
    case godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_ENABLED:
      return "enabled";
    case godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO:
      return "auto";
  }
  return "unknown";
}

mcp::JsonValue transition_json(
    godot::AnimationNodeStateMachine *state_machine, int32_t index) {
  mcp::JsonValue item(mcp::JsonValue::object_tag);
  godot::Ref<godot::AnimationNodeStateMachineTransition> transition =
      state_machine->get_transition(index);
  if (transition.is_null())
    return item;
  item["index"] = mcp::JsonValue(static_cast<int64_t>(index));
  item["from"] = mcp::JsonValue(
      util::to_std(godot::String(state_machine->get_transition_from(index))));
  item["to"] = mcp::JsonValue(
      util::to_std(godot::String(state_machine->get_transition_to(index))));
  item["priority"] =
      mcp::JsonValue(static_cast<int64_t>(transition->get_priority()));
  int32_t switch_mode = static_cast<int32_t>(transition->get_switch_mode());
  item["switch_mode"] = mcp::JsonValue(static_cast<int64_t>(switch_mode));
  item["switch_mode_name"] = mcp::JsonValue(switch_mode_name(switch_mode));
  int32_t advance_mode = static_cast<int32_t>(transition->get_advance_mode());
  item["advance_mode"] = mcp::JsonValue(static_cast<int64_t>(advance_mode));
  item["advance_mode_name"] = mcp::JsonValue(advance_mode_name(advance_mode));
  item["advance_condition"] = mcp::JsonValue(util::to_std(godot::String(
      transition->get_advance_condition())));
  item["xfade"] =
      mcp::JsonValue(static_cast<double>(transition->get_xfade_time()));
  return item;
}

mcp::JsonValue transitions_json(
    godot::AnimationNodeStateMachine *state_machine) {
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (int32_t i = 0; i < state_machine->get_transition_count(); i++)
    arr.PushBack(transition_json(state_machine, i));
  return arr;
}

mcp::JsonValue states_json(godot::AnimationNodeStateMachine *state_machine) {
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (const std::string &name : state_names(state_machine))
    arr.PushBack(mcp::JsonValue(name));
  return arr;
}

mcp::JsonValue library_snapshot(godot::AnimationMixer *mixer) {
  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  godot::TypedArray<godot::StringName> libs =
      mixer->get_animation_library_list();
  for (int i = 0; i < libs.size(); i++) {
    godot::StringName lib_name = libs[i];
    godot::Ref<godot::AnimationLibrary> lib =
        mixer->get_animation_library(lib_name);
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    std::string name = util::to_std(godot::String(lib_name));
    item["name"] = mcp::JsonValue(name);
    item["default"] = mcp::JsonValue(name.empty());
    mcp::JsonValue animations(mcp::JsonValue::array_tag);
    int64_t count = 0;
    if (lib.is_valid()) {
      godot::TypedArray<godot::StringName> names = lib->get_animation_list();
      for (int k = 0; k < names.size(); k++) {
        animations.PushBack(
            mcp::JsonValue(util::to_std(godot::String(names[k]))));
        count++;
      }
    }
    item["animation_count"] = mcp::JsonValue(count);
    item["animations"] = std::move(animations);
    arr.PushBack(std::move(item));
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["library_count"] =
      mcp::JsonValue(static_cast<int64_t>(libs.size()));
  out["libraries"] = std::move(arr);
  return out;
}

std::vector<std::string> parameter_names(godot::Object *object) {
  std::vector<std::string> names;
  godot::TypedArray<godot::Dictionary> props = object->get_property_list();
  for (int i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name"))
      continue;
    std::string name =
        util::to_std(dict["name"].operator godot::String());
    if (name.rfind(PARAMETER_PREFIX, 0) == 0)
      names.push_back(name);
  }
  return names;
}

bool resolve_tree_and_machine(const mcp::JsonValue &args,
                              std::string &tree_path,
                              godot::AnimationTree *&tree,
                              godot::AnimationNodeStateMachine *&state_machine,
                              mcp::JsonValue &error) {
  if (!require_string_arg(args, "animation_tree_path", tree_path)) {
    error = util::error_json("missing required parameter: animation_tree_path");
    return false;
  }
  std::string msg;
  tree = resolve_tree(tree_path, msg);
  if (!tree) {
    error = util::error_json(msg);
    return false;
  }
  state_machine = resolve_state_machine(tree);
  if (!state_machine) {
    error = util::error_json(
        "tree_root of " + tree_path +
        " is not an AnimationNodeStateMachine — recreate the tree with "
        "animation_tree action=create");
    return false;
  }
  return true;
}

bool parse_switch_mode(const mcp::JsonValue &value, int32_t &out) {
  if (value.IsInt()) {
    int64_t mode = value.GetInt();
    if (mode >= 0 && mode <= 2) {
      out = static_cast<int32_t>(mode);
      return true;
    }
    return false;
  }
  if (!value.IsString())
    return false;
  const std::string text = value.GetString();
  if (text == "immediate" || text == "0") {
    out = godot::AnimationNodeStateMachineTransition::SWITCH_MODE_IMMEDIATE;
    return true;
  }
  if (text == "sync" || text == "1") {
    out = godot::AnimationNodeStateMachineTransition::SWITCH_MODE_SYNC;
    return true;
  }
  if (text == "at_end" || text == "2") {
    out = godot::AnimationNodeStateMachineTransition::SWITCH_MODE_AT_END;
    return true;
  }
  return false;
}

bool parse_advance_mode(const mcp::JsonValue &value, int32_t &out) {
  if (value.IsInt()) {
    int64_t mode = value.GetInt();
    if (mode >= 0 && mode <= 2) {
      out = static_cast<int32_t>(mode);
      return true;
    }
    return false;
  }
  if (!value.IsString())
    return false;
  const std::string text = value.GetString();
  if (text == "disabled" || text == "0") {
    out = godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_DISABLED;
    return true;
  }
  if (text == "enabled" || text == "1") {
    out = godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_ENABLED;
    return true;
  }
  if (text == "auto" || text == "2") {
    out = godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO;
    return true;
  }
  return false;
}

} // namespace

mcp::JsonValue handle_create_player(const mcp::JsonValue &args) {
  std::string parent_path;
  if (!require_string_arg(args, "parent_path", parent_path))
    return util::error_json("missing required parameter: parent_path");

  std::string name = DEFAULT_PLAYER_NAME;
  read_string_arg(args, "name", name);

  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root) {
    return util::error_json(
        "no scene currently open in the editor — open or create a scene first");
  }

  std::string hint;
  godot::Node *parent =
      util::resolve_scene_node(parent_path, scene_root, &hint);
  if (!parent)
    return util::error_json("parent node not found: " + parent_path + " — " + hint);

  godot::AnimationPlayer *player = memnew(godot::AnimationPlayer);
  player->set_name(godot::StringName(name.c_str()));
  parent->add_child(player);
  player->set_owner(scene_root);

  std::string rel = relative_scene_path(player, scene_root);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(rel);
  inner["type"] = mcp::JsonValue("AnimationPlayer");
  inner["undo"] = mcp::JsonValue("delete node " + rel +
                                 " (delete_scene_node)");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_create_animation(const mcp::JsonValue &args) {
  std::string player_path;
  std::string name;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "name", name))
    return util::error_json("missing required parameter: name");

  double length_sec =
      read_number_arg(args, "length_sec", DEFAULT_ANIMATION_LENGTH);
  if (length_sec <= 0.0)
    return util::error_json("length_sec must be > 0 (got " +
                      std::to_string(length_sec) + ")");

  int64_t loop_mode =
      static_cast<int64_t>(read_number_arg(args, "loop_mode", 0.0));
  if (loop_mode < 0 || loop_mode > LOOP_MODE_MAX)
    return util::error_json("loop_mode must be 0 (none), 1 (linear) or 2 "
                      "(pingpong), got " +
                      std::to_string(loop_mode));

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_name(name.c_str());
  if (player->has_animation(anim_name))
    return util::error_json("animation already exists on " + player_path + ": " +
                      name);

  godot::Ref<godot::AnimationLibrary> lib = ensure_default_library(player, msg);
  if (lib.is_null())
    return util::error_json(msg);

  godot::Ref<godot::Animation> anim;
  anim.instantiate();
  anim->set_length(static_cast<float>(length_sec));
  anim->set_loop_mode(static_cast<godot::Animation::LoopMode>(loop_mode));

  godot::Error err = lib->add_animation(anim_name, anim);
  if (err != godot::Error::OK)
    return util::error_json("failed to add animation \"" + name +
                      "\" to the default library (Error " +
                      std::to_string(static_cast<int>(err)) + ")");

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["name"] = mcp::JsonValue(name);
  inner["length"] = mcp::JsonValue(length_sec);
  inner["loop_mode"] = mcp::JsonValue(loop_mode);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_remove_animation(const mcp::JsonValue &args) {
  std::string player_path;
  std::string name;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "name", name))
    return util::error_json("missing required parameter: name");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_name(name.c_str());
  if (!player->has_animation(anim_name))
    return util::error_json("animation not found on " + player_path + ": " + name);

  godot::TypedArray<godot::StringName> libs =
      player->get_animation_library_list();
  for (int i = 0; i < libs.size(); i++) {
    godot::StringName lib_name = libs[i];
    godot::Ref<godot::AnimationLibrary> lib =
        player->get_animation_library(lib_name);
    if (lib.is_valid() && lib->has_animation(anim_name)) {
      lib->remove_animation(anim_name);
      mcp::JsonValue inner(mcp::JsonValue::object_tag);
      inner["removed"] = mcp::JsonValue(name);
      inner["library"] = mcp::JsonValue(util::to_std(godot::String(lib_name)));
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = std::move(inner);
      scene_dirty_tracker::mark_scene_modified();
      return r;
    }
  }
  return util::error_json("animation not found in any library: " + name);
}

mcp::JsonValue handle_get_list(const mcp::JsonValue &args) {
  std::string player_path;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  std::string action = "list";
  auto *action_p = args.Find("action");
  if (action_p) {
    if (!action_p->IsString() || action_p->GetString().empty())
      return util::error_json(
          "action must be a non-empty string: list, inspect or library");
    action = action_p->GetString();
  }

  if (action == "list") {
    mcp::JsonValue list(mcp::JsonValue::array_tag);
    godot::PackedStringArray names = player->get_animation_list();
    for (int i = 0; i < names.size(); i++) {
      godot::String n = names[i];
      godot::Ref<godot::Animation> anim =
          player->get_animation(godot::StringName(n));
      if (anim.is_null())
        continue;
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["name"] = mcp::JsonValue(util::to_std(n));
      item["length"] = mcp::JsonValue(static_cast<double>(anim->get_length()));
      item["loop_mode"] = mcp::JsonValue(
          static_cast<int64_t>(anim->get_loop_mode()));
      list.PushBack(std::move(item));
    }

    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["player_path"] = mcp::JsonValue(player_path);
    inner["count"] = mcp::JsonValue(static_cast<int64_t>(names.size()));
    inner["animations"] = std::move(list);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    return r;
  }

  if (action == "inspect") {
    std::string anim_name;
    if (!require_string_arg(args, "anim_name", anim_name))
      return util::error_json(
          "missing required parameter for action 'inspect': anim_name");
    godot::StringName anim_sn(anim_name.c_str());
    if (!player->has_animation(anim_sn))
      return util::error_json("animation not found on " + player_path + ": " +
                        anim_name + unknown_animation_hint(player));
    godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
    if (anim.is_null())
      return util::error_json("animation is not readable on " + player_path + ": " +
                        anim_name);

    int64_t max_tracks = DEFAULT_MAX_TRACKS;
    if (args.Find("max_tracks") != nullptr) {
      if (!read_int_arg(args, "max_tracks", max_tracks))
        return util::error_json("max_tracks must be an integer (0 or -1 means "
                          "unlimited)");
      if (max_tracks < -1 || max_tracks > MAX_TRACKS_LIMIT)
        return util::error_json("max_tracks must be between -1 and " +
                          std::to_string(MAX_TRACKS_LIMIT) + " (0 or -1 means "
                          "unlimited)");
      if (max_tracks == 0)
        max_tracks = -1;
    }
    int64_t max_keys = DEFAULT_MAX_KEYS;
    if (args.Find("max_keys") != nullptr) {
      if (!read_int_arg(args, "max_keys", max_keys))
        return util::error_json("max_keys must be an integer (0 or -1 means "
                          "unlimited, applies per track)");
      if (max_keys < -1 || max_keys > MAX_KEYS_LIMIT)
        return util::error_json("max_keys must be between -1 and " +
                          std::to_string(MAX_KEYS_LIMIT) + " (0 or -1 means "
                          "unlimited)");
      if (max_keys == 0)
        max_keys = -1;
    }
    double time = read_number_arg(args, "time", 0.0);
    if (time < 0.0)
      return util::error_json("time must be >= 0 (got " + std::to_string(time) + ")");

    int64_t track_count = anim->get_track_count();
    int64_t returned_tracks =
        max_tracks < 0 ? track_count : std::min(track_count, max_tracks);
    bool truncated = returned_tracks < track_count;
    mcp::JsonValue tracks(mcp::JsonValue::array_tag);
    for (int64_t i = 0; i < returned_tracks; i++) {
      int32_t index = static_cast<int32_t>(i);
      godot::Animation::TrackType track_type = anim->track_get_type(index);
      mcp::JsonValue item(mcp::JsonValue::object_tag);
      item["index"] = mcp::JsonValue(i);
      item["type"] = mcp::JsonValue(track_type_name(track_type));
      item["type_id"] = mcp::JsonValue(static_cast<int64_t>(track_type));
      item["path"] = mcp::JsonValue(
          util::to_std(godot::String(anim->track_get_path(index))));
      int64_t key_count = anim->track_get_key_count(index);
      int64_t returned_keys =
          max_keys < 0 ? key_count : std::min(key_count, max_keys);
      mcp::JsonValue keys(mcp::JsonValue::array_tag);
      for (int64_t k = 0; k < returned_keys; k++) {
        int32_t key_index = static_cast<int32_t>(k);
        mcp::JsonValue key(mcp::JsonValue::object_tag);
        key["time"] = mcp::JsonValue(anim->track_get_key_time(index, key_index));
        key["value"] =
            VariantJson::serialize(anim->track_get_key_value(index, key_index));
        keys.PushBack(std::move(key));
      }
      item["key_count"] = mcp::JsonValue(key_count);
      item["returned_key_count"] = mcp::JsonValue(returned_keys);
      item["truncated"] = mcp::JsonValue(returned_keys < key_count);
      item["keys"] = std::move(keys);
      if (track_type == godot::Animation::TYPE_VALUE) {
        item["interpolated_value"] = VariantJson::serialize(
            anim->value_track_interpolate(index, time));
      }
      tracks.PushBack(std::move(item));
    }

    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["player_path"] = mcp::JsonValue(player_path);
    inner["anim_name"] = mcp::JsonValue(anim_name);
    inner["length"] = mcp::JsonValue(static_cast<double>(anim->get_length()));
    inner["loop_mode"] =
        mcp::JsonValue(static_cast<int64_t>(anim->get_loop_mode()));
    inner["time"] = mcp::JsonValue(time);
    inner["track_count"] = mcp::JsonValue(track_count);
    inner["returned_track_count"] = mcp::JsonValue(returned_tracks);
    inner["truncated"] = mcp::JsonValue(truncated);
    inner["tracks"] = std::move(tracks);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    return r;
  }

  if (action == "library") {
    auto *op_p = args.Find("library_action");
    if (!op_p || !op_p->IsString() || op_p->GetString().empty())
      return util::error_json(
          "missing required parameter for action 'library': library_action "
          "(list, add, remove or rename)");
    const std::string op = op_p->GetString();
    const bool has_library = args.Find("library") != nullptr;
    std::string library;
    if (has_library && !read_string_arg(args, "library", library))
      return util::error_json("library must be a string (\"\" is the default "
                        "library)");

    if (op == "list") {
      mcp::JsonValue inner = library_snapshot(player);
      inner["player_path"] = mcp::JsonValue(player_path);
      inner["action"] = mcp::JsonValue("library");
      inner["library_action"] = mcp::JsonValue(op);
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = std::move(inner);
      return r;
    }

    if (op != "add" && op != "remove" && op != "rename")
      return util::error_json("unknown library_action '" + op +
                        "' — expected list, add, remove or rename");

    if (!has_library)
      return util::error_json("missing required parameter for library_action '" +
                        op + "': library (\"\" is the default library)");

    godot::StringName lib_sn(library.c_str());
    if (op == "add") {
      if (player->has_animation_library(lib_sn))
        return util::error_json("animation library already exists on " + player_path +
                          ": \"" + library + "\"");
      godot::Ref<godot::AnimationLibrary> lib;
      lib.instantiate();
      godot::Error err = player->add_animation_library(lib_sn, lib);
      if (err != godot::Error::OK)
        return util::error_json("failed to add animation library \"" + library +
                          "\" (Error " +
                          std::to_string(static_cast<int>(err)) + ")");
      mcp::JsonValue inner = library_snapshot(player);
      inner["player_path"] = mcp::JsonValue(player_path);
      inner["action"] = mcp::JsonValue("library");
      inner["library_action"] = mcp::JsonValue(op);
      inner["library"] = mcp::JsonValue(library);
      inner["added"] = mcp::JsonValue(true);
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = std::move(inner);
      scene_dirty_tracker::mark_scene_modified();
      return r;
    }

    if (library.empty())
      return util::error_json("the default library (\"\") cannot be removed or "
                        "renamed — operate on a named library, or remove "
                        "individual animations with remove_animation");
    if (!player->has_animation_library(lib_sn))
      return util::error_json("animation library not found on " + player_path +
                        ": \"" + library + "\"");

    if (op == "remove") {
      godot::Ref<godot::AnimationLibrary> lib =
          player->get_animation_library(lib_sn);
      int64_t count =
          lib.is_valid() ? lib->get_animation_list().size() : 0;
      if (count > 0)
        return util::error_json("animation library \"" + library + "\" still holds " +
                          std::to_string(count) +
                          " animations — remove them with remove_animation "
                          "first (that tool requires confirm)");
      player->remove_animation_library(lib_sn);
      mcp::JsonValue inner = library_snapshot(player);
      inner["player_path"] = mcp::JsonValue(player_path);
      inner["action"] = mcp::JsonValue("library");
      inner["library_action"] = mcp::JsonValue(op);
      inner["library"] = mcp::JsonValue(library);
      inner["removed"] = mcp::JsonValue(true);
      mcp::JsonValue r(mcp::JsonValue::object_tag);
      r["result"] = std::move(inner);
      scene_dirty_tracker::mark_scene_modified();
      return r;
    }

    std::string new_name;
    if (!require_string_arg(args, "new_name", new_name))
      return util::error_json(
          "missing required parameter for library_action 'rename': new_name");
    godot::StringName new_sn(new_name.c_str());
    if (player->has_animation_library(new_sn))
      return util::error_json("animation library already exists on " + player_path +
                        ": \"" + new_name + "\"");
    player->rename_animation_library(lib_sn, new_sn);
    mcp::JsonValue inner = library_snapshot(player);
    inner["player_path"] = mcp::JsonValue(player_path);
    inner["action"] = mcp::JsonValue("library");
    inner["library_action"] = mcp::JsonValue(op);
    inner["old_name"] = mcp::JsonValue(library);
    inner["new_name"] = mcp::JsonValue(new_name);
    inner["renamed"] = mcp::JsonValue(true);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    scene_dirty_tracker::mark_scene_modified();
    return r;
  }

  return util::error_json("unknown action '" + action +
                    "' for get_animation_list — expected list, inspect or "
                    "library");
}

mcp::JsonValue handle_create_track(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  std::string track_type;
  std::string node_path;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return util::error_json("missing required parameter: anim_name");
  if (!require_string_arg(args, "track_type", track_type))
    return util::error_json(
        "missing required parameter: track_type (\"value\" or \"method\")");
  if (!require_string_arg(args, "node_path", node_path))
    return util::error_json("missing required parameter: node_path");

  std::string property;
  bool has_property = read_string_arg(args, "property", property);

  godot::Animation::TrackType type;
  std::string track_path;
  if (track_type == "value") {
    if (!has_property || property.empty())
      return util::error_json("value tracks require 'property' (e.g. \"position:x\", "
                        "\"modulate\") — the track path becomes "
                        "<node_path>:<property>");
    type = godot::Animation::TYPE_VALUE;
    track_path = node_path + ":" + property;
  } else if (track_type == "method") {
    type = godot::Animation::TYPE_METHOD;
    track_path = node_path;
  } else {
    return util::error_json("unsupported track_type: " + track_type +
                      " — use \"value\" or \"method\"");
  }

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return util::error_json("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  godot::NodePath path(track_path.c_str());
  if (anim->find_track(path, type) >= 0)
    return util::error_json("a " + track_type + " track for \"" + track_path +
                      "\" already exists in animation \"" + anim_name + "\"");

  int32_t idx = anim->add_track(type);
  anim->track_set_path(idx, path);
  if (type == godot::Animation::TYPE_VALUE)
    anim->value_track_set_update_mode(idx,
                                      godot::Animation::UPDATE_CONTINUOUS);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["anim_name"] = mcp::JsonValue(anim_name);
  inner["track_index"] = mcp::JsonValue(static_cast<int64_t>(idx));
  inner["track_path"] = mcp::JsonValue(track_path);
  inner["track_type"] = mcp::JsonValue(track_type);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_insert_keyframe(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return util::error_json("missing required parameter: anim_name");

  auto *idx_v = args.Find("track_index");
  if (!idx_v || !idx_v->IsInt())
    return util::error_json("missing required parameter: track_index (int)");
  int64_t track_index = idx_v->GetInt();

  double time = read_number_arg(args, "time", -1.0);
  if (time < 0.0)
    return util::error_json("missing or invalid parameter: time (seconds since "
                      "animation start, must be >= 0)");

  auto *value_v = args.Find("value");
  if (!value_v || value_v->IsNull())
    return util::error_json("missing required parameter: value");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return util::error_json("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  if (track_index < 0 || track_index >= anim->get_track_count())
    return util::error_json("track_index out of range: " +
                      std::to_string(track_index) + " (animation \"" +
                      anim_name + "\" has " +
                      std::to_string(anim->get_track_count()) + " tracks)");

  godot::Animation::TrackType track_type = anim->track_get_type(
      static_cast<int32_t>(track_index));
  godot::Variant key_value;
  if (track_type == godot::Animation::TYPE_METHOD) {
    if (!value_v->IsObject())
      return util::error_json("method track keys need an object value of the form "
                        "{\"method\": \"do_something\", \"args\": [...]}, got "
                        "a non-object JSON value");
    auto *method_v = value_v->Find("method");
    if (!method_v || !method_v->IsString() || method_v->GetString().empty())
      return util::error_json("method track key object requires a non-empty string "
                        "field 'method'");
    godot::Dictionary key;
    key[godot::StringName("method")] =
        godot::Variant(godot::StringName(method_v->GetString().c_str()));
    godot::Array call_args;
    auto *args_v = value_v->Find("args");
    if (args_v && args_v->IsArray()) {
      const mcp::JsonValue::Array &arr = args_v->GetArray();
      for (const mcp::JsonValue &item : arr)
        call_args.append(VariantJson::deserialize(item));
    }
    key[godot::StringName("args")] = godot::Variant(call_args);
    key_value = key;
  } else {
    key_value = VariantJson::deserialize(*value_v);
  }

  anim->track_insert_key(static_cast<int32_t>(track_index), time, key_value);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["anim_name"] = mcp::JsonValue(anim_name);
  inner["track_index"] = mcp::JsonValue(track_index);
  inner["time"] = mcp::JsonValue(time);
  inner["key_count"] = mcp::JsonValue(static_cast<int64_t>(
      anim->track_get_key_count(static_cast<int32_t>(track_index))));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_set_track_bundle(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  std::string track_type;
  std::string node_path;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return util::error_json("missing required parameter: anim_name");
  if (!require_string_arg(args, "track_type", track_type))
    return util::error_json(
        "missing required parameter: track_type (\"value\" or \"method\")");
  if (!require_string_arg(args, "node_path", node_path))
    return util::error_json("missing required parameter: node_path");

  std::string property;
  bool has_property = read_string_arg(args, "property", property);

  auto *keys_v = args.Find("keys");
  if (!keys_v || !keys_v->IsArray())
    return util::error_json("missing required parameter: keys (array of 1-256 "
                      "{\"time\": <number >= 0>, \"value\": <json>} objects)");
  const mcp::JsonValue::Array &keys_arr = keys_v->GetArray();
  if (keys_arr.empty() || keys_arr.size() > 256)
    return util::error_json("keys must contain 1-256 entries (got " +
                      std::to_string(keys_arr.size()) + ")");

  bool overwrite = false;
  auto *overwrite_v = args.Find("overwrite");
  if (overwrite_v) {
    if (!overwrite_v->IsBool())
      return util::error_json("overwrite must be a boolean");
    overwrite = overwrite_v->GetBool();
  }

  godot::Animation::TrackType type;
  std::string track_path;
  if (track_type == "value") {
    if (!has_property || property.empty())
      return util::error_json("value tracks require 'property' (e.g. \"position:x\", "
                        "\"modulate\") — the track path becomes "
                        "<node_path>:<property>");
    type = godot::Animation::TYPE_VALUE;
    track_path = node_path + ":" + property;
  } else if (track_type == "method") {
    type = godot::Animation::TYPE_METHOD;
    track_path = node_path;
  } else {
    return util::error_json("unsupported track_type: " + track_type +
                      " — use \"value\" or \"method\"");
  }

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return util::error_json("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  godot::NodePath path(track_path.c_str());
  if (anim->find_track(path, type) >= 0)
    return util::error_json("a " + track_type + " track for \"" + track_path +
                      "\" already exists in animation \"" + anim_name + "\"");

  int32_t idx = anim->add_track(type);
  anim->track_set_path(idx, path);
  if (type == godot::Animation::TYPE_VALUE)
    anim->value_track_set_update_mode(idx,
                                      godot::Animation::UPDATE_CONTINUOUS);

  struct BundleKey {
    double time = 0.0;
    const mcp::JsonValue *value = nullptr;
    size_t order = 0;
  };
  std::vector<BundleKey> work;
  work.reserve(keys_arr.size());
  for (size_t i = 0; i < keys_arr.size(); i++) {
    const mcp::JsonValue &entry = keys_arr[i];
    std::string where = "keys[" + std::to_string(i) + "]";
    if (!entry.IsObject()) {
      anim->remove_track(idx);
      return util::error_json(where + " must be an object of the form {\"time\": "
                               "<number >= 0>, \"value\": <json>}");
    }
    auto *time_v = entry.Find("time");
    if (!time_v || !time_v->IsNumber()) {
      anim->remove_track(idx);
      return util::error_json(where + " requires 'time' (number, seconds since "
                               "animation start, must be >= 0)");
    }
    double time = time_v->IsInt() ? static_cast<double>(time_v->GetInt())
                                  : time_v->GetDouble();
    if (time < 0.0) {
      anim->remove_track(idx);
      return util::error_json(where + " requires 'time' >= 0 (got " +
                        std::to_string(time) + ")");
    }
    auto *value_v = entry.Find("value");
    if (!value_v || value_v->IsNull()) {
      anim->remove_track(idx);
      return util::error_json(where + " requires 'value'");
    }
    if (type == godot::Animation::TYPE_METHOD) {
      if (!value_v->IsObject()) {
        anim->remove_track(idx);
        return util::error_json(where + " method track keys need an object value of "
                                 "the form {\"method\": \"do_something\", "
                                 "\"args\": [...]}, got a non-object JSON "
                                 "value");
      }
      auto *method_v = value_v->Find("method");
      if (!method_v || !method_v->IsString() ||
          method_v->GetString().empty()) {
        anim->remove_track(idx);
        return util::error_json(where + " method track key object requires a "
                                 "non-empty string field 'method'");
      }
      auto *args_v = value_v->Find("args");
      if (args_v && !args_v->IsArray()) {
        anim->remove_track(idx);
        return util::error_json(where + " method track key field 'args' must be an "
                                 "array when present");
      }
    }
    BundleKey item;
    item.time = time;
    item.value = value_v;
    item.order = i;
    work.push_back(item);
  }

  std::stable_sort(work.begin(), work.end(),
                   [](const BundleKey &a, const BundleKey &b) {
                     if (a.time != b.time)
                       return a.time < b.time;
                     return a.order < b.order;
                   });

  if (!overwrite) {
    for (size_t i = 1; i < work.size(); i++) {
      if (work[i].time == work[i - 1].time) {
        anim->remove_track(idx);
        return util::error_json("duplicate key time " +
                          std::to_string(work[i].time) +
                          " — pass overwrite=true to keep the last key at a "
                          "repeated time");
      }
    }
  } else {
    std::vector<BundleKey> deduped;
    deduped.reserve(work.size());
    for (size_t i = 0; i < work.size(); i++) {
      if (i + 1 < work.size() && work[i + 1].time == work[i].time)
        continue;
      deduped.push_back(work[i]);
    }
    work.swap(deduped);
  }

  mcp::JsonValue times(mcp::JsonValue::array_tag);
  for (const BundleKey &item : work) {
    godot::Variant key_value;
    if (type == godot::Animation::TYPE_METHOD) {
      auto *method_v = item.value->Find("method");
      godot::Dictionary key;
      key[godot::StringName("method")] =
          godot::Variant(godot::StringName(method_v->GetString().c_str()));
      godot::Array call_args;
      auto *args_v = item.value->Find("args");
      if (args_v && args_v->IsArray()) {
        const mcp::JsonValue::Array &arr = args_v->GetArray();
        for (const mcp::JsonValue &call_item : arr)
          call_args.append(VariantJson::deserialize(call_item));
      }
      key[godot::StringName("args")] = godot::Variant(call_args);
      key_value = key;
    } else {
      key_value = VariantJson::deserialize(*item.value);
    }
    anim->track_insert_key(idx, item.time, key_value);
    times.PushBack(mcp::JsonValue(item.time));
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["anim_name"] = mcp::JsonValue(anim_name);
  inner["track_index"] = mcp::JsonValue(static_cast<int64_t>(idx));
  inner["track_path"] = mcp::JsonValue(track_path);
  inner["track_type"] = mcp::JsonValue(track_type);
  inner["key_count"] = mcp::JsonValue(static_cast<int64_t>(
      anim->track_get_key_count(idx)));
  inner["times"] = std::move(times);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_remove_track(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return util::error_json("missing required parameter: anim_name");

  auto *idx_v = args.Find("track_index");
  if (!idx_v || !idx_v->IsInt())
    return util::error_json("missing required parameter: track_index (int)");
  int64_t track_index = idx_v->GetInt();

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return util::error_json("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  if (track_index < 0 || track_index >= anim->get_track_count())
    return util::error_json("track_index out of range: " +
                      std::to_string(track_index) + " (animation \"" +
                      anim_name + "\" has " +
                      std::to_string(anim->get_track_count()) + " tracks)");

  godot::NodePath removed_path =
      anim->track_get_path(static_cast<int32_t>(track_index));
  anim->remove_track(static_cast<int32_t>(track_index));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["player_path"] = mcp::JsonValue(player_path);
  inner["anim_name"] = mcp::JsonValue(anim_name);
  inner["removed_track_index"] = mcp::JsonValue(track_index);
  inner["removed_track_path"] =
      mcp::JsonValue(util::to_std(godot::String(removed_path)));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_create_tree(const mcp::JsonValue &args) {
  std::string parent_path;
  if (!require_string_arg(args, "parent_path", parent_path))
    return util::error_json("missing required parameter: parent_path");

  std::string name = DEFAULT_TREE_NAME;
  read_string_arg(args, "name", name);

  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root) {
    return util::error_json(
        "no scene currently open in the editor — open or create a scene first");
  }

  std::string hint;
  godot::Node *parent =
      util::resolve_scene_node(parent_path, scene_root, &hint);
  if (!parent)
    return util::error_json("parent node not found: " + parent_path + " — " + hint);

  std::string anim_player_path;
  godot::Node *player_node = nullptr;
  if (read_string_arg(args, "anim_player", anim_player_path) &&
      !anim_player_path.empty()) {
    player_node =
        util::resolve_scene_node(anim_player_path, scene_root, &hint);
    if (!player_node)
      return util::error_json("anim_player node not found: " + anim_player_path +
                        " — " + hint);
    if (!godot::Object::cast_to<godot::AnimationPlayer>(player_node))
      return util::error_json("anim_player is not an AnimationPlayer: " +
                        anim_player_path + " (found " +
                        util::to_std(player_node->get_class()) + ")");
  }

  godot::AnimationTree *tree = memnew(godot::AnimationTree);
  tree->set_name(godot::StringName(name.c_str()));

  godot::Ref<godot::AnimationNodeStateMachine> state_machine;
  state_machine.instantiate();
  tree->set_tree_root(state_machine);

  parent->add_child(tree);
  tree->set_owner(scene_root);

  if (player_node)
    tree->set_animation_player(tree->get_path_to(player_node));

  std::string rel = relative_scene_path(tree, scene_root);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(rel);
  inner["type"] = mcp::JsonValue("AnimationTree");
  inner["tree_root"] = mcp::JsonValue("AnimationNodeStateMachine");
  inner["states"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  if (player_node) {
    inner["anim_player"] = mcp::JsonValue(relative_scene_path(
        player_node, scene_root));
  }
  inner["undo"] = mcp::JsonValue("delete node " + rel +
                                 " (delete_scene_node)");
  inner["next"] = mcp::JsonValue(
      "call animation_tree with action=add_node and animation_tree_path=" + rel +
      " to add states, then action=connect to wire transitions");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_add_state(const mcp::JsonValue &args) {
  std::string tree_path;
  std::string state_name;
  if (!require_string_arg(args, "animation_tree_path", tree_path))
    return util::error_json("missing required parameter: animation_tree_path");
  if (!require_string_arg(args, "state_name", state_name))
    return util::error_json("missing required parameter: state_name");
  if (state_name == "Start" || state_name == "End")
    return util::error_json("state name \"" + state_name +
                      "\" is reserved by the implicit Start/End states");
  if (state_name.find('/') != std::string::npos)
    return util::error_json("state_name must not contain '/', got: " + state_name);

  std::string animation;
  bool has_animation = read_string_arg(args, "animation", animation);

  std::string msg;
  godot::AnimationTree *tree = resolve_tree(tree_path, msg);
  if (!tree)
    return util::error_json(msg);

  godot::AnimationNodeStateMachine *sm = resolve_state_machine(tree);
  if (!sm)
    return util::error_json("tree_root of " + tree_path +
                      " is not an AnimationNodeStateMachine — recreate the "
                      "tree with animation_tree action=create");

  godot::StringName state_sn(state_name.c_str());
  if (sm->has_node(state_sn))
    return util::error_json("state already exists in " + tree_path + ": " +
                      state_name);

  godot::Ref<godot::AnimationNodeAnimation> node;
  node.instantiate();
  if (has_animation && !animation.empty())
    node->set_animation(godot::StringName(animation.c_str()));
  sm->add_node(state_sn, node);

  mcp::JsonValue states(mcp::JsonValue::array_tag);
  godot::TypedArray<godot::StringName> node_list = sm->get_node_list();
  for (int i = 0; i < node_list.size(); i++) {
    godot::String state = node_list[i];
    states.PushBack(mcp::JsonValue(util::to_std(state)));
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["state"] = mcp::JsonValue(state_name);
  if (has_animation && !animation.empty())
    inner["animation"] = mcp::JsonValue(animation);
  inner["states"] = std::move(states);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_connect_states(const mcp::JsonValue &args) {
  std::string tree_path;
  std::string from_state;
  std::string to_state;
  if (!require_string_arg(args, "animation_tree_path", tree_path))
    return util::error_json("missing required parameter: animation_tree_path");
  if (!require_string_arg(args, "from_state", from_state))
    return util::error_json("missing required parameter: from_state");
  if (!require_string_arg(args, "to_state", to_state))
    return util::error_json("missing required parameter: to_state");

  std::string condition;
  bool has_condition = read_string_arg(args, "condition", condition);
  if (has_condition && !condition.empty() &&
      (condition.find('/') != std::string::npos ||
       condition.find(':') != std::string::npos))
    return util::error_json("condition must not contain '/' or ':', got: " +
                      condition);

  std::string msg;
  godot::AnimationTree *tree = resolve_tree(tree_path, msg);
  if (!tree)
    return util::error_json(msg);

  godot::AnimationNodeStateMachine *sm = resolve_state_machine(tree);
  if (!sm)
    return util::error_json("tree_root of " + tree_path +
                      " is not an AnimationNodeStateMachine — recreate the "
                      "tree with animation_tree action=create");

  godot::StringName from_sn(from_state.c_str());
  godot::StringName to_sn(to_state.c_str());
  if (!sm->has_node(from_sn))
    return util::error_json("from_state not found in " + tree_path + ": " +
                      from_state);
  if (!sm->has_node(to_sn))
    return util::error_json("to_state not found in " + tree_path + ": " + to_state);
  if (sm->has_transition(from_sn, to_sn))
    return util::error_json("transition already exists: " + from_state + " -> " +
                      to_state);

  godot::Ref<godot::AnimationNodeStateMachineTransition> transition;
  transition.instantiate();
  if (has_condition && !condition.empty()) {
    transition->set_advance_condition(godot::StringName(condition.c_str()));
    transition->set_advance_mode(
        godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["from_state"] = mcp::JsonValue(from_state);
  inner["to_state"] = mcp::JsonValue(to_state);
  if (has_condition && !condition.empty()) {
    inner["advance_condition"] = mcp::JsonValue(condition);
    inner["note"] = mcp::JsonValue(
        "transition advance_mode was set to AUTO because a condition was "
        "given; the condition must be a bool parameter on the AnimationTree "
        "(set via animation_runtime action=set_parameter)");
  }
  if (const mcp::JsonValue *priority_v = args.Find("priority")) {
    if (!priority_v->IsInt())
      return util::error_json("priority must be an integer");
    transition->set_priority(static_cast<int32_t>(priority_v->GetInt()));
    inner["priority"] = *priority_v;
  }
  if (const mcp::JsonValue *xfade_v = args.Find("xfade")) {
    if (!xfade_v->IsNumber())
      return util::error_json("xfade must be a number >= 0");
    double xfade = xfade_v->IsInt() ? static_cast<double>(xfade_v->GetInt())
                                    : xfade_v->GetDouble();
    if (xfade < 0.0)
      return util::error_json("xfade must be >= 0 (got " + std::to_string(xfade) +
                        ")");
    transition->set_xfade_time(static_cast<float>(xfade));
    inner["xfade"] = mcp::JsonValue(xfade);
  }
  if (const mcp::JsonValue *switch_v = args.Find("switch_mode")) {
    int32_t mode = 0;
    if (!parse_switch_mode(*switch_v, mode))
      return util::error_json("switch_mode must be immediate, sync or at_end "
                        "(or 0, 1, 2)");
    transition->set_switch_mode(
        static_cast<godot::AnimationNodeStateMachineTransition::SwitchMode>(
            mode));
    inner["switch_mode"] = mcp::JsonValue(switch_mode_name(mode));
  }
  if (const mcp::JsonValue *advance_v = args.Find("advance_mode")) {
    int32_t mode = 0;
    if (!parse_advance_mode(*advance_v, mode))
      return util::error_json("advance_mode must be disabled, enabled or auto "
                        "(or 0, 1, 2)");
    transition->set_advance_mode(
        static_cast<godot::AnimationNodeStateMachineTransition::AdvanceMode>(
            mode));
    inner["advance_mode"] = mcp::JsonValue(advance_mode_name(mode));
  }
  sm->add_transition(from_sn, to_sn, transition);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

int32_t find_transition_index(godot::AnimationNodeStateMachine *state_machine,
                              const godot::StringName &from,
                              const godot::StringName &to) {
  for (int32_t i = 0; i < state_machine->get_transition_count(); i++) {
    if (state_machine->get_transition_from(i) == from &&
        state_machine->get_transition_to(i) == to)
      return i;
  }
  return -1;
}

mcp::JsonValue inspect_tree(const mcp::JsonValue &args) {
  std::string tree_path;
  godot::AnimationTree *tree = nullptr;
  godot::AnimationNodeStateMachine *state_machine = nullptr;
  mcp::JsonValue error;
  if (!resolve_tree_and_machine(args, tree_path, tree, state_machine, error))
    return error;

  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  godot::TypedArray<godot::StringName> node_list = state_machine->get_node_list();
  for (int i = 0; i < node_list.size(); i++) {
    godot::StringName node_name = node_list[i];
    godot::Ref<godot::AnimationNode> node = state_machine->get_node(node_name);
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["name"] = mcp::JsonValue(util::to_std(godot::String(node_name)));
    if (node.is_valid()) {
      item["type"] = mcp::JsonValue(util::to_std(node->get_class()));
      auto *animation_node =
          godot::Object::cast_to<godot::AnimationNodeAnimation>(node.ptr());
      if (animation_node)
        item["animation"] = mcp::JsonValue(util::to_std(
            godot::String(animation_node->get_animation())));
    }
    nodes.PushBack(std::move(item));
  }

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["tree_root"] = mcp::JsonValue(util::to_std(state_machine->get_class()));
  godot::NodePath player_path = tree->get_animation_player();
  if (!player_path.is_empty())
    inner["animation_player"] =
        mcp::JsonValue(util::to_std(godot::String(player_path)));
  inner["node_count"] = mcp::JsonValue(static_cast<int64_t>(node_list.size()));
  inner["nodes"] = std::move(nodes);
  inner["transition_count"] = mcp::JsonValue(
      static_cast<int64_t>(state_machine->get_transition_count()));
  inner["transitions"] = transitions_json(state_machine);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

mcp::JsonValue remove_state_node(const mcp::JsonValue &args) {
  std::string tree_path;
  godot::AnimationTree *tree = nullptr;
  godot::AnimationNodeStateMachine *state_machine = nullptr;
  mcp::JsonValue error;
  if (!resolve_tree_and_machine(args, tree_path, tree, state_machine, error))
    return error;

  std::string state_name;
  if (!require_string_arg(args, "state_name", state_name))
    return util::error_json(
        "missing required parameter for action 'remove_node': state_name");
  if (state_name == "Start" || state_name == "End")
    return util::error_json("state \"" + state_name +
                      "\" is an implicit state of the state machine and "
                      "cannot be removed");
  godot::StringName state_sn(state_name.c_str());
  if (!state_machine->has_node(state_sn))
    return util::error_json("state not found in " + tree_path + ": " + state_name +
                      unknown_state_hint(state_machine));
  state_machine->remove_node(state_sn);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["removed_node"] = mcp::JsonValue(state_name);
  inner["node_count"] = mcp::JsonValue(
      static_cast<int64_t>(state_machine->get_node_list().size()));
  inner["nodes"] = states_json(state_machine);
  inner["transition_count"] = mcp::JsonValue(
      static_cast<int64_t>(state_machine->get_transition_count()));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue rename_state_node(const mcp::JsonValue &args) {
  std::string tree_path;
  godot::AnimationTree *tree = nullptr;
  godot::AnimationNodeStateMachine *state_machine = nullptr;
  mcp::JsonValue error;
  if (!resolve_tree_and_machine(args, tree_path, tree, state_machine, error))
    return error;

  std::string state_name;
  std::string new_name;
  if (!require_string_arg(args, "state_name", state_name))
    return util::error_json(
        "missing required parameter for action 'rename_node': state_name");
  if (!require_string_arg(args, "new_name", new_name))
    return util::error_json(
        "missing required parameter for action 'rename_node': new_name");
  if (new_name == "Start" || new_name == "End")
    return util::error_json("state name \"" + new_name +
                      "\" is reserved by the implicit Start/End states");
  if (new_name.find('/') != std::string::npos)
    return util::error_json("new_name must not contain '/', got: " + new_name);
  if (state_name == "Start" || state_name == "End")
    return util::error_json("state \"" + state_name +
                      "\" is an implicit state and cannot be renamed");
  if (state_name == new_name)
    return util::error_json("new_name must differ from state_name (" + state_name +
                      ")");
  godot::StringName state_sn(state_name.c_str());
  godot::StringName new_sn(new_name.c_str());
  if (!state_machine->has_node(state_sn))
    return util::error_json("state not found in " + tree_path + ": " + state_name +
                      unknown_state_hint(state_machine));
  if (state_machine->has_node(new_sn))
    return util::error_json("state already exists in " + tree_path + ": " + new_name);
  state_machine->rename_node(state_sn, new_sn);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["old_name"] = mcp::JsonValue(state_name);
  inner["new_name"] = mcp::JsonValue(new_name);
  inner["node_count"] = mcp::JsonValue(
      static_cast<int64_t>(state_machine->get_node_list().size()));
  inner["nodes"] = states_json(state_machine);
  inner["transitions"] = transitions_json(state_machine);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue remove_transition_action(const mcp::JsonValue &args) {
  std::string tree_path;
  godot::AnimationTree *tree = nullptr;
  godot::AnimationNodeStateMachine *state_machine = nullptr;
  mcp::JsonValue error;
  if (!resolve_tree_and_machine(args, tree_path, tree, state_machine, error))
    return error;

  std::string from_state;
  std::string to_state;
  if (!require_string_arg(args, "from_state", from_state))
    return util::error_json(
        "missing required parameter for action 'remove_transition': "
        "from_state");
  if (!require_string_arg(args, "to_state", to_state))
    return util::error_json(
        "missing required parameter for action 'remove_transition': to_state");
  godot::StringName from_sn(from_state.c_str());
  godot::StringName to_sn(to_state.c_str());
  if (!state_machine->has_transition(from_sn, to_sn)) {
    mcp::JsonValue e = util::error_json("transition not found: " + from_state +
                                  " -> " + to_state +
                                  " — available transitions: " +
                                  join_limited(transition_names(state_machine)));
    e["candidates"] = names_json(transition_names(state_machine));
    return e;
  }
  state_machine->remove_transition(from_sn, to_sn);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["removed_transition"] = mcp::JsonValue(from_state + " -> " + to_state);
  inner["from_state"] = mcp::JsonValue(from_state);
  inner["to_state"] = mcp::JsonValue(to_state);
  inner["transition_count"] = mcp::JsonValue(
      static_cast<int64_t>(state_machine->get_transition_count()));
  inner["transitions"] = transitions_json(state_machine);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue set_transition_params(const mcp::JsonValue &args) {
  std::string tree_path;
  godot::AnimationTree *tree = nullptr;
  godot::AnimationNodeStateMachine *state_machine = nullptr;
  mcp::JsonValue error;
  if (!resolve_tree_and_machine(args, tree_path, tree, state_machine, error))
    return error;

  std::string from_state;
  std::string to_state;
  if (!require_string_arg(args, "from_state", from_state))
    return util::error_json(
        "missing required parameter for action 'set_transition': from_state");
  if (!require_string_arg(args, "to_state", to_state))
    return util::error_json(
        "missing required parameter for action 'set_transition': to_state");
  godot::StringName from_sn(from_state.c_str());
  godot::StringName to_sn(to_state.c_str());
  if (!state_machine->has_transition(from_sn, to_sn)) {
    mcp::JsonValue e = util::error_json("transition not found: " + from_state +
                                  " -> " + to_state +
                                  " — available transitions: " +
                                  join_limited(transition_names(state_machine)));
    e["candidates"] = names_json(transition_names(state_machine));
    return e;
  }

  const mcp::JsonValue *priority_v = args.Find("priority");
  const mcp::JsonValue *switch_v = args.Find("switch_mode");
  const mcp::JsonValue *advance_v = args.Find("advance_mode");
  const mcp::JsonValue *condition_v = args.Find("advance_condition");
  const mcp::JsonValue *xfade_v = args.Find("xfade");
  if (!priority_v && !switch_v && !advance_v && !condition_v && !xfade_v)
    return util::error_json(
        "action 'set_transition' requires at least one of priority, "
        "switch_mode, advance_mode, advance_condition or xfade");

  int32_t switch_mode = 0;
  int32_t advance_mode = 0;
  bool has_switch_mode = false;
  bool has_advance_mode = false;
  if (switch_v) {
    if (!parse_switch_mode(*switch_v, switch_mode))
      return util::error_json("switch_mode must be immediate, sync or at_end "
                        "(or 0, 1, 2)");
    has_switch_mode = true;
  }
  if (advance_v) {
    if (!parse_advance_mode(*advance_v, advance_mode))
      return util::error_json("advance_mode must be disabled, enabled or auto "
                        "(or 0, 1, 2)");
    has_advance_mode = true;
  }
  if (condition_v && !condition_v->IsString())
    return util::error_json(
        "advance_condition must be a string (use \"\" to clear it)");
  if (condition_v) {
    const std::string condition = condition_v->GetString();
    if (!condition.empty() &&
        (condition.find('/') != std::string::npos ||
         condition.find(':') != std::string::npos))
      return util::error_json("advance_condition must not contain '/' or ':', "
                        "got: " + condition);
  }
  double xfade = 0.0;
  if (xfade_v) {
    if (!xfade_v->IsNumber())
      return util::error_json("xfade must be a number >= 0");
    xfade = xfade_v->IsInt() ? static_cast<double>(xfade_v->GetInt())
                             : xfade_v->GetDouble();
    if (xfade < 0.0)
      return util::error_json("xfade must be >= 0 (got " + std::to_string(xfade) +
                        ")");
  }

  int32_t index = find_transition_index(state_machine, from_sn, to_sn);
  godot::Ref<godot::AnimationNodeStateMachineTransition> transition =
      state_machine->get_transition(index);
  if (transition.is_null())
    return util::error_json("transition not found: " + from_state + " -> " +
                      to_state);
  if (priority_v) {
    if (!priority_v->IsInt())
      return util::error_json("priority must be an integer");
    transition->set_priority(static_cast<int32_t>(priority_v->GetInt()));
  }
  if (has_switch_mode)
    transition->set_switch_mode(
        static_cast<godot::AnimationNodeStateMachineTransition::SwitchMode>(
            switch_mode));
  if (has_advance_mode)
    transition->set_advance_mode(
        static_cast<godot::AnimationNodeStateMachineTransition::AdvanceMode>(
            advance_mode));
  if (condition_v) {
    const std::string condition = condition_v->GetString();
    transition->set_advance_condition(godot::StringName(condition.c_str()));
    if (!condition.empty() && !has_advance_mode)
      transition->set_advance_mode(
          godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
  }
  if (xfade_v)
    transition->set_xfade_time(static_cast<float>(xfade));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["from_state"] = mcp::JsonValue(from_state);
  inner["to_state"] = mcp::JsonValue(to_state);
  inner["transition"] = transition_json(state_machine, index);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_tree(const mcp::JsonValue &args) {
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString() || action_p->GetString().empty())
    return util::error_json(
        "missing required parameter: action (one of: create, inspect, "
        "add_node, remove_node, rename_node, connect, remove_transition, "
        "set_transition)");
  const std::string action = action_p->GetString();
  if (action == "create")
    return handle_create_tree(args);
  if (action == "inspect")
    return inspect_tree(args);
  if (action == "add_node")
    return handle_add_state(args);
  if (action == "remove_node")
    return remove_state_node(args);
  if (action == "rename_node")
    return rename_state_node(args);
  if (action == "connect")
    return handle_connect_states(args);
  if (action == "remove_transition")
    return remove_transition_action(args);
  if (action == "set_transition")
    return set_transition_params(args);
  return util::error_json("unknown action '" + action +
                    "' for animation_tree — expected create, inspect, "
                    "add_node, remove_node, rename_node, connect, "
                    "remove_transition or set_transition");
}

mcp::JsonValue tree_parameter(const mcp::JsonValue &args, bool write) {
  std::string tree_path;
  std::string path;
  if (!require_string_arg(args, "animation_tree_path", tree_path))
    return util::error_json("missing required parameter: animation_tree_path");
  if (!require_string_arg(args, "path", path))
    return util::error_json(
        "missing required parameter: path (e.g. "
        "\"parameters/conditions/is_running\")");
  if (path.rfind(PARAMETER_PREFIX, 0) != 0)
    return util::error_json("parameter path must start with \"" +
                      std::string(PARAMETER_PREFIX) + "\", got: " + path);
  std::string msg;
  godot::AnimationTree *tree = resolve_tree(tree_path, msg);
  if (!tree)
    return util::error_json(msg);

  std::vector<std::string> names = parameter_names(tree);
  if (std::find(names.begin(), names.end(), path) == names.end()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    if (names.empty()) {
      e["error"] = mcp::JsonValue(
          "the AnimationTree at " + tree_path +
          " exposes no parameters/... properties — assign a tree_root and an "
          "animation_player first");
    } else {
      e["error"] = mcp::JsonValue(
          "unknown animation tree parameter: " + path + " — available: " +
          join_limited(names));
    }
    e["candidates"] = names_json(names);
    return e;
  }

  godot::StringName name(path.c_str());
  if (!write) {
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["animation_tree_path"] = mcp::JsonValue(tree_path);
    inner["path"] = mcp::JsonValue(path);
    inner["value"] = parameter_value_json(tree->get(name));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    return r;
  }

  auto *value_v = args.Find("value");
  if (!value_v || value_v->IsNull())
    return util::error_json("missing required parameter for action 'set_parameter': "
                      "value");
  godot::Variant desired = VariantJson::deserialize(*value_v);
  mcp::JsonValue old_value = parameter_value_json(tree->get(name));
  tree->set(name, desired);
  mcp::JsonValue new_value = parameter_value_json(tree->get(name));
  if (new_value.Dump() == old_value.Dump() &&
      parameter_value_json(desired).Dump() != old_value.Dump())
    return util::error_json("animation tree parameter is read-only or rejected the "
                      "value: " + path);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["path"] = mcp::JsonValue(path);
  inner["old"] = std::move(old_value);
  inner["new"] = std::move(new_value);
  inner["changed"] = mcp::JsonValue(
      inner["old"].Dump() != inner["new"].Dump());
  inner["note"] = mcp::JsonValue(
      "parameter writes are runtime state only; the scene file is not "
      "modified");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

mcp::JsonValue handle_runtime(const mcp::JsonValue &args) {
  auto *action_p = args.Find("action");
  if (!action_p || !action_p->IsString() || action_p->GetString().empty())
    return util::error_json(
        "missing required parameter: action (one of: play, pause, stop, seek, "
        "play_backwards, set_speed, queue, current, get_parameter, "
        "set_parameter)");
  const std::string action = action_p->GetString();

  if (action == "get_parameter")
    return tree_parameter(args, false);
  if (action == "set_parameter")
    return tree_parameter(args, true);

  std::string player_path;
  if (!require_string_arg(args, "player_path", player_path))
    return util::error_json("missing required parameter: player_path");
  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return util::error_json(msg);

  std::string animation;
  bool has_animation =
      read_string_arg(args, "animation", animation) && !animation.empty();

  if (action == "play" || action == "play_backwards") {
    if (has_animation) {
      godot::StringName anim_sn(animation.c_str());
      if (!player->has_animation(anim_sn))
        return util::error_json("animation not found on " + player_path + ": " +
                          animation +
                          unknown_animation_hint(player));
      if (action == "play")
        player->play(anim_sn);
      else
        player->play_backwards(anim_sn);
    } else if (action == "play") {
      player->play();
    } else {
      player->play_backwards();
    }
    mcp::JsonValue out = player_result(player_path, action, player);
    mcp::JsonValue *inner = out.Find("result");
    if (inner != nullptr && inner->IsObject() && has_animation)
      (*inner)["animation"] = mcp::JsonValue(animation);
    return out;
  }

  if (action == "pause") {
    player->pause();
    return player_result(player_path, action, player);
  }

  if (action == "stop") {
    bool keep_state = false;
    if (const mcp::JsonValue *keep_v = args.Find("keep_state")) {
      if (!keep_v->IsBool())
        return util::error_json("keep_state must be a boolean");
      keep_state = keep_v->GetBool();
    }
    player->stop(keep_state);
    return player_result(player_path, action, player);
  }

  if (action == "seek") {
    auto *position_v = args.Find("position");
    if (!position_v || !position_v->IsNumber())
      return util::error_json(
          "missing required parameter for action 'seek': position (number, "
          "seconds >= 0)");
    double position =
        position_v->IsInt() ? static_cast<double>(position_v->GetInt())
                            : position_v->GetDouble();
    if (position < 0.0)
      return util::error_json("position must be >= 0 (got " +
                        std::to_string(position) + ")");
    bool update = false;
    if (const mcp::JsonValue *update_v = args.Find("update")) {
      if (!update_v->IsBool())
        return util::error_json("update must be a boolean");
      update = update_v->GetBool();
    }
    player->seek(position, update);
    return player_result(player_path, action, player);
  }

  if (action == "set_speed") {
    auto *speed_v = args.Find("speed_scale");
    if (!speed_v || !speed_v->IsNumber())
      return util::error_json(
          "missing required parameter for action 'set_speed': speed_scale "
          "(number, 1.0 is normal speed)");
    double speed = speed_v->IsInt() ? static_cast<double>(speed_v->GetInt())
                                    : speed_v->GetDouble();
    if (!std::isfinite(speed))
      return util::error_json("speed_scale must be a finite number");
    float old_speed = player->get_speed_scale();
    player->set_speed_scale(static_cast<float>(speed));
    mcp::JsonValue out = player_result(player_path, action, player);
    mcp::JsonValue *inner = out.Find("result");
    if (inner != nullptr && inner->IsObject())
      (*inner)["old_speed_scale"] = mcp::JsonValue(
          static_cast<double>(old_speed));
    return out;
  }

  if (action == "queue") {
    if (has_animation) {
      godot::StringName anim_sn(animation.c_str());
      if (!player->has_animation(anim_sn))
        return util::error_json("animation not found on " + player_path + ": " +
                          animation +
                          unknown_animation_hint(player));
      player->queue(anim_sn);
    }
    mcp::JsonValue out = player_result(player_path, action, player);
    mcp::JsonValue *inner = out.Find("result");
    if (inner != nullptr && inner->IsObject() && has_animation)
      (*inner)["queued"] = mcp::JsonValue(animation);
    return out;
  }

  if (action == "current")
    return player_result(player_path, action, player);

  return util::error_json("unknown action '" + action +
                    "' for animation_runtime — expected play, pause, stop, "
                    "seek, play_backwards, set_speed, queue, current, "
                    "get_parameter or set_parameter");
}

} // namespace animation_ops
} // namespace godot_autopilot
