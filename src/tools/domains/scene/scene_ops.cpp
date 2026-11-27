#include "tools/domains/scene/scene_ops.hpp"
#include "tools/domains/scene/reparent_undo_order.hpp"
#include "tools/domains/scene/scene_safety.hpp"
#include "tools/domains/scene/scene_spec_ops.hpp"
#include "util/resource_fs.hpp"
#include "util/scene_path.hpp"
#include "util/scene_verify.hpp"
#include "util/project_path.hpp"
#include "core/observability/log_system.hpp"
#include "core/editor_readiness.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "tools/domains/editor/editor_ops.hpp"
#include "tools/domains/property/property_ops.hpp"
#include "tools/domains/resource/resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/curve2d.hpp>
#include <godot_cpp/classes/curve3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/path2d.hpp>
#include <godot_cpp/classes/path3d.hpp>
#include <godot_cpp/classes/path_follow2d.hpp>
#include <godot_cpp/classes/path_follow3d.hpp>
#include <godot_cpp/classes/scene_state.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot_autopilot {
namespace scene_ops {

namespace {

constexpr int DEFAULT_MAX_DEPTH = 8;
constexpr int UNLIMITED_TREE_DEPTH = 100000;
constexpr int MAX_PROPERTY_COUNT = 20;
constexpr int MAX_SCREEN_RECT_PATHS = 50;

struct TreeDetailOptions {
  bool include_properties = false;
  bool include_groups = false;
  bool include_owner = false;
  bool include_index = false;
  bool include_scene_file_path = false;
};

bool bool_arg(const godot_autopilot::util::JsonValue &args, const char *key) {
  const godot_autopilot::util::JsonValue *value = args.Find(key);
  return value != nullptr && value->IsBool() && value->GetBool();
}

TreeDetailOptions tree_detail_options(const godot_autopilot::util::JsonValue &args) {
  TreeDetailOptions options;
  options.include_properties = bool_arg(args, "include_properties");
  options.include_groups = bool_arg(args, "include_groups");
  options.include_owner = bool_arg(args, "include_owner");
  options.include_index = bool_arg(args, "include_index");
  options.include_scene_file_path = bool_arg(args, "include_scene_file_path");
  return options;
}

int property_summary_key_weight(const std::string &name) {
  if (name == "scale")
    return 100;
  if (name == "z_index")
    return 90;
  if (name == "position")
    return 80;
  if (name == "rotation")
    return 70;
  if (name == "size")
    return 60;
  if (name == "modulate")
    return 50;
  if (name == "self_modulate")
    return 40;
  if (name == "text")
    return 30;
  if (name == "offset")
    return 20;
  return 0;
}

void collect_property_summary(godot::Node *node, godot_autopilot::util::JsonValue &j) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  godot::ClassDBSingleton *cdbs = godot::ClassDBSingleton::get_singleton();
  const godot::StringName cls = node->get_class();
  struct Candidate {
    std::string name;
    godot::Variant value;
    int tier;
  };
  std::vector<Candidate> cands;
  for (int i = 0; i < props.size(); i++) {
    godot::Dictionary prop = props[i];
    godot::Variant name_v = prop["name"];
    godot::Variant::Type name_type = name_v.get_type();
    if (name_type != godot::Variant::STRING &&
        name_type != godot::Variant::STRING_NAME)
      continue;
    godot::StringName prop_name = name_v;
    std::string name_str = util::to_std(godot::String(prop_name));
    if (name_str.rfind("metadata/", 0) == 0)
      continue;
    if (!name_str.empty() && name_str[0] == '_')
      continue;
    if (static_cast<godot::Variant::Type>(static_cast<int>(prop["type"])) ==
        godot::Variant::OBJECT)
      continue;
    godot::Variant current = node->get(prop_name);
    bool is_default = false;
    if (cdbs) {
      godot::Variant def =
          cdbs->class_get_property_default_value(cls, prop_name);
      if (def.get_type() == godot::Variant::NIL)
        is_default = current.get_type() == godot::Variant::NIL;
      else
        is_default = current == def;
    }
    const int weight = property_summary_key_weight(name_str);
    int tier = 3;
    if (!is_default)
      tier = weight > 0 ? 0 : 1;
    else if (weight > 0)
      tier = 2;
    cands.push_back(Candidate{name_str, current, tier});
  }
  std::stable_sort(cands.begin(), cands.end(),
                   [](const Candidate &a, const Candidate &b) {
                     return a.tier < b.tier;
                   });
  int count = 0;
  for (const Candidate &c : cands) {
    if (count >= MAX_PROPERTY_COUNT)
      break;
    j[c.name] = VariantJson::serialize(c.value);
    count++;
  }
}

void node_to_json(godot::Node *node, int remaining_depth,
                  const TreeDetailOptions &options, godot_autopilot::util::JsonValue &j) {
  if (!node)
    return;
  j["name"] = godot_autopilot::util::JsonValue(util::to_std(node->get_name()));
  j["type"] = godot_autopilot::util::JsonValue(util::to_std(node->get_class()));

  godot::Node *root =
      godot::EditorInterface::get_singleton()
          ? godot::EditorInterface::get_singleton()->get_edited_scene_root()
          : nullptr;
  godot::Node *n = node;
  std::vector<std::string> parts;
  while (n && n != root) {
    parts.push_back(util::to_std(n->get_name()));
    n = n->get_parent();
  }
  std::reverse(parts.begin(), parts.end());
  std::string path;
  for (size_t i = 0; i < parts.size(); i++) {
    if (i > 0)
      path += "/";
    path += parts[i];
  }
  if (path.empty())
    path = util::to_std(node->get_name());
  j["path"] = godot_autopilot::util::JsonValue(path);
  if (options.include_properties) {
    godot_autopilot::util::JsonValue props(godot_autopilot::util::JsonValue::object_tag);
    collect_property_summary(node, props);
    j["properties"] = std::move(props);
  }
  if (options.include_groups) {
    godot_autopilot::util::JsonValue groups(godot_autopilot::util::JsonValue::array_tag);
    godot::TypedArray<godot::StringName> names = node->get_groups();
    for (int i = 0; i < names.size(); i++) {
      groups.PushBack(godot_autopilot::util::JsonValue(util::to_std(godot::String(names[i]))));
    }
    j["groups"] = std::move(groups);
  }
  if (options.include_owner) {
    godot::Node *owner = node->get_owner();
    j["owner"] = godot_autopilot::util::JsonValue(
        owner ? scene_relative_path(owner, root) : std::string());
  }
  if (options.include_index)
    j["index"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(node->get_index()));
  if (options.include_scene_file_path)
    j["scene_file_path"] =
        godot_autopilot::util::JsonValue(util::to_std(node->get_scene_file_path()));
  if (remaining_depth <= 0)
    return;
  j["children"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child) {
      godot_autopilot::util::JsonValue child_j(godot_autopilot::util::JsonValue::object_tag);
      node_to_json(child, remaining_depth - 1, options, child_j);
      j["children"].PushBack(std::move(child_j));
    }
  }
}

bool property_value_is_object_typed(const godot::Node *node,
                                    const std::string &prop_name,
                                    const godot_autopilot::util::JsonValue &raw_value) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name") || !dict.has("type"))
      continue;
    if (util::to_std(dict["name"].operator godot::String()) != prop_name)
      continue;
    return static_cast<godot::Variant::Type>(static_cast<int>(dict["type"])) ==
           godot::Variant::OBJECT;
  }
  if (raw_value.IsString()) {
    const std::string str = raw_value.GetString();
    return str.rfind("res://", 0) == 0 || str.rfind("memory://", 0) == 0;
  }
  if (raw_value.IsObject()) {
    const godot_autopilot::util::JsonValue *ref = raw_value.Find("path");
    if (!ref)
      ref = raw_value.Find("resource");
    return ref != nullptr && ref->IsString();
  }
  return false;
}

godot::Node *instance_state_node(godot::Node *instance,
                                 const godot::NodePath &rel) {
  if (!instance)
    return nullptr;
  const std::string rel_str = util::to_std(rel);
  if (rel_str.empty() || rel_str == ".")
    return instance;
  return instance->get_node_or_null(rel);
}

void audit_instance_persist_connections(godot::PackedScene *packed_scene,
                                        godot::Node *instance, int &skipped,
                                        int &connected, int &unresolved) {
  skipped = 0;
  connected = 0;
  unresolved = 0;
  if (!packed_scene || !instance)
    return;
  godot::Ref<godot::SceneState> state = packed_scene->get_state();
  if (state.is_null())
    return;
  const int32_t total = state->get_connection_count();
  for (int32_t i = 0; i < total; i++) {
    const int32_t flags = state->get_connection_flags(i);
    if ((flags &
         static_cast<int32_t>(godot::Object::CONNECT_PERSIST)) == 0)
      continue;
    godot::Node *src_node =
        instance_state_node(instance, state->get_connection_source(i));
    godot::Node *dst_node =
        instance_state_node(instance, state->get_connection_target(i));
    if (!src_node || !dst_node) {
      unresolved++;
      continue;
    }
    godot::StringName sig = state->get_connection_signal(i);
    godot::StringName method = state->get_connection_method(i);
    godot::Callable callable(dst_node, method);
    if (src_node->is_connected(sig, callable)) {
      skipped++;
      continue;
    }
    godot::Error err =
        src_node->connect(sig, callable, godot::Object::CONNECT_PERSIST);
    if (err == godot::OK)
      connected++;
    else
      unresolved++;
  }
}

bool child_name_taken(godot::Node *parent, const std::string &name,
                      const godot::Node *skip) {
  auto children = parent->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child && child != skip && util::to_std(child->get_name()) == name)
      return true;
  }
  return false;
}

std::string unique_child_name(godot::Node *parent, const std::string &base,
                              const godot::Node *skip) {
  if (!child_name_taken(parent, base, skip))
    return base;
  int index = 2;
  while (true) {
    const std::string candidate = base + std::to_string(index);
    if (!child_name_taken(parent, candidate, skip))
      return candidate;
    ++index;
  }
}

void set_unowned_descendants(godot::Node *node, godot::Node *owner) {
  if (!node || !owner)
    return;
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (!child)
      continue;
    if (!child->get_owner())
      child->set_owner(owner);
    set_unowned_descendants(child, owner);
  }
}

void free_detached_node(godot::Node *node) {
  if (!node)
    return;
  if (godot::Node *parent = node->get_parent())
    parent->remove_child(node);
  memdelete(node);
}

godot_autopilot::util::JsonValue edit_action_error(const std::string &action,
                                 const std::string &message) {
  godot_autopilot::util::JsonValue e = util::error_json(message);
  e["action"] = godot_autopilot::util::JsonValue(action);
  return e;
}

