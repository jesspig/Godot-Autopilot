#ifndef GODOT_AUTOPILOT_GROUP_TOOLS_HPP
#define GODOT_AUTOPILOT_GROUP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/group/group_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace group_tools {

namespace {

const std::vector<std::string> kNodeGroupActions = {"add", "remove"};

const std::vector<ParamSpec> kNodeGroupParams = {
    {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
    {"group_name", "string", "Group name to add or remove (string, e.g. 'enemies')", true},
};

inline mcp::JsonValue node_group_alias_args_with_action(
    const mcp::JsonValue &args, const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline ToolAlias node_group_action_alias(const char *name,
                                         const char *canonical,
                                         const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return node_group_alias_args_with_action(args, action);
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
      "node_group",
      "Change group membership of a scene node in the edited scene in one action tool; every action requires 'node_path' and 'group_name', is WritesScene and is registered with the editor undo/redo. action='add' adds the node to the group persistently (membership is saved with the scene) and is idempotent: an existing membership is removed first, then re-added. action='remove' removes the membership; calling it for a node not in the group is a safe no-op. The result reports 'added' or 'removed' with node_path, group and, for add, persistent=true. Use get_scene_tree(include_groups=true) to read current membership; has_group_node is a deprecated alias of that call. Deprecated aliases: add_group_node (action=add), remove_group_node (action=remove).",
      "Group", {"group", "add", "remove", "add_group_node", "remove_group_node",
                "has_group_node"},
      SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kNodeGroupParams, group_ops::handle_node_group,
      schema::build_action_schema(kNodeGroupActions, kNodeGroupParams,
                                  "Group membership action: add|remove"),
      {
          node_group_action_alias("add_group_node", "node_group", "add"),
          node_group_action_alias("remove_group_node", "node_group",
                                  "remove"),
      },
      {"{\"action\":\"add\",\"node_path\":\"Enemies/Enemy1\",\"group_name\":\"enemies\"}",
       "{\"action\":\"remove\",\"node_path\":\"Enemies/Enemy1\",\"group_name\":\"enemies\"}"}}));
  return v;
}

} // namespace group_tools
} // namespace godot_autopilot

#endif
