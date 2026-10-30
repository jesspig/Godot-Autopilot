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
    {"name", "string", "Node name (string, default: NewNode); optional — the node is named NewNode when the parameter is omitted", false},
    {"type", "string", "Node class type (string, default: Node); optional — a Node is created when the parameter is omitted. Must be a Node subclass, e.g. Node2D, Sprite2D — other classes error", false},
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

const std::vector<ParamSpec> kGetSceneSnapshotParams = {
    {"max_depth", "integer", "Maximum tree depth to include (integer, default: 8; -1 means unlimited)", false},
    {"include_properties", "boolean", "Add up to 20 properties per node, skipping metadata/* keys, names starting with '_' and Object-typed values (boolean, default: false)", false},
    {"verify", "boolean", "Compare the in-memory tree against the on-disk scene file and attach a verify summary with match, node and connection counts plus missing_paths (boolean, default: true)", false},
    {"observe", "boolean", "Observe/capture toggle: append a fresh editor viewport capture to the result through the observe pipeline (boolean, default: false); a capture failure is reported as observe_error without failing the call", false},
    {"paths", "array", "Optional non-empty array (max 50) of node paths in the edited scene; when present a nodes section with per-node screen positions is attached and per-item errors never fail the call", false},
};

const std::vector<ParamSpec> kSpawnNodeBundleParams = {
    {"type", "string", "Node class type (string, default: Node). Must be a Node subclass, e.g. Node2D, Sprite2D — other classes error", false},
    {"name", "string", "Node name (string, default: Node); '/' and ':' are rejected", false},
    {"parent_path", "string", "Parent node path in the edited scene (string, default: edited scene root); omit only while the scene has no root, in which case the new node becomes the root", false},
    {"properties", "object", "Optional map of property names to values applied via set_properties_batch with atomic:true (object, e.g. {\"visible\": false}); any failing property rolls the whole bundle back", false},
    {"script_path", "string", "Optional Script resource path to attach via attach_script_to_node (string, must start with 'res://'); the script is re-read from disk before attaching", false},
    {"groups", "array", "Optional array of 1..16 group names added persistently via add_group_node with editor undo registration", false},
    {"connections", "array", "Optional array of {signal, target_path, method, persist} objects connected from the new node via signal_connect; persist defaults to true (CONNECT_PERSIST, saved with the scene)", false},
    {"owner", "boolean", "Set scene ownership so the node is saved with the scene (boolean, default: true)", false},
    {"dry_run", "boolean", "Validate parameters and return the preview without writing to the scene (default: false); either dry_run or preview enables preview-only mode", false},
    {"preview", "boolean", "Alias of dry_run: validate and preview without writing (default: false)", false},
};

const std::vector<ParamSpec> kCreatePathNodeParams = {
    {"name", "string", "Path node name (e.g. 'EnemyPath')", true},
    {"parent_path", "string", "Parent node path in the edited scene; omit only while the scene has no root, in which case the new path node becomes the root", false},
    {"curve", "string", "Registration name of a curve from create_curve_resource (memory://name); a Curve2D creates a Path2D, a Curve3D creates a Path3D", true},
};

const std::vector<ParamSpec> kAdvancePathFollowParams = {
    {"path", "string", "Node path of a PathFollow2D/PathFollow3D in the edited scene (e.g. 'EnemyPath/Follower')", true},
    {"offset", "number", "Distance in pixels to advance along the curve; negative values move backward", true},
};

const std::vector<ParamSpec> kFocusCameraOnNodeParams = {
    {"camera_path", "string", "Camera node path in the edited scene (a Camera2D for 2D targets, a Camera3D for 3D targets)", true},
    {"target_path", "string", "Target node path in the edited scene to focus on (Node2D or Control for 2D cameras, Node3D for 3D cameras)", true},
    {"margin", "number", "Extra padding: grows the 2D focus rect around the target anchor, or pulls a 3D camera back along its view direction by this many world units (number, default: 0, must not be negative)", false},
};

const std::vector<ParamSpec> kSetCameraParamsParams = {
    {"camera_path", "string", "Camera node path in the edited scene (Camera2D and Camera3D accept different params keys)", true},
    {"params", "object", "Camera settings to apply with at least one entry: 'zoom' (Camera2D only, number above 0 or {x, y} object with components above 0), 'fov' (Camera3D only, 1 to 179), 'near' (Camera3D only, above 0 and below far), 'far' (Camera3D only, above 0 and above near), 'limits' (Camera2D only, object with any of limit_left, limit_top, limit_right, limit_bottom integers and limit_smoothed boolean); unknown keys error", true},
};

const std::vector<ParamSpec> kGetCameraViewRectParams = {
    {"camera_path", "string", "Camera node path in the edited scene (Camera2D or Camera3D)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_node",
      "Create a new node in the currently edited scene. Provide 'type' (any Node subclass, default Node) and 'name' (default NewNode); omit 'parent_path' only while the scene has no root, otherwise pass the parent node path. Optional 'properties' object applies values right after creation through the same conversion chain as property_set (NodePath strings become node references, resource references are resolved, and an inline resource description such as {\"type\": \"RectangleShape2D\", \"properties\": {...}} creates a pathless sub-resource saved as a sub_resource) — any failing property frees the new node and reports the property name. Returns the new node 'path', the 'applied_properties' array (property names applied without error; a property may still have been adjusted by the engine — it then also appears in 'property_warnings'), the 'inline_resources' array ({property, type} entries present only when inline descriptions were applied) and an 'undo' hint describing how to remove the node via delete_scene_node. When the engine adjusts or flags a value after a successful set, the response additionally includes 'property_warnings', an array of {path, property, warning} objects; the field is absent when there are no warnings.",
      "Scene", {"node", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneNodeParams, scene_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "delete_scene_node",
      "Delete a node from the edited scene by 'path'. Managed by the editor undo/redo history (action 'Delete Node <name>'): do removes the child and queue_frees it, undo re-adds it under the old parent with owner and sibling index restored — effective while the deferred free has not destroyed the node yet. Refuses to delete the scene root (close the scene first). Returns 'deleted' with 'undoable' true and an 'undo' info object (name, type, parent_path). This tool is classified as a destructive side effect because it destroys a scene entity that already exists in the host, and the editor undo stack is a session-only recovery path that is lost when the editor exits.",
      "Scene", {"node", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kDeleteSceneNodeParams, scene_ops::handle_delete}));
  v.push_back(make_spec_tool(ToolSpec{
      "rename_scene_node",
      "Rename a node in the edited scene: pass 'path' and 'new_name' ('/' and ':' are rejected). Refuses names already used by a sibling, mirroring Godot's uniqueness rule. Wrapped in one editor undo/redo step ('Rename Node <old>') restoring the previous name on undo; returns result 'renamed' with 'undoable' true and the new relative path.",
      "Scene", {"node", "rename"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kRenameSceneNodeParams, scene_ops::handle_rename}));
  v.push_back(make_spec_tool(ToolSpec{
      "reparent_node",
      "Reparent a node in the edited scene to a new parent: pass 'path' and 'new_parent_path'; refuses the scene root, itself, its own descendants and its current parent. One editor undo/redo step ('Reparent Node <name>'): do removes from the old parent then adds under the new one, undo restores the old parent with original sibling index and owner. Optional 'keep_world_position' (default false) preserves the world transform when the node and new parent are both Node2D or both Node3D; other type combinations ignore it. If the previous owner was the scene root it stays owned by the current scene root so the node is saved.",
      "Scene", {"node", "reparent"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kReparentNodeParams, scene_ops::handle_reparent}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_tree",
      "Walk the node tree of the currently edited scene, returning name, type, path and children for each node. Optional 'max_depth' (default 8; -1 for unlimited) and 'include_properties' (true adds up to 20 properties per node, skipping metadata/ keys, names starting with underscore and Object-typed properties). Errors if no scene is open — create or open one first.",
      "Scene", {"tree", "structure"}, SideEffect::None, tool_flags::kNone,
      kGetSceneTreeParams, scene_ops::handle_get_tree}));
  v.push_back(make_spec_tool(ToolSpec{
      "instantiate_scene",
      "Instantiate a PackedScene from 'path' (.tscn file) into the edited scene. Optional 'parent_path' (default: scene root), 'name' and 'owner' (default true — sets scene ownership so children are saved). Returns the instance 'path', 'name' and 'type'. Instances inherit CONNECT_PERSIST connections saved in the source scene; do not reconnect the same signal pairs.",
      "Scene", {"scene", "instance"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kInstantiateSceneParams, scene_ops::handle_instance}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_node_screen_rect",
      "Map nodes of the edited scene to editor window client-area coordinates, e.g. to aim click_input_mouse at a canvas node or to verify a node is on screen. Pass 'paths' (1-50 node paths in the edited scene). Optional 'viewport' picks the coordinate space: 'auto' (default) chooses the 2D canvas transform or the 3D viewport camera per node type, while '2d'/'3d' force one space and report a per-item error on a type mismatch. Node3D is projected through the 3D editor viewport camera (item 'behind' flags points behind it); Node2D and Control map through the 2D editor viewport, and Control items also carry the transformed 'rect'. Each item reports {path, ok, type, screen_position, rect?, behind?} and per-item errors never fail the whole call. The top-level 'mapping' (scale_x, scale_y, offset_x, offset_y) converts capture_editor_viewport image pixels to window client coordinates (client = offset + pixel * scale). Read-only.",
      "Scene", {"scene", "node", "screen", "rect"}, SideEffect::None, tool_flags::kNone,
      kGetSceneNodeScreenRectParams, scene_ops::handle_get_node_screen_rect}));
  v.push_back(make_spec_tool(ToolSpec{
      "build_nodes_from_spec",
      "用声明式 spec 一次建好整棵子树：传 'spec' 对象 {type（Node 子类，默认 Node）、name（默认 Node，'/' 与 ':' 非法）、props（经 property_set 同转换链逐节点应用，OBJECT 型值先行）、children（同形 spec 数组递归）}；可选 'parent_path'（默认场景根，仅在无场景时省略并让顶层 spec 节点成根）、'dry_run'/'preview'（任一 true 则只校验并返回节点清单，不写场景）。自顶向下挂接并设 owner 为场景根以保证落盘；任一步失败整体回滚（摘除顶层已建节点并释放子树，无残留）。递归上限 32 层、单次上限 500 节点。与 create_scene_node、code_execute 并存：定型结构用本工具，单次零散节点用 create_scene_node，异形逻辑（循环、条件、计算值）用 code_execute。返回 mode（built/dry_run）、node_count、created [{path, type}] 与 delete_scene_node 撤销提示。",
      "Scene", {"node", "build", "spec"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget,
      kBuildNodesFromSpecParams, scene_spec_ops::handle_build_from_spec}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_scene_snapshot",
      "Read-only snapshot of the currently edited scene in one call: tree (same name/type/path/children walk as get_scene_tree with max_depth and include_properties), verify summary (same memory-versus-disk comparison as verify_scene_saved with match, node and connection counts plus missing_paths; unavailable when the scene has no file path) and, when paths is given, a nodes section with per-node screen positions (same items as get_scene_node_screen_rect; per-item errors never fail the call). Optional verify (default true) toggles the verify summary; optional observe (default false) appends a fresh editor viewport capture through the observe pipeline and a capture failure is reported as observe_error without failing the call. Never marks the scene dirty and never touches undo history. Errors only when no scene is open or paths is malformed.",
      "Scene", {"snapshot", "tree", "verify"}, SideEffect::None, tool_flags::kObserve,
      kGetSceneSnapshotParams, scene_ops::handle_get_snapshot}));
  v.push_back(make_spec_tool(ToolSpec{
      "spawn_node_bundle",
      "Create one node and finish its common setup in a single atomic call: 'type' (any Node subclass, default Node) and 'name' (default Node, '/' and ':' rejected) under 'parent_path' (default: edited scene root; omit only while the scene has no root, in which case the new node becomes the root). Optional 'properties' object is applied via set_properties_batch with atomic:true, optional 'script_path' (must start with 'res://') is attached via attach_script_to_node with a fresh disk re-read, optional 'groups' (array of 1..16 names) is added persistently via add_group_node with editor undo registration, and optional 'connections' (array of {signal, target_path, method, persist} with persist defaulting to true for CONNECT_PERSIST scene-saved wiring) is connected from the new node via signal_connect. Optional 'owner' (default true) keeps scene ownership so the node is saved; 'dry_run'/'preview' (either true) only validates and previews without writing. The steps run in order properties, script, groups, connections; any failing step removes the new node and rolls the whole bundle back with no residue. Returns the new node 'path' and 'type', the 'applied' property name array, 'groups_connected' and 'signals_connected' counts, and an 'undo' hint describing how to remove the node via delete_scene_node.",
      "Scene", {"node", "bundle", "spawn"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSpawnNodeBundleParams, scene_spec_ops::handle_spawn_bundle}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_path_node",
      "Create a Path2D/Path3D node in the edited scene bound to a curve created by create_curve_resource. Requires 'name' and 'curve' (the memory:// reference); the curve type decides the node type (Curve2D creates Path2D, Curve3D creates Path3D). Optional 'parent_path' (default: scene root; omit only while the scene has no root, in which case the new node becomes the root). Sets scene ownership so the node is saved. Returns the new node 'path', 'type', 'curve' and an 'undo' hint describing how to remove the node via delete_scene_node.",
      "Scene", {"node", "path", "create"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreatePathNodeParams, scene_ops::handle_create_path_node}));
  v.push_back(make_spec_tool(ToolSpec{
      "advance_path_follow",
      "Advance a PathFollow2D/PathFollow3D node along its parent path curve by 'offset' pixels (negative moves backward). Requires 'path' (the follower node path in the edited scene) and 'offset' (number). Sets progress to current progress plus offset and reports the new progress, progress_ratio and global position. Marks the scene modified.",
      "Scene", {"node", "path", "follow", "advance"}, SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kAdvancePathFollowParams, scene_ops::handle_advance_path_follow}));
  v.push_back(make_spec_tool(ToolSpec{
      "focus_camera_on_node",
      "Point a scene camera at a node: pass 'camera_path' (a Camera2D for 2D targets, a Camera3D for 3D targets) and 'target_path' (Node2D or Control for 2D cameras, Node3D for 3D cameras; other types error naming the actual class). A 2D camera is centered on the target anchor (the Control global-rect center, otherwise the canvas origin) and reports the padded 'focus_rect'; a 3D camera keeps its position and turns toward the target via look_at, reporting whether the target is 'behind' it. Optional 'margin' (number, default 0, must not be negative) grows the 2D focus rect, or pulls a 3D camera back along its view direction before aiming. One editor undo step; marks the scene modified. Returns camera and target paths and types, space, position, zoom and focus_rect (2D) or target_position and behind (3D), plus margin.",
      "Scene", {"camera", "focus"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kFocusCameraOnNodeParams, scene_ops::handle_focus_camera_on_node}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_camera_params",
      "Update settings on a scene camera in one editor undo step: pass 'camera_path' and 'params' (object with at least one entry). 'zoom' (Camera2D only: number above 0 or {x, y} object with components above 0), 'fov' (Camera3D only: 1 to 179), 'near' and 'far' (Camera3D only: above 0 with near below far), 'limits' (Camera2D only: object with any of limit_left, limit_top, limit_right, limit_bottom integers and limit_smoothed boolean); keys for the wrong camera type or unknown keys error naming the actual class. Marks the scene modified. Returns the camera path, type, the applied key array, and the resulting position, zoom and limits (2D) or fov, near and far (3D).",
      "Scene", {"camera", "params"}, SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSetCameraParamsParams, scene_ops::handle_set_camera_params}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_camera_view_rect",
      "Describe what a scene camera sees in editor window client-area coordinates with the same mapping as get_scene_node_screen_rect (client = offset + pixel * scale). Pass 'camera_path' (a Camera2D or Camera3D; other types error naming the actual class). Returns camera_path, camera_type, viewport, window_id, space, world_position, window_center, 'view_rect' ({position, size} centered on the projected camera origin with the editor viewport size), 'behind' (always false for 2D; true when a 3D camera origin sits behind the 3D editor camera), zoom (2D) or forward, fov, near and far (3D), viewport_size and the pixel mapping. Read-only.",
      "Scene", {"camera", "view", "rect"}, SideEffect::None, tool_flags::kNone,
      kGetCameraViewRectParams, scene_ops::handle_get_camera_view_rect}));
  return v;
}

} // namespace scene_tools
} // namespace godot_autopilot

#endif