std::string joined_paths(const std::vector<std::string> &paths) {
  std::string joined;
  for (size_t i = 0; i < paths.size(); i++) {
    if (i > 0)
      joined += ", ";
    joined += paths[i];
  }
  return joined;
}

godot_autopilot::util::JsonValue duplicate_scene_node_impl(const godot_autopilot::util::JsonValue &args,
                                         godot::Node *node,
                                         godot::Node *scene_root) {
  if (!scene_root)
    return edit_action_error("duplicate", "no edited scene root available");
  if (node == scene_root)
    return edit_action_error(
        "duplicate", "cannot duplicate the scene root — duplicate a child node");
  godot::Node *parent = node->get_parent();
  if (!parent)
    return edit_action_error("duplicate",
                             "node has no parent to duplicate under");
  int64_t count = 1;
  if (const godot_autopilot::util::JsonValue *count_p = args.Find("count")) {
    if (!count_p->IsInt() || count_p->GetInt() < 1)
      return edit_action_error("duplicate",
                               "parameter 'count' must be an integer >= 1");
    count = count_p->GetInt();
  }
  std::string suffix = "_copy";
  if (const godot_autopilot::util::JsonValue *suffix_p = args.Find("name_suffix")) {
    if (!suffix_p->IsString())
      return edit_action_error("duplicate",
                               "parameter 'name_suffix' must be a string");
    if (!suffix_p->GetString().empty())
      suffix = suffix_p->GetString();
  }
  if (suffix.find('/') != std::string::npos ||
      suffix.find(':') != std::string::npos)
    return edit_action_error(
        "duplicate",
        "invalid name_suffix '" + suffix +
            "' — '/' and ':' are not allowed in node names");
  const bool editable_instance = bool_arg(args, "editable_instance");
  const std::string base_name = util::to_std(node->get_name());
  const std::string source_path = scene_relative_path(node, scene_root);
  const int insert_index = node->get_index() + 1;
  std::vector<godot::Node *> created;
  std::vector<std::string> created_paths;
  std::vector<std::string> editable_paths;
  int skipped_editable = 0;
  auto rollback = [&created]() {
    for (auto it = created.rbegin(); it != created.rend(); ++it)
      free_detached_node(*it);
    created.clear();
  };
  for (int64_t i = 0; i < count; i++) {
    godot::Node *copy = node->duplicate(godot::Node::DUPLICATE_DEFAULT);
    if (!copy) {
      rollback();
      return edit_action_error("duplicate",
                               "failed to duplicate node at '" + source_path +
                                   "'");
    }
    std::string copy_name = base_name + suffix;
    if (count > 1)
      copy_name += std::to_string(i + 1);
    copy->set_name(godot::StringName(
        unique_child_name(parent, copy_name, nullptr).c_str()));
    parent->add_child(copy);
    parent->move_child(copy, insert_index + static_cast<int>(i));
    copy->set_owner(scene_root);
    set_unowned_descendants(copy, scene_root);
    if (editable_instance) {
      if (!copy->get_scene_file_path().is_empty()) {
        scene_root->set_editable_instance(copy, true);
        editable_paths.push_back(scene_relative_path(copy, scene_root));
      } else {
        skipped_editable++;
      }
    }
    created.push_back(copy);
    created_paths.push_back(scene_relative_path(copy, scene_root));
  }
  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["action"] = godot_autopilot::util::JsonValue("duplicate");
  inner["source_path"] = godot_autopilot::util::JsonValue(source_path);
  inner["count"] = godot_autopilot::util::JsonValue(count);
  godot_autopilot::util::JsonValue paths(godot_autopilot::util::JsonValue::array_tag);
  for (const std::string &path : created_paths)
    paths.PushBack(godot_autopilot::util::JsonValue(path));
  inner["paths"] = std::move(paths);
  if (!editable_paths.empty()) {
    godot_autopilot::util::JsonValue editables(godot_autopilot::util::JsonValue::array_tag);
    for (const std::string &path : editable_paths)
      editables.PushBack(godot_autopilot::util::JsonValue(path));
    inner["editable_instances"] = std::move(editables);
  }
  if (skipped_editable > 0) {
    inner["note"] = godot_autopilot::util::JsonValue(
        std::to_string(skipped_editable) +
        " copies are not scene instances, editable_instance was not applied");
  }
  inner["undo"] = godot_autopilot::util::JsonValue("delete nodes " + joined_paths(created_paths) +
                                 " (delete_scene_node)");
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

godot_autopilot::util::JsonValue replace_scene_node_impl(const godot_autopilot::util::JsonValue &args,
                                       godot::Node *node,
                                       godot::Node *scene_root) {
  if (!scene_root)
    return edit_action_error("replace", "no edited scene root available");
  if (node == scene_root)
    return edit_action_error(
        "replace", "cannot replace the scene root — replace a child node");
  const godot_autopilot::util::JsonValue *replacement_path_p = args.Find("replacement_path");
  if (!replacement_path_p || !replacement_path_p->IsString() ||
      replacement_path_p->GetString().empty())
    return edit_action_error(
        "replace",
        "missing required parameter for action 'replace': replacement_path");
  const std::string replacement_path = replacement_path_p->GetString();
  std::string hint;
  godot::Node *replacement =
      godot_autopilot::util::resolve_scene_node(replacement_path, scene_root,
                                                &hint);
  if (!replacement)
    return edit_action_error("replace", "replacement node not found: " +
                                            replacement_path + " — " + hint);
  if (replacement == node)
    return edit_action_error(
        "replace", "replacement_path must not be the node being replaced");
  if (replacement->is_ancestor_of(node) || node->is_ancestor_of(replacement))
    return edit_action_error(
        "replace",
        "the replaced node and the replacement must not be ancestors of each "
        "other");
  godot::Node *parent = node->get_parent();
  if (!parent)
    return edit_action_error("replace",
                             "node has no parent to replace it under");
  const std::string old_name = util::to_std(node->get_name());
  const std::string old_path = scene_relative_path(node, scene_root);
  const std::string old_type = util::to_std(node->get_class());
  std::string backup_name;
  for (char c : old_name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-')
      backup_name.push_back(c);
    else
      backup_name.push_back('_');
  }
  if (backup_name.empty())
    backup_name = "node";
  const uint64_t serial = resource_fs::next_file_transaction_serial();
  const std::string backup_path =
      "user://gda_replace_backups/" + backup_name + "-" +
      std::to_string(serial) + ".tscn";
  std::string backup_dir_created;
  std::string backup_dir_error;
  if (!resource_fs::ensure_parent_directory(backup_path, backup_dir_created,
                                            backup_dir_error))
    return edit_action_error("replace",
                             "failed to prepare replacement backup: " +
                                 backup_dir_error);
  godot::Ref<godot::PackedScene> backup;
  backup.instantiate();
  if (backup.is_null())
    return edit_action_error("replace", "failed to create replacement backup");
  const godot::Error backup_pack_error = backup->pack(node);
  if (backup_pack_error != godot::OK)
    return edit_action_error(
        "replace", "failed to pack replaced subtree for backup (error " +
                       std::to_string(static_cast<int>(backup_pack_error)) + ")");
  auto *backup_saver = godot::ResourceSaver::get_singleton();
  if (!backup_saver)
    return edit_action_error("replace",
                             "ResourceSaver not available for backup");
  const godot::Error backup_save_error =
      backup_saver->save(backup, godot::String::utf8(backup_path.c_str()));
  if (backup_save_error != godot::OK)
    return edit_action_error(
        "replace", "failed to save replacement backup (error " +
                       std::to_string(static_cast<int>(backup_save_error)) + ")");
  godot::Node *old_owner = node->get_owner();
  godot::Node *copy = replacement->duplicate(godot::Node::DUPLICATE_DEFAULT);
  if (!copy)
    return edit_action_error("replace", "failed to duplicate replacement node '" +
                                            replacement_path + "'");
  const godot::String copy_scene_file_path = copy->get_scene_file_path();
  copy->set_name(godot::StringName(
      unique_child_name(parent, old_name + "_replacement", node).c_str()));
  auto old_children = node->get_children();
  for (int i = static_cast<int>(old_children.size()) - 1; i >= 0; --i) {
    auto *child = godot::Object::cast_to<godot::Node>(old_children[i]);
    if (!child)
      continue;
    node->remove_child(child);
    memdelete(child);
  }
  node->replace_by(copy, true);
  copy->set_scene_file_path(copy_scene_file_path);
  copy->set_name(godot::StringName(old_name.c_str()));
  godot::Node *effective_owner =
      (old_owner && old_owner != node) ? old_owner : scene_root;
  copy->set_owner(effective_owner);
  set_unowned_descendants(copy, effective_owner);
  if (auto *editor = godot::EditorInterface::get_singleton()) {
    if (auto *selection = editor->get_selection())
      selection->remove_node(node);
  }
  free_detached_node(node);
  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["action"] = godot_autopilot::util::JsonValue("replace");
  inner["path"] = godot_autopilot::util::JsonValue(scene_relative_path(copy, scene_root));
  inner["replaced_path"] = godot_autopilot::util::JsonValue(old_path);
  inner["replacement_source"] = godot_autopilot::util::JsonValue(replacement_path);
  inner["replaced_type"] = godot_autopilot::util::JsonValue(old_type);
  inner["type"] = godot_autopilot::util::JsonValue(util::to_std(copy->get_class()));
  inner["owner"] = godot_autopilot::util::JsonValue(
      effective_owner ? scene_relative_path(effective_owner, scene_root)
                      : std::string());
  inner["destructive"] = godot_autopilot::util::JsonValue(true);
  inner["undoable"] = godot_autopilot::util::JsonValue(false);
  inner["backup_path"] = godot_autopilot::util::JsonValue(backup_path);
  inner["recovery"] = godot_autopilot::util::JsonValue(
      "instantiate backup_path under " +
      scene_relative_path(parent, scene_root) +
      " with create_scene_node action=scene, then remove the replacement; "
      "the editor undo stack cannot restore the replaced subtree");
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

godot_autopilot::util::JsonValue reorder_scene_node_impl(const godot_autopilot::util::JsonValue &args,
                                       godot::Node *node,
                                       godot::Node *scene_root) {
  if (!scene_root)
    return edit_action_error("reorder", "no edited scene root available");
  if (node == scene_root)
    return edit_action_error(
        "reorder", "cannot reorder the scene root — reorder a child node");
  godot::Node *parent = node->get_parent();
  if (!parent)
    return edit_action_error("reorder", "node has no parent to reorder within");
  const godot_autopilot::util::JsonValue *index_p = args.Find("index");
  const godot_autopilot::util::JsonValue *before_p = args.Find("before_path");
  const godot_autopilot::util::JsonValue *after_p = args.Find("after_path");
  int provided = (index_p ? 1 : 0) + (before_p ? 1 : 0) + (after_p ? 1 : 0);
  if (provided != 1)
    return edit_action_error(
        "reorder",
        "provide exactly one of index, before_path or after_path");
  const std::string node_path = scene_relative_path(node, scene_root);
  const int old_index = node->get_index();
  const int child_count = parent->get_child_count();
  int target_index = old_index;
  if (index_p) {
    if (!index_p->IsInt() || index_p->GetInt() < 0 ||
        index_p->GetInt() >= child_count)
      return edit_action_error(
          "reorder", "parameter 'index' must be an integer between 0 and " +
                         std::to_string(child_count - 1));
    target_index = static_cast<int>(index_p->GetInt());
  } else {
    const godot_autopilot::util::JsonValue *sibling_p = before_p ? before_p : after_p;
    if (!sibling_p->IsString() || sibling_p->GetString().empty())
      return edit_action_error(
          "reorder", std::string("parameter '") +
                         (before_p ? "before_path" : "after_path") +
                         "' must be a non-empty node path");
    const std::string sibling_path = sibling_p->GetString();
    std::string hint;
    godot::Node *sibling =
        godot_autopilot::util::resolve_scene_node(sibling_path, scene_root,
                                                  &hint);
    if (!sibling)
      return edit_action_error("reorder", "sibling node not found: " +
                                              sibling_path + " — " + hint);
    if (sibling == node)
      return edit_action_error(
          "reorder", "the sibling path must not be the node being reordered");
    if (sibling->get_parent() != parent)
      return edit_action_error(
          "reorder", "sibling '" + sibling_path +
                         "' is not a child of the same parent");
    const int sibling_index = sibling->get_index();
    if (before_p) {
      target_index =
          old_index < sibling_index ? sibling_index - 1 : sibling_index;
    } else {
      target_index =
          old_index < sibling_index ? sibling_index : sibling_index + 1;
    }
  }
  if (target_index == old_index) {
    godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
    inner["action"] = godot_autopilot::util::JsonValue("reorder");
    inner["path"] = godot_autopilot::util::JsonValue(node_path);
    inner["old_index"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(old_index));
    inner["new_index"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(old_index));
    inner["moved"] = godot_autopilot::util::JsonValue(false);
    util::add_scene_info_fields(inner, util::edited_scene_info());
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = std::move(inner);
    util::add_scene_info_fields(r, util::edited_scene_info());
    return r;
  }
  parent->move_child(node, target_index);
  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["action"] = godot_autopilot::util::JsonValue("reorder");
  inner["path"] = godot_autopilot::util::JsonValue(node_path);
  inner["parent_path"] =
      godot_autopilot::util::JsonValue(scene_relative_path(parent, scene_root));
  inner["old_index"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(old_index));
  inner["new_index"] =
      godot_autopilot::util::JsonValue(static_cast<int64_t>(node->get_index()));
  inner["moved"] = godot_autopilot::util::JsonValue(true);
  inner["undo"] = godot_autopilot::util::JsonValue("reorder " + node_path + " back to index " +
                                 std::to_string(old_index) +
                                 " (edit_scene_node action=reorder)");
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

struct PackOwnerChange {
  godot::Node *node;
  godot::Node *owner;
};

godot_autopilot::util::JsonValue pack_owner_chain_error(const std::string &reason,
                                      const std::string &node_path,
                                      const std::string &owner_path) {
  godot_autopilot::util::JsonValue e = util::error_json(reason);
  e["code"] = godot_autopilot::util::JsonValue("owner_chain_invalid");
  godot_autopilot::util::JsonValue details(godot_autopilot::util::JsonValue::object_tag);
  details["code"] = godot_autopilot::util::JsonValue("owner_chain_invalid");
  details["node_path"] = godot_autopilot::util::JsonValue(node_path);
  details["owner_path"] = godot_autopilot::util::JsonValue(owner_path);
  details["reason"] = godot_autopilot::util::JsonValue(reason);
  e["structured_error"] = std::move(details);
  return e;
}

bool collect_pack_owner_changes(godot::Node *pack_root,
                                godot::Node *scene_root, godot::Node *node,
                                std::vector<PackOwnerChange> &changes,
                                godot_autopilot::util::JsonValue &error) {
  if (node != pack_root) {
    godot::Node *owner = node->get_owner();
    const std::string node_path = scene_relative_path(node, scene_root);
    if (!owner) {
      error = pack_owner_chain_error(
          "node has no owner and would not be saved: " + node_path, node_path,
          "");
      return false;
    }
    if (owner == scene_root) {
      changes.push_back(PackOwnerChange{node, owner});
    } else if (owner != pack_root && !pack_root->is_ancestor_of(owner)) {
      const std::string owner_path = scene_relative_path(owner, scene_root);
      error = pack_owner_chain_error(
          "node is owned by a node outside the packed subtree: " + node_path +
              " (owner " + owner_path + ")",
          node_path, owner_path);
      return false;
    }
  }
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child && !collect_pack_owner_changes(pack_root, scene_root, child,
                                             changes, error))
      return false;
  }
  return true;
}

} // namespace

