#include "scene_spec_ops.hpp"
#include "../util/scene_path.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "property_ops.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include "util/type_hint.hpp"
#include "util/variant_json.hpp"
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

bool spec_trial_find_property(godot::Node *node, const std::string &prop_name,
                              godot::Dictionary &out_info) {
  godot::TypedArray<godot::Dictionary> props = node->get_property_list();
  for (int64_t i = 0; i < props.size(); i++) {
    godot::Dictionary dict = props[i];
    if (!dict.has("name"))
      continue;
    if (util::to_std(dict["name"].operator godot::String()) != prop_name)
      continue;
    out_info = dict;
    return true;
  }
  return false;
}

bool spec_trial_value(const godot::Dictionary &info,
                      const std::string &prop_name,
                      const mcp::JsonValue &raw_value, const std::string &loc,
                      std::string &error) {
  godot::Variant::Type value_type = godot::Variant::NIL;
  if (info.has("type"))
    value_type =
        static_cast<godot::Variant::Type>(static_cast<int>(info["type"]));
  const bool object_typed = value_type == godot::Variant::OBJECT;
  if (value_type == godot::Variant::NODE_PATH) {
    if (!raw_value.IsString()) {
      error = "应用属性 '" + prop_name + "'（位于 " + loc +
              "）失败：node path property requires a string value";
      return false;
    }
    return true;
  }
  if (object_typed && raw_value.IsString()) {
    const std::string s = raw_value.GetString();
    if (s.rfind("res://", 0) != 0 && s.rfind("memory://", 0) != 0)
      return true;
  }
  if (object_typed && raw_value.IsObject()) {
    godot::Variant tmp;
    std::string resource_error;
    if (resource_ops::try_resolve_resource_value(raw_value, tmp,
                                                 resource_error)) {
      if (!resource_error.empty()) {
        error = "应用属性 '" + prop_name + "'（位于 " + loc +
                "）失败：" + resource_error;
        return false;
      }
    }
    return true;
  }
  if (value_type == godot::Variant::ARRAY) {
    if (!raw_value.IsArray()) {
      error = "应用属性 '" + prop_name + "'（位于 " + loc +
              "）失败：refusing to write (a non-array value would silently "
              "clear the array)";
      return false;
    }
    for (const auto &element : raw_value.GetArray()) {
      bool resource_shaped = false;
      if (element.IsString()) {
        const std::string s = element.GetString();
        resource_shaped =
            s.rfind("res://", 0) == 0 || s.rfind("memory://", 0) == 0;
      } else if (element.IsObject()) {
        const mcp::JsonValue *ref = element.Find("path");
        if (!ref)
          ref = element.Find("resource");
        resource_shaped = ref != nullptr && ref->IsString();
      }
      if (!resource_shaped)
        continue;
      godot::Variant tmp;
      std::string resource_error;
      if (resource_ops::try_resolve_resource_value(element, tmp,
                                                   resource_error) &&
          !resource_error.empty()) {
        error = "应用属性 '" + prop_name + "'（位于 " + loc +
                "）失败：" + resource_error;
        return false;
      }
    }
    return true;
  }
  godot::Variant converted;
  std::string resource_error;
  if (resource_ops::try_resolve_resource_value(raw_value, converted,
                                               resource_error)) {
    if (!resource_error.empty()) {
      error = "应用属性 '" + prop_name + "'（位于 " + loc +
              "）失败：" + resource_error;
      return false;
    }
    return true;
  }
  try {
    VariantJson::deserialize_strict(
        raw_value, util::infer_type_hint(info, std::string()));
  } catch (const std::exception &e) {
    error = "应用属性 '" + prop_name + "'（位于 " + loc +
            "）失败：cannot convert value: " + e.what();
    return false;
  } catch (...) {
    error = "应用属性 '" + prop_name + "'（位于 " + loc +
            "）失败：cannot convert value";
    return false;
  }
  return true;
}

bool spec_trial_props(godot::Node *node, const mcp::JsonValue &props,
                      const std::string &loc, int64_t &checked_count,
                      std::string &error) {
  for (const auto &kv : props) {
    godot::Dictionary info;
    if (!spec_trial_find_property(node, kv.first, info)) {
      error = "应用属性 '" + kv.first + "'（位于 " + loc +
              "）失败：property '" + kv.first + "' does not exist on " +
              util::to_std(node->get_class());
      return false;
    }
    if (!spec_trial_value(info, kv.first, kv.second, loc, error))
      return false;
    checked_count++;
  }
  return true;
}

bool spec_trial_node(const mcp::JsonValue &spec,
                     godot::ClassDBSingleton *cdbs, const std::string &loc,
                     int64_t &node_count, int64_t &prop_count,
                     std::string &error) {
  std::string type, name;
  spec_read_node_fields(spec, type, name);
  godot::Variant obj_var =
      cdbs->instantiate(godot::StringName(type.c_str()));
  godot::Node *node = godot::Object::cast_to<godot::Node>(obj_var);
  if (!node) {
    error = "实例化节点类型失败：" + type + "（位于 " + loc + "）";
    return false;
  }
  node_count++;
  bool ok = true;
  if (auto *props = spec.Find("props"); props && props->IsObject())
    ok = spec_trial_props(node, *props, loc, prop_count, error);
  if (ok) {
    if (auto *children = spec.Find("children");
        children && children->IsArray()) {
      const auto &arr = children->GetArray();
      for (size_t i = 0; i < arr.size() && ok; i++) {
        ok = spec_trial_node(arr[i], cdbs,
                             loc + ".children[" + std::to_string(i) + "]",
                             node_count, prop_count, error);
      }
    }
  }
  memdelete(node);
  return ok;
}

