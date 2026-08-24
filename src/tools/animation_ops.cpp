#include "animation_ops.hpp"
#include "../core/scene_dirty_tracker.hpp"
#include "../util/error_util.hpp"
#include "../util/scene_path.hpp"
#include "../util/variant_json.hpp"
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

mcp::JsonValue make_error(const std::string &msg) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(msg);
  return e;
}

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
  return read_string_arg(args, key, out) && !out.empty();
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

} // namespace

mcp::JsonValue handle_create_player(const mcp::JsonValue &args) {
  std::string parent_path;
  if (!require_string_arg(args, "parent_path", parent_path))
    return make_error("missing required parameter: parent_path");

  std::string name = DEFAULT_PLAYER_NAME;
  read_string_arg(args, "name", name);

  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root) {
    return make_error(
        "no scene currently open in the editor — open or create a scene first");
  }

  std::string hint;
  godot::Node *parent =
      util::resolve_scene_node(parent_path, scene_root, &hint);
  if (!parent)
    return make_error("parent node not found: " + parent_path + " — " + hint);

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
    return make_error("missing required parameter: player_path");
  if (!require_string_arg(args, "name", name))
    return make_error("missing required parameter: name");

  double length_sec =
      read_number_arg(args, "length_sec", DEFAULT_ANIMATION_LENGTH);
  if (length_sec <= 0.0)
    return make_error("length_sec must be > 0 (got " +
                      std::to_string(length_sec) + ")");

  int64_t loop_mode =
      static_cast<int64_t>(read_number_arg(args, "loop_mode", 0.0));
  if (loop_mode < 0 || loop_mode > LOOP_MODE_MAX)
    return make_error("loop_mode must be 0 (none), 1 (linear) or 2 "
                      "(pingpong), got " +
                      std::to_string(loop_mode));

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

  godot::StringName anim_name(name.c_str());
  if (player->has_animation(anim_name))
    return make_error("animation already exists on " + player_path + ": " +
                      name);

  godot::Ref<godot::AnimationLibrary> lib = ensure_default_library(player, msg);
  if (lib.is_null())
    return make_error(msg);

  godot::Ref<godot::Animation> anim;
  anim.instantiate();
  anim->set_length(static_cast<float>(length_sec));
  anim->set_loop_mode(static_cast<godot::Animation::LoopMode>(loop_mode));

  godot::Error err = lib->add_animation(anim_name, anim);
  if (err != godot::Error::OK)
    return make_error("failed to add animation \"" + name +
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
    return make_error("missing required parameter: player_path");
  if (!require_string_arg(args, "name", name))
    return make_error("missing required parameter: name");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

  godot::StringName anim_name(name.c_str());
  if (!player->has_animation(anim_name))
    return make_error("animation not found on " + player_path + ": " + name);

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
  return make_error("animation not found in any library: " + name);
}

