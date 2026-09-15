#include "scene_ops.hpp"
#include "../util/scene_path.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "property_ops.hpp"
#include "util/error_util.hpp"
#include "util/variant_json.hpp"
#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/canvas_item.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
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

void collect_property_summary(godot::Node *node, mcp::JsonValue &j) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  int count = 0;
  for (int i = 0; i < props.size() && count < MAX_PROPERTY_COUNT; i++) {
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
    j[name_str] = VariantJson::serialize(node->get(prop_name));
    count++;
  }
}

void node_to_json(godot::Node *node, int remaining_depth,
                  bool include_properties, mcp::JsonValue &j) {
  if (!node)
    return;
  j["name"] = mcp::JsonValue(util::to_std(node->get_name()));
  j["type"] = mcp::JsonValue(util::to_std(node->get_class()));

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
  j["path"] = mcp::JsonValue(path);
  if (include_properties) {
    mcp::JsonValue props(mcp::JsonValue::object_tag);
    collect_property_summary(node, props);
    j["properties"] = std::move(props);
  }
  if (remaining_depth <= 0)
    return;
  j["children"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  auto children = node->get_children();
  for (int i = 0; i < children.size(); i++) {
    auto *child = godot::Object::cast_to<godot::Node>(children[i]);
    if (child) {
      mcp::JsonValue child_j(mcp::JsonValue::object_tag);
      node_to_json(child, remaining_depth - 1, include_properties, child_j);
      j["children"].PushBack(std::move(child_j));
    }
  }
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

mcp::JsonValue handle_create(const mcp::JsonValue &args) {
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
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDB singleton not available");
    return e;
  }

  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Node"))) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(type + " is not a Node subclass");
    return e;
  }

  auto *props_it = args.Find("properties");
  if (props_it && !props_it->IsObject()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("parameter 'properties' must be an object mapping "
                       "property names to values");
    return e;
  }

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to instantiate node type: " + type);
    return e;
  }

  auto *obj = godot::Object::cast_to<godot::Node>(obj_var);
  if (!obj) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("instantiated object is not a Node: " + type);
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
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parent node not found: " + parent_path +
                                  " — " + hint);
      return e;
    }
    parent->add_child(obj);
    auto *scene_root = editor->get_edited_scene_root();
    if (scene_root)
      obj->set_owner(scene_root);
  } else {
    if (!editor) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("EditorInterface not available");
      return e;
    }
    auto *existing_root = editor->get_edited_scene_root();
    if (existing_root) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "scene already has a root, use parent_path to add children");
      return e;
    }
    editor->add_root_node(obj);
  }

  std::vector<std::string> applied_properties;
  std::vector<mcp::JsonValue> property_warnings;
  if (props_it) {
    auto *scene_root_now =
        editor ? editor->get_edited_scene_root() : nullptr;
    std::string node_rel_path = scene_relative_path(obj, scene_root_now);
    for (const auto &kv : *props_it) {
      mcp::JsonValue prop_args(mcp::JsonValue::object_tag);
      prop_args["path"] = mcp::JsonValue(node_rel_path);
      prop_args["property"] = mcp::JsonValue(kv.first);
      prop_args["value"] = kv.second;
      mcp::JsonValue prop_result;
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
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] =
            mcp::JsonValue("failed to apply property '" + kv.first +
                           "' on created node: " + err->GetString());
        return e;
      }
      auto *warn = prop_result.Find("warning");
      if (warn && warn->IsString()) {
        mcp::JsonValue w(mcp::JsonValue::object_tag);
        w["path"] = mcp::JsonValue(node_rel_path);
        w["property"] = mcp::JsonValue(kv.first);
        w["warning"] = mcp::JsonValue(warn->GetString());
        property_warnings.push_back(std::move(w));
      }
      applied_properties.push_back(kv.first);
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(result_path);
  mcp::JsonValue applied(mcp::JsonValue::array_tag);
  for (const auto &applied_name : applied_properties)
    applied.PushBack(mcp::JsonValue(applied_name));
  inner["applied_properties"] = std::move(applied);
  if (!property_warnings.empty()) {
    mcp::JsonValue warnings(mcp::JsonValue::array_tag);
    for (auto &w : property_warnings)
      warnings.PushBack(std::move(w));
    inner["property_warnings"] = std::move(warnings);
  }
  inner["undo"] =
      mcp::JsonValue("delete node " + result_path + " (delete_scene_node)");
  r["result"] = std::move(inner);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_delete(const mcp::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = p->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }

  if (node == scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
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
    undo_redo->add_do_method(node, godot::StringName("queue_free"));
    undo_redo->add_undo_method(parent, godot::StringName("move_child"), node,
                               child_index);
    undo_redo->add_undo_method(node, godot::StringName("set_owner"),
                               old_owner);
    undo_redo->add_undo_method(parent, godot::StringName("add_child"), node);
    undo_redo->commit_action();
  } else {
    node->queue_free();
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("deleted");
  r["undoable"] = mcp::JsonValue(undo_redo != nullptr && parent != nullptr);
  r["note"] = mcp::JsonValue(
      "undo restores the node via add_child while the deferred queue_free has "
      "not yet destroyed it (same frame); once the node is freed a later undo "
      "cannot resurrect it");
  mcp::JsonValue undo_info(mcp::JsonValue::object_tag);
  undo_info["name"] = mcp::JsonValue(node_name);
  undo_info["type"] = mcp::JsonValue(node_type);
  undo_info["parent_path"] = mcp::JsonValue(parent_path);
  undo_info["hint"] =
      mcp::JsonValue("recreate node " + node_name + " (" + node_type +
                     ") under " + parent_path + " (create_scene_node)");
  r["undo"] = std::move(undo_info);
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_rename(const mcp::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *nn = args.Find("new_name");
  if (!nn || !nn->IsString() || nn->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing or empty required parameter: new_name");
    return e;
  }
  std::string path = p->GetString();
  std::string new_name = nn->GetString();
  if (new_name.find('/') != std::string::npos ||
      new_name.find(':') != std::string::npos) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("invalid new_name '" + new_name +
                       "' — '/' and ':' are not allowed in node names");
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }

  std::string old_name = util::to_std(node->get_name());
  if (godot::Node *par = node->get_parent()) {
    auto children = par->get_children();
    for (int i = 0; i < children.size(); i++) {
      auto *sibling = godot::Object::cast_to<godot::Node>(children[i]);
      if (sibling && sibling != node &&
          util::to_std(sibling->get_name()) == new_name) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("renamed");
  r["undoable"] = mcp::JsonValue(undo_redo != nullptr);
  r["old_name"] = mcp::JsonValue(old_name);
  r["name"] = mcp::JsonValue(util::to_std(node->get_name()));
  r["path"] =
      mcp::JsonValue(scene_relative_path(node, scene_root));
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_reparent(const mcp::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *npp = args.Find("new_parent_path");
  if (!npp || !npp->IsString() || npp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("missing required parameter: new_parent_path");
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
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }
  if (node == scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "cannot reparent the scene root node — the root must stay at the top "
        "of the edited scene");
    return e;
  }

  auto *new_parent = godot_autopilot::util::resolve_scene_node(
      new_parent_path, scene_root, &hint);
  if (!new_parent) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("parent node not found: " + new_parent_path +
                                " — " + hint);
    return e;
  }
  if (new_parent == node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("cannot reparent a node under itself: " + path);
    return e;
  }
  for (godot::Node *anc = new_parent->get_parent(); anc; anc = anc->get_parent()) {
    if (anc == node) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "cannot reparent a node under one of its own descendants: " +
          new_parent_path);
      return e;
    }
  }
  auto *old_parent = node->get_parent();
  if (!old_parent) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] =
        mcp::JsonValue("node has no parent to detach from: " + path);
    return e;
  }
  if (new_parent == old_parent) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node is already a child of '" +
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
    undo_redo->add_undo_method(old_parent, godot::StringName("move_child"),
                               node, old_index);
    if (transform_managed)
      undo_redo->add_undo_method(node, godot::StringName("set_transform"),
                                 old_local_var);
    undo_redo->add_undo_method(node, godot::StringName("set_owner"),
                               old_owner);
    undo_redo->add_undo_method(old_parent, godot::StringName("add_child"),
                               node);
    undo_redo->add_undo_method(new_parent, godot::StringName("remove_child"),
                               node);
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("reparented");
  r["undoable"] = mcp::JsonValue(undo_redo != nullptr);
  r["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  r["old_parent_path"] =
      mcp::JsonValue(scene_relative_path(old_parent, scene_root));
  r["new_parent_path"] = mcp::JsonValue(new_parent_path);
  r["note"] = mcp::JsonValue(
      std::string("if the node's previous owner was the edited scene root it "
                  "stays owned by the current scene root so it is saved with "
                  "the scene; keep_world_position applies only when the node "
                  "and the new parent are both Node2D or both Node3D") +
      (transform_managed ? "" : " (ignored here)"));
  scene_dirty_tracker::mark_scene_modified();
  return r;
}