std::string scene_relative_path(godot::Node *node, godot::Node *scene_root) {
  if (!node)
    return "";
  if (!scene_root)
    return util::to_std(node->get_name());
  std::string abs_path = util::to_std(node->get_path());
  std::string root_pref = util::to_std(scene_root->get_path());
  if (abs_path == root_pref)
    return util::to_std(node->get_name());
  if (abs_path.find(root_pref + "/") == 0)
    return abs_path.substr(root_pref.size() + 1);
  return abs_path;
}

godot_autopilot::util::JsonValue handle_create(const godot_autopilot::util::JsonValue &args) {
  std::string name = "NewNode";
  auto *n = args.Find("name");
  if (n && n->IsString())
    name = n->GetString();

  std::string type = "Node";
  auto *t = args.Find("type");
  if (t && t->IsString())
    type = t->GetString();

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("ClassDB singleton not available");
    return e;
  }

  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Node"))) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(type + " is not a Node subclass");
    return e;
  }

  auto *props_it = args.Find("properties");
  if (props_it && !props_it->IsObject()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("parameter 'properties' must be an object mapping "
                       "property names to values");
    return e;
  }

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to instantiate node type: " + type);
    return e;
  }

  auto *obj = godot::Object::cast_to<godot::Node>(obj_var);
  if (!obj) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("instantiated object is not a Node: " + type);
    return e;
  }

  obj->set_name(godot::StringName(name.c_str()));

  auto *pp = args.Find("parent_path");
  bool has_parent = pp && pp->IsString() && !pp->GetString().empty();

  auto *editor = godot::EditorInterface::get_singleton();

  if (has_parent) {
    std::string parent_path = pp->GetString();
    std::string hint;
    auto *root = editor ? editor->get_edited_scene_root() : nullptr;
    auto *parent =
        godot_autopilot::util::resolve_scene_node(parent_path, root, &hint);
    if (!parent) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("parent node not found: " + parent_path +
                                  " — " + hint);
      return startup_gated_error(editor_in_startup_window(), e);
    }
    parent->add_child(obj);
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root)
      obj->set_owner(scene_root);
  } else {
    if (!editor) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
      return e;
    }
    auto *existing_root = editor->get_edited_scene_root();
    if (existing_root) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "scene already has a root, use parent_path to add children");
      return e;
    }
    editor->add_root_node(obj);
  }

  std::vector<std::string> applied_properties;
  std::vector<godot_autopilot::util::JsonValue> property_warnings;
  std::vector<godot_autopilot::util::JsonValue> inline_resources;
  if (props_it) {
    auto *scene_root_now =
        editor ? editor->get_edited_scene_root() : nullptr;
    std::string node_rel_path = scene_relative_path(obj, scene_root_now);

    auto apply_property = [&](const std::string &prop_name,
                              const godot_autopilot::util::JsonValue &prop_value) {
      godot_autopilot::util::JsonValue prop_args(godot_autopilot::util::JsonValue::object_tag);
      prop_args["path"] = godot_autopilot::util::JsonValue(node_rel_path);
      prop_args["property"] = godot_autopilot::util::JsonValue(prop_name);
      prop_args["value"] = prop_value;
      godot_autopilot::util::JsonValue prop_result;
      try {
        prop_result = godot_autopilot::property_ops::handle_set(prop_args);
      } catch (const std::exception &) {
        if (godot::Node *cur_parent = obj->get_parent())
          cur_parent->remove_child(obj);
        memdelete(obj);
        throw;
      } catch (...) {
        if (godot::Node *cur_parent = obj->get_parent())
          cur_parent->remove_child(obj);
        memdelete(obj);
        throw;
      }
      auto *err = prop_result.Find("error");
      if (err) {
        if (godot::Node *cur_parent = obj->get_parent())
          cur_parent->remove_child(obj);
        memdelete(obj);
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] =
            godot_autopilot::util::JsonValue("failed to apply property '" + prop_name +
                           "' on created node: " + err->GetString());
        return e;
      }
      auto *warn = prop_result.Find("warning");
      if (warn && warn->IsString()) {
        godot_autopilot::util::JsonValue w(godot_autopilot::util::JsonValue::object_tag);
        w["path"] = godot_autopilot::util::JsonValue(node_rel_path);
        w["property"] = godot_autopilot::util::JsonValue(prop_name);
        w["warning"] = godot_autopilot::util::JsonValue(warn->GetString());
        property_warnings.push_back(std::move(w));
      }
      auto *inline_rec = prop_result.Find("inline_resources");
      if (inline_rec && inline_rec->IsArray()) {
        for (const auto &entry : inline_rec->GetArray()) {
          inline_resources.push_back(entry);
        }
      }
      applied_properties.push_back(prop_name);
      return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
    };

    std::vector<std::string> object_props;
    for (const auto &kv : *props_it) {
      if (property_value_is_object_typed(obj, kv.first, kv.second))
        object_props.push_back(kv.first);
    }
    for (int pass = 0; pass < 2; pass++) {
      for (const auto &kv : *props_it) {
        const bool object_typed =
            std::find(object_props.begin(), object_props.end(), kv.first) !=
            object_props.end();
        if (object_typed != (pass == 0))
          continue;
        godot_autopilot::util::JsonValue apply_error = apply_property(kv.first, kv.second);
        if (apply_error.Find("error"))
          return apply_error;
      }
    }
  }

  if (editor) {
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root) {
      godot::StringName type_sn(type.c_str());
      if (cdbs->is_parent_class(type_sn, godot::StringName("Node2D"))) {
        editor->set_main_screen_editor(godot::String("2D"));
      } else if (cdbs->is_parent_class(type_sn, godot::StringName("Node3D"))) {
        editor->set_main_screen_editor(godot::String("3D"));
      }
    }
  }

  std::string result_path = name;
  if (editor) {
    auto *scene_root = editor->get_edited_scene_root();
    result_path = scene_relative_path(obj, scene_root);
  }

  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["path"] = godot_autopilot::util::JsonValue(result_path);
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue applied(godot_autopilot::util::JsonValue::array_tag);
  for (const auto &applied_name : applied_properties)
    applied.PushBack(godot_autopilot::util::JsonValue(applied_name));
  inner["applied_properties"] = std::move(applied);
  if (!inline_resources.empty()) {
    godot_autopilot::util::JsonValue inlines(godot_autopilot::util::JsonValue::array_tag);
    for (auto &entry : inline_resources)
      inlines.PushBack(std::move(entry));
    inner["inline_resources"] = std::move(inlines);
  }
  if (!property_warnings.empty()) {
    godot_autopilot::util::JsonValue warnings(godot_autopilot::util::JsonValue::array_tag);
    for (auto &w : property_warnings)
      warnings.PushBack(std::move(w));
    inner["property_warnings"] = std::move(warnings);
  }
  inner["undo"] =
      godot_autopilot::util::JsonValue("delete node " + result_path + " (delete_scene_node)");
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  if (!scene_info.has_scene) {
    r["scene_info_note"] = godot_autopilot::util::JsonValue(
        "no edited scene root — scene_path is empty and the write target is "
        "unknown");
  }
  return r;
}