mcp::JsonValue handle_get_list(const mcp::JsonValue &args) {
  std::string player_path;
  if (!require_string_arg(args, "player_path", player_path))
    return make_error("missing required parameter: player_path");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

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

mcp::JsonValue handle_create_track(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  std::string track_type;
  std::string node_path;
  if (!require_string_arg(args, "player_path", player_path))
    return make_error("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return make_error("missing required parameter: anim_name");
  if (!require_string_arg(args, "track_type", track_type))
    return make_error(
        "missing required parameter: track_type (\"value\" or \"method\")");
  if (!require_string_arg(args, "node_path", node_path))
    return make_error("missing required parameter: node_path");

  std::string property;
  bool has_property = read_string_arg(args, "property", property);

  godot::Animation::TrackType type;
  std::string track_path;
  if (track_type == "value") {
    if (!has_property || property.empty())
      return make_error("value tracks require 'property' (e.g. \"position:x\", "
                        "\"modulate\") — the track path becomes "
                        "<node_path>:<property>");
    type = godot::Animation::TYPE_VALUE;
    track_path = node_path + ":" + property;
  } else if (track_type == "method") {
    type = godot::Animation::TYPE_METHOD;
    track_path = node_path;
  } else {
    return make_error("unsupported track_type: " + track_type +
                      " — use \"value\" or \"method\"");
  }

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return make_error("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  godot::NodePath path(track_path.c_str());
  if (anim->find_track(path, type) >= 0)
    return make_error("a " + track_type + " track for \"" + track_path +
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
    return make_error("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return make_error("missing required parameter: anim_name");

  auto *idx_v = args.Find("track_index");
  if (!idx_v || !idx_v->IsInt())
    return make_error("missing required parameter: track_index (int)");
  int64_t track_index = idx_v->GetInt();

  double time = read_number_arg(args, "time", -1.0);
  if (time < 0.0)
    return make_error("missing or invalid parameter: time (seconds since "
                      "animation start, must be >= 0)");

  auto *value_v = args.Find("value");
  if (!value_v || value_v->IsNull())
    return make_error("missing required parameter: value");

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return make_error("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  if (track_index < 0 || track_index >= anim->get_track_count())
    return make_error("track_index out of range: " +
                      std::to_string(track_index) + " (animation \"" +
                      anim_name + "\" has " +
                      std::to_string(anim->get_track_count()) + " tracks)");

  godot::Animation::TrackType track_type = anim->track_get_type(
      static_cast<int32_t>(track_index));
  godot::Variant key_value;
  if (track_type == godot::Animation::TYPE_METHOD) {
    if (!value_v->IsObject())
      return make_error("method track keys need an object value of the form "
                        "{\"method\": \"do_something\", \"args\": [...]}, got "
                        "a non-object JSON value");
    auto *method_v = value_v->Find("method");
    if (!method_v || !method_v->IsString() || method_v->GetString().empty())
      return make_error("method track key object requires a non-empty string "
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

mcp::JsonValue handle_remove_track(const mcp::JsonValue &args) {
  std::string player_path;
  std::string anim_name;
  if (!require_string_arg(args, "player_path", player_path))
    return make_error("missing required parameter: player_path");
  if (!require_string_arg(args, "anim_name", anim_name))
    return make_error("missing required parameter: anim_name");

  auto *idx_v = args.Find("track_index");
  if (!idx_v || !idx_v->IsInt())
    return make_error("missing required parameter: track_index (int)");
  int64_t track_index = idx_v->GetInt();

  std::string msg;
  godot::AnimationPlayer *player = resolve_player(player_path, msg);
  if (!player)
    return make_error(msg);

  godot::StringName anim_sn(anim_name.c_str());
  if (!player->has_animation(anim_sn))
    return make_error("animation not found on " + player_path + ": " +
                      anim_name);

  godot::Ref<godot::Animation> anim = player->get_animation(anim_sn);
  if (track_index < 0 || track_index >= anim->get_track_count())
    return make_error("track_index out of range: " +
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
    return make_error("missing required parameter: parent_path");

  std::string name = DEFAULT_TREE_NAME;
  read_string_arg(args, "name", name);

  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root) {
    return make_error(
        "no scene currently open in the editor — open or create a scene first");
  }

  std::string hint;
  godot::Node *parent =
      util::resolve_scene_node(parent_path, scene_root, &hint);
  if (!parent)
    return make_error("parent node not found: " + parent_path + " — " + hint);

  std::string anim_player_path;
  godot::Node *player_node = nullptr;
  if (read_string_arg(args, "anim_player", anim_player_path) &&
      !anim_player_path.empty()) {
    player_node =
        util::resolve_scene_node(anim_player_path, scene_root, &hint);
    if (!player_node)
      return make_error("anim_player node not found: " + anim_player_path +
                        " — " + hint);
    if (!godot::Object::cast_to<godot::AnimationPlayer>(player_node))
      return make_error("anim_player is not an AnimationPlayer: " +
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
      "call add_animation_machine_state with animation_tree_path=" + rel +
      " to add states, then connect_animation_states to wire transitions");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_add_state(const mcp::JsonValue &args) {
  std::string tree_path;
  std::string state_name;
  if (!require_string_arg(args, "animation_tree_path", tree_path))
    return make_error("missing required parameter: animation_tree_path");
  if (!require_string_arg(args, "state_name", state_name))
    return make_error("missing required parameter: state_name");

  std::string animation;
  bool has_animation = read_string_arg(args, "animation", animation);

  std::string msg;
  godot::AnimationTree *tree = resolve_tree(tree_path, msg);
  if (!tree)
    return make_error(msg);

  godot::AnimationNodeStateMachine *sm = resolve_state_machine(tree);
  if (!sm)
    return make_error("tree_root of " + tree_path +
                      " is not an AnimationNodeStateMachine — recreate the "
                      "tree with create_scene_animation_tree");

  godot::StringName state_sn(state_name.c_str());
  if (sm->has_node(state_sn))
    return make_error("state already exists in " + tree_path + ": " +
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
    return make_error("missing required parameter: animation_tree_path");
  if (!require_string_arg(args, "from_state", from_state))
    return make_error("missing required parameter: from_state");
  if (!require_string_arg(args, "to_state", to_state))
    return make_error("missing required parameter: to_state");

  std::string condition;
  bool has_condition = read_string_arg(args, "condition", condition);

  std::string msg;
  godot::AnimationTree *tree = resolve_tree(tree_path, msg);
  if (!tree)
    return make_error(msg);

  godot::AnimationNodeStateMachine *sm = resolve_state_machine(tree);
  if (!sm)
    return make_error("tree_root of " + tree_path +
                      " is not an AnimationNodeStateMachine — recreate the "
                      "tree with create_scene_animation_tree");

  godot::StringName from_sn(from_state.c_str());
  godot::StringName to_sn(to_state.c_str());
  if (!sm->has_node(from_sn))
    return make_error("from_state not found in " + tree_path + ": " +
                      from_state);
  if (!sm->has_node(to_sn))
    return make_error("to_state not found in " + tree_path + ": " + to_state);
  if (sm->has_transition(from_sn, to_sn))
    return make_error("transition already exists: " + from_state + " -> " +
                      to_state);

  godot::Ref<godot::AnimationNodeStateMachineTransition> transition;
  transition.instantiate();
  if (has_condition && !condition.empty()) {
    transition->set_advance_condition(godot::StringName(condition.c_str()));
    transition->set_advance_mode(
        godot::AnimationNodeStateMachineTransition::ADVANCE_MODE_AUTO);
  }
  sm->add_transition(from_sn, to_sn, transition);

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["animation_tree_path"] = mcp::JsonValue(tree_path);
  inner["from_state"] = mcp::JsonValue(from_state);
  inner["to_state"] = mcp::JsonValue(to_state);
  if (has_condition && !condition.empty()) {
    inner["advance_condition"] = mcp::JsonValue(condition);
    inner["note"] = mcp::JsonValue(
        "transition advance_mode was set to AUTO because a condition was "
        "given; the condition must be a bool parameter on the AnimationTree "
        "(set via code_execute or runtime code)");
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

} // namespace animation_ops
} // namespace godot_autopilot
