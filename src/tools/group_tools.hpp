#ifndef GODOT_AUTOPILOT_GROUP_TOOLS_HPP
#define GODOT_AUTOPILOT_GROUP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/group_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace group_tools {

namespace {

const std::vector<ParamSpec> kAddGroupNodeParams = {
    {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
    {"group_name", "string", "Group name to add the node to (string, e.g. 'enemies')", true},
};

const std::vector<ParamSpec> kRemoveGroupNodeParams = {
    {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
    {"group_name", "string", "Group name to remove the node from (string, e.g. 'enemies')", true},
};

const std::vector<ParamSpec> kHasGroupNodeParams = {
    {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
    {"group_name", "string", "Group name to check (string, e.g. 'enemies')", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(make_spec_tool(ToolSpec{
      "add_group_node",
      "Add a scene node to a group persistently: the membership is saved with the scene and registered with the editor undo/redo. Requires 'node_path' and 'group_name'. Idempotent — if the node is already in the group it is removed first, then re-added. Returns 'added' with node_path, group and persistent.",
      "Group", {"group", "add"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kAddGroupNodeParams, group_ops::handle_add_node_to_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_group_node",
      "Remove a scene node from a group; the change is registered with the editor undo/redo. Requires 'node_path' and 'group_name'. Returns 'removed' — or an error if the node is still in the group after the operation. Calling it for a node not in the group is a safe no-op.",
      "Group", {"group", "remove"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kRemoveGroupNodeParams, group_ops::handle_remove_node_from_group}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_group_node",
      "Check whether a scene node belongs to a group. Requires 'node_path' and 'group_name'. Groups are shared across the whole scene tree and membership added by add_group_node is saved with the scene. Returns true or false — errors if the node is not found.",
      "Group", {"group", "check"}, SideEffect::None, tool_flags::kNone,
      kHasGroupNodeParams, group_ops::handle_has_node_in_group}));
  return v;
}

} // namespace group_tools
} // namespace godot_autopilot

#endif