bool spec_build_top_item(const mcp::JsonValue &spec, godot::Node *parent,
                         bool as_root, godot::EditorInterface *editor,
                         godot::Node *&scene_root,
                         godot::ClassDBSingleton *cdbs, const std::string &loc,
                         std::vector<mcp::JsonValue> &created,
                         std::vector<mcp::JsonValue> &warnings,
                         int64_t &applied_count, std::string &error,
                         godot::Node *&out_top) {
  out_top = nullptr;
  std::string type, top_name;
  spec_read_node_fields(spec, type, top_name);
  godot::Node *top = nullptr;
  try {
    godot::Variant obj_var =
        cdbs->instantiate(godot::StringName(type.c_str()));
    top = godot::Object::cast_to<godot::Node>(obj_var);
    if (!top) {
      error = "实例化节点类型失败：" + type + "（位于 " + loc + "）";
      return false;
    }
    top->set_name(godot::StringName(top_name.c_str()));
    if (as_root) {
      editor->add_root_node(top);
      scene_root = editor->get_edited_scene_root();
    } else {
      if (!parent) {
        error = "场景根不可用，无法确定新建节点的 owner";
        spec_free_subtree(top);
        return false;
      }
      parent->add_child(top);
    }
    if (!scene_root) {
      error = "场景根不可用，无法确定新建节点的 owner";
      spec_rollback_subtree(top);
      top = nullptr;
      return false;
    }
    if (top != scene_root)
      top->set_owner(scene_root);
    std::string top_rel = spec_relative_path(top, scene_root);
    mcp::JsonValue created_item(mcp::JsonValue::object_tag);
    created_item["path"] = mcp::JsonValue(top_rel);
    created_item["type"] = mcp::JsonValue(util::to_std(top->get_class()));
    created.push_back(std::move(created_item));
    if (auto *props = spec.Find("props"); props && props->IsObject()) {
      if (!spec_apply_props(top, *props, top_rel, loc, applied_count, warnings,
                            error)) {
        spec_rollback_subtree(top);
        top = nullptr;
        return false;
      }
    }
    if (auto *children = spec.Find("children");
        children && children->IsArray()) {
      const auto &arr = children->GetArray();
      for (size_t i = 0; i < arr.size(); i++) {
        if (!spec_build_node(arr[i], top, scene_root,
                             loc + ".children[" + std::to_string(i) + "]",
                             cdbs, created, warnings, applied_count, error)) {
          spec_rollback_subtree(top);
          top = nullptr;
          return false;
        }
      }
    }
  } catch (const std::exception &e) {
    error = std::string("构建子树时发生异常：") + e.what();
    if (top) {
      spec_rollback_subtree(top);
      top = nullptr;
    }
    return false;
  } catch (...) {
    error = "构建子树时发生未知异常";
    if (top) {
      spec_rollback_subtree(top);
      top = nullptr;
    }
    return false;
  }
  out_top = top;
  return true;
}

} // namespace

