#ifndef GODOT_AUTOPILOT_SCENE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/scene_ops.hpp"
#include "tools/scene_spec_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace scene_tools {

GDA_TOOL_CLASS(CreateSceneNodeTool, "create_scene_node",
               "Create a new node in the currently edited scene. Provide 'type' (any Node subclass, default Node) and 'name' (default NewNode); omit 'parent_path' only while the scene has no root, otherwise pass the parent node path. Optional 'properties' object applies values right after creation through the same conversion chain as property_set (NodePath strings become node references, resource references are resolved, and an inline resource description such as {\"type\": \"RectangleShape2D\", \"properties\": {...}} creates a pathless sub-resource saved as a sub_resource) — any failing property frees the new node and reports the property name. Returns the new node 'path', the 'applied_properties' array (property names applied without error; a property may still have been adjusted by the engine — it then also appears in 'property_warnings'), the 'inline_resources' array ({property, type} entries present only when inline descriptions were applied) and an 'undo' hint describing how to remove the node via delete_scene_node. When the engine adjusts or flags a value after a successful set, the response additionally includes 'property_warnings', an array of {path, property, warning} objects; the field is absent when there are no warnings.",
               "Scene", std::vector<std::string>({"node", "create"}), scene_ops::handle_create, true)

GDA_TOOL_CLASS(DeleteSceneNodeTool, "delete_scene_node",
               "Delete a node from the edited scene by 'path'. Managed by the editor undo/redo history (action 'Delete Node <name>'): do removes the child and queue_frees it, undo re-adds it under the old parent with owner and sibling index restored — effective while the deferred free has not destroyed the node yet. Refuses to delete the scene root (close the scene first). Returns 'deleted' with 'undoable' true and an 'undo' info object (name, type, parent_path).",
               "Scene", std::vector<std::string>({"node", "delete"}), scene_ops::handle_delete, true)

GDA_TOOL_CLASS(RenameSceneNodeTool, "rename_scene_node",
               "Rename a node in the edited scene: pass 'path' and 'new_name' ('/' and ':' are rejected). Refuses names already used by a sibling, mirroring Godot's uniqueness rule. Wrapped in one editor undo/redo step ('Rename Node <old>') restoring the previous name on undo; returns result 'renamed' with 'undoable' true and the new relative path.",
               "Scene", std::vector<std::string>({"node", "rename"}), scene_ops::handle_rename, true)

GDA_TOOL_CLASS(ReparentNodeTool, "reparent_node",
               "Reparent a node in the edited scene to a new parent: pass 'path' and 'new_parent_path'; refuses the scene root, itself, its own descendants and its current parent. One editor undo/redo step ('Reparent Node <name>'): do removes from the old parent then adds under the new one, undo restores the old parent with original sibling index and owner. Optional 'keep_world_position' (default false) preserves the world transform when the node and new parent are both Node2D or both Node3D; other type combinations ignore it. If the previous owner was the scene root it stays owned by the current scene root so the node is saved.",
               "Scene", std::vector<std::string>({"node", "reparent"}), scene_ops::handle_reparent, true)

GDA_TOOL_CLASS(GetSceneTreeTool, "get_scene_tree",
               "Walk the node tree of the currently edited scene, returning name, type, path and children for each node. Optional 'max_depth' (default 8; -1 for unlimited) and 'include_properties' (true adds up to 20 properties per node, skipping metadata/ keys, names starting with underscore and Object-typed properties). Errors if no scene is open — create or open one first.",
               "Scene", std::vector<std::string>({"tree", "structure"}), scene_ops::handle_get_tree, true)

GDA_TOOL_CLASS(InstantiateSceneTool, "instantiate_scene",
               "Instantiate a PackedScene from 'path' (.tscn file) into the edited scene. Optional 'parent_path' (default: scene root), 'name' and 'owner' (default true — sets scene ownership so children are saved). Returns the instance 'path', 'name' and 'type'. Instances inherit CONNECT_PERSIST connections saved in the source scene; do not reconnect the same signal pairs.",
               "Scene", std::vector<std::string>({"scene", "instance"}), scene_ops::handle_instance, true)

GDA_TOOL_CLASS(GetSceneNodeScreenRectTool, "get_scene_node_screen_rect",
               "Map nodes of the edited scene to editor window client-area coordinates, e.g. to aim click_input_mouse at a canvas node or to verify a node is on screen. Pass 'paths' (1-50 node paths in the edited scene). Optional 'viewport' picks the coordinate space: 'auto' (default) chooses the 2D canvas transform or the 3D viewport camera per node type, while '2d'/'3d' force one space and report a per-item error on a type mismatch. Node3D is projected through the 3D editor viewport camera (item 'behind' flags points behind it); Node2D and Control map through the 2D editor viewport, and Control items also carry the transformed 'rect'. Each item reports {path, ok, type, screen_position, rect?, behind?} and per-item errors never fail the whole call. The top-level 'mapping' (scale_x, scale_y, offset_x, offset_y) converts capture_editor_viewport image pixels to window client coordinates (client = offset + pixel * scale). Read-only.",
               "Scene", std::vector<std::string>({"scene", "node", "screen", "rect"}), scene_ops::handle_get_node_screen_rect, true)

GDA_TOOL_CLASS_SIDE(BuildNodesFromSpecTool, "build_nodes_from_spec",
                "用声明式 spec 一次建好整棵子树：传 'spec' 对象 {type（Node 子类，默认 Node）、name（默认 Node，'/' 与 ':' 非法）、props（经 property_set 同转换链逐节点应用，OBJECT 型值先行）、children（同形 spec 数组递归）}；可选 'parent_path'（默认场景根，仅在无场景时省略并让顶层 spec 节点成根）、'dry_run'/'preview'（任一 true 则只校验并返回节点清单，不写场景）。自顶向下挂接并设 owner 为场景根以保证落盘；任一步失败整体回滚（摘除顶层已建节点并释放子树，无残留）。递归上限 32 层、单次上限 500 节点。与 create_scene_node、code_execute 并存：定型结构用本工具，单次零散节点用 create_scene_node，异形逻辑（循环、条件、计算值）用 code_execute。返回 mode（built/dry_run）、node_count、created [{path, type}] 与 delete_scene_node 撤销提示。",
                "Scene", std::vector<std::string>({"node", "build", "spec"}), scene_spec_ops::handle_build_from_spec, true, ::godot_autopilot::SideEffect::None)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(std::make_unique<CreateSceneNodeTool>());
  v.push_back(std::make_unique<DeleteSceneNodeTool>());
  v.push_back(std::make_unique<RenameSceneNodeTool>());
  v.push_back(std::make_unique<ReparentNodeTool>());
  v.push_back(std::make_unique<GetSceneTreeTool>());
  v.push_back(std::make_unique<InstantiateSceneTool>());
  v.push_back(std::make_unique<GetSceneNodeScreenRectTool>());
  v.push_back(std::make_unique<BuildNodesFromSpecTool>());
  return v;
}

} // namespace scene_tools
} // namespace godot_autopilot

#endif