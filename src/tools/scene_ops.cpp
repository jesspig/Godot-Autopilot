#include "scene_ops.hpp"
#include "../util/scene_path.hpp"
#include "../util/scene_verify.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "property_ops.hpp"
#include "resource_ops.hpp"
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

void collect_property_summary(godot::Node *node, mcp::JsonValue &j) {
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

bool property_value_is_object_typed(const godot::Node *node,
                                    const std::string &prop_name,
                                    const mcp::JsonValue &raw_value) {
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
    const mcp::JsonValue *ref = raw_value.Find("path");
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
  std::vector<mcp::JsonValue> inline_resources;
  if (props_it) {
    auto *scene_root_now =
        editor ? editor->get_edited_scene_root() : nullptr;
    std::string node_rel_path = scene_relative_path(obj, scene_root_now);

    auto apply_property = [&](const std::string &prop_name,
                              const mcp::JsonValue &prop_value) {
      mcp::JsonValue prop_args(mcp::JsonValue::object_tag);
      prop_args["path"] = mcp::JsonValue(node_rel_path);
      prop_args["property"] = mcp::JsonValue(prop_name);
      prop_args["value"] = prop_value;
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
            mcp::JsonValue("failed to apply property '" + prop_name +
                           "' on created node: " + err->GetString());
        return e;
      }
      auto *warn = prop_result.Find("warning");
      if (warn && warn->IsString()) {
        mcp::JsonValue w(mcp::JsonValue::object_tag);
        w["path"] = mcp::JsonValue(node_rel_path);
        w["property"] = mcp::JsonValue(prop_name);
        w["warning"] = mcp::JsonValue(warn->GetString());
        property_warnings.push_back(std::move(w));
      }
      auto *inline_rec = prop_result.Find("inline_resources");
      if (inline_rec && inline_rec->IsArray()) {
        for (const auto &entry : inline_rec->GetArray()) {
          inline_resources.push_back(entry);
        }
      }
      applied_properties.push_back(prop_name);
      return mcp::JsonValue(mcp::JsonValue::object_tag);
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
        mcp::JsonValue apply_error = apply_property(kv.first, kv.second);
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(result_path);
  util::add_scene_info_fields(inner, scene_info);
  mcp::JsonValue applied(mcp::JsonValue::array_tag);
  for (const auto &applied_name : applied_properties)
    applied.PushBack(mcp::JsonValue(applied_name));
  inner["applied_properties"] = std::move(applied);
  if (!inline_resources.empty()) {
    mcp::JsonValue inlines(mcp::JsonValue::array_tag);
    for (auto &entry : inline_resources)
      inlines.PushBack(std::move(entry));
    inner["inline_resources"] = std::move(inlines);
  }
  if (!property_warnings.empty()) {
    mcp::JsonValue warnings(mcp::JsonValue::array_tag);
    for (auto &w : property_warnings)
      warnings.PushBack(std::move(w));
    inner["property_warnings"] = std::move(warnings);
  }
  inner["undo"] =
      mcp::JsonValue("delete node " + result_path + " (delete_scene_node)");
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  if (!scene_info.has_scene) {
    r["scene_info_note"] = mcp::JsonValue(
        "no edited scene root — scene_path is empty and the write target is "
        "unknown");
  }
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

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("deleted");
  r["undoable"] = mcp::JsonValue(undo_redo != nullptr && parent != nullptr);
  r["note"] = mcp::JsonValue(
      "undo restores the node via add_child/move_child/set_owner (native "
      "SceneTreeDock order); the node is kept alive by an undo reference, so "
      "undo works across frames");
  mcp::JsonValue undo_info(mcp::JsonValue::object_tag);
  undo_info["name"] = mcp::JsonValue(node_name);
  undo_info["type"] = mcp::JsonValue(node_type);
  undo_info["parent_path"] = mcp::JsonValue(parent_path);
  undo_info["hint"] =
      mcp::JsonValue("recreate node " + node_name + " (" + node_type +
                     ") under " + parent_path + " (create_scene_node)");
  r["undo"] = std::move(undo_info);
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
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
  util::add_scene_info_fields(r, util::edited_scene_info());
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
  util::add_scene_info_fields(r, util::edited_scene_info());
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

  int persist_skipped = 0;
  int persist_connected = 0;
  int persist_unresolved = 0;
  audit_instance_persist_connections(packed_scene, instance, persist_skipped,
                                     persist_connected, persist_unresolved);

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

mcp::JsonValue handle_get_snapshot(const mcp::JsonValue &args) {
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
  bool include_properties = false;
  auto *ip = args.Find("include_properties");
  if (ip && ip->IsBool())
    include_properties = ip->GetBool();
  bool want_verify = true;
  auto *vp = args.Find("verify");
  if (vp && vp->IsBool())
    want_verify = vp->GetBool();
  const mcp::JsonValue *paths_p = args.Find("paths");
  bool has_paths = false;
  if (paths_p && !paths_p->IsNull()) {
    if (!paths_p->IsArray()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'paths' must be an array of strings");
      return e;
    }
    const auto &paths_arr = paths_p->GetArray();
    if (!paths_arr.empty()) {
      if (paths_arr.size() > static_cast<size_t>(MAX_SCREEN_RECT_PATHS)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "parameter 'paths' must contain between 1 and " +
            std::to_string(MAX_SCREEN_RECT_PATHS) + " entries");
        return e;
      }
      for (const auto &entry : paths_arr) {
        if (!entry.IsString()) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(
              "parameter 'paths' must be an array of strings");
          return e;
        }
      }
      has_paths = true;
    }
  }
  mcp::JsonValue tree(mcp::JsonValue::object_tag);
  node_to_json(root, max_depth, include_properties, tree);
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["tree"] = std::move(tree);
  if (want_verify) {
    mcp::JsonValue vsum(mcp::JsonValue::object_tag);
    std::string target_path = util::to_std(root->get_scene_file_path());
    if (target_path.empty()) {
      vsum["available"] = mcp::JsonValue(false);
      vsum["error"] = mcp::JsonValue(
          "current scene has no file path — save it first");
      vsum["path"] = mcp::JsonValue("");
    } else if (!godot::FileAccess::file_exists(
                   godot::String(target_path.c_str()))) {
      vsum["available"] = mcp::JsonValue(false);
      vsum["error"] =
          mcp::JsonValue("scene file not found on disk: " + target_path);
      vsum["path"] = mcp::JsonValue(target_path);
    } else {
      bool disk_ok = false;
      const std::string text =
          scene_verify::read_text_file(target_path, disk_ok);
      if (!disk_ok) {
        vsum["available"] = mcp::JsonValue(false);
        vsum["error"] =
            mcp::JsonValue("scene file could not be read: " + target_path);
        vsum["path"] = mcp::JsonValue(target_path);
      } else {
        const scene_verify::VerifyResult vr =
            scene_verify::compare_tree_with_text(root, text, false);
        vsum["available"] = mcp::JsonValue(true);
        vsum["match"] = mcp::JsonValue(vr.match);
        vsum["path"] = mcp::JsonValue(target_path);
        vsum["memory_nodes"] = mcp::JsonValue(vr.memory_nodes);
        vsum["disk_nodes"] = mcp::JsonValue(vr.disk_nodes);
        vsum["memory_connections"] = mcp::JsonValue(vr.memory_connections);
        vsum["disk_connections"] = mcp::JsonValue(vr.disk_connections);
        vsum["connections_match"] =
            mcp::JsonValue(vr.memory_connections == vr.disk_connections);
        mcp::JsonValue arr(mcp::JsonValue::array_tag);
        for (const std::string &p : vr.missing_paths)
          arr.PushBack(mcp::JsonValue(p));
        vsum["missing_paths"] = std::move(arr);
        if (vr.missing_truncated)
          vsum["missing_truncated"] = mcp::JsonValue(true);
        vsum["inherited_nodes"] = mcp::JsonValue(vr.inherited_nodes);
        vsum["inherited_connections"] =
            mcp::JsonValue(vr.inherited_connections);
        if (!vr.inherited_paths.empty()) {
          mcp::JsonValue iarr(mcp::JsonValue::array_tag);
          for (const std::string &p : vr.inherited_paths)
            iarr.PushBack(mcp::JsonValue(p));
          vsum["inherited_paths"] = std::move(iarr);
        }
        if (vr.inherited_truncated)
          vsum["inherited_truncated"] = mcp::JsonValue(true);
      }
    }
    inner["verify"] = std::move(vsum);
  }
  if (has_paths) {
    mcp::JsonValue rect_args(mcp::JsonValue::object_tag);
    rect_args["paths"] = *paths_p;
    rect_args["viewport"] = mcp::JsonValue("auto");
    mcp::JsonValue rect_res = handle_get_node_screen_rect(rect_args);
    const mcp::JsonValue *rect_inner = rect_res.Find("result");
    if (rect_inner && rect_inner->IsObject()) {
      inner["nodes"] = *rect_inner;
    } else {
      const mcp::JsonValue *rect_err = rect_res.Find("error");
      if (rect_err && rect_err->IsString())
        inner["nodes_error"] = *rect_err;
      else
        inner["nodes_error"] = mcp::JsonValue("screen rect lookup failed");
    }
  }
  util::add_scene_info_fields(inner, util::edited_scene_info());
  mcp::JsonValue snap(mcp::JsonValue::object_tag);
  snap["result"] = std::move(inner);
  return snap;
}

mcp::JsonValue handle_create_path_node(const mcp::JsonValue &args) {
  auto *nm = args.Find("name");
  if (!nm || !nm->IsString() || nm->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: name");
    return e;
  }
  auto *cv = args.Find("curve");
  if (!cv || !cv->IsString() || cv->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: curve");
    return e;
  }
  std::string name = nm->GetString();
  std::string curve_name = cv->GetString();
  godot::Ref<godot::Resource> curve_res =
      resource_ops::resolve_memory_resource(curve_name);
  if (curve_res.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("memory resource not found: " + curve_name +
                                " (create it with create_curve_resource first)");
    return e;
  }
  godot::Ref<godot::Curve2D> c2 = curve_res;
  godot::Ref<godot::Curve3D> c3 = curve_res;
  if (c2.is_null() && c3.is_null()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("memory resource is not a Curve2D/Curve3D: " +
                                curve_name);
    return e;
  }
  const bool is_3d = c3.is_valid();
  const std::string type = is_3d ? "Path3D" : "Path2D";
  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("ClassDB singleton not available");
    return e;
  }
  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("failed to instantiate node type: " + type);
    return e;
  }
  auto *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("instantiated object is not a Node: " + type);
    return e;
  }
  node->set_name(godot::StringName(name.c_str()));
  if (is_3d) {
    auto *path3d = godot::Object::cast_to<godot::Path3D>(node);
    if (!path3d) {
      memdelete(node);
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("instantiated object is not a Path3D");
      return e;
    }
    path3d->set_curve(c3);
  } else {
    auto *path2d = godot::Object::cast_to<godot::Path2D>(node);
    if (!path2d) {
      memdelete(node);
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("instantiated object is not a Path2D");
      return e;
    }
    path2d->set_curve(c2);
  }
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    memdelete(node);
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("EditorInterface not available");
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
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parent node not found: " + pp->GetString() +
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
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "scene already has a root, use parent_path to add children");
      return e;
    }
    editor->add_root_node(node);
  }
  auto *scene_root = editor->get_edited_scene_root();
  scene_dirty_tracker::mark_scene_modified();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  inner["type"] = mcp::JsonValue(type);
  inner["curve"] = mcp::JsonValue(curve_name);
  inner["undo"] = mcp::JsonValue("delete node " +
                                 util::to_std(node->get_name()) +
                                 " (delete_scene_node)");
  util::add_scene_info_fields(inner, scene_info);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info);
  return r;
}