mcp::JsonValue handle_instance(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "instantiate_scene called");

  auto *p = args.Find("path");
  if (!p || !p->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  std::string path = p->GetString();

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ResourceLoader not available");
    return e;
  }

  godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
  auto *packed_scene = godot::Object::cast_to<godot::PackedScene>(res.ptr());
  if (!packed_scene) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to load packed scene: " + path);
    return e;
  }

  godot::Node *instance = packed_scene->instantiate();
  if (!instance) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to instantiate scene: " + path);
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
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parent node not found: " + pp->GetString() +
                                  " — " + hint);
      return e;
    }
  } else if (!scene_root) {
    memdelete(instance);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no scene open");
    return e;
  } else {
    parent = scene_root;
  }

  auto *n = args.Find("name");
  if (n && n->IsString() && !n->GetString().empty()) {
    instance->set_name(godot::StringName(n->GetString().c_str()));
  }

  parent->add_child(instance);

  bool set_owner = true;
  auto *o = args.Find("owner");
  if (o && o->IsBool())
    set_owner = o->GetBool();
  if (set_owner && scene_root) {
    instance->set_owner(scene_root);
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(result_path);
  inner["name"] = mcp::JsonValue(instance_name);
  inner["type"] = mcp::JsonValue(instance_type);
  inner["undo"] =
      mcp::JsonValue("delete node " + result_path + " (delete_scene_node)");
  r["result"] = std::move(inner);
  r["note"] = mcp::JsonValue(
      "instance inherits CONNECT_PERSIST connections saved in its source "
      "scene; do not reconnect the same signal+callable pairs on instances to "
      "avoid duplicate-connection errors");

  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "instantiate_scene completed");
  return r;
}

