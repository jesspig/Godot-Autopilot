#include "group_ops.hpp"
#include "core/log_system.hpp"
#include "core/scene_dirty_tracker.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_autopilot {
namespace group_ops {

namespace {

godot::Node *find_node(const std::string &path_str) {
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor)
    return nullptr;
  auto *root = editor->get_edited_scene_root();
  if (!root)
    return nullptr;

  std::string clean = path_str;
  if (!clean.empty() && clean[0] == '/') {
    clean = clean.substr(1);
  }
  if (clean.size() > 5 && clean.compare(0, 5, "root/") == 0) {
    clean = clean.substr(5);
  }
  if (clean.empty() || clean == util::to_std(root->get_name())) {
    return root;
  }

  auto *node =
      root->get_node_or_null(godot::NodePath(godot::String(clean.c_str())));
  if (!node) {
    std::string root_name = util::to_std(root->get_name());
    if (clean.size() > root_name.size() + 1 &&
        clean.compare(0, root_name.size(), root_name) == 0 &&
        clean[root_name.size()] == '/') {
      std::string sub = clean.substr(root_name.size() + 1);
      if (!sub.empty()) {
        node =
            root->get_node_or_null(godot::NodePath(godot::String(sub.c_str())));
      }
    }
  }
  if (!node)
    return nullptr;

  auto *p = node->get_parent();
  while (p) {
    if (p == root)
      return node;
    p = p->get_parent();
  }
  return nullptr;
}

} // namespace

mcp::JsonValue handle_add_node_to_group(const mcp::JsonValue &args) {
  auto *np = args.Find("node_path");
  auto *gp = args.Find("group_name");

  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!gp || !gp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: group_name");
    return e;
  }

  std::string node_path = np->GetString();
  std::string group_name = gp->GetString();

  auto *node = find_node(node_path);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path);
    return e;
  }

  godot::StringName group(group_name.c_str());
  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action("Add Node to Group");
    undo_redo->add_do_method(node, godot::StringName("remove_from_group"),
                             group);
    undo_redo->add_do_method(node, godot::StringName("add_to_group"), group,
                             true);
    undo_redo->add_undo_method(node, godot::StringName("remove_from_group"),
                               group);
    undo_redo->commit_action();
  } else {
    if (node->is_in_group(group)) {
      node->remove_from_group(group);
    }
    node->add_to_group(group, true);
  }

  if (!node->is_in_group(group)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "add_to_group failed: node is not in group after operation: " +
        node_path + " group=" + group_name);
    return e;
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("added");
  r["node_path"] = mcp::JsonValue(node_path);
  r["group"] = mcp::JsonValue(group_name);
  r["persistent"] = mcp::JsonValue(true);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "group_add_node_to_group: " + node_path + " -> " +
                                group_name);
  return r;
}

mcp::JsonValue handle_remove_node_from_group(const mcp::JsonValue &args) {
  auto *np = args.Find("node_path");
  auto *gp = args.Find("group_name");

  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!gp || !gp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: group_name");
    return e;
  }

  std::string node_path = np->GetString();
  std::string group_name = gp->GetString();

  auto *node = find_node(node_path);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path);
    return e;
  }

  godot::StringName group(group_name.c_str());
  auto *editor = godot::EditorInterface::get_singleton();
  auto *undo_redo = editor ? editor->get_editor_undo_redo() : nullptr;
  if (undo_redo) {
    undo_redo->create_action("Remove Node from Group");
    undo_redo->add_do_method(node, godot::StringName("remove_from_group"),
                             group);
    undo_redo->add_undo_method(node, godot::StringName("add_to_group"), group,
                               true);
    undo_redo->commit_action();
  } else {
    node->remove_from_group(group);
  }

  if (node->is_in_group(group)) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue(
        "remove_from_group failed: node is still in group after operation: " +
        node_path + " group=" + group_name);
    return e;
  }

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue("removed");
  r["node_path"] = mcp::JsonValue(node_path);
  r["group"] = mcp::JsonValue(group_name);
  scene_dirty_tracker::mark_scene_modified();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "group_remove_node_from_group: " + node_path +
                                " -> " + group_name);
  return r;
}

mcp::JsonValue handle_has_node_in_group(const mcp::JsonValue &args) {
  auto *np = args.Find("node_path");
  auto *gp = args.Find("group_name");

  if (!np || !np->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: node_path");
    return e;
  }
  if (!gp || !gp->IsString()) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("missing required parameter: group_name");
    return e;
  }

  std::string node_path = np->GetString();
  std::string group_name = gp->GetString();

  auto *node = find_node(node_path);
  if (!node) {
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("node not found: " + node_path);
    return e;
  }

  bool in_group = node->is_in_group(godot::StringName(group_name.c_str()));

  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(in_group);
  return r;
}

} // namespace group_ops
} // namespace godot_autopilot