godot_autopilot::util::JsonValue handle_create_scene_node(const godot_autopilot::util::JsonValue &args) {
  std::string mode = "node";
  if (const godot_autopilot::util::JsonValue *mode_p = args.Find("mode")) {
    if (!mode_p->IsString() || mode_p->GetString().empty()) {
      return util::error_json(
          "parameter 'mode' must be a non-empty string: node, scene, spec, "
          "bundle or path");
    }
    mode = mode_p->GetString();
  }
  if (mode == "node")
    return handle_create(args);
  if (mode == "scene")
    return handle_instance(args);
  if (mode == "spec")
    return scene_spec_ops::handle_build_from_spec(args);
  if (mode == "bundle")
    return scene_spec_ops::handle_spawn_bundle(args);
  if (mode == "path")
    return handle_create_path_node(args);
  return util::error_json("unknown mode '" + mode +
                          "' for create_scene_node — expected node, scene, "
                          "spec, bundle or path");
}

godot_autopilot::util::JsonValue handle_edit_scene_node(const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *action_p = args.Find("action");
  if (action_p == nullptr || !action_p->IsString() ||
      action_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: action (one of: rename, reparent, "
        "advance_path, duplicate, reorder, replace)");
  }
  const std::string action = action_p->GetString();
  if (action == "rename")
    return handle_rename(args);
  if (action == "reparent")
    return handle_reparent(args);
  if (action == "advance_path")
    return handle_advance_path_follow(args);
  if (action != "duplicate" && action != "reorder" && action != "replace") {
    return util::error_json("unknown action '" + action +
                            "' for edit_scene_node — expected rename, "
                            "reparent, advance_path, duplicate, reorder or "
                            "replace");
  }
  const godot_autopilot::util::JsonValue *path_p = args.Find("path");
  if (path_p == nullptr || !path_p->IsString() ||
      path_p->GetString().empty()) {
    return util::error_json("missing required parameter for action '" +
                            action + "': path");
  }
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  godot::Node *node = godot_autopilot::util::resolve_scene_node(
      path_p->GetString(), scene_root, &hint);
  if (!node) {
    return util::error_json("node not found: " + path_p->GetString() + " — " +
                            hint);
  }
  if (action == "duplicate")
    return duplicate_scene_node_impl(args, node, scene_root);
  if (action == "replace") {
    const godot_autopilot::util::JsonValue *confirm_p = args.Find("confirm");
    const bool confirmed = confirm_p != nullptr && confirm_p->IsBool() &&
                           confirm_p->GetBool();
    if (scene_safety::check_replace_confirm(confirmed) !=
        scene_safety::ReplaceConfirmFailure::kNone) {
      godot_autopilot::util::JsonValue e = util::error_json(
          "action 'replace' permanently removes the old subtree and is not "
          "undoable through the editor undo stack; pass confirm=true to proceed");
      e["code"] = godot_autopilot::util::JsonValue("confirmation_required");
      e["confirm_required"] = godot_autopilot::util::JsonValue(true);
      e["side_effect"] = godot_autopilot::util::JsonValue("destructive");
      return e;
    }
    return replace_scene_node_impl(args, node, scene_root);
  }
  return reorder_scene_node_impl(args, node, scene_root);
}

godot_autopilot::util::JsonValue handle_scene_camera(const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *action_p = args.Find("action");
  if (action_p == nullptr || !action_p->IsString() ||
      action_p->GetString().empty()) {
    return util::error_json(
        "missing required parameter: action (one of: focus, set_params, "
        "project, unproject, project_ray, behind)");
  }
  const std::string action = action_p->GetString();
  if (action == "focus")
    return handle_focus_camera_on_node(args);
  if (action == "set_params")
    return handle_set_camera_params(args);
  if (action == "project" || action == "unproject" ||
      action == "project_ray" || action == "behind")
    return handle_camera_projection(args);
  return util::error_json(
      "unknown action '" + action +
      "' for scene_camera — expected focus, set_params, project, unproject, "
      "project_ray or behind");
}

godot_autopilot::util::JsonValue handle_delete(const godot_autopilot::util::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = p->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }

  if (node == scene_root) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "cannot delete the scene root node — use close_editor_scene to close "
        "the scene, then create_editor_scene");
    return e;
  }

  std::string node_name = util::to_std(node->get_name());
  std::string node_type = util::to_std(node->get_class());
  std::string parent_path;
  auto *parent = node->get_parent();
  int child_index = node->get_index();
  godot::Node *old_owner = node->get_owner();
  if (parent) {
    std::string abs_parent = util::to_std(parent->get_path());
    if (scene_root) {
      std::string root_pref = util::to_std(scene_root->get_path());
      if (abs_parent == root_pref) {
        parent_path = util::to_std(parent->get_name());
      } else if (abs_parent.find(root_pref + "/") == 0) {
        parent_path = abs_parent.substr(root_pref.size() + 1);
      } else {
        parent_path = abs_parent;
      }
    } else {
      parent_path = abs_parent;
    }
  }

  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo && parent) {
    undo_redo->create_action(godot::String(("Delete Node " + node_name).c_str()));
    undo_redo->add_do_method(parent, godot::StringName("remove_child"), node);
    undo_redo->add_undo_method(parent, godot::StringName("add_child"), node);
    undo_redo->add_undo_method(parent, godot::StringName("move_child"), node,
                               child_index);
    undo_redo->add_undo_method(node, godot::StringName("set_owner"),
                               old_owner);
    undo_redo->add_undo_reference(node);
    undo_redo->commit_action();
  } else {
    node->queue_free();
  }

  if (editor) {
    if (auto *sel = editor->get_selection()) {
      sel->remove_node(node);
    }
  }

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("deleted");
  r["undoable"] = godot_autopilot::util::JsonValue(undo_redo != nullptr && parent != nullptr);
  r["note"] = godot_autopilot::util::JsonValue(
      "undo restores the node via add_child/move_child/set_owner (native "
      "SceneTreeDock order); the node is kept alive by an undo reference, so "
      "undo works across frames");
  godot_autopilot::util::JsonValue undo_info(godot_autopilot::util::JsonValue::object_tag);
  undo_info["name"] = godot_autopilot::util::JsonValue(node_name);
  undo_info["type"] = godot_autopilot::util::JsonValue(node_type);
  undo_info["parent_path"] = godot_autopilot::util::JsonValue(parent_path);
  undo_info["hint"] =
      godot_autopilot::util::JsonValue("recreate node " + node_name + " (" + node_type +
                     ") under " + parent_path + " (create_scene_node)");
  r["undo"] = std::move(undo_info);
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

godot_autopilot::util::JsonValue handle_rename(const godot_autopilot::util::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  auto *nn = args.Find("new_name");
  if (!nn || !nn->IsString() || nn->GetString().empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing or empty required parameter: new_name");
    return e;
  }
  std::string path = p->GetString();
  std::string new_name = nn->GetString();
  if (new_name.find('/') != std::string::npos ||
      new_name.find(':') != std::string::npos) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("invalid new_name '" + new_name +
                       "' — '/' and ':' are not allowed in node names");
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }

  std::string old_name = util::to_std(node->get_name());
  if (godot::Node *par = node->get_parent()) {
    auto children = par->get_children();
    for (int i = 0; i < children.size(); i++) {
      auto *sibling = godot::Object::cast_to<godot::Node>(children[i]);
      if (sibling && sibling != node &&
          util::to_std(sibling->get_name()) == new_name) {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue(
            "a sibling named '" + new_name + "' already exists under '" +
            util::to_std(par->get_name()) +
            "' — node names must be unique among siblings");
        return e;
      }
    }
  }

  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action(godot::String(("Rename Node " + old_name).c_str()));
    undo_redo->add_do_method(node, godot::StringName("set_name"),
                             godot::StringName(new_name.c_str()));
    undo_redo->add_undo_method(node, godot::StringName("set_name"),
                               node->get_name());
    undo_redo->commit_action();
  } else {
    node->set_name(godot::StringName(new_name.c_str()));
  }

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("renamed");
  r["undoable"] = godot_autopilot::util::JsonValue(undo_redo != nullptr);
  r["old_name"] = godot_autopilot::util::JsonValue(old_name);
  r["name"] = godot_autopilot::util::JsonValue(util::to_std(node->get_name()));
  r["path"] =
      godot_autopilot::util::JsonValue(scene_relative_path(node, scene_root));
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

