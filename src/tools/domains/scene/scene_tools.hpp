#ifndef GODOT_AUTOPILOT_SCENE_TOOLS_HPP
#define GODOT_AUTOPILOT_SCENE_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/scene/scene_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace scene_tools {

namespace {

const std::vector<ParamSpec> kCreateSceneNodeParams = {
    {"mode", "string", "Operation to run: 'node' (default) creates a single node, 'scene' instantiates a .tscn file, 'spec' builds a declarative subtree, 'bundle' creates one node and finishes its setup atomically, 'path' creates a Path2D/Path3D bound to a curve resource", false},
    {"parent_path", "string", "node/scene/spec/bundle/path: parent node path in the edited scene (e.g. 'Player' or 'Player/Weapon'); omit only while the scene has no root — required once the scene already has a root. The response includes scene_path and scene_unsaved so the caller can confirm which scene received the node", false},
    {"name", "string", "node/bundle: node name (default: NewNode / Node); path: required path node name; scene: override the instance root name (default: scene root name). '/' and ':' are rejected where the mode validates names", false},
    {"type", "string", "node/bundle: node class type (default: Node). Must be a Node subclass, e.g. Node2D, Sprite2D — other classes error", false},
    {"properties", "object", "node/bundle: map of property names to values applied right after creation (object, e.g. {\"visible\": false, \"position\": {\"x\": 10, \"y\": 20}}); node mode applies them through the same conversion chain as property_set and frees the new node on failure, bundle mode applies them atomically via set_properties_batch and rolls the whole bundle back on failure", false},
    {"path", "string", "scene: path to the .tscn scene file to instantiate (string, e.g. 'res://scenes/Enemy.tscn')", false},
    {"owner", "boolean", "scene/bundle: set scene ownership so nodes are saved with the scene (default: true)", false},
    {"editable_instance", "boolean", "scene: mark the instantiated scene root as an editable instance so its internal nodes can be edited and saved with the current scene (default: false)", false},
    {"spec", "object", "spec: declarative subtree spec {type, name, props, children}: type is a Node subclass (default: Node), name must not contain '/' or ':' (default: Node), props maps property names to values applied through the property_set conversion chain with OBJECT-typed values applied first, children is an array of nested specs; recursion is capped at 32 levels and 500 nodes per call and any failure rolls the whole subtree back", false},
    {"dry_run", "boolean", "spec/bundle: validate the parameters and return the preview without writing to the scene (default: false); either dry_run or preview enables preview-only mode", false},
    {"preview", "boolean", "spec/bundle: alias of dry_run: validate and preview without writing (default: false)", false},
    {"script_path", "string", "bundle: optional Script resource path to attach via attach_script_to_node (string, must start with 'res://'); the script is re-read from disk before attaching", false},
    {"groups", "array", "bundle: optional array of 1..16 group names added persistently via node_group action=add with editor undo registration", false},
    {"connections", "array", "bundle: optional array of {signal, target_path, method, persist} objects connected from the new node via signal_connect; persist defaults to true (CONNECT_PERSIST, saved with the scene)", false},
    {"curve", "string", "path: registration name of a curve from create_curve_resource (memory://name); a Curve2D creates a Path2D, a Curve3D creates a Path3D", false},
};

const std::vector<ParamSpec> kEditSceneNodeParams = {
    {"path", "string", "Node path in the edited scene (e.g. 'Player/Sprite2D'). The response includes scene_path and scene_unsaved to confirm which scene was written", true},
    {"new_name", "string", "rename: new node name; must be unique among siblings and cannot contain '/' or ':' (string, e.g. 'Hero')", false},
    {"new_parent_path", "string", "reparent: target parent node path in the edited scene; must not be the node itself or one of its descendants (string, e.g. 'Inventory')", false},
    {"keep_world_position", "boolean", "reparent: preserve the world transform when the node and the new parent are both Node2D or both Node3D; other type combinations ignore it (boolean, default: false)", false},
    {"offset", "number", "advance_path: distance in pixels to advance along the curve; negative values move backward", false},
    {"count", "integer", "duplicate: number of copies to create, an integer >= 1 (default: 1)", false},
    {"name_suffix", "string", "duplicate: suffix appended to each copy's name before the copy number (default: '_copy'); '/' and ':' are rejected", false},
    {"editable_instance", "boolean", "duplicate: mark copies that are scene instances as editable instances of the current scene (default: false)", false},
    {"replacement_path", "string", "replace: node path of the subtree that replaces the node at 'path'; the replacement is duplicated, the node at 'path' and its subtree are removed", false},
    {"index", "integer", "reorder: target sibling index inside the parent, 0-based and below the parent child count; mutually exclusive with before_path and after_path", false},
    {"before_path", "string", "reorder: sibling node path to move the node before; mutually exclusive with index and after_path", false},
    {"after_path", "string", "reorder: sibling node path to move the node after; mutually exclusive with index and before_path", false},
};

const std::vector<ParamSpec> kPackSceneParams = {
    {"node_path", "string", "Node path of the subtree root to pack into a scene file (e.g. 'Player/Weapon'); the edited scene root and childless subtrees are rejected", true},
    {"dest_path", "string", "Destination scene file inside res:// ending with .tscn or .scn (e.g. 'res://prefabs/weapon.tscn'); missing directories are created", true},
    {"overwrite", "boolean", "Overwrite dest_path when the file already exists (default: false)", false},
};

const std::vector<ParamSpec> kDeleteSceneNodeParams = {
    {"path", "string", "Node path in the edited scene (e.g. 'Enemies/Enemy1'); the scene root cannot be deleted. The response includes scene_path and scene_unsaved to confirm which scene was written", true},
};

const std::vector<ParamSpec> kGetSceneTreeParams = {
    {"max_depth", "integer", "Maximum tree depth to include in the unfiltered tree (integer, default: 8; -1 means unlimited); filter mode searches the whole subtree and ignores it", false},
    {"include_properties", "boolean", "Add up to 20 properties per node, skipping metadata/* keys, names starting with '_' and Object-typed values (boolean, default: false)", false},
    {"include_groups", "boolean", "Attach the group membership of every node as a 'groups' string array (boolean, default: false); use it as the membership check that replaces has_group_node", false},
    {"include_owner", "boolean", "Attach the scene-relative owner path of every node as 'owner' (boolean, default: false)", false},
    {"include_index", "boolean", "Attach the sibling index of every node as 'index' (boolean, default: false)", false},
    {"include_scene_file_path", "boolean", "Attach the source scene path of instanced nodes as 'scene_file_path' (empty for local nodes; boolean, default: false)", false},
    {"pattern", "string", "Filter mode: Godot match pattern applied through find_children on the edited scene root (e.g. 'Enemy*'; '*' matches every name). Supplying pattern, type or group returns {root, filters, count, nodes} with matching descendants instead of the nested tree; the root itself is never matched and max_depth does not apply", false},
    {"type", "string", "Filter mode: node class name passed to find_children (e.g. 'Node2D', non-empty)", false},
    {"group", "string", "Filter mode: keep only find_children matches that are in this group (is_in_group)", false},
    {"verify", "boolean", "Attach a verify summary comparing the in-memory tree against the on-disk scene file (default: false); when true or when paths is given the result switches to the snapshot envelope {tree, verify?, nodes?} plus scene_path/scene_unsaved; cannot be combined with pattern/type/group", false},
    {"observe", "boolean", "Append a fresh editor viewport capture through the observe pipeline (boolean, default: false); a capture failure is reported as observe_error without failing the call", false},
    {"paths", "array", "Optional non-empty array (max 50) of node paths; attaches a nodes section with per-node screen positions and per-item errors never fail the call", false},
    {"viewport", "string", "Coordinate space for paths: 'auto' (default, picks 2D or 3D by node type), '2d' or '3d'", false},
    {"scene_path", "string", "Scene file path compared by verify, e.g. 'res://game.tscn'; defaults to the edited scene file path", false},
    {"include_hash", "boolean", "With verify: attach FNV-1a structural hashes memory_hash/disk_hash computed over the sorted node-path sets (default: false); node paths only — properties, script references and file bytes do not participate, so this is a structure check, not a content proof", false},
};

const std::vector<ParamSpec> kSceneCameraParams = {
    {"camera_path", "string", "Camera node path in the edited scene (a Camera2D for 2D targets, a Camera3D for 3D targets; also the camera queried by project, unproject, project_ray and behind)", true},
    {"target_path", "string", "focus: target node path in the edited scene to focus on (Node2D or Control for 2D cameras, Node3D for 3D cameras)", false},
    {"margin", "number", "focus: extra padding: grows the 2D focus rect around the target anchor, or pulls a 3D camera back along its view direction by this many world units (number, default: 0, must not be negative)", false},
    {"params", "object", "set_params: camera settings to apply with at least one entry: 'zoom' (Camera2D only, number above 0 or {x, y} object with components above 0), 'fov' (Camera3D only, 1 to 179), 'near' (Camera3D only, above 0 and below far), 'far' (Camera3D only, above 0 and above near), 'limits' (Camera2D only, object with any of limit_left, limit_top, limit_right, limit_bottom integers and limit_smoothed boolean); unknown keys error", false},
    {"point", "object", "project/behind: world-space point to convert, {x, y} for a Camera2D or {x, y, z} for a Camera3D, e.g. {\"x\": 100, \"y\": 50}", false},
    {"screen_point", "object", "unproject/project_ray: point in the camera viewport client area in pixels as {x, y}, e.g. {\"x\": 640, \"y\": 360}", false},
    {"depth", "number", "unproject: distance from the Camera3D plane along the view direction used to place the world point (number greater than 0, default 1.0); ignored by a Camera2D", false},
};

const std::vector<ParamSpec> kGetCameraViewRectParams = {
    {"camera_path", "string", "Camera node path in the edited scene (Camera2D or Camera3D)", true},
};

const std::vector<std::string> kEditSceneNodeActions = {
    "rename", "reparent", "advance_path", "duplicate", "reorder", "replace"};

const std::vector<std::string> kSceneCameraActions = {
    "focus", "set_params", "project", "unproject", "project_ray", "behind"};

inline mcp::JsonValue scene_create_schema() {
  mcp::JsonValue schema_value = schema::build_schema(kCreateSceneNodeParams);
  mcp::JsonValue *properties = schema_value.Find("properties");
  if (properties != nullptr && properties->IsObject()) {
    mcp::JsonValue mode(mcp::JsonValue::object_tag);
    mode["type"] = mcp::JsonValue("string");
    mode["description"] = mcp::JsonValue(
        "Operation to run: node (default)|scene|spec|bundle|path");
    mcp::JsonValue values(mcp::JsonValue::array_tag);
    for (const char *value : {"node", "scene", "spec", "bundle", "path"}) {
      values.PushBack(mcp::JsonValue(value));
    }
    mode["enum"] = std::move(values);
    (*properties)["mode"] = std::move(mode);
  }
  return schema_value;
}

inline std::function<mcp::JsonValue(const mcp::JsonValue &)> scene_mode_args(
    const char *mode) {
  return [mode](const mcp::JsonValue &args) {
    mcp::JsonValue out(mcp::JsonValue::object_tag);
    if (args.IsObject()) {
      out = args;
    }
    out["mode"] = mcp::JsonValue(mode);
    return out;
  };
}

inline mcp::JsonValue scene_alias_args_with_action(const mcp::JsonValue &args,
                                                   const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline ToolAlias scene_action_alias(const char *name, const char *canonical,
                                    const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return scene_alias_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

mcp::JsonValue scene_alias_args_with_verify(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  if (out.Find("verify") == nullptr) {
    out["verify"] = mcp::JsonValue(true);
  }
  return out;
}

mcp::JsonValue screen_rect_alias_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    const mcp::JsonValue *paths = args.Find("paths");
    if (paths != nullptr) {
      out["paths"] = *paths;
    }
    const mcp::JsonValue *viewport = args.Find("viewport");
    if (viewport != nullptr) {
      out["viewport"] = *viewport;
    }
  }
  return out;
}

mcp::JsonValue screen_rect_alias_validate(const mcp::JsonValue &args) {
  const mcp::JsonValue *paths = args.Find("paths");
  if (paths == nullptr || !paths->IsArray()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] =
        mcp::JsonValue("missing required parameter: paths (array)");
    return error;
  }
  const auto &items = paths->GetArray();
  if (items.empty() || items.size() > 50) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue(
        "parameter 'paths' must contain between 1 and 50 entries");
    return error;
  }
  for (const auto &item : items) {
    if (!item.IsString()) {
      mcp::JsonValue error(mcp::JsonValue::object_tag);
      error["error"] =
          mcp::JsonValue("parameter 'paths' must be an array of strings");
      return error;
    }
  }
  if (const mcp::JsonValue *viewport = args.Find("viewport")) {
    if (!viewport->IsString()) {
      mcp::JsonValue error(mcp::JsonValue::object_tag);
      error["error"] =
          mcp::JsonValue("parameter 'viewport' must be a string");
      return error;
    }
    const std::string mode = viewport->GetString();
    if (mode != "auto" && mode != "2d" && mode != "3d") {
      mcp::JsonValue error(mcp::JsonValue::object_tag);
      error["error"] = mcp::JsonValue(
          "parameter 'viewport' must be one of: auto, 2d, 3d");
      return error;
    }
  }
  return mcp::JsonValue();
}