mcp::JsonValue handle_get_tree(const mcp::JsonValue &args) {
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
  bool include_properties = false;
  auto *ip = args.Find("include_properties");
  if (ip && ip->IsBool())
    include_properties = ip->GetBool();

  mcp::JsonValue result(mcp::JsonValue::object_tag);
  node_to_json(root, max_depth, include_properties, result);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  return r;
}

mcp::JsonValue handle_get_node_screen_rect(const mcp::JsonValue &args) {
  auto *paths_p = args.Find("paths");
  if (!paths_p || !paths_p->IsArray()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: paths (array)");
    return e;
  }
  const auto &paths_arr = paths_p->GetArray();
  if (paths_arr.empty() ||
      paths_arr.size() > static_cast<size_t>(MAX_SCREEN_RECT_PATHS)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("parameter 'paths' must contain between 1 and " +
                                std::to_string(MAX_SCREEN_RECT_PATHS) +
                                " entries");
    return e;
  }
  for (const auto &p : paths_arr) {
    if (!p.IsString()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'paths' must be an array of strings");
      return e;
    }
  }

  std::string viewport_mode = "auto";
  auto *vp_arg = args.Find("viewport");
  if (vp_arg) {
    if (!vp_arg->IsString()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'viewport' must be a string");
      return e;
    }
    viewport_mode = vp_arg->GetString();
    if (viewport_mode != "auto" && viewport_mode != "2d" &&
        viewport_mode != "3d") {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'viewport' must be one of: auto, 2d, 3d");
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

  mcp::JsonValue items(mcp::JsonValue::array_tag);
  std::string resolved_viewport;
  godot::Transform2D resolved_transform;
  bool has_resolved = false;

  for (const auto &p : paths_arr) {
    std::string path = p.GetString();
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["path"] = mcp::JsonValue(path);

    std::string hint;
    godot::Node *node = util::resolve_scene_node(path, scene_root, &hint);
    if (!node) {
      item["ok"] = mcp::JsonValue(false);
      item["error"] = mcp::JsonValue("node not found");
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
      item["ok"] = mcp::JsonValue(false);
      item["error"] = mcp::JsonValue("unsupported node type");
      items.PushBack(std::move(item));
      continue;
    }

    if (viewport_mode != "auto" && viewport_mode != space) {
      item["ok"] = mcp::JsonValue(false);
      item["error"] = mcp::JsonValue(
          "node type " + type + " is not compatible with viewport '" +
          viewport_mode + "'");
      items.PushBack(std::move(item));
      continue;
    }

    if (space == "3d") {
      if (!vp3d) {
        item["ok"] = mcp::JsonValue(false);
        item["error"] = mcp::JsonValue("3D editor viewport not available");
        items.PushBack(std::move(item));
        continue;
      }
      if (!camera) {
        item["ok"] = mcp::JsonValue(false);
        item["error"] = mcp::JsonValue("3D editor viewport has no camera");
        items.PushBack(std::move(item));
        continue;
      }
      godot::Vector3 world = node_3d->get_global_transform().get_origin();
      godot::Vector2 local = camera->unproject_position(world);
      godot::Vector2 pos = t3d.xform(local);
      item["ok"] = mcp::JsonValue(true);
      item["type"] = mcp::JsonValue(type);
      mcp::JsonValue screen_position(mcp::JsonValue::object_tag);
      screen_position["x"] = mcp::JsonValue(static_cast<double>(pos.x));
      screen_position["y"] = mcp::JsonValue(static_cast<double>(pos.y));
      item["screen_position"] = std::move(screen_position);
      item["behind"] = mcp::JsonValue(camera->is_position_behind(world));
      if (!has_resolved) {
        has_resolved = true;
        resolved_viewport = "3d";
        resolved_transform = t3d;
      }
      items.PushBack(std::move(item));
      continue;
    }

    if (!vp2d) {
      item["ok"] = mcp::JsonValue(false);
      item["error"] = mcp::JsonValue("2D editor viewport not available");
      items.PushBack(std::move(item));
      continue;
    }
    godot::Vector2 pos =
        t2d.xform(canvas_item->get_global_transform().get_origin());
    item["ok"] = mcp::JsonValue(true);
    item["type"] = mcp::JsonValue(type);
    mcp::JsonValue screen_position(mcp::JsonValue::object_tag);
    screen_position["x"] = mcp::JsonValue(static_cast<double>(pos.x));
    screen_position["y"] = mcp::JsonValue(static_cast<double>(pos.y));
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
      mcp::JsonValue rect(mcp::JsonValue::object_tag);
      mcp::JsonValue rect_position(mcp::JsonValue::object_tag);
      rect_position["x"] = mcp::JsonValue(min_x);
      rect_position["y"] = mcp::JsonValue(min_y);
      mcp::JsonValue rect_size(mcp::JsonValue::object_tag);
      rect_size["x"] = mcp::JsonValue(max_x - min_x);
      rect_size["y"] = mcp::JsonValue(max_y - min_y);
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
  mcp::JsonValue mapping(mcp::JsonValue::object_tag);
  mapping["scale_x"] = mcp::JsonValue(static_cast<double>(resolved_scale.x));
  mapping["scale_y"] = mcp::JsonValue(static_cast<double>(resolved_scale.y));
  mapping["offset_x"] = mcp::JsonValue(static_cast<double>(resolved_origin.x));
  mapping["offset_y"] = mcp::JsonValue(static_cast<double>(resolved_origin.y));

  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["viewport"] = mcp::JsonValue(resolved_viewport);
  inner["window_id"] = mcp::JsonValue(0);
  inner["space"] = mcp::JsonValue("window");
  inner["mapping"] = std::move(mapping);
  inner["items"] = std::move(items);

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}

} // namespace scene_ops
} // namespace godot_autopilot