godot_autopilot::util::JsonValue handle_reparent(const godot_autopilot::util::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  auto *npp = args.Find("new_parent_path");
  if (!npp || !npp->IsString() || npp->GetString().empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("missing required parameter: new_parent_path");
    return e;
  }
  std::string path = p->GetString();
  std::string new_parent_path = npp->GetString();

  bool keep_world = false;
  auto *kwp = args.Find("keep_world_position");
  if (kwp && kwp->IsBool())
    keep_world = kwp->GetBool();

  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;

  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }
  if (node == scene_root) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue(
        "cannot reparent the scene root node — the root must stay at the top "
        "of the edited scene");
    return e;
  }

  auto *new_parent = godot_autopilot::util::resolve_scene_node(
      new_parent_path, scene_root, &hint);
  if (!new_parent) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("parent node not found: " + new_parent_path +
                                " — " + hint);
    return e;
  }
  if (new_parent == node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("cannot reparent a node under itself: " + path);
    return e;
  }
  for (godot::Node *anc = new_parent->get_parent(); anc; anc = anc->get_parent()) {
    if (anc == node) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "cannot reparent a node under one of its own descendants: " +
          new_parent_path);
      return e;
    }
  }
  auto *old_parent = node->get_parent();
  if (!old_parent) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] =
        godot_autopilot::util::JsonValue("node has no parent to detach from: " + path);
    return e;
  }
  if (new_parent == old_parent) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("node is already a child of '" +
                                util::to_std(old_parent->get_name()) + "': " +
                                path);
    return e;
  }

  int old_index = node->get_index();
  godot::Node *old_owner = node->get_owner();

  godot::Node2D *node_2d = godot::Object::cast_to<godot::Node2D>(node);
  godot::Node3D *node_3d = godot::Object::cast_to<godot::Node3D>(node);
  godot::Node2D *parent_2d =
      godot::Object::cast_to<godot::Node2D>(new_parent);
  godot::Node3D *parent_3d =
      godot::Object::cast_to<godot::Node3D>(new_parent);

  bool transform_managed = false;
  godot::Variant old_local_var;
  godot::Variant new_local_var;
  if (keep_world && node_2d && parent_2d) {
    old_local_var = godot::Variant(node_2d->get_transform());
    new_local_var =
        godot::Variant(parent_2d->get_global_transform().affine_inverse() *
                       node_2d->get_global_transform());
    transform_managed = true;
  } else if (keep_world && node_3d && parent_3d) {
    old_local_var = godot::Variant(node_3d->get_transform());
    new_local_var =
        godot::Variant(parent_3d->get_global_transform().affine_inverse() *
                       node_3d->get_global_transform());
    transform_managed = true;
  }

  std::string node_name = util::to_std(node->get_name());
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action(godot::String(("Reparent Node " + node_name).c_str()));
    undo_redo->add_do_method(old_parent, godot::StringName("remove_child"),
                             node);
    undo_redo->add_do_method(new_parent, godot::StringName("add_child"), node);
    if (transform_managed)
      undo_redo->add_do_method(node, godot::StringName("set_transform"),
                               new_local_var);
    undo_redo->add_do_method(node, godot::StringName("set_owner"),
                             old_owner);
    for (ReparentUndoStep step : reparent_undo_order(transform_managed)) {
      switch (step) {
        case ReparentUndoStep::kDetachFromNewParent:
          undo_redo->add_undo_method(new_parent,
                                     godot::StringName("remove_child"), node);
          break;
        case ReparentUndoStep::kAttachToOldParent:
          undo_redo->add_undo_method(old_parent,
                                     godot::StringName("add_child"), node);
          break;
        case ReparentUndoStep::kMoveChildToOldIndex:
          undo_redo->add_undo_method(old_parent,
                                     godot::StringName("move_child"), node,
                                     old_index);
          break;
        case ReparentUndoStep::kRestoreTransform:
          undo_redo->add_undo_method(node, godot::StringName("set_transform"),
                                     old_local_var);
          break;
        case ReparentUndoStep::kRestoreOwner:
          undo_redo->add_undo_method(node, godot::StringName("set_owner"),
                                     old_owner);
          break;
      }
    }
    undo_redo->add_undo_reference(node);
    undo_redo->commit_action();
  } else {
    old_parent->remove_child(node);
    new_parent->add_child(node);
    if (transform_managed) {
      if (node_2d)
        node_2d->set_transform(
            static_cast<godot::Transform2D>(new_local_var));
      else if (node_3d)
        node_3d->set_transform(
            static_cast<godot::Transform3D>(new_local_var));
    }
    node->set_owner(old_owner);
  }

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue("reparented");
  r["undoable"] = godot_autopilot::util::JsonValue(undo_redo != nullptr);
  r["path"] = godot_autopilot::util::JsonValue(scene_relative_path(node, scene_root));
  r["old_parent_path"] =
      godot_autopilot::util::JsonValue(scene_relative_path(old_parent, scene_root));
  r["new_parent_path"] = godot_autopilot::util::JsonValue(new_parent_path);
  r["note"] = godot_autopilot::util::JsonValue(
      std::string("if the node's previous owner was the edited scene root it "
                  "stays owned by the current scene root so it is saved with "
                  "the scene; keep_world_position applies only when the node "
                  "and the new parent are both Node2D or both Node3D") +
      (transform_managed ? "" : " (ignored here)"));
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

godot_autopilot::util::JsonValue handle_instance(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "instantiate_scene called");

  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = p->GetString();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
  auto *packed_scene = godot::Object::cast_to<godot::PackedScene>(res.ptr());
  if (!packed_scene) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to load packed scene: " + path);
    return e;
  }

  godot::Node *instance = packed_scene->instantiate();
  if (!instance) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to instantiate scene: " + path);
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;

  auto *pp = args.Find("parent_path");
  godot::Node *parent = nullptr;
  if (pp && pp->IsString() && !pp->GetString().empty()) {
    std::string hint;
    parent = godot_autopilot::util::resolve_scene_node(pp->GetString(),
                                                          scene_root, &hint);
    if (!parent) {
      memdelete(instance);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("parent node not found: " + pp->GetString() +
                                  " — " + hint);
      return e;
    }
  } else if (!scene_root) {
    memdelete(instance);
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("no scene open");
    return e;
  } else {
    parent = scene_root;
  }

  auto *n = args.Find("name");
  if (n && n->IsString() && !n->GetString().empty()) {
    instance->set_name(godot::StringName(n->GetString().c_str()));
  }

  int persist_skipped = 0;
  int persist_connected = 0;
  int persist_unresolved = 0;
  audit_instance_persist_connections(packed_scene, instance, persist_skipped,
                                     persist_connected, persist_unresolved);

  bool editable_instance = false;
  if (auto *ei = args.Find("editable_instance")) {
    if (!ei->IsBool()) {
      memdelete(instance);
      return util::error_json(
          "parameter 'editable_instance' must be a boolean");
    }
    editable_instance = ei->GetBool();
  }
  if (editable_instance && instance->get_scene_file_path().is_empty()) {
    memdelete(instance);
    return util::error_json(
        "editable_instance requires a scene instantiated from a file path");
  }

  parent->add_child(instance);

  bool set_owner = true;
  auto *o = args.Find("owner");
  if (o && o->IsBool())
    set_owner = o->GetBool();
  if (set_owner && scene_root) {
    instance->set_owner(scene_root);
  }
  if (editable_instance && scene_root) {
    scene_root->set_editable_instance(instance, true);
  }

  std::string instance_name = util::to_std(instance->get_name());
  std::string instance_type = util::to_std(instance->get_class());
  std::string result_path = instance_name;
  if (scene_root) {
    std::string abs_path = util::to_std(instance->get_path());
    std::string root_pref = util::to_std(scene_root->get_path());
    if (abs_path == root_pref) {
      result_path = instance_name;
    } else if (abs_path.find(root_pref + "/") == 0) {
      result_path = abs_path.substr(root_pref.size() + 1);
    } else {
      result_path = abs_path;
    }
  }

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["path"] = godot_autopilot::util::JsonValue(result_path);
  inner["name"] = godot_autopilot::util::JsonValue(instance_name);
  inner["type"] = godot_autopilot::util::JsonValue(instance_type);
  if (editable_instance)
    inner["editable_instance"] = godot_autopilot::util::JsonValue(true);
  inner["undo"] =
      godot_autopilot::util::JsonValue("delete node " + result_path + " (delete_scene_node)");
  r["result"] = std::move(inner);
  r["note"] = godot_autopilot::util::JsonValue(
      "instance inherits CONNECT_PERSIST connections saved in its source "
      "scene; do not reconnect the same signal+callable pairs on instances to "
      "avoid duplicate-connection errors; persist audit on this instance: " +
      std::to_string(persist_skipped) + " already connected (skipped), " +
      std::to_string(persist_connected) + " missing and reconnected, " +
      std::to_string(persist_unresolved) + " unresolvable");

  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "instantiate_scene completed");
  return r;
}