mcp::JsonValue handle_build_from_spec(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "build_nodes_from_spec called");

  auto *spec_arg = args.Find("spec");
  if (!spec_arg)
    return spec_error("missing required parameter: spec (object or array of "
                      "sibling objects)");
  std::vector<const mcp::JsonValue *> top_specs;
  std::vector<std::string> top_locs;
  if (spec_arg->IsObject()) {
    top_specs.push_back(spec_arg);
    top_locs.emplace_back("spec");
  } else if (spec_arg->IsArray()) {
    const auto &arr = spec_arg->GetArray();
    if (arr.empty())
      return spec_error("parameter 'spec' array must not be empty");
    for (size_t i = 0; i < arr.size(); i++) {
      if (!arr[i].IsObject())
        return spec_error("spec[" + std::to_string(i) + "] 必须为对象");
      top_specs.push_back(&arr[i]);
      top_locs.push_back("spec[" + std::to_string(i) + "]");
    }
  } else {
    return spec_error("missing required parameter: spec (object or array of "
                      "sibling objects)");
  }

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
  for (size_t t = 0; t < top_specs.size(); t++) {
    if (!spec_validate_node(*top_specs[t], 0, count, max_depth_seen,
                            top_locs[t], cdbs, validation_error)) {
      mcp::JsonValue e = spec_error(validation_error);
      e["rolled_back"] = mcp::JsonValue(false);
      return e;
    }
  }

  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return spec_error("EditorInterface not available");
  godot::Node *scene_root = editor->get_edited_scene_root();
  const util::EditedSceneInfo scene_info = util::edited_scene_info();

  if (dry_run) {
    mcp::JsonValue nodes(mcp::JsonValue::array_tag);
    for (size_t t = 0; t < top_specs.size(); t++)
      spec_collect_preview(*top_specs[t], parent_path, nodes);
    int64_t trial_nodes = 0;
    int64_t trial_props = 0;
    std::string trial_error;
    for (size_t t = 0; t < top_specs.size() && trial_error.empty(); t++) {
      spec_trial_node(*top_specs[t], cdbs, top_locs[t], trial_nodes,
                      trial_props, trial_error);
    }
    if (!trial_error.empty()) {
      mcp::JsonValue e = spec_error(trial_error);
      e["rolled_back"] = mcp::JsonValue(false);
      return e;
    }
    mcp::JsonValue inner(mcp::JsonValue::object_tag);
    inner["mode"] = mcp::JsonValue("dry_run");
    inner["node_count"] = mcp::JsonValue(static_cast<int64_t>(count));
    inner["max_depth"] = mcp::JsonValue(static_cast<int64_t>(max_depth_seen));
    inner["parent_path"] = mcp::JsonValue(parent_path);
    inner["nodes"] = std::move(nodes);
    inner["trial_node_count"] = mcp::JsonValue(trial_nodes);
    inner["trial_property_count"] = mcp::JsonValue(trial_props);
    inner["writable"] = mcp::JsonValue(true);
    inner["note"] = mcp::JsonValue(
        "dry_run 已对每项做真实属性转换试运行且全部通过，未写入场景，可写入；"
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

  std::vector<mcp::JsonValue> created;
  std::vector<mcp::JsonValue> warnings;
  std::vector<godot::Node *> built_tops;
  int64_t applied_count = 0;
  std::string build_error;
  bool built_ok = false;
  try {
    built_ok = true;
    for (size_t t = 0; t < top_specs.size(); t++) {
      const bool as_root = top_becomes_root && t == 0;
      godot::Node *attach =
          as_root ? nullptr : (top_becomes_root ? scene_root : parent);
      godot::Node *built = nullptr;
      std::string item_error;
      if (!spec_build_top_item(*top_specs[t], attach, as_root, editor,
                               scene_root, cdbs, top_locs[t], created, warnings,
                               applied_count, item_error, built)) {
        build_error = item_error;
        built_ok = false;
        break;
      }
      built_tops.push_back(built);
    }
  } catch (const std::exception &e) {
    build_error = std::string("构建子树时发生异常：") + e.what();
    built_ok = false;
  } catch (...) {
    build_error = "构建子树时发生未知异常";
    built_ok = false;
  }
  if (!built_ok) {
    for (godot::Node *built : built_tops)
      spec_rollback_subtree(built);
    built_tops.clear();
    mcp::JsonValue e = spec_error(build_error.empty()
                                      ? "构建子树失败，已整体回滚"
                                      : build_error + "；已整体回滚，无残留");
    e["rolled_back"] = mcp::JsonValue(true);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "build_nodes_from_spec rolled back");
    return e;
  }

  std::string first_type, first_name;
  spec_read_node_fields(*top_specs[0], first_type, first_name);
  godot::Node *first_top = built_tops.front();
  if (scene_root) {
    godot::StringName top_sn(first_type.c_str());
    if (cdbs->is_parent_class(top_sn, godot::StringName("Node2D"))) {
      editor->set_main_screen_editor(godot::String("2D"));
    } else if (cdbs->is_parent_class(top_sn, godot::StringName("Node3D"))) {
      editor->set_main_screen_editor(godot::String("3D"));
    }
  }

  std::string result_path = spec_relative_path(first_top, scene_root);
  const util::EditedSceneInfo scene_info_now = util::edited_scene_info();
  mcp::JsonValue inner(mcp::JsonValue::object_tag);
  inner["mode"] = mcp::JsonValue("built");
  if (top_specs.size() == 1) {
    inner["path"] = mcp::JsonValue(result_path);
    inner["name"] = mcp::JsonValue(util::to_std(first_top->get_name()));
    inner["type"] = mcp::JsonValue(util::to_std(first_top->get_class()));
  } else {
    inner["spec_count"] =
        mcp::JsonValue(static_cast<int64_t>(top_specs.size()));
    mcp::JsonValue paths_arr(mcp::JsonValue::array_tag);
    for (godot::Node *built : built_tops)
      paths_arr.PushBack(
          mcp::JsonValue(spec_relative_path(built, scene_root)));
    inner["paths"] = std::move(paths_arr);
  }
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
  std::string undo_text;
  if (top_specs.size() == 1 || top_becomes_root) {
    undo_text = top_becomes_root
                    ? "close scene " + result_path +
                          " without saving (close_editor_scene)"
                    : "delete node " + result_path + " (delete_scene_node)";
  } else {
    undo_text = "delete node ";
    for (size_t i = 0; i < built_tops.size(); i++) {
      if (i > 0)
        undo_text += ", ";
      undo_text += spec_relative_path(built_tops[i], scene_root);
    }
    undo_text += " (delete_scene_node)";
  }
  inner["undo"] = mcp::JsonValue(undo_text);
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
