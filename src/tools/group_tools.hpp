#ifndef GODOT_AUTOPILOT_GROUP_TOOLS_HPP
#define GODOT_AUTOPILOT_GROUP_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/group_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace group_tools {

GDA_TOOL_CLASS(AddGroupNodeTool, "add_group_node",
               "Add a scene node to a group persistently: the membership is saved with the scene and registered with the editor undo/redo. Requires 'node_path' and 'group_name'. Idempotent — if the node is already in the group it is removed first, then re-added. Returns 'added' with node_path, group and persistent.",
               "Group", std::vector<std::string>({"group", "add"}), group_ops::handle_add_node_to_group, true)

GDA_TOOL_CLASS(RemoveGroupNodeTool, "remove_group_node",
               "Remove a scene node from a group; the change is registered with the editor undo/redo. Requires 'node_path' and 'group_name'. Returns 'removed' — or an error if the node is still in the group after the operation. Calling it for a node not in the group is a safe no-op.",
               "Group", std::vector<std::string>({"group", "remove"}), group_ops::handle_remove_node_from_group, true)

GDA_TOOL_CLASS(HasGroupNodeTool, "has_group_node",
               "Check whether a scene node belongs to a group. Requires 'node_path' and 'group_name'. Groups are shared across the whole scene tree and membership added by add_group_node is saved with the scene. Returns true or false — errors if the node is not found.",
               "Group", std::vector<std::string>({"group", "check"}), group_ops::handle_has_node_in_group, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(std::make_unique<AddGroupNodeTool>());
  v.push_back(std::make_unique<RemoveGroupNodeTool>());
  v.push_back(std::make_unique<HasGroupNodeTool>());
  return v;
}

} // namespace group_tools
} // namespace godot_autopilot

#endif