void copy_field(const mcp::JsonValue &source, mcp::JsonValue &target,
                const char *key) {
  if (const mcp::JsonValue *value = source.Find(key)) {
    target[key] = *value;
  }
}

mcp::JsonValue screen_rect_alias_project(const mcp::JsonValue &canonical) {
  if (canonical.Find("error") != nullptr) {
    return canonical;
  }
  const mcp::JsonValue *inner = canonical.Find("result");
  if (inner == nullptr || !inner->IsObject()) {
    return canonical;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (const mcp::JsonValue *nodes = inner->Find("nodes")) {
    if (nodes->IsObject()) {
      out["result"] = *nodes;
      return out;
    }
  }
  if (const mcp::JsonValue *nodes_error = inner->Find("nodes_error")) {
    if (nodes_error->IsString()) {
      out["error"] = *nodes_error;
      return out;
    }
  }
  return canonical;
}

mcp::JsonValue verify_alias_project(const mcp::JsonValue &canonical) {
  if (canonical.Find("error") != nullptr) {
    return canonical;
  }
  const mcp::JsonValue *inner = canonical.Find("result");
  if (inner == nullptr || !inner->IsObject()) {
    return canonical;
  }
  const mcp::JsonValue *verify = inner->Find("verify");
  if (verify == nullptr || !verify->IsObject()) {
    return canonical;
  }
  const mcp::JsonValue *available = verify->Find("available");
  if (available != nullptr && available->IsBool() &&
      !available->GetBool()) {
    mcp::JsonValue out(mcp::JsonValue::object_tag);
    if (const mcp::JsonValue *error = verify->Find("error")) {
      out["error"] = *error;
    } else {
      out["error"] = mcp::JsonValue("scene file is unavailable");
    }
    copy_field(*verify, out, "path");
    return out;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *match = verify->Find("match");
  out["result"] = mcp::JsonValue(
      match != nullptr && match->IsBool() && match->GetBool() ? "match"
                                                             : "mismatch");
  const char *keys[] = {"match",
                        "path",
                        "memory_nodes",
                        "disk_nodes",
                        "memory_connections",
                        "disk_connections",
                        "connections_match",
                        "missing_paths",
                        "missing_truncated",
                        "inherited_nodes",
                        "inherited_connections",
                        "inherited_paths",
                        "inherited_truncated",
                        "memory_hash",
                        "disk_hash"};
  for (const char *key : keys) {
    copy_field(*verify, out, key);
  }
  return out;
}

mcp::JsonValue has_group_node_args(const mcp::JsonValue &) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["include_groups"] = mcp::JsonValue(true);
  out["max_depth"] = mcp::JsonValue(static_cast<int64_t>(-1));
  return out;
}

mcp::JsonValue has_group_node_validate(const mcp::JsonValue &args) {
  const mcp::JsonValue *node_path = args.Find("node_path");
  if (node_path == nullptr || !node_path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: node_path");
    return error;
  }
  const mcp::JsonValue *group_name = args.Find("group_name");
  if (group_name == nullptr || !group_name->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: group_name");
    return error;
  }
  return mcp::JsonValue();
}

std::string group_query_relative_path(const std::string &raw,
                                      const mcp::JsonValue &tree) {
  std::string clean = raw;
  if (!clean.empty() && clean[0] == '/') {
    clean = clean.substr(1);
  }
  auto strip_segment = [&clean](const std::string &segment) {
    if (segment.empty()) {
      return;
    }
    if (clean == segment) {
      clean.clear();
      return;
    }
    if (clean.size() > segment.size() &&
        clean.compare(0, segment.size(), segment) == 0 &&
        clean[segment.size()] == '/') {
      clean = clean.substr(segment.size() + 1);
    }
  };
  strip_segment("root");
  const mcp::JsonValue *root_name = tree.Find("name");
  if (root_name != nullptr && root_name->IsString()) {
    strip_segment(root_name->GetString());
  }
  return clean;
}

const mcp::JsonValue *find_tree_node(const mcp::JsonValue &node,
                                     const std::string &target) {
  if (!node.IsObject()) {
    return nullptr;
  }
  const mcp::JsonValue *path = node.Find("path");
  if (path != nullptr && path->IsString() &&
      path->GetString() == target) {
    return &node;
  }
  const mcp::JsonValue *children = node.Find("children");
  if (children != nullptr && children->IsArray()) {
    for (const mcp::JsonValue &child : children->GetArray()) {
      if (const mcp::JsonValue *hit = find_tree_node(child, target)) {
        return hit;
      }
    }
  }
  return nullptr;
}

mcp::JsonValue has_group_node_project(const mcp::JsonValue &canonical,
                                      const mcp::JsonValue &args) {
  if (canonical.Find("error") != nullptr) {
    return canonical;
  }
  const mcp::JsonValue *node_path = args.Find("node_path");
  const mcp::JsonValue *group_name = args.Find("group_name");
  const mcp::JsonValue *tree = canonical.Find("result");
  if (node_path == nullptr || !node_path->IsString() ||
      group_name == nullptr || !group_name->IsString() || tree == nullptr ||
      !tree->IsObject()) {
    return canonical;
  }
  const std::string target =
      group_query_relative_path(node_path->GetString(), *tree);
  const mcp::JsonValue *found =
      target.empty() ? tree : find_tree_node(*tree, target);
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (found == nullptr) {
    out["error"] =
        mcp::JsonValue("node not found: " + node_path->GetString());
    return out;
  }
  bool in_group = false;
  if (const mcp::JsonValue *groups = found->Find("groups")) {
    if (groups->IsArray()) {
      for (const mcp::JsonValue &entry : groups->GetArray()) {
        if (entry.IsString() && entry.GetString() == group_name->GetString()) {
          in_group = true;
          break;
        }
      }
    }
  }
  out["result"] = mcp::JsonValue(in_group);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(7);
  v.push_back(make_spec_tool(ToolSpec{
      "create_scene_node",
      "Create scene content in the currently edited scene with one mode-based tool. mode='node' (default) creates a single node: provide 'type' (any Node subclass, default Node) and 'name' (default NewNode); omit 'parent_path' only while the scene has no root. Optional 'properties' applies values right after creation through the same conversion chain as property_set (NodePath strings become node references, resource references are resolved, an inline resource description such as {\"type\": \"RectangleShape2D\", \"properties\": {...}} creates a pathless sub-resource saved as a sub_resource); any failing property frees the new node and reports the property name. mode='scene' instantiates the PackedScene at 'path' (.tscn) under 'parent_path' (default: scene root), with optional 'name', 'owner' (default true) and 'editable_instance' (default false; marks the instance root as an editable instance of the current scene). mode='spec' builds a declarative subtree from 'spec' {type, name, props, children}, capped at 32 levels and 500 nodes with a whole-subtree rollback, and 'dry_run'/'preview' validates without writing. mode='bundle' creates one node and finishes properties, script, groups and signal connections atomically in that order, rolling the whole bundle back on any failure. mode='path' creates a Path2D/Path3D bound to a curve created by create_curve_resource ('name' and 'curve' required). Every mode is WritesScene, marks the scene modified and returns the previous per-mode result shape; the result also carries scene_path and scene_unsaved. Deprecated aliases: instantiate_scene (mode=scene), build_nodes_from_spec (mode=spec), spawn_node_bundle (mode=bundle), create_path_node (mode=path).",
      "Scene",
      {"node", "create", "mode", "instantiate_scene", "build_nodes_from_spec",
       "spawn_node_bundle", "create_path_node", "editable_instance"},
      SideEffect::WritesScene, tool_flags::kNone | tool_flags::kSceneTarget,
      kCreateSceneNodeParams, scene_ops::handle_create_scene_node,
      scene_create_schema(),
      {
          ToolAlias{"instantiate_scene", "create_scene_node",
                    scene_mode_args("scene"), {}, "0.4.0", "0.6.0"},
          ToolAlias{"build_nodes_from_spec", "create_scene_node",
                    scene_mode_args("spec"), {}, "0.4.0", "0.6.0"},
          ToolAlias{"spawn_node_bundle", "create_scene_node",
                    scene_mode_args("bundle"), {}, "0.4.0", "0.6.0"},
          ToolAlias{"create_path_node", "create_scene_node",
                    scene_mode_args("path"), {}, "0.4.0", "0.6.0"},
      },
      {"{\"mode\":\"node\",\"type\":\"Sprite2D\",\"name\":\"Icon\",\"parent_path\":\"UI\"}",
       "{\"mode\":\"scene\",\"path\":\"res://scenes/Enemy.tscn\",\"editable_instance\":true}",
       "{\"mode\":\"spec\",\"spec\":{\"type\":\"Node2D\",\"name\":\"Group\",\"children\":[{\"type\":\"Sprite2D\",\"name\":\"Icon\"}]}}",
       "{\"mode\":\"path\",\"name\":\"EnemyPath\",\"curve\":\"memory://enemy_curve\"}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "edit_scene_node",
      "Edit an existing node in the edited scene in one action tool. action='rename' renames 'path' to 'new_name' ('/' and ':' rejected; sibling names must stay unique) in one editor undo step. action='reparent' moves 'path' under 'new_parent_path' with optional 'keep_world_position' (default false; preserved only when both the node and the new parent are Node2D or both Node3D); the scene root, the node itself, its current parent and its own descendants are rejected. action='advance_path' advances a PathFollow2D/PathFollow3D at 'path' along its curve by 'offset' pixels (negative moves backward) and reports the new progress, progress_ratio and global position. action='duplicate' copies the subtree at 'path' with Node.duplicate (signals, groups and scripts preserved) 'count' times (default 1), names each copy with 'name_suffix' (default '_copy') plus a copy number when count > 1, inserts the copies right after the source and sets their owner to the edited scene root; 'editable_instance' also marks copies that are scene instances as editable instances. action='replace' duplicates the subtree at 'replacement_path', replaces the node at 'path' through Node.replace_by (groups of the replaced node are kept), renames the copy to the replaced node's name and frees the replaced subtree; both nodes must exist and neither may be an ancestor of the other. action='reorder' moves 'path' inside its parent to 'index' or before/after the sibling at 'before_path'/'after_path' (exactly one of the three); the scene root cannot be reordered. All actions are WritesScene and mark the scene modified; duplicate, replace and reorder return their own result object with the affected paths and index changes. Deprecated aliases: rename_scene_node (action=rename), reparent_node (action=reparent), advance_path_follow (action=advance_path).",
      "Scene",
      {"node", "edit", "rename", "reparent", "advance_path", "duplicate",
       "reorder", "replace", "replacement_path", "name_suffix",
       "editable_instance", "rename_scene_node", "reparent_node",
       "advance_path_follow"},
      SideEffect::WritesScene,
      tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kEditSceneNodeParams, scene_ops::handle_edit_scene_node,
      schema::build_action_schema(
          kEditSceneNodeActions, kEditSceneNodeParams,
          "Node edit action: rename|reparent|advance_path|duplicate|reorder|replace"),
      {
          scene_action_alias("rename_scene_node", "edit_scene_node", "rename"),
          scene_action_alias("reparent_node", "edit_scene_node", "reparent"),
          scene_action_alias("advance_path_follow", "edit_scene_node",
                             "advance_path"),
      },
      {"{\"action\":\"rename\",\"path\":\"Player/Sprite2D\",\"new_name\":\"Hero\"}",
       "{\"action\":\"reparent\",\"path\":\"Player/Weapon\",\"new_parent_path\":\"Inventory\"}",
       "{\"action\":\"advance_path\",\"path\":\"EnemyPath/Follower\",\"offset\":48}",
       "{\"action\":\"duplicate\",\"path\":\"Enemies/Enemy1\",\"count\":2,\"name_suffix\":\"_clone\"}",
       "{\"action\":\"replace\",\"path\":\"Enemies/Enemy1\",\"replacement_path\":\"Templates/Enemy\"}",
       "{\"action\":\"reorder\",\"path\":\"UI/HealthBar\",\"index\":0}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "delete_scene_node",
      "Delete a node from the edited scene by 'path'. Managed by the editor undo/redo history (action 'Delete Node <name>'): do removes the child and queue_frees it, undo re-adds it under the old parent with owner and sibling index restored — effective while the deferred free has not destroyed the node yet. Refuses to delete the scene root (close the scene first). Returns 'deleted' with 'undoable' true and an 'undo' info object (name, type, parent_path). This tool is classified as a destructive side effect because it destroys a scene entity that already exists in the host, and the editor undo stack is a session-only recovery path that is lost when the editor exits.",
      "Scene", {"node", "delete"}, SideEffect::Destructive, tool_flags::kNone | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kDeleteSceneNodeParams, scene_ops::handle_delete}));
  v.push_back(make_spec_tool(ToolSpec{
      "pack_scene",
      "Pack a subtree of the edited scene into a PackedScene file on disk (WritesFile). Pass 'node_path' (the subtree root; the edited scene root and childless nodes are rejected) and 'dest_path' (res:// .tscn/.scn; missing directories are created; existing files require 'overwrite' true). The owner chain is validated first: every saved descendant must be owned by the edited scene root, the pack root, or an ancestor inside the subtree; a node with no owner or a foreign owner fails with structured_error.code owner_chain_invalid before any file is written. The tool temporarily re-owns scene-root-owned descendants to the pack root, calls PackedScene::pack, restores the owners, saves with ResourceSaver, refreshes the editor filesystem, then reloads and instantiates the file to set 'verified'. Returns {path, node_path, node_count, connection_count, verified, overwritten, directories_created, undo}. Read the packed structure afterwards with analyze_project action=validate_scene include_state=true.",
      "Scene", {"scene", "pack", "prefab", "tscn", "subtree"}, SideEffect::WritesFile,
      tool_flags::kMutating | tool_flags::kSceneTarget,
      kPackSceneParams, scene_ops::handle_pack_scene,
      {},
      {},
      {"{\"node_path\":\"Player/Weapon\",\"dest_path\":\"res://prefabs/weapon.tscn\"}",
       "{\"node_path\":\"Enemies\",\"dest_path\":\"res://prefabs/enemies.tscn\",\"overwrite\":true}"}}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_scene_tree",
      "Walk the node tree of the currently edited scene, returning name, type, path and children for each node. Optional 'max_depth' (default 8; -1 for unlimited), 'include_properties' (true adds up to 20 properties per node, skipping metadata/ keys, names starting with underscore and Object-typed properties) and 'include_groups' (true adds a 'groups' string array to every node; this is the membership check that replaces the removed has_group_node tool). Optional 'include_owner' (scene-relative owner path), 'include_index' (sibling index) and 'include_scene_file_path' (source scene of instanced nodes) attach those fields to every node. Supplying any of 'pattern' (Godot match pattern, e.g. 'Enemy*'), 'type' (class name) or 'group' (group membership) switches to filter mode: the tool calls find_children on the edited scene root (owned descendants only, the root itself is never matched, max_depth does not apply), keeps group matches through is_in_group and returns {root, filters, count, nodes} with one entry per match instead of the nested tree. Optional 'verify' (default false) attaches a memory-versus-disk verify summary with match, node and connection counts plus missing_paths; optional 'paths' (1-50) attaches per-node screen positions under a nodes section; optional 'observe' appends a fresh editor viewport capture. With verify or paths the result is the snapshot envelope {tree, verify?, nodes?, scene_path, scene_unsaved}; without them the result is the tree object itself (or the filter envelope). verify, paths, scene_path and include_hash cannot be combined with pattern/type/group. Errors if no scene is open — create or open one first. Deprecated aliases: get_scene_snapshot, get_scene_node_screen_rect, verify_scene_saved, has_group_node.",
      "Scene", {"tree", "structure", "groups", "has_group_node",
                "get_scene_snapshot", "get_scene_node_screen_rect",
                "verify_scene_saved", "find", "filter", "pattern", "owner",
                "index"}, SideEffect::None, tool_flags::kObserve,
      kGetSceneTreeParams, scene_ops::handle_get_tree,
      {},
      {},
      {"{\"pattern\":\"Enemy*\",\"type\":\"Node2D\",\"include_owner\":true}",
       "{\"group\":\"enemies\",\"include_groups\":true,\"include_index\":true}",
       "{\"max_depth\":-1,\"include_scene_file_path\":true}"}},
      {
          ToolAlias{"get_scene_snapshot", "get_scene_tree",
                    scene_alias_args_with_verify, {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_scene_node_screen_rect", "get_scene_tree",
                    screen_rect_alias_args, screen_rect_alias_project, "0.4.0",
                    "0.6.0", screen_rect_alias_validate},
          ToolAlias{"verify_scene_saved", "get_scene_tree",
                    scene_alias_args_with_verify, verify_alias_project, "0.4.0",
                    "0.6.0"},
          ToolAlias{"has_group_node", "get_scene_tree", has_group_node_args,
                    {}, "0.4.0", "0.6.0", has_group_node_validate,
                    has_group_node_project},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "scene_camera",
      "Point or configure a camera in the edited scene, or convert coordinates through it, in one action tool. action='focus' points 'camera_path' (a Camera2D for 2D targets, a Camera3D for 3D targets) at 'target_path' (Node2D or Control for 2D cameras, Node3D for 3D cameras; other types error naming the actual class): a 2D camera is centered on the target anchor (the Control global-rect center, otherwise the canvas origin) and reports the padded 'focus_rect'; a 3D camera keeps its position and turns toward the target via look_at, reporting whether the target is 'behind' it. Optional 'margin' (number, default 0, must not be negative) grows the 2D focus rect, or pulls a 3D camera back along its view direction before aiming. action='set_params' applies 'params' (object with at least one entry) to 'camera_path': 'zoom' (Camera2D only), 'fov', 'near' and 'far' (Camera3D only), 'limits' (Camera2D only); keys for the wrong camera type or unknown keys error naming the actual class. action='project' converts a world 'point' ({x,y} or {x,y,z}) to 'screen_point' pixels; action='unproject' converts a viewport client-area 'screen_point' back to 'world_point' (3D uses 'depth', default 1.0, as the distance along the view direction); a Camera2D converts through its canvas transform built from get_screen_center_position/get_screen_rotation/get_zoom, so project then unproject round-trips, while a Camera3D uses Camera3D::unproject_position/project_position; action='project_ray' (Camera3D only) returns the 'origin' and 'normal' of the view ray through 'screen_point'; action='behind' reports whether 'point' sits behind a Camera3D (always false for a Camera2D). focus and set_params are WritesScene with one editor undo step and mark the scene modified; project, unproject, project_ray and behind are read-only and leave the scene untouched. Deprecated aliases: focus_camera_on_node (action=focus), set_camera_params (action=set_params).",
      "Scene", {"camera", "focus", "params", "project", "unproject", "ray",
                "behind", "screen", "focus_camera_on_node",
                "set_camera_params"},
      SideEffect::WritesScene, tool_flags::kMutating | tool_flags::kSceneTarget | tool_flags::kUndoable,
      kSceneCameraParams, scene_ops::handle_scene_camera,
      schema::build_action_schema(kSceneCameraActions, kSceneCameraParams,
                                  "Camera action: focus|set_params|project|unproject|project_ray|behind"),
      {
          scene_action_alias("focus_camera_on_node", "scene_camera", "focus"),
          scene_action_alias("set_camera_params", "scene_camera", "set_params"),
      },
      {"{\"action\":\"focus\",\"camera_path\":\"Camera2D\",\"target_path\":\"Player\",\"margin\":16}",
       "{\"action\":\"set_params\",\"camera_path\":\"Camera3D\",\"params\":{\"fov\":60}}",
       "{\"action\":\"project\",\"camera_path\":\"Camera3D\",\"point\":{\"x\":0,\"y\":1,\"z\":-5}}",
       "{\"action\":\"unproject\",\"camera_path\":\"Camera2D\",\"screen_point\":{\"x\":640,\"y\":360}}"}}));
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