mcp::JsonValue handle_advance_path_follow(const mcp::JsonValue &args) {
  auto *p = args.Find("path");
  if (!p || !p->IsString() || p->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: path");
    return e;
  }
  auto *o = args.Find("offset");
  if (!o || !o->IsNumber()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: offset (number "
                                "of pixels to advance; negative moves back)");
    return e;
  }
  const std::string path = p->GetString();
  const double offset = util::json_number(o, 0.0);
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  auto *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + path + " — " + hint);
    return e;
  }
  auto *follow2d = godot::Object::cast_to<godot::PathFollow2D>(node);
  auto *follow3d = godot::Object::cast_to<godot::PathFollow3D>(node);
  if (!follow2d && !follow3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node is not a PathFollow2D/PathFollow3D: " +
                                path);
    return e;
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  if (follow2d) {
    const float next = follow2d->get_progress() + static_cast<float>(offset);
    follow2d->set_progress(next);
    r["result"] = mcp::JsonValue("advanced");
    r["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
    r["type"] = mcp::JsonValue("PathFollow2D");
    r["progress"] =
        mcp::JsonValue(static_cast<double>(follow2d->get_progress()));
    r["progress_ratio"] =
        mcp::JsonValue(static_cast<double>(follow2d->get_progress_ratio()));
    r["position"] =
        util::vec2_to_json(follow2d->get_global_position());
  } else {
    const float next = follow3d->get_progress() + static_cast<float>(offset);
    follow3d->set_progress(next);
    r["result"] = mcp::JsonValue("advanced");
    r["path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
    r["type"] = mcp::JsonValue("PathFollow3D");
    r["progress"] =
        mcp::JsonValue(static_cast<double>(follow3d->get_progress()));
    r["progress_ratio"] =
        mcp::JsonValue(static_cast<double>(follow3d->get_progress_ratio()));
    r["position"] =
        util::vec3_to_json(follow3d->get_global_position());
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_focus_camera_on_node(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  auto *tp = args.Find("target_path");
  if (!tp || !tp->IsString() || tp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: target_path");
    return e;
  }
  double margin_value = 0.0;
  auto *mp = args.Find("margin");
  if (mp) {
    if (!mp->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'margin' must be a number");
      return e;
    }
    margin_value =
        mp->IsInt() ? static_cast<double>(mp->GetInt()) : mp->GetDouble();
    if (!(margin_value >= 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'margin' must not be negative");
      return e;
    }
  }
  std::string camera_path = cp->GetString();
  std::string target_path = tp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  godot::Node *camera_node = godot_autopilot::util::resolve_scene_node(
      camera_path, scene_root, &hint);
  if (!camera_node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + camera_path +
                                " — " + hint);
    return e;
  }
  godot::Node *target = godot_autopilot::util::resolve_scene_node(
      target_path, scene_root, &hint);
  if (!target) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + target_path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d =
      godot::Object::cast_to<godot::Camera2D>(camera_node);
  godot::Camera3D *cam3d =
      godot::Object::cast_to<godot::Camera3D>(camera_node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node is not a Camera2D/Camera3D: " +
                                camera_path + " (actual class: " +
                                util::to_std(camera_node->get_class()) + ")");
    return e;
  }
  godot::Node3D *target_3d = godot::Object::cast_to<godot::Node3D>(target);
  godot::CanvasItem *target_canvas =
      godot::Object::cast_to<godot::CanvasItem>(target);
  std::string space;
  if (target_3d)
    space = "3d";
  else if (target_canvas)
    space = "2d";
  else {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("unsupported node type: " + target_path +
                                " (actual class: " +
                                util::to_std(target->get_class()) + ")");
    return e;
  }
  if (space == "2d" && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "camera type " + util::to_std(camera_node->get_class()) +
        " is not compatible with 2D target '" + target_path + "'");
    return e;
  }
  if (space == "3d" && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "camera type " + util::to_std(camera_node->get_class()) +
        " is not compatible with 3D target '" + target_path + "'");
    return e;
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  if (cam2d) {
    godot::Vector2 anchor;
    godot::Rect2 base_rect;
    if (auto *ctrl = godot::Object::cast_to<godot::Control>(target)) {
      godot::Rect2 gr = ctrl->get_global_rect();
      anchor = gr.position + gr.size * 0.5f;
      base_rect = gr;
    } else {
      anchor = target_canvas->get_global_transform().get_origin();
      base_rect = godot::Rect2(anchor, godot::Vector2());
    }
    godot::Rect2 focus_rect =
        base_rect.grow(static_cast<float>(margin_value));
    godot::Vector2 center = focus_rect.get_center();
    godot::Vector2 old_pos = cam2d->get_global_position();
    if (undo_redo) {
      undo_redo->create_action(godot::String("Focus Camera On Node"));
      undo_redo->add_do_method(cam2d, godot::StringName("set_global_position"),
                               godot::Variant(center));
      undo_redo->add_undo_method(cam2d,
                                 godot::StringName("set_global_position"),
                                 godot::Variant(old_pos));
      undo_redo->commit_action();
    } else {
      cam2d->set_global_position(center);
    }
    r["result"] = mcp::JsonValue("focused");
    r["camera_path"] =
        mcp::JsonValue(scene_relative_path(camera_node, scene_root));
    r["camera_type"] =
        mcp::JsonValue(util::to_std(camera_node->get_class()));
    r["target_path"] = mcp::JsonValue(scene_relative_path(target, scene_root));
    r["target_type"] = mcp::JsonValue(util::to_std(target->get_class()));
    r["space"] = mcp::JsonValue("2d");
    r["position"] = util::vec2_to_json(cam2d->get_global_position());
    r["zoom"] = util::vec2_to_json(cam2d->get_zoom());
    mcp::JsonValue rect_pos(mcp::JsonValue::object_tag);
    rect_pos["x"] = mcp::JsonValue(static_cast<double>(focus_rect.position.x));
    rect_pos["y"] = mcp::JsonValue(static_cast<double>(focus_rect.position.y));
    mcp::JsonValue rect_size(mcp::JsonValue::object_tag);
    rect_size["x"] = mcp::JsonValue(static_cast<double>(focus_rect.size.x));
    rect_size["y"] = mcp::JsonValue(static_cast<double>(focus_rect.size.y));
    mcp::JsonValue focus_rect_j(mcp::JsonValue::object_tag);
    focus_rect_j["position"] = std::move(rect_pos);
    focus_rect_j["size"] = std::move(rect_size);
    r["focus_rect"] = std::move(focus_rect_j);
    r["margin"] = mcp::JsonValue(margin_value);
  } else {
    godot::Vector3 focus = target_3d->get_global_transform().get_origin();
    godot::Transform3D old_gt = cam3d->get_global_transform();
    godot::Vector3 eye = old_gt.get_origin();
    godot::Vector3 aim_eye = eye;
    if (margin_value > 0.0) {
      godot::Vector3 back = eye - focus;
      if (back.length_squared() < 0.00000001)
        back = godot::Vector3(0.0f, 0.0f, 1.0f);
      aim_eye = eye + back.normalized() * static_cast<float>(margin_value);
    }
    if ((focus - aim_eye).length_squared() < 0.00000001) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "camera and target coincide — move the camera before look_at: " +
          scene_relative_path(camera_node, scene_root));
      return e;
    }
    godot::Transform3D next_gt = old_gt;
    next_gt.origin = aim_eye;
    godot::Transform3D aimed = next_gt.looking_at(focus);
    if (undo_redo) {
      undo_redo->create_action(godot::String("Focus Camera On Node"));
      undo_redo->add_do_method(cam3d, godot::StringName("set_global_transform"),
                               godot::Variant(aimed));
      undo_redo->add_undo_method(cam3d,
                                 godot::StringName("set_global_transform"),
                                 godot::Variant(old_gt));
      undo_redo->commit_action();
    } else {
      cam3d->set_global_transform(aimed);
    }
    bool behind = cam3d->is_position_behind(focus);
    r["result"] = mcp::JsonValue("focused");
    r["camera_path"] =
        mcp::JsonValue(scene_relative_path(camera_node, scene_root));
    r["camera_type"] =
        mcp::JsonValue(util::to_std(camera_node->get_class()));
    r["target_path"] = mcp::JsonValue(scene_relative_path(target, scene_root));
    r["target_type"] = mcp::JsonValue(util::to_std(target->get_class()));
    r["space"] = mcp::JsonValue("3d");
    r["position"] = util::vec3_to_json(cam3d->get_global_position());
    r["target_position"] = util::vec3_to_json(focus);
    r["behind"] = mcp::JsonValue(behind);
    r["margin"] = mcp::JsonValue(margin_value);
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_set_camera_params(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  auto *params_p = args.Find("params");
  if (!params_p || !params_p->IsObject()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "missing required parameter: params (object with any of zoom, fov, "
        "near, far, limits)");
    return e;
  }
  std::string path = cp->GetString();
  const char *limit_names[5] = {"limit_left", "limit_top", "limit_right",
                                "limit_bottom", "limit_smoothed"};
  for (const auto &kv : *params_p) {
    if (kv.first != "zoom" && kv.first != "fov" && kv.first != "near" &&
        kv.first != "far" && kv.first != "limits") {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("unknown params key '" + kv.first +
                                  "' — expected any of zoom, fov, near, far, "
                                  "limits");
      return e;
    }
  }
  godot::Vector2 zoom2(1.0f, 1.0f);
  bool has_zoom = false;
  auto *zoom_p = params_p->Find("zoom");
  if (zoom_p) {
    if (zoom_p->IsNumber()) {
      double z = zoom_p->IsInt() ? static_cast<double>(zoom_p->GetInt())
                                 : zoom_p->GetDouble();
      if (!(z > 0.0)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] =
            mcp::JsonValue("parameter 'zoom' must be greater than 0");
        return e;
      }
      zoom2 = godot::Vector2(static_cast<float>(z), static_cast<float>(z));
      has_zoom = true;
    } else if (zoom_p->IsObject()) {
      auto *x = zoom_p->Find("x");
      auto *y = zoom_p->Find("y");
      if (!x || !x->IsNumber() || !y || !y->IsNumber()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "parameter 'zoom' object must have numeric x and y");
        return e;
      }
      double zx = util::json_number(x, 0.0);
      double zy = util::json_number(y, 0.0);
      if (!(zx > 0.0) || !(zy > 0.0)) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(
            "parameter 'zoom' components must be greater than 0");
        return e;
      }
      zoom2 = godot::Vector2(static_cast<float>(zx), static_cast<float>(zy));
      has_zoom = true;
    } else {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'zoom' must be a number or {x, y} object");
      return e;
    }
  }
  double fov_value = 0.0;
  bool has_fov = false;
  auto *fov_p = params_p->Find("fov");
  if (fov_p) {
    if (!fov_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'fov' must be a number");
      return e;
    }
    fov_value = fov_p->IsInt() ? static_cast<double>(fov_p->GetInt())
                               : fov_p->GetDouble();
    if (!(fov_value >= 1.0) || !(fov_value <= 179.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'fov' must be between 1 and 179");
      return e;
    }
    has_fov = true;
  }
  double near_value = 0.0;
  bool has_near = false;
  auto *near_p = params_p->Find("near");
  if (near_p) {
    if (!near_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'near' must be a number");
      return e;
    }
    near_value = near_p->IsInt() ? static_cast<double>(near_p->GetInt())
                                 : near_p->GetDouble();
    if (!(near_value > 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] =
          mcp::JsonValue("parameter 'near' must be greater than 0");
      return e;
    }
    has_near = true;
  }
  double far_value = 0.0;
  bool has_far = false;
  auto *far_p = params_p->Find("far");
  if (far_p) {
    if (!far_p->IsNumber()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'far' must be a number");
      return e;
    }
    far_value = far_p->IsInt() ? static_cast<double>(far_p->GetInt())
                               : far_p->GetDouble();
    if (!(far_value > 0.0)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'far' must be greater than 0");
      return e;
    }
    has_far = true;
  }
  struct LimitEntry {
    std::string name;
    godot::Variant value;
  };
  std::vector<LimitEntry> limit_entries;
  bool has_limits = false;
  auto *limits_p = params_p->Find("limits");
  if (limits_p) {
    if (!limits_p->IsObject()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'limits' must be an object");
      return e;
    }
    for (const auto &kv : *limits_p) {
      bool known = false;
      for (int i = 0; i < 5; i++) {
        if (kv.first == limit_names[i]) {
          known = true;
          break;
        }
      }
      if (!known) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("unknown limits key '" + kv.first + "'");
        return e;
      }
    }
    for (int i = 0; i < 5; i++) {
      const mcp::JsonValue *entry = limits_p->Find(limit_names[i]);
      if (!entry)
        continue;
      if (i < 4) {
        if (!entry->IsNumber()) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(std::string("parameter 'limits.") +
                                      limit_names[i] + "' must be a number");
          return e;
        }
        double v = entry->IsInt() ? static_cast<double>(entry->GetInt())
                                  : entry->GetDouble();
        limit_entries.push_back(
            LimitEntry{limit_names[i], godot::Variant(static_cast<int>(v))});
      } else {
        if (!entry->IsBool()) {
          mcp::JsonValue e(mcp::JsonValue::object_tag);
          e["error"] = mcp::JsonValue(
              "parameter 'limits.limit_smoothed' must be a boolean");
          return e;
        }
        limit_entries.push_back(
            LimitEntry{limit_names[i], godot::Variant(entry->GetBool())});
      }
    }
    if (limit_entries.empty()) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue(
          "parameter 'limits' must contain at least one of limit_left, "
          "limit_top, limit_right, limit_bottom, limit_smoothed");
      return e;
    }
    has_limits = true;
  }
  if (!has_zoom && !has_fov && !has_near && !has_far && !has_limits) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "nothing to set — params must contain at least one of: zoom, fov, "
        "near, far, limits");
    return e;
  }
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("no edited scene: open or create a scene first");
    return e;
  }
  std::string hint;
  godot::Node *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d = godot::Object::cast_to<godot::Camera2D>(node);
  godot::Camera3D *cam3d = godot::Object::cast_to<godot::Camera3D>(node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "node is not a Camera2D/Camera3D: " + path + " (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_zoom && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'zoom' applies only to a Camera2D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_limits && !cam2d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'limits' applies only to a Camera2D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (has_fov && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameter 'fov' applies only to a Camera3D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if ((has_near || has_far) && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "parameters 'near' and 'far' apply only to a Camera3D (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  if (cam3d && (has_near || has_far)) {
    double eff_near =
        has_near ? near_value : static_cast<double>(cam3d->get_near());
    double eff_far =
        has_far ? far_value : static_cast<double>(cam3d->get_far());
    if (!(eff_near < eff_far)) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("parameter 'near' must be below 'far'");
      return e;
    }
  }
  auto *undo_redo = editor->get_editor_undo_redo();
  mcp::JsonValue applied(mcp::JsonValue::array_tag);
  if (undo_redo) {
    undo_redo->create_action(godot::String("Set Camera Params"));
    if (has_zoom) {
      undo_redo->add_do_method(cam2d, godot::StringName("set_zoom"),
                               godot::Variant(zoom2));
      undo_redo->add_undo_method(cam2d, godot::StringName("set_zoom"),
                                 godot::Variant(cam2d->get_zoom()));
      applied.PushBack(mcp::JsonValue("zoom"));
    }
    if (has_fov) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_fov"),
                               godot::Variant(static_cast<float>(fov_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_fov"),
                                 godot::Variant(cam3d->get_fov()));
      applied.PushBack(mcp::JsonValue("fov"));
    }
    if (has_near) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_near"),
                               godot::Variant(static_cast<float>(near_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_near"),
                                 godot::Variant(cam3d->get_near()));
      applied.PushBack(mcp::JsonValue("near"));
    }
    if (has_far) {
      undo_redo->add_do_method(cam3d, godot::StringName("set_far"),
                               godot::Variant(static_cast<float>(far_value)));
      undo_redo->add_undo_method(cam3d, godot::StringName("set_far"),
                                 godot::Variant(cam3d->get_far()));
      applied.PushBack(mcp::JsonValue("far"));
    }
    if (has_limits) {
      for (const LimitEntry &entry : limit_entries) {
        undo_redo->add_do_method(cam2d, godot::StringName("set"),
                                 godot::StringName(entry.name.c_str()),
                                 entry.value);
        undo_redo->add_undo_method(
            cam2d, godot::StringName("set"),
            godot::StringName(entry.name.c_str()),
            cam2d->get(godot::StringName(entry.name.c_str())));
      }
      applied.PushBack(mcp::JsonValue("limits"));
    }
    undo_redo->commit_action();
  } else {
    if (has_zoom) {
      cam2d->set_zoom(zoom2);
      applied.PushBack(mcp::JsonValue("zoom"));
    }
    if (has_fov) {
      cam3d->set_fov(static_cast<float>(fov_value));
      applied.PushBack(mcp::JsonValue("fov"));
    }
    if (has_near) {
      cam3d->set_near(static_cast<float>(near_value));
      applied.PushBack(mcp::JsonValue("near"));
    }
    if (has_far) {
      cam3d->set_far(static_cast<float>(far_value));
      applied.PushBack(mcp::JsonValue("far"));
    }
    if (has_limits) {
      for (const LimitEntry &entry : limit_entries)
        cam2d->set(godot::StringName(entry.name.c_str()), entry.value);
      applied.PushBack(mcp::JsonValue("limits"));
    }
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("ok");
  r["camera_path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  r["type"] = mcp::JsonValue(util::to_std(node->get_class()));
  r["applied"] = std::move(applied);
  if (cam2d) {
    r["position"] = util::vec2_to_json(cam2d->get_global_position());
    r["zoom"] = util::vec2_to_json(cam2d->get_zoom());
    mcp::JsonValue limits_j(mcp::JsonValue::object_tag);
    for (int i = 0; i < 5; i++)
      limits_j[limit_names[i]] = VariantJson::serialize(
          cam2d->get(godot::StringName(limit_names[i])));
    r["limits"] = std::move(limits_j);
  } else {
    r["position"] = util::vec3_to_json(cam3d->get_global_position());
    r["fov"] = mcp::JsonValue(static_cast<double>(cam3d->get_fov()));
    r["near"] = mcp::JsonValue(static_cast<double>(cam3d->get_near()));
    r["far"] = mcp::JsonValue(static_cast<double>(cam3d->get_far()));
  }
  scene_dirty_tracker::mark_scene_modified();
  util::add_scene_info_fields(r, util::edited_scene_info());
  return r;
}

mcp::JsonValue handle_get_camera_view_rect(const mcp::JsonValue &args) {
  auto *cp = args.Find("camera_path");
  if (!cp || !cp->IsString() || cp->GetString().empty()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: camera_path");
    return e;
  }
  std::string path = cp->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  auto *scene_root = editor ? editor->get_edited_scene_root() : nullptr;
  if (!editor || !scene_root) {
    return util::error_detail(
        "no edited scene root available",
        "scene_ops.cpp handle_get_camera_view_rect", "an edited scene",
        "open or create a scene first (create_editor_scene)");
  }
  std::string hint;
  godot::Node *node =
      godot_autopilot::util::resolve_scene_node(path, scene_root, &hint);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("camera node not found: " + path + " — " + hint);
    return e;
  }
  godot::Camera2D *cam2d = godot::Object::cast_to<godot::Camera2D>(node);
  godot::Camera3D *cam3d = godot::Object::cast_to<godot::Camera3D>(node);
  if (!cam2d && !cam3d) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "node is not a Camera2D/Camera3D: " + path + " (actual class: " +
        util::to_std(node->get_class()) + ")");
    return e;
  }
  std::string space = cam2d ? "2d" : "3d";
  godot::SubViewport *vp2d = editor->get_editor_viewport_2d();
  godot::SubViewport *vp3d = editor->get_editor_viewport_3d(0);
  godot::Transform2D t2d;
  if (vp2d)
    t2d = vp2d->get_screen_transform() * vp2d->get_global_canvas_transform();
  godot::Transform2D t3d;
  if (vp3d)
    t3d = vp3d->get_screen_transform();
  godot::Camera3D *editor_camera = vp3d ? vp3d->get_camera_3d() : nullptr;
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["camera_path"] = mcp::JsonValue(scene_relative_path(node, scene_root));
  inner["camera_type"] = mcp::JsonValue(util::to_std(node->get_class()));
  inner["viewport"] = mcp::JsonValue(space);
  inner["window_id"] = mcp::JsonValue(0);
  inner["space"] = mcp::JsonValue("window");
  if (space == "2d") {
    if (!vp2d) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("2D editor viewport not available");
      return e;
    }
    godot::Vector2 world = cam2d->get_global_position();
    godot::Vector2 center = t2d.xform(world);
    godot::Vector2 zoom = cam2d->get_zoom();
    godot::Vector2 vp_size = vp2d->get_visible_rect().size;
    inner["world_position"] = util::vec2_to_json(world);
    mcp::JsonValue window_center(mcp::JsonValue::object_tag);
    window_center["x"] = mcp::JsonValue(static_cast<double>(center.x));
    window_center["y"] = mcp::JsonValue(static_cast<double>(center.y));
    inner["window_center"] = std::move(window_center);
    inner["zoom"] = util::vec2_to_json(zoom);
    inner["behind"] = mcp::JsonValue(false);
    mcp::JsonValue viewport_size(mcp::JsonValue::object_tag);
    viewport_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    viewport_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    inner["viewport_size"] = std::move(viewport_size);
    mcp::JsonValue view_pos(mcp::JsonValue::object_tag);
    view_pos["x"] =
        mcp::JsonValue(static_cast<double>(center.x - vp_size.x * 0.5));
    view_pos["y"] =
        mcp::JsonValue(static_cast<double>(center.y - vp_size.y * 0.5));
    mcp::JsonValue view_size(mcp::JsonValue::object_tag);
    view_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    view_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    mcp::JsonValue view_rect(mcp::JsonValue::object_tag);
    view_rect["position"] = std::move(view_pos);
    view_rect["size"] = std::move(view_size);
    inner["view_rect"] = std::move(view_rect);
    godot::Vector2 scale = t2d.get_scale();
    godot::Vector2 origin = t2d.get_origin();
    mcp::JsonValue mapping(mcp::JsonValue::object_tag);
    mapping["scale_x"] = mcp::JsonValue(static_cast<double>(scale.x));
    mapping["scale_y"] = mcp::JsonValue(static_cast<double>(scale.y));
    mapping["offset_x"] = mcp::JsonValue(static_cast<double>(origin.x));
    mapping["offset_y"] = mcp::JsonValue(static_cast<double>(origin.y));
    inner["mapping"] = std::move(mapping);
  } else {
    if (!vp3d) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("3D editor viewport not available");
      return e;
    }
    if (!editor_camera) {
      mcp::JsonValue e(mcp::JsonValue::object_tag);
      e["error"] = mcp::JsonValue("3D editor viewport has no camera");
      return e;
    }
    godot::Vector3 world = cam3d->get_global_position();
    godot::Vector2 local = editor_camera->unproject_position(world);
    godot::Vector2 center = t3d.xform(local);
    godot::Transform3D gt = cam3d->get_global_transform();
    godot::Vector3 tip = gt.xform(godot::Vector3(0.0f, 0.0f, -1.0f));
    godot::Vector3 forward = tip - gt.get_origin();
    godot::Vector2 vp_size = vp3d->get_visible_rect().size;
    inner["world_position"] = util::vec3_to_json(world);
    inner["forward"] = util::vec3_to_json(forward);
    inner["fov"] = mcp::JsonValue(static_cast<double>(cam3d->get_fov()));
    inner["near"] = mcp::JsonValue(static_cast<double>(cam3d->get_near()));
    inner["far"] = mcp::JsonValue(static_cast<double>(cam3d->get_far()));
    mcp::JsonValue window_center(mcp::JsonValue::object_tag);
    window_center["x"] = mcp::JsonValue(static_cast<double>(center.x));
    window_center["y"] = mcp::JsonValue(static_cast<double>(center.y));
    inner["window_center"] = std::move(window_center);
    inner["behind"] =
        mcp::JsonValue(editor_camera->is_position_behind(world));
    mcp::JsonValue viewport_size(mcp::JsonValue::object_tag);
    viewport_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    viewport_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    inner["viewport_size"] = std::move(viewport_size);
    mcp::JsonValue view_pos(mcp::JsonValue::object_tag);
    view_pos["x"] =
        mcp::JsonValue(static_cast<double>(center.x - vp_size.x * 0.5));
    view_pos["y"] =
        mcp::JsonValue(static_cast<double>(center.y - vp_size.y * 0.5));
    mcp::JsonValue view_size(mcp::JsonValue::object_tag);
    view_size["x"] = mcp::JsonValue(static_cast<double>(vp_size.x));
    view_size["y"] = mcp::JsonValue(static_cast<double>(vp_size.y));
    mcp::JsonValue view_rect(mcp::JsonValue::object_tag);
    view_rect["position"] = std::move(view_pos);
    view_rect["size"] = std::move(view_size);
    inner["view_rect"] = std::move(view_rect);
    godot::Vector2 scale = t3d.get_scale();
    godot::Vector2 origin = t3d.get_origin();
    mcp::JsonValue mapping(mcp::JsonValue::object_tag);
    mapping["scale_x"] = mcp::JsonValue(static_cast<double>(scale.x));
    mapping["scale_y"] = mcp::JsonValue(static_cast<double>(scale.y));
    mapping["offset_x"] = mcp::JsonValue(static_cast<double>(origin.x));
    mapping["offset_y"] = mcp::JsonValue(static_cast<double>(origin.y));
    inner["mapping"] = std::move(mapping);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  return r;
}
} // namespace scene_ops
} // namespace godot_autopilot
