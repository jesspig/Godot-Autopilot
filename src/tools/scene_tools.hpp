#ifndef GODOT_AUTOPILOT_SCENE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/scene_ops.hpp"
#include "tools/scene_spec_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace scene_tools {

namespace {

const std::vector<ParamSpec> kCreateSceneNodeParams = {
    {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Player' or 'Player/Weapon'); omit only while the scene has no root — required once the scene already has a root. The response includes scene_path and scene_unsaved so the caller can confirm which scene received the node", false},
    {"name", "string", "Node name (string, default: NewNode)", true},
    {"type", "string", "Node class type (string, default: Node). Must be a Node subclass, e.g. Node2D, Sprite2D — other classes error", true},
    {"properties", "object", "Optional map of property names to values applied right after creation via the same conversion chain as property_set (object, e.g. {\"visible\": false, \"position\": {\"x\": 10, \"y\": 20}}); any failing property frees the new node and reports the failing name", false},
};

const std::vector<ParamSpec> kDeleteSceneNodeParams = {
    {"path", "string", "Node path in the edited scene (e.g. 'Enemies/Enemy1'); the scene root cannot be deleted. The response includes scene_path and scene_unsaved to confirm which scene was written", true},
};

const std::vector<ParamSpec> kRenameSceneNodeParams = {
    {"path", "string", "Node path in the edited scene (e.g. 'Player/Sprite2D'). The response includes scene_path and scene_unsaved to confirm which scene was written", true},
    {"new_name", "string", "New node name; must be unique among siblings and cannot contain '/' or ':' (string, e.g. 'Hero')", true},
};

const std::vector<ParamSpec> kReparentNodeParams = {
    {"path", "string", "Node path in the edited scene to reparent (e.g. 'Player/Weapon')", true},
    {"new_parent_path", "string", "Target parent node path in the edited scene; must not be the node itself or one of its descendants (string, e.g. 'Inventory')", true},
    {"keep_world_position", "boolean", "Preserve the world transform when the node and the new parent are both Node2D or both Node3D; other type combinations ignore it (boolean, default: false)", false},
};

const std::vector<ParamSpec> kGetSceneTreeParams = {
    {"max_depth", "integer", "Maximum tree depth to include (integer, default: 8; -1 means unlimited)", false},
    {"include_properties", "boolean", "Add up to 20 properties per node, skipping metadata/* keys, names starting with '_' and Object-typed values (boolean, default: false)", false},
};

const std::vector<ParamSpec> kInstantiateSceneParams = {
    {"path", "string", "Path to the .tscn scene file to instantiate (string, e.g. 'res://scenes/Enemy.tscn')", true},
    {"parent_path", "string", "Parent node path (string, default: edited scene root)", false},
    {"name", "string", "Node name to override the instance root (string, default: scene root name)", false},
    {"owner", "boolean", "Set scene ownership so nodes are saved with the scene (boolean, default: true)", false},
};

const std::vector<ParamSpec> kGetSceneNodeScreenRectParams = {
    {"paths", "array", "Non-empty array (max 50) of node paths in the edited scene, e.g. ['Player','UI/HealthBar']", true},
    {"viewport", "string", "Coordinate space: 'auto' (default, picks 2D or 3D by node type), '2d' or '3d'", false},
};

const std::vector<ParamSpec> kBuildNodesFromSpecParams = {
    {"spec", "object", "Declarative subtree spec {type, name, props, children}: type is a Node subclass (default: Node), name must not contain '/' or ':' (default: Node), props maps property names to values applied through the property_set conversion chain with OBJECT-typed values applied first, children is an array of nested specs; recursion is capped at 32 levels and 500 nodes per call and any failure rolls the whole subtree back", true},
    {"parent_path", "string", "Parent node path for the top spec node (default: scene root); omit only while the scene has no root, in which case the top spec node becomes the root", false},
    {"dry_run", "boolean", "Validate the spec and return the node list without writing to the scene (default: false); either dry_run or preview enables preview-only mode", false},
    {"preview", "boolean", "Alias of dry_run: validate and preview without writing (default: false)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(8);
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_node",
      "Create a new node in the currently edited scene. Provide 'type' (any Node subclass, default Node) and 'name' (default NewNode); omit 'parent_path' only while the scene has no root, otherwise pass the parent node path. Optional 'properties' object applies values right after creation through the same conversion chain as property_set (NodePath strings become node references, resource references are resolved, and an inline resource description such as {\"type\": \"RectangleShape2D\", \"properties\": {...}} creates a pathless sub-resource saved as a sub_resource) — any failing property frees the new node and reports the property name. Returns the new node 'path', the 'applied_properties' array (property names applied without error; a property may still have been adjusted by the engine — it then also appears in 'property_warnings'), the 'inline_resources' array ({property, type} entries present only when inline descriptions were applied) and an 'undo' hint describing how to remove the node via delete_scene_node. When the engine adjusts or flags a value after a successful set, the response additionally includes 'property_warnings', an array of {path, property, warning} objects; the field is absent when there are no warnings.",
      "Scene", {"node", "create"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneNodeParams, scene_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "delete_scene_node",
      "Delete a node from the edited scene by 'path'. Managed by the editor undo/redo history (action 'Delete Node <name>'): do removes the child and queue_frees it, undo re-adds it under the old parent with owner and sibling index restored — effective while the deferred free has not destroyed the node yet. Refuses to delete the scene root (close the scene first). Returns 'deleted' with 'undoable' true and an 'undo' info object (name, type, parent_path).",
      "Scene", {"node", "delete"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kDeleteSceneNodeParams, scene_ops::handle_delete}));
  v.push_back(make_spec_tool(ToolSpec{
      "rename_scene_node",
      "Rename a node in the edited scene: pass 'path' and 'new_name' ('/' and ':' are rejected). Refuses names already used by a sibling, mirroring Godot's uniqueness rule. Wrapped in one editor undo/redo step ('Rename Node <old>') restoring the previous name on undo; returns result 'renamed' with 'undoable' true and the new relative path.",
      "Scene", {"node", "rename"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kRenameSceneNodeParams, scene_ops::handle_rename}));
  v.push_back(make_spec_tool(ToolSpec{
      "reparent_node",
      "Reparent a node in the edited scene to a new parent: pass 'path' and 'new_parent_path'; refuses the scene root, itself, its own descendants and its current parent. One editor undo/redo step ('Reparent Node <name>'): do removes from the old parent then adds under the new one, undo restores the old parent with original sibling index and owner. Optional 'keep_world_position' (default false) preserves the world transform when the node and new parent are both Node2D or both Node3D; other type combinations ignore it. If the previous owner was the scene root it stays owned by the current scene root so the node is saved.",
      "Scene", {"node", "reparent"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kReparentNodeParams, scene_ops::handle_reparent}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_tree",
      "Walk the node tree of the currently edited scene, returning name, type, path and children for each node. Optional 'max_depth' (default 8; -1 for unlimited) and 'include_properties' (true adds up to 20 properties per node, skipping metadata/ keys, names starting with underscore and Object-typed properties). Errors if no scene is open — create or open one first.",
      "Scene", {"tree", "structure"}, SideEffect::None, tool_flags::kNone,
      kGetSceneTreeParams, scene_ops::handle_get_tree}));
  v.push_back(make_spec_tool(ToolSpec{
      "instantiate_scene",
      "Instantiate a PackedScene from 'path' (.tscn file) into the edited scene. Optional 'parent_path' (default: scene root), 'name' and 'owner' (default true — sets scene ownership so children are saved). Returns the instance 'path', 'name' and 'type'. Instances inherit CONNECT_PERSIST connections saved in the source scene; do not reconnect the same signal pairs.",
      "Scene", {"scene", "instance"}, SideEffect::None, tool_flags::kNone | tool_flags::kSceneTarget,
      kInstantiateSceneParams, scene_ops::handle_instance}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_node_screen_rect",
      "Map nodes of the edited scene to editor window client-area coordinates, e.g. to aim click_input_mouse at a canvas node or to verify a node is on screen. Pass 'paths' (1-50 node paths in the edited scene). Optional 'viewport' picks the coordinate space: 'auto' (default) chooses the 2D canvas transform or the 3D viewport camera per node type, while '2d'/'3d' force one space and report a per-item error on a type mismatch. Node3D is projected through the 3D editor viewport camera (item 'behind' flags points behind it); Node2D and Control map through the 2D editor viewport, and Control items also carry the transformed 'rect'. Each item reports {path, ok, type, screen_position, rect?, behind?} and per-item errors never fail the whole call. The top-level 'mapping' (scale_x, scale_y, offset_x, offset_y) converts capture_editor_viewport image pixels to window client coordinates (client = offset + pixel * scale). Read-only.",
      "Scene", {"scene", "node", "screen", "rect"}, SideEffect::None, tool_flags::kNone,
      kGetSceneNodeScreenRectParams, scene_ops::handle_get_node_screen_rect}));
  v.push_back(make_spec_tool(ToolSpec{
      "build_nodes_from_spec",
      "用声明式 spec 一次建好整棵子树：传 'spec' 对象 {type（Node 子类，默认 Node）、name（默认 Node，'/' 与 ':' 非法）、props（经 property_set 同转换链逐节点应用，OBJECT 型值先行）、children（同形 spec 数组递归）}；可选 'parent_path'（默认场景根，仅在无场景时省略并让顶层 spec 节点成根）、'dry_run'/'preview'（任一 true 则只校验并返回节点清单，不写场景）。自顶向下挂接并设 owner 为场景根以保证落盘；任一步失败整体回滚（摘除顶层已建节点并释放子树，无残留）。递归上限 32 层、单次上限 500 节点。与 create_scene_node、code_execute 并存：定型结构用本工具，单次零散节点用 create_scene_node，异形逻辑（循环、条件、计算值）用 code_execute。返回 mode（built/dry_run）、node_count、created [{path, type}] 与 delete_scene_node 撤销提示。",
      "Scene", {"node", "build", "spec"}, SideEffect::None, tool_flags::kMutating | tool_flags::kSceneTarget,
      kBuildNodesFromSpecParams, scene_spec_ops::handle_build_from_spec}));
  return v;
}

} // namespace scene_tools
} // namespace godot_autopilot

#endif
