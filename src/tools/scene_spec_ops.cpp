#include "scene_spec_ops.hpp"
#include "../util/scene_path.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "property_ops.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>

namespace godot_autopilot {
namespace scene_spec_ops {

namespace {

bool spec_name_is_valid(const std::string &name) {
  return !name.empty() && name.find('/') == std::string::npos &&
         name.find(':') == std::string::npos;
}

mcp::JsonValue spec_error(const std::string &msg) {
  mcp::JsonValue e(mcp::JsonValue::object_tag);
  e["error"] = mcp::JsonValue(msg);
  return e;
}

std::string spec_relative_path(godot::Node *node, godot::Node *scene_root) {
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

bool spec_prop_is_object_typed(const godot::Node *node,
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

void spec_read_node_fields(const mcp::JsonValue &spec, std::string &type,
                           std::string &name) {
  type = "Node";
  if (auto *t = spec.Find("type"); t && t->IsString() && !t->GetString().empty())
    type = t->GetString();
  name = "Node";
  if (auto *n = spec.Find("name"); n && n->IsString() && !n->GetString().empty())
    name = n->GetString();
}

bool spec_validate_node(const mcp::JsonValue &spec, int depth, int &count,
                        int &max_depth_seen, const std::string &loc,
                        godot::ClassDBSingleton *cdbs, std::string &error) {
  if (depth > SPEC_MAX_DEPTH) {
    error = "spec 递归深度超过上限 " + std::to_string(SPEC_MAX_DEPTH) +
            "（位于 " + loc + "）";
    return false;
  }
  if (!spec.IsObject()) {
    error = "spec 节点必须为对象（位于 " + loc + "）";
    return false;
  }
  if (++count > SPEC_MAX_NODES) {
    error = "spec 节点总数超过上限 " + std::to_string(SPEC_MAX_NODES) +
            "（位于 " + loc + "）";
    return false;
  }
  if (depth > max_depth_seen)
    max_depth_seen = depth;

  std::string type, name;
  spec_read_node_fields(spec, type, name);
  if (auto *t = spec.Find("type");
      t && (!t->IsString() || t->GetString().empty())) {
    error = "spec.type 必须为非空字符串（位于 " + loc + "）";
    return false;
  }
  if (auto *n = spec.Find("name"); n && !n->IsString()) {
    error = "spec.name 必须为字符串（位于 " + loc + "）";
    return false;
  }
  if (!spec_name_is_valid(name)) {
    error = "spec.name '" + name + "' 非法（位于 " + loc +
            "）——'/' 与 ':' 不允许出现在节点名中";
    return false;
  }
  if (!cdbs) {
    error = "ClassDB singleton 不可用";
    return false;
  }
  if (!cdbs->is_parent_class(godot::StringName(type.c_str()),
                             godot::StringName("Node"))) {
    error = type + " 不是 Node 子类（位于 " + loc + "）";
    return false;
  }
  if (auto *props = spec.Find("props");
      props && !props->IsObject()) {
    error = "spec.props 必须为对象（位于 " + loc + "）";
    return false;
  }
  auto *children = spec.Find("children");
  if (!children)
    return true;
  if (!children->IsArray()) {
    error = "spec.children 必须为数组（位于 " + loc + "）";
    return false;
  }
  const auto &arr = children->GetArray();
  for (size_t i = 0; i < arr.size(); i++) {
    if (!spec_validate_node(arr[i], depth + 1, count, max_depth_seen,
                            loc + ".children[" + std::to_string(i) + "]", cdbs,
                            error))
      return false;
  }
  return true;
}

void spec_collect_preview(const mcp::JsonValue &spec, const std::string &parent_rel,
                          mcp::JsonValue &out) {
  std::string type, name;
  spec_read_node_fields(spec, type, name);
  std::string rel = parent_rel.empty() ? name : parent_rel + "/" + name;
  mcp::JsonValue item(mcp::JsonValue::object_tag);
  item["path"] = mcp::JsonValue(rel);
  item["type"] = mcp::JsonValue(type);
  item["name"] = mcp::JsonValue(name);
  int64_t prop_count = 0;
  if (auto *props = spec.Find("props"); props && props->IsObject())
    prop_count = static_cast<int64_t>(props->GetObject().size());
  item["property_count"] = mcp::JsonValue(prop_count);
  out.PushBack(std::move(item));
  if (auto *children = spec.Find("children");
      children && children->IsArray()) {
    for (const auto &child : children->GetArray())
      spec_collect_preview(child, rel, out);
  }
}

bool spec_apply_props(godot::Node *node, const mcp::JsonValue &props,
                      const std::string &node_rel, const std::string &loc,
                      int64_t &applied_count,
                      std::vector<mcp::JsonValue> &warnings, std::string &error) {
  std::vector<std::string> object_props;
  for (const auto &kv : props) {
    if (spec_prop_is_object_typed(node, kv.first, kv.second))
      object_props.push_back(kv.first);
  }
  for (int pass = 0; pass < 2; pass++) {
    for (const auto &kv : props) {
      const bool object_typed =
          std::find(object_props.begin(), object_props.end(), kv.first) !=
          object_props.end();
      if (object_typed != (pass == 0))
        continue;
      mcp::JsonValue prop_args(mcp::JsonValue::object_tag);
      prop_args["path"] = mcp::JsonValue(node_rel);
      prop_args["property"] = mcp::JsonValue(kv.first);
      prop_args["value"] = kv.second;
      mcp::JsonValue prop_result;
      try {
        prop_result = godot_autopilot::property_ops::handle_set(prop_args);
      } catch (const std::exception &e) {
        error = "应用属性 '" + kv.first + "'（位于 " + loc +
                "）时发生异常：" + e.what();
        return false;
      } catch (...) {
        error = "应用属性 '" + kv.first + "'（位于 " + loc + "）时发生未知异常";
        return false;
      }
      if (auto *err = prop_result.Find("error")) {
        error = "应用属性 '" + kv.first + "'（位于 " + loc +
                "）失败：" + err->GetString();
        return false;
      }
      if (auto *warn = prop_result.Find("warning");
          warn && warn->IsString()) {
        mcp::JsonValue w(mcp::JsonValue::object_tag);
        w["path"] = mcp::JsonValue(node_rel);
        w["property"] = mcp::JsonValue(kv.first);
        w["warning"] = mcp::JsonValue(warn->GetString());
        warnings.push_back(std::move(w));
      }
      applied_count++;
    }
  }
  return true;
}

bool spec_build_node(const mcp::JsonValue &spec, godot::Node *parent,
                     godot::Node *scene_root, const std::string &loc,
                     godot::ClassDBSingleton *cdbs,
                     std::vector<mcp::JsonValue> &created,
                     std::vector<mcp::JsonValue> &warnings,
                     int64_t &applied_count, std::string &error) {
  std::string type, name;
  spec_read_node_fields(spec, type, name);

  godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
  if (obj_var.get_type() == godot::Variant::NIL) {
    error = "实例化节点类型失败：" + type + "（位于 " + loc + "）";
    return false;
  }
  auto *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    error = "实例化结果不是 Node：" + type + "（位于 " + loc + "）";
    return false;
  }
  node->set_name(godot::StringName(name.c_str()));
  parent->add_child(node);
  if (node != scene_root)
    node->set_owner(scene_root);

  std::string node_rel = spec_relative_path(node, scene_root);
  mcp::JsonValue created_item(mcp::JsonValue::object_tag);
  created_item["path"] = mcp::JsonValue(node_rel);
  created_item["type"] = mcp::JsonValue(util::to_std(node->get_class()));
  created.push_back(std::move(created_item));

  if (auto *props = spec.Find("props"); props && props->IsObject()) {
    if (!spec_apply_props(node, *props, node_rel, loc, applied_count, warnings,
                          error))
      return false;
  }
  if (auto *children = spec.Find("children");
      children && children->IsArray()) {
    const auto &arr = children->GetArray();
    for (size_t i = 0; i < arr.size(); i++) {
      if (!spec_build_node(arr[i], node, scene_root,
                           loc + ".children[" + std::to_string(i) + "]", cdbs,
                           created, warnings, applied_count, error))
        return false;
    }
  }
  return true;
}

void spec_free_subtree(godot::Node *top) {
  if (!top)
    return;
  auto children = top->get_children();
  for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
    if (auto *child = godot::Object::cast_to<godot::Node>(children[i])) {
      top->remove_child(child);
      spec_free_subtree(child);
    }
  }
  memdelete(top);
}

void spec_rollback_subtree(godot::Node *top) {
  if (!top)
    return;
  if (godot::Node *parent = top->get_parent())
    parent->remove_child(top);
  spec_free_subtree(top);
}

} // namespace

mcp::JsonValue handle_build_from_spec(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_nodes_from_spec called");

  auto *spec_arg = args.Find("spec");
  if (!spec_arg || !spec_arg->IsObject())
    return spec_error("missing required parameter: spec (object)");

  std::string parent_path;
  if (auto *pp = args.Find("parent_path")) {
    if (!pp->IsString())
      return spec_error("parameter 'parent_path' must be a string");
    parent_path = pp->GetString();
  }
  bool dry_run = false;
  for (const char *flag : {"dry_run", "preview"}) {
    if (auto *f = args.Find(flag)) {
      if (!f->IsBool())
        return spec_error(std::string("parameter '") + flag +
                          "' must be a boolean");
      dry_run = dry_run || f->GetBool();
    }
  }

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  int count = 0;
  int max_depth_seen = 0;
  std::string validation_error;
  if (!spec_validate_node(*spec_arg, 0, count, max_depth_seen, "spec", cdbs,
                          validation_error)) {
    mcp::JsonValue e = spec_error(validation_error);
    e["rolled_back"] = mcp::JsonValue(false);
    return e;
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return spec_error("EditorInterface not available");
  godot::Node *scene_root = editor->get_edited_scene_root();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();

  if (dry_run) {
    mcp::JsonValue nodes(mcp::JsonValue::array_tag);
    spec_collect_preview(*spec_arg, parent_path, nodes);
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["mode"] = mcp::JsonValue("dry_run");
    inner["node_count"] = mcp::JsonValue(static_cast<int64_t>(count));
    inner["max_depth"] = mcp::JsonValue(static_cast<int64_t>(max_depth_seen));
    inner["parent_path"] = mcp::JsonValue(parent_path);
    inner["nodes"] = std::move(nodes);
    inner["note"] = mcp::JsonValue(
        "dry_run 仅校验 spec 并返回节点清单，未写入场景；"
        "同级重名节点会被引擎自动重命名，实际路径以 built 响应为准");
    util::add_scene_info_fields(inner, scene_info);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(inner);
    util::add_scene_info_fields(r, scene_info);
    return r;
  }

  godot::Node *parent = nullptr;
  bool top_becomes_root = false;
  if (parent_path.empty()) {
    if (scene_root) {
      parent = scene_root;
    } else {
      top_becomes_root = true;
    }
  } else {
    std::string hint;
    parent = godot_autopilot::util::resolve_scene_node(parent_path, scene_root,
                                                       &hint);
    if (!parent) {
      mcp::JsonValue e = spec_error("parent node not found: " + parent_path +
                                    " — " + hint);
      e["rolled_back"] = mcp::JsonValue(false);
      return e;
    }
  }

  std::string type, top_name;
  spec_read_node_fields(*spec_arg, type, top_name);

  godot::Node *top = nullptr;
  std::vector<mcp::JsonValue> created;
  std::vector<mcp::JsonValue> warnings;
  int64_t applied_count = 0;
  std::string build_error;
  bool built_ok = false;
  try {
    godot::Variant obj_var =
        cdbs->instantiate(godot::StringName(type.c_str()));
    top = godot::Object::cast_to<godot::Node>(obj_var);
    if (!top) {
      build_error = "实例化节点类型失败：" + type + "（位于 spec）";
    } else {
      top->set_name(godot::StringName(top_name.c_str()));
      if (top_becomes_root) {
        editor->add_root_node(top);
        scene_root = editor->get_edited_scene_root();
      } else {
        parent->add_child(top);
      }
      if (!scene_root) {
        build_error = "场景根不可用，无法确定新建节点的 owner";
        spec_rollback_subtree(top);
        top = nullptr;
      } else {
        if (top != scene_root)
          top->set_owner(scene_root);
        std::string top_rel = spec_relative_path(top, scene_root);
        mcp::JsonValue created_item(mcp::JsonValue::object_tag);
        created_item["path"] = mcp::JsonValue(top_rel);
        created_item["type"] = mcp::JsonValue(util::to_std(top->get_class()));
        created.push_back(std::move(created_item));
        if (build_error.empty()) {
          if (auto *props = spec_arg->Find("props");
              props && props->IsObject()) {
            if (!spec_apply_props(top, *props, top_rel, "spec", applied_count,
                                  warnings, build_error)) {
              spec_rollback_subtree(top);
              top = nullptr;
            }
          }
        }
        if (build_error.empty() && top) {
          if (auto *children = spec_arg->Find("children");
              children && children->IsArray()) {
            const auto &arr = children->GetArray();
            for (size_t i = 0; i < arr.size(); i++) {
              if (!spec_build_node(arr[i], top, scene_root,
                                   "spec.children[" + std::to_string(i) + "]",
                                   cdbs, created, warnings, applied_count,
                                   build_error)) {
                spec_rollback_subtree(top);
                top = nullptr;
                break;
              }
            }
          }
        }
        built_ok = build_error.empty() && top != nullptr;
      }
    }
  } catch (const std::exception &e) {
    build_error = std::string("构建子树时发生异常：") + e.what();
  } catch (...) {
    build_error = "构建子树时发生未知异常";
  }
  if (!built_ok) {
    if (top) {
      spec_rollback_subtree(top);
      top = nullptr;
    }
    mcp::JsonValue e = spec_error(build_error.empty()
                                      ? "构建子树失败，已整体回滚"
                                      : build_error + "；已整体回滚，无残留");
    e["rolled_back"] = mcp::JsonValue(true);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "build_nodes_from_spec rolled back");
    return e;
  }

  if (scene_root) {
    godot::StringName top_sn(type.c_str());
    if (cdbs->is_parent_class(top_sn, godot::StringName("Node2D"))) {
      editor->set_main_screen_editor(godot::String("2D"));
    } else if (cdbs->is_parent_class(top_sn, godot::StringName("Node3D"))) {
      editor->set_main_screen_editor(godot::String("3D"));
    }
  }

  std::string result_path = spec_relative_path(top, scene_root);
  const util::EditedSceneInfo scene_info_now = util::edited_scene_info();
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["mode"] = mcp::JsonValue("built");
  inner["path"] = mcp::JsonValue(result_path);
  inner["name"] = mcp::JsonValue(util::to_std(top->get_name()));
  inner["type"] = mcp::JsonValue(util::to_std(top->get_class()));
  inner["node_count"] = mcp::JsonValue(static_cast<int64_t>(created.size()));
  inner["applied_property_count"] = mcp::JsonValue(applied_count);
  mcp::JsonValue created_arr(mcp::JsonValue::array_tag);
  for (auto &item : created)
    created_arr.PushBack(std::move(item));
  inner["created"] = std::move(created_arr);
  if (!warnings.empty()) {
    mcp::JsonValue warnings_arr(mcp::JsonValue::array_tag);
    for (auto &w : warnings)
      warnings_arr.PushBack(std::move(w));
    inner["property_warnings"] = std::move(warnings_arr);
  }
  inner["rolled_back"] = mcp::JsonValue(false);
  inner["undo"] = mcp::JsonValue(
      top_becomes_root
          ? "close scene " + result_path + " without saving (close_editor_scene)"
          : "delete node " + result_path + " (delete_scene_node)");
  util::add_scene_info_fields(inner, scene_info_now);
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(inner);
  util::add_scene_info_fields(r, scene_info_now);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_nodes_from_spec completed");
  return r;
}

} // namespace scene_spec_ops
} // namespace godot_autopilot