godot_autopilot::util::JsonValue handle_pack_scene(const godot_autopilot::util::JsonValue &args) {
  const godot_autopilot::util::JsonValue *node_path_p = args.Find("node_path");
  if (!node_path_p || !node_path_p->IsString() ||
      node_path_p->GetString().empty())
    return util::error_json("missing required parameter: node_path");
  const godot_autopilot::util::JsonValue *dest_path_p = args.Find("dest_path");
  if (!dest_path_p || !dest_path_p->IsString() ||
      dest_path_p->GetString().empty())
    return util::error_json("missing required parameter: dest_path");
  bool overwrite = false;
  if (auto *overwrite_p = args.Find("overwrite")) {
    if (!overwrite_p->IsBool())
      return util::error_json("parameter 'overwrite' must be a boolean");
    overwrite = overwrite_p->GetBool();
  }
  const util::ProjectPath checked =
      util::normalize_project_path(dest_path_p->GetString(), false, false);
  if (!checked.valid())
    return util::error_detail("destination path rejected", "dest_path",
                              checked.error,
                              "use a res:// path below the project root");
  const std::string dest_path = checked.value;
  const bool tscn_ext =
      dest_path.size() >= 5 &&
      dest_path.compare(dest_path.size() - 5, 5, ".tscn") == 0;
  const bool scn_ext =
      dest_path.size() >= 4 &&
      dest_path.compare(dest_path.size() - 4, 4, ".scn") == 0;
  if (!tscn_ext && !scn_ext)
    return util::error_json(
        "parameter 'dest_path' must end with .tscn or .scn");
  const std::string node_path = node_path_p->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor ? editor->get_edited_scene_root() : nullptr;
  if (!scene_root)
    return util::error_detail(
        "no scene currently open in the editor", "pack_scene",
        "an edited scene",
        "open or create a scene first (create_editor_scene)");
  std::string hint;
  godot::Node *pack_root =
      godot_autopilot::util::resolve_scene_node(node_path, scene_root, &hint);
  if (!pack_root)
    return util::error_json("node not found: " + node_path + " — " + hint);
  if (pack_root == scene_root)
    return util::error_json(
        "cannot pack the edited scene root — pass a child node_path");
  if (pack_root->get_child_count() == 0)
    return util::error_json("cannot pack an empty subtree: '" + node_path +
                            "' has no children");
  std::vector<PackOwnerChange> owner_changes;
  godot_autopilot::util::JsonValue owner_error;
  if (!collect_pack_owner_changes(pack_root, scene_root, pack_root,
                                  owner_changes, owner_error))
    return owner_error;
  const bool existed =
      godot::FileAccess::file_exists(godot::String(dest_path.c_str()));
  if (existed && !overwrite) {
    godot_autopilot::util::JsonValue e = util::error_json(
        "destination scene already exists: " + dest_path +
        " — pass overwrite=true to replace it");
    e["code"] = godot_autopilot::util::JsonValue("file_exists");
    godot_autopilot::util::JsonValue details(godot_autopilot::util::JsonValue::object_tag);
    details["code"] = godot_autopilot::util::JsonValue("file_exists");
    details["path"] = godot_autopilot::util::JsonValue(dest_path);
    e["structured_error"] = std::move(details);
    return e;
  }
  godot::Ref<godot::PackedScene> packed;
  packed.instantiate();
  if (packed.is_null())
    return util::error_json("failed to create a PackedScene resource");
  for (const PackOwnerChange &change : owner_changes)
    change.node->set_owner(pack_root);
  const godot::Error pack_error = packed->pack(pack_root);
  for (const PackOwnerChange &change : owner_changes)
    change.node->set_owner(change.owner);
  if (pack_error != godot::OK) {
    godot_autopilot::util::JsonValue e = util::error_json(
        "PackedScene::pack failed for '" + node_path + "' with error " +
        std::to_string(static_cast<int>(pack_error)));
    e["code"] = godot_autopilot::util::JsonValue("pack_failed");
    return e;
  }
  godot::Ref<godot::SceneState> state = packed->get_state();
  const int64_t node_count =
      state.is_valid() ? static_cast<int64_t>(state->get_node_count()) : 0;
  const int64_t connection_count =
      state.is_valid() ? static_cast<int64_t>(state->get_connection_count())
                       : 0;
  bool dirs_created = false;
  const size_t slash = dest_path.find_last_of('/');
  if (slash != std::string::npos) {
    const std::string dir = dest_path.substr(0, slash);
    if (dir != "res:" && dir != "res:/" && dir != "res://" &&
        !godot::DirAccess::dir_exists_absolute(godot::String(dir.c_str()))) {
      const godot::Error mkdir_error =
          godot::DirAccess::make_dir_recursive_absolute(
              godot::String(dir.c_str()));
      if (mkdir_error != godot::OK) {
        godot_autopilot::util::JsonValue e = util::error_json(
            "failed to create destination directory: " + dir + " (error " +
            std::to_string(static_cast<int>(mkdir_error)) + ")");
        e["code"] = godot_autopilot::util::JsonValue("directory_create_failed");
        return e;
      }
      dirs_created = true;
    }
  }
  auto *saver = godot::ResourceSaver::get_singleton();
  if (!saver)
    return util::error_json("ResourceSaver not available");
  resource_fs::GodotFileTransactionFs fs;
  const uint64_t serial = resource_fs::next_file_transaction_serial();
  const std::string temp_path =
      dest_path + ".gda-pack-tmp-" + std::to_string(serial);
  const std::string backup_path =
      dest_path + ".gda-pack-bak-" + std::to_string(serial);
  const godot::Error save_error =
      saver->save(packed, godot::String(temp_path.c_str()));
  if (save_error != godot::OK) {
    godot_autopilot::util::JsonValue e = util::error_json(
        "failed to stage packed scene at " + temp_path + " (error " +
        std::to_string(static_cast<int>(save_error)) + ")");
    e["code"] = godot_autopilot::util::JsonValue("save_failed");
    return e;
  }
  bool verified = false;
  std::string verify_error;
  if (auto *loader = godot::ResourceLoader::get_singleton()) {
    godot::Ref<godot::Resource> reloaded = loader->load(
        godot::String(temp_path.c_str()), godot::String(),
        godot::ResourceLoader::CACHE_MODE_IGNORE);
    auto *reloaded_scene =
        godot::Object::cast_to<godot::PackedScene>(reloaded.ptr());
    if (!reloaded_scene) {
      verify_error = "staged file is not a PackedScene";
    } else {
      godot::Node *instance = reloaded_scene->instantiate();
      if (!instance) {
        verify_error = "staged PackedScene could not instantiate";
      } else {
        memdelete(instance);
        verified = true;
      }
    }
  } else {
    verify_error = "ResourceLoader not available";
  }
  std::string ignored_error;
  if (scene_safety::pack_recovery_action(existed, verified, true) ==
      scene_safety::PackRecoveryAction::kAbortBeforeCommit) {
    fs.remove_file(temp_path, ignored_error);
    godot_autopilot::util::JsonValue e = util::error_json(
        "packed scene failed verification; original file left unchanged: " +
        verify_error);
    e["code"] = godot_autopilot::util::JsonValue("verification_failed");
    e["verified"] = godot_autopilot::util::JsonValue(false);
    e["original_preserved"] = godot_autopilot::util::JsonValue(existed);
    return e;
  }
  bool backup_created = false;
  if (existed) {
    std::string backup_error;
    if (!fs.rename_unique(dest_path, backup_path, backup_error)) {
      fs.remove_file(temp_path, ignored_error);
      godot_autopilot::util::JsonValue e = util::error_json(
          "failed to back up destination before overwrite: " + backup_error);
      e["code"] = godot_autopilot::util::JsonValue("backup_failed");
      return e;
    }
    backup_created = true;
  }
  std::string commit_error;
  bool commit_ok = fs.commit_staged(temp_path, dest_path, commit_error);
  if (!commit_ok) {
    std::vector<std::string> compensation_errors;
    if (backup_created) {
      std::string restore_error;
      if (!fs.commit_staged(backup_path, dest_path, restore_error))
        compensation_errors.push_back(restore_error);
    }
    fs.remove_file(temp_path, ignored_error);
    godot_autopilot::util::JsonValue e = util::error_json(
        "failed to commit packed scene to " + dest_path + ": " + commit_error);
    e["code"] = godot_autopilot::util::JsonValue("write_failed");
    e["restored"] = godot_autopilot::util::JsonValue(backup_created && compensation_errors.empty());
    if (!compensation_errors.empty()) {
      godot_autopilot::util::JsonValue compensation(godot_autopilot::util::JsonValue::array_tag);
      for (const std::string &item : compensation_errors)
        compensation.PushBack(godot_autopilot::util::JsonValue(item));
      e["compensation_errors"] = std::move(compensation);
    }
    return e;
  }
  if (backup_created)
    fs.remove_file(backup_path, ignored_error);
  if (auto *efs = editor->get_resource_filesystem())
    efs->update_file(godot::String(dest_path.c_str()));
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["path"] = godot_autopilot::util::JsonValue(dest_path);
  inner["node_path"] = godot_autopilot::util::JsonValue(scene_relative_path(pack_root, scene_root));
  inner["node_count"] = godot_autopilot::util::JsonValue(node_count);
  inner["connection_count"] = godot_autopilot::util::JsonValue(connection_count);
  inner["verified"] = godot_autopilot::util::JsonValue(verified);
  if (!verified)
    inner["verification_error"] = godot_autopilot::util::JsonValue(verify_error);
  inner["overwritten"] = godot_autopilot::util::JsonValue(existed);
  inner["backup_created"] = godot_autopilot::util::JsonValue(backup_created);
  inner["restored"] = godot_autopilot::util::JsonValue(false);
  if (dirs_created)
    inner["directories_created"] = godot_autopilot::util::JsonValue(true);
  inner["owner_chain_checked"] = godot_autopilot::util::JsonValue(true);
  inner["undo"] = godot_autopilot::util::JsonValue("remove " + dest_path +
                                 " (resource_file action=remove)");
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  r["path"] = godot_autopilot::util::JsonValue(dest_path);
  r["verified"] = godot_autopilot::util::JsonValue(verified);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

static godot_autopilot::util::JsonValue find_scene_nodes(const godot_autopilot::util::JsonValue &args,
                                       godot::Node *root) {
  const godot_autopilot::util::JsonValue *pattern_p = args.Find("pattern");
  const godot_autopilot::util::JsonValue *type_p = args.Find("type");
  const godot_autopilot::util::JsonValue *group_p = args.Find("group");
  std::string pattern = "*";
  if (pattern_p) {
    if (!pattern_p->IsString() || pattern_p->GetString().empty())
      return util::error_json(
          "parameter 'pattern' must be a non-empty string");
    pattern = pattern_p->GetString();
  }
  std::string type;
  if (type_p) {
    if (!type_p->IsString() || type_p->GetString().empty())
      return util::error_json("parameter 'type' must be a non-empty string");
    type = type_p->GetString();
  }
  std::string group;
  if (group_p) {
    if (!group_p->IsString() || group_p->GetString().empty())
      return util::error_json("parameter 'group' must be a non-empty string");
    group = group_p->GetString();
  }
  const godot_autopilot::util::JsonValue *verify_p = args.Find("verify");
  const godot_autopilot::util::JsonValue *scene_path_p = args.Find("scene_path");
  const godot_autopilot::util::JsonValue *include_hash_p = args.Find("include_hash");
  if ((verify_p && verify_p->IsBool() && verify_p->GetBool()) ||
      args.Find("paths") != nullptr ||
      (scene_path_p && scene_path_p->IsString() &&
       !scene_path_p->GetString().empty()) ||
      (include_hash_p && include_hash_p->IsBool() &&
       include_hash_p->GetBool())) {
    return util::error_json(
        "parameters pattern/type/group cannot be combined with verify, paths, "
        "scene_path or include_hash — query the filtered nodes first, then "
        "call get_scene_tree without filters for those fields");
  }
  const TreeDetailOptions options = tree_detail_options(args);
  godot::TypedArray<godot::Node> matches = root->find_children(
      godot::String(pattern.c_str()), godot::String(type.c_str()), true, true);
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  int64_t count = 0;
  for (int i = 0; i < matches.size(); i++) {
    auto *match = godot::Object::cast_to<godot::Node>(matches[i]);
    if (!match)
      continue;
    if (!group.empty() &&
        !match->is_in_group(godot::StringName(group.c_str())))
      continue;
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    node_to_json(match, 0, options, item);
    nodes.PushBack(std::move(item));
    count++;
  }
  godot_autopilot::util::JsonValue filters(godot_autopilot::util::JsonValue::object_tag);
  filters["pattern"] = godot_autopilot::util::JsonValue(pattern);
  filters["type"] = godot_autopilot::util::JsonValue(type);
  filters["group"] = godot_autopilot::util::JsonValue(group);
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["root"] = godot_autopilot::util::JsonValue(util::to_std(root->get_name()));
  inner["filters"] = std::move(filters);
  inner["count"] = godot_autopilot::util::JsonValue(count);
  inner["nodes"] = std::move(nodes);
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

godot_autopilot::util::JsonValue handle_get_tree(const godot_autopilot::util::JsonValue &args) {
  if (args.IsObject()) {
    const bool filter_mode = args.Find("pattern") != nullptr ||
                             args.Find("type") != nullptr ||
                             args.Find("group") != nullptr;
    if (filter_mode) {
      auto *editor = godot::EditorInterface::get_singleton();
      godot::Node *root =
          editor ? editor->get_edited_scene_root() : nullptr;
      if (!root) {
        return util::error_detail(
            "no scene currently open in the editor", "get_scene_tree",
            "an edited scene",
            "open or create a scene first (create_editor_scene)");
      }
      return find_scene_nodes(args, root);
    }
    const godot_autopilot::util::JsonValue *verify = args.Find("verify");
    const godot_autopilot::util::JsonValue *observe = args.Find("observe");
    const bool extended =
        (verify != nullptr && verify->IsBool() && verify->GetBool()) ||
        (observe != nullptr && observe->IsBool() && observe->GetBool()) ||
        args.Find("paths") != nullptr ||
        args.Find("scene_path") != nullptr ||
        args.Find("include_hash") != nullptr;
    if (extended) {
      godot_autopilot::util::JsonValue snapshot_args = args;
      if (snapshot_args.Find("verify") == nullptr) {
        snapshot_args["verify"] = godot_autopilot::util::JsonValue(false);
      }
      return handle_get_snapshot(snapshot_args);
    }
  }
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *root = nullptr;
  if (editor) {
    root = editor->get_edited_scene_root();
  }
  if (!root) {
    return util::error_detail(
        "no scene currently open in the editor", "get_scene_tree",
        "an edited scene", "open or create a scene first (create_editor_scene)");
  }
  int max_depth = DEFAULT_MAX_DEPTH;
  auto *dp = args.Find("max_depth");
  if (dp && dp->IsInt()) {
    max_depth = static_cast<int>(dp->GetInt());
    if (max_depth == -1) {
      max_depth = UNLIMITED_TREE_DEPTH;
    } else if (max_depth < 1) {
      max_depth = 1;
    }
  }
  const TreeDetailOptions options = tree_detail_options(args);

  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  node_to_json(root, max_depth, options, result);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

godot_autopilot::util::JsonValue handle_get_node_screen_rect(const godot_autopilot::util::JsonValue &args) {
  auto *paths_p = args.Find("paths");
  if (!paths_p || !paths_p->IsArray()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: paths (array)");
    return e;
  }
  const auto &paths_arr = paths_p->GetArray();
  if (paths_arr.empty() ||
      paths_arr.size() > static_cast<size_t>(MAX_SCREEN_RECT_PATHS)) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("parameter 'paths' must contain between 1 and " +
                                std::to_string(MAX_SCREEN_RECT_PATHS) +
                                " entries");
    return e;
  }
  for (const auto &p : paths_arr) {
    if (!p.IsString()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] =
          godot_autopilot::util::JsonValue("parameter 'paths' must be an array of strings");
      return e;
    }
  }

  std::string viewport_mode = "auto";
  auto *vp_arg = args.Find("viewport");
  if (vp_arg) {
    if (!vp_arg->IsString()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("parameter 'viewport' must be a string");
      return e;
    }
    viewport_mode = vp_arg->GetString();
    if (viewport_mode != "auto" && viewport_mode != "2d" &&
        viewport_mode != "3d") {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] =
          godot_autopilot::util::JsonValue("parameter 'viewport' must be one of: auto, 2d, 3d");
      return e;
    }
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    return util::error_detail(
        "no edited scene root available",
        "scene_ops.cpp handle_get_node_screen_rect", "an edited scene",
        "open or create a scene first (create_editor_scene)");
  }

  godot::SubViewport *vp2d = editor->get_editor_viewport_2d();
  godot::SubViewport *vp3d = editor->get_editor_viewport_3d(0);
  godot::Transform2D t2d;
  if (vp2d)
    t2d = vp2d->get_screen_transform() * vp2d->get_global_canvas_transform();
  godot::Transform2D t3d;
  if (vp3d)
    t3d = vp3d->get_screen_transform();
  godot::Camera3D *camera = vp3d ? vp3d->get_camera_3d() : nullptr;

  godot_autopilot::util::JsonValue items(godot_autopilot::util::JsonValue::array_tag);
  std::string resolved_viewport;
  godot::Transform2D resolved_transform;
  bool has_resolved = false;

  for (const auto &p : paths_arr) {
    std::string path = p.GetString();
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    item["path"] = godot_autopilot::util::JsonValue(path);

    std::string hint;
    godot::Node *node = util::resolve_scene_node(path, scene_root, &hint);
    if (!node) {
      item["ok"] = godot_autopilot::util::JsonValue(false);
      item["error"] = godot_autopilot::util::JsonValue("node not found");
      items.PushBack(std::move(item));
      continue;
    }

    std::string type = util::to_std(node->get_class());
    godot::Node3D *node_3d = godot::Object::cast_to<godot::Node3D>(node);
    godot::CanvasItem *canvas_item =
        godot::Object::cast_to<godot::CanvasItem>(node);
    std::string space;
    if (node_3d)
      space = "3d";
    else if (canvas_item)
      space = "2d";
    else {
      item["ok"] = godot_autopilot::util::JsonValue(false);
      item["error"] = godot_autopilot::util::JsonValue("unsupported node type");
      items.PushBack(std::move(item));
      continue;
    }

    if (viewport_mode != "auto" && viewport_mode != space) {
      item["ok"] = godot_autopilot::util::JsonValue(false);
      item["error"] = godot_autopilot::util::JsonValue(
          "node type " + type + " is not compatible with viewport '" +
          viewport_mode + "'");
      items.PushBack(std::move(item));
      continue;
    }

    if (space == "3d") {
      if (!vp3d) {
        item["ok"] = godot_autopilot::util::JsonValue(false);
        item["error"] = godot_autopilot::util::JsonValue("3D editor viewport not available");
        items.PushBack(std::move(item));
        continue;
      }
      if (!camera) {
        item["ok"] = godot_autopilot::util::JsonValue(false);
        item["error"] = godot_autopilot::util::JsonValue("3D editor viewport has no camera");
        items.PushBack(std::move(item));
        continue;
      }
      godot::Vector3 world = node_3d->get_global_transform().get_origin();
      godot::Vector2 local = camera->unproject_position(world);
      godot::Vector2 pos = t3d.xform(local);
      item["ok"] = godot_autopilot::util::JsonValue(true);
      item["type"] = godot_autopilot::util::JsonValue(type);
      godot_autopilot::util::JsonValue screen_position(godot_autopilot::util::JsonValue::object_tag);
      screen_position["x"] = godot_autopilot::util::JsonValue(static_cast<double>(pos.x));
      screen_position["y"] = godot_autopilot::util::JsonValue(static_cast<double>(pos.y));
      item["screen_position"] = std::move(screen_position);
      item["behind"] = godot_autopilot::util::JsonValue(camera->is_position_behind(world));
      if (!has_resolved) {
        has_resolved = true;
        resolved_viewport = "3d";
        resolved_transform = t3d;
      }
      items.PushBack(std::move(item));
      continue;
    }

    if (!vp2d) {
      item["ok"] = godot_autopilot::util::JsonValue(false);
      item["error"] = godot_autopilot::util::JsonValue("2D editor viewport not available");
      items.PushBack(std::move(item));
      continue;
    }
    godot::Vector2 pos =
        t2d.xform(canvas_item->get_global_transform().get_origin());
    item["ok"] = godot_autopilot::util::JsonValue(true);
    item["type"] = godot_autopilot::util::JsonValue(type);
    godot_autopilot::util::JsonValue screen_position(godot_autopilot::util::JsonValue::object_tag);
    screen_position["x"] = godot_autopilot::util::JsonValue(static_cast<double>(pos.x));
    screen_position["y"] = godot_autopilot::util::JsonValue(static_cast<double>(pos.y));
    item["screen_position"] = std::move(screen_position);
    godot::Control *control = godot::Object::cast_to<godot::Control>(node);
    if (control) {
      godot::Transform2D global = control->get_global_transform();
      godot::Vector2 corner_a = t2d.xform(global.xform(godot::Vector2()));
      godot::Vector2 corner_b = t2d.xform(global.xform(control->get_size()));
      double min_x = std::min(static_cast<double>(corner_a.x),
                              static_cast<double>(corner_b.x));
      double min_y = std::min(static_cast<double>(corner_a.y),
                              static_cast<double>(corner_b.y));
      double max_x = std::max(static_cast<double>(corner_a.x),
                              static_cast<double>(corner_b.x));
      double max_y = std::max(static_cast<double>(corner_a.y),
                              static_cast<double>(corner_b.y));
      godot_autopilot::util::JsonValue rect(godot_autopilot::util::JsonValue::object_tag);
      godot_autopilot::util::JsonValue rect_position(godot_autopilot::util::JsonValue::object_tag);
      rect_position["x"] = godot_autopilot::util::JsonValue(min_x);
      rect_position["y"] = godot_autopilot::util::JsonValue(min_y);
      godot_autopilot::util::JsonValue rect_size(godot_autopilot::util::JsonValue::object_tag);
      rect_size["x"] = godot_autopilot::util::JsonValue(max_x - min_x);
      rect_size["y"] = godot_autopilot::util::JsonValue(max_y - min_y);
      rect["position"] = std::move(rect_position);
      rect["size"] = std::move(rect_size);
      item["rect"] = std::move(rect);
    }
    if (!has_resolved) {
      has_resolved = true;
      resolved_viewport = "2d";
      resolved_transform = t2d;
    }
    items.PushBack(std::move(item));
  }

  if (!has_resolved) {
    resolved_viewport = viewport_mode == "3d" ? "3d" : "2d";
    resolved_transform = viewport_mode == "3d" ? t3d : t2d;
  }

  godot::Size2 resolved_scale = resolved_transform.get_scale();
  godot::Vector2 resolved_origin = resolved_transform.get_origin();
  godot_autopilot::util::JsonValue mapping(godot_autopilot::util::JsonValue::object_tag);
  mapping["scale_x"] = godot_autopilot::util::JsonValue(static_cast<double>(resolved_scale.x));
  mapping["scale_y"] = godot_autopilot::util::JsonValue(static_cast<double>(resolved_scale.y));
  mapping["offset_x"] = godot_autopilot::util::JsonValue(static_cast<double>(resolved_origin.x));
  mapping["offset_y"] = godot_autopilot::util::JsonValue(static_cast<double>(resolved_origin.y));

  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["viewport"] = godot_autopilot::util::JsonValue(resolved_viewport);
  inner["window_id"] = godot_autopilot::util::JsonValue(0);
  inner["space"] = godot_autopilot::util::JsonValue("window");
  inner["mapping"] = std::move(mapping);
  inner["items"] = std::move(items);

  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

godot_autopilot::util::JsonValue handle_get_snapshot(const godot_autopilot::util::JsonValue &args) {
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *root = nullptr;
  if (editor) {
    root = editor->get_edited_scene_root();
  }
  if (!root) {
    return util::error_detail(
        "no scene currently open in the editor", "get_scene_snapshot",
        "an edited scene", "open or create a scene first (create_editor_scene)");
  }
  int max_depth = DEFAULT_MAX_DEPTH;
  auto *dp = args.Find("max_depth");
  if (dp && dp->IsInt()) {
    max_depth = static_cast<int>(dp->GetInt());
    if (max_depth == -1) {
      max_depth = UNLIMITED_TREE_DEPTH;
    } else if (max_depth < 1) {
      max_depth = 1;
    }
  }
  const TreeDetailOptions options = tree_detail_options(args);
  bool want_verify = true;
  auto *vp = args.Find("verify");
  if (vp && vp->IsBool())
    want_verify = vp->GetBool();
  const godot_autopilot::util::JsonValue *paths_p = args.Find("paths");
  bool has_paths = false;
  if (paths_p && !paths_p->IsNull()) {
    if (!paths_p->IsArray()) {
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] =
          godot_autopilot::util::JsonValue("parameter 'paths' must be an array of strings");
      return e;
    }
    const auto &paths_arr = paths_p->GetArray();
    if (!paths_arr.empty()) {
      if (paths_arr.size() > static_cast<size_t>(MAX_SCREEN_RECT_PATHS)) {
        godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
        e["error"] = godot_autopilot::util::JsonValue(
            "parameter 'paths' must contain between 1 and " +
            std::to_string(MAX_SCREEN_RECT_PATHS) + " entries");
        return e;
      }
      for (const auto &entry : paths_arr) {
        if (!entry.IsString()) {
          godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
          e["error"] = godot_autopilot::util::JsonValue(
              "parameter 'paths' must be an array of strings");
          return e;
        }
      }
      has_paths = true;
    }
  }
  godot_autopilot::util::JsonValue tree(godot_autopilot::util::JsonValue::object_tag);
  node_to_json(root, max_depth, options, tree);
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["tree"] = std::move(tree);
  if (want_verify) {
    godot_autopilot::util::JsonValue vsum(godot_autopilot::util::JsonValue::object_tag);
    std::string target_path;
    const godot_autopilot::util::JsonValue *scene_path_p = args.Find("scene_path");
    if (scene_path_p != nullptr && scene_path_p->IsString() &&
        !scene_path_p->GetString().empty()) {
      target_path = scene_path_p->GetString();
    } else {
      target_path = util::to_std(root->get_scene_file_path());
    }
    bool want_hash = false;
    if (const godot_autopilot::util::JsonValue *hash_p = args.Find("include_hash")) {
      want_hash = hash_p->IsBool() && hash_p->GetBool();
    }
    if (target_path.empty()) {
      vsum["available"] = godot_autopilot::util::JsonValue(false);
      vsum["error"] = godot_autopilot::util::JsonValue(
          "current scene has no file path — save it first (save_editor_scene), "
          "or provide scene_path");
      vsum["path"] = godot_autopilot::util::JsonValue("");
    } else if (!godot::FileAccess::file_exists(
                   godot::String(target_path.c_str()))) {
      vsum["available"] = godot_autopilot::util::JsonValue(false);
      vsum["error"] =
          godot_autopilot::util::JsonValue("scene file not found on disk: " + target_path);
      vsum["path"] = godot_autopilot::util::JsonValue(target_path);
    } else {
      bool disk_ok = false;
      const std::string text =
          scene_verify::read_text_file(target_path, disk_ok);
      if (!disk_ok) {
        vsum["available"] = godot_autopilot::util::JsonValue(false);
        vsum["error"] =
            godot_autopilot::util::JsonValue("scene file could not be read: " + target_path);
        vsum["path"] = godot_autopilot::util::JsonValue(target_path);
      } else {
        const scene_verify::VerifyResult vr =
            scene_verify::compare_tree_with_text(root, text, want_hash);
        vsum["available"] = godot_autopilot::util::JsonValue(true);
        vsum["match"] = godot_autopilot::util::JsonValue(vr.match);
        vsum["path"] = godot_autopilot::util::JsonValue(target_path);
        vsum["memory_nodes"] = godot_autopilot::util::JsonValue(vr.memory_nodes);
        vsum["disk_nodes"] = godot_autopilot::util::JsonValue(vr.disk_nodes);
        vsum["memory_connections"] = godot_autopilot::util::JsonValue(vr.memory_connections);
        vsum["disk_connections"] = godot_autopilot::util::JsonValue(vr.disk_connections);
        vsum["connections_match"] =
            godot_autopilot::util::JsonValue(vr.memory_connections == vr.disk_connections);
        godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
        for (const std::string &p : vr.missing_paths)
          arr.PushBack(godot_autopilot::util::JsonValue(p));
        vsum["missing_paths"] = std::move(arr);
        if (vr.missing_truncated)
          vsum["missing_truncated"] = godot_autopilot::util::JsonValue(true);
        vsum["inherited_nodes"] = godot_autopilot::util::JsonValue(vr.inherited_nodes);
        vsum["inherited_connections"] =
            godot_autopilot::util::JsonValue(vr.inherited_connections);
        if (!vr.inherited_paths.empty()) {
          godot_autopilot::util::JsonValue iarr(godot_autopilot::util::JsonValue::array_tag);
          for (const std::string &p : vr.inherited_paths)
            iarr.PushBack(godot_autopilot::util::JsonValue(p));
          vsum["inherited_paths"] = std::move(iarr);
        }
        if (vr.inherited_truncated)
          vsum["inherited_truncated"] = godot_autopilot::util::JsonValue(true);
        if (want_hash && vr.hash_computed) {
          vsum["memory_hash"] =
              godot_autopilot::util::JsonValue(editor_ops::hash_hex(vr.memory_hash));
          vsum["disk_hash"] = godot_autopilot::util::JsonValue(editor_ops::hash_hex(vr.disk_hash));
        }
      }
    }
    inner["verify"] = std::move(vsum);
  }
  if (has_paths) {
    godot_autopilot::util::JsonValue rect_args(godot_autopilot::util::JsonValue::object_tag);
    rect_args["paths"] = *paths_p;
    const godot_autopilot::util::JsonValue *viewport_p = args.Find("viewport");
    rect_args["viewport"] = viewport_p != nullptr && viewport_p->IsString()
                                ? *viewport_p
                                : godot_autopilot::util::JsonValue("auto");
    godot_autopilot::util::JsonValue rect_res = handle_get_node_screen_rect(rect_args);
    const godot_autopilot::util::JsonValue *rect_inner = rect_res.Find("result");
    if (rect_inner && rect_inner->IsObject()) {
      inner["nodes"] = *rect_inner;
    } else {
      const godot_autopilot::util::JsonValue *rect_err = rect_res.Find("error");
      if (rect_err && rect_err->IsString())
        inner["nodes_error"] = *rect_err;
      else
        inner["nodes_error"] = godot_autopilot::util::JsonValue("screen rect lookup failed");
    }
  }
  util::add_scene_info_fields(inner, util::edited_scene_info());
  godot_autopilot::util::JsonValue snap(godot_autopilot::util::JsonValue::object_tag);
  snap["result"] = std::move(inner);
  return snap;
}

godot_autopilot::util::JsonValue handle_create_path_node(const godot_autopilot::util::JsonValue &args) {
  auto *nm = args.Find("name");
  if (!nm || !nm->IsString() || nm->GetString().empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: name");
    return e;
  }
  auto *cv = args.Find("curve");
  if (!cv || !cv->IsString() || cv->GetString().empty()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("missing required parameter: curve");
    return e;
  }
  std::string name = nm->GetString();
  std::string curve_name = cv->GetString();
  godot::Ref<godot::Resource> curve_res =
      resource_ops::resolve_memory_resource(curve_name);
  if (curve_res.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("memory resource not found: " + curve_name +
                                " (create it with create_curve_resource first)");
    return e;
  }
  godot::Ref<godot::Curve2D> c2 = curve_res;
  godot::Ref<godot::Curve3D> c3 = curve_res;
  if (c2.is_null() && c3.is_null()) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("memory resource is not a Curve2D/Curve3D: " +
                                curve_name);
    return e;
  }
  const bool is_3d = c3.is_valid();
  const std::string type = is_3d ? "Path3D" : "Path2D";
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("ClassDB singleton not available");
    return e;
  }
  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("failed to instantiate node type: " + type);
    return e;
  }
  auto *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("instantiated object is not a Node: " + type);
    return e;
  }
  node->set_name(godot::StringName(name.c_str()));
  if (is_3d) {
    auto *path3d = godot::Object::cast_to<godot::Path3D>(node);
    if (!path3d) {
      memdelete(node);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("instantiated object is not a Path3D");
      return e;
    }
    path3d->set_curve(c3);
  } else {
    auto *path2d = godot::Object::cast_to<godot::Path2D>(node);
    if (!path2d) {
      memdelete(node);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("instantiated object is not a Path2D");
      return e;
    }
    path2d->set_curve(c2);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    memdelete(node);
    godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
    e["error"] = godot_autopilot::util::JsonValue("EditorInterface not available");
    return e;
  }
  auto *pp = args.Find("parent_path");
  const bool has_parent =
      pp && pp->IsString() && !pp->GetString().empty();
  if (has_parent) {
    std::string hint;
    auto *root = editor->get_edited_scene_root();
    auto *parent =
        godot_autopilot::util::resolve_scene_node(pp->GetString(), root, &hint);
    if (!parent) {
      memdelete(node);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue("parent node not found: " + pp->GetString() +
                                  " — " + hint);
      return e;
    }
    parent->add_child(node);
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root)
      node->set_owner(scene_root);
  } else {
    auto *existing_root = editor->get_edited_scene_root();
    if (existing_root) {
      memdelete(node);
      godot_autopilot::util::JsonValue e(godot_autopilot::util::JsonValue::object_tag);
      e["error"] = godot_autopilot::util::JsonValue(
          "scene already has a root, use parent_path to add children");
      return e;
    }
    editor->add_root_node(node);
  }
  auto *scene_root = editor->get_edited_scene_root();
  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  godot_autopilot::util::JsonValue inner(godot_autopilot::util::JsonValue::object_tag);
  inner["path"] = godot_autopilot::util::JsonValue(scene_relative_path(node, scene_root));
  inner["type"] = godot_autopilot::util::JsonValue(type);
  inner["curve"] = godot_autopilot::util::JsonValue(curve_name);
  inner["undo"] = godot_autopilot::util::JsonValue("delete node " +
                                 util::to_std(node->get_name()) +
                                 " (delete_scene_node)");
  util::add_scene_info_fields(inner, scene_info);
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

} // namespace scene_ops
} // namespace godot_autopilot
