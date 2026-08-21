#ifndef GODOT_AUTOPILOT_SCENE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/scene_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace scene_tools {

GDA_TOOL_CLASS(CreateSceneNodeTool, "create_scene_node",
               "Create a new node in the currently edited scene. Provide 'type' (any Node subclass, default Node) and 'name' (default NewNode); omit 'parent_path' only while the scene has no root, otherwise pass the parent node path. Returns the new node 'path' plus an 'undo' hint describing how to remove it via delete_scene_node.",
               "Scene", std::vector<std::string>({"node", "create"}), scene_ops::handle_create, true)

GDA_TOOL_CLASS(DeleteSceneNodeTool, "delete_scene_node",
               "Delete a node from the edited scene by 'path'. Uses queue_free, so removal is deferred: the node may still be visible to immediate queries until the end of the frame. Refuses to delete the scene root (close the scene first). Returns 'deleted' with an 'undo' hint (name, type, parent_path).",
               "Scene", std::vector<std::string>({"node", "delete"}), scene_ops::handle_delete, true)

GDA_TOOL_CLASS(GetSceneTreeTool, "get_scene_tree",
               "Walk the node tree of the currently edited scene, returning name, type, path and children for each node. Optional 'max_depth' (default 8; -1 for unlimited) and 'include_properties' (true adds up to 20 properties per node, skipping metadata/ keys, names starting with underscore and Object-typed properties). Errors if no scene is open — create or open one first.",
               "Scene", std::vector<std::string>({"tree", "structure"}), scene_ops::handle_get_tree, true)

GDA_TOOL_CLASS(InstantiateSceneTool, "instantiate_scene",
               "Instantiate a PackedScene from 'path' (.tscn file) into the edited scene. Optional 'parent_path' (default: scene root), 'name' and 'owner' (default true — sets scene ownership so children are saved). Returns the instance 'path', 'name' and 'type'. Instances inherit CONNECT_PERSIST connections saved in the source scene; do not reconnect the same signal pairs.",
               "Scene", std::vector<std::string>({"scene", "instance"}), scene_ops::handle_instance, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(std::make_unique<CreateSceneNodeTool>());
  v.push_back(std::make_unique<DeleteSceneNodeTool>());
  v.push_back(std::make_unique<GetSceneTreeTool>());
  v.push_back(std::make_unique<InstantiateSceneTool>());
  return v;
}

} // namespace scene_tools
} // namespace godot_autopilot

#endif