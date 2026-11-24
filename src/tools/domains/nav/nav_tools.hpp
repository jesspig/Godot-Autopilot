#ifndef GODOT_AUTOPILOT_NAV_TOOLS_HPP
#define GODOT_AUTOPILOT_NAV_TOOLS_HPP

#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/nav/nav_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace nav_tools {

namespace {

const std::vector<ParamSpec> kCreateNav2dMapParams = {
    {"active", "boolean", "Mark the map active for pathfinding (boolean, default: false)", false},
};

const std::vector<ParamSpec> kCreateNav2dRegionParams = {
    {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
    {"enabled", "boolean", "Whether the region participates in pathfinding (boolean, default: true)", false},
    {"navigation_layers", "integer", "Navigation layers bitmask (integer, default: 1 — layer 1 only)", false},
};

const std::vector<ParamSpec> kCreateNav2dAgentParams = {
    {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
    {"position", "object", "Agent start position (object with x and y numbers)", true},
    {"radius", "number", "Agent radius used for avoidance (number)", false},
    {"max_speed", "number", "Maximum speed in meters per second used for avoidance (number)", false},
    {"avoidance_enabled", "boolean", "Enable velocity avoidance for this agent (boolean, default: false)", false},
};

const std::vector<ParamSpec> kSetNav2dAgentVelocityParams = {
    {"agent_rid", "integer", "RID (integer) of the 2D navigation agent, from create_nav_2d_agent", true},
    {"velocity", "object", "Desired velocity (object with x and y numbers, e.g. {\"x\": 100, \"y\": 0})", true},
};

const std::vector<ParamSpec> kCreateNav3dMapParams = {
    {"active", "boolean", "Mark the map active for pathfinding (boolean, default: false)", false},
    {"cell_size", "number", "Map cell size in meters (number, default: 1.0)", false},
    {"cell_height", "number", "Map cell height in meters (number, default: 1.0)", false},
    {"up", "object", "Up direction (object with x, y and z numbers, default: {\"x\": 0, \"y\": 1, \"z\": 0})", false},
};

const std::vector<ParamSpec> kSetNav3dMapCellSizeParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"cell_size", "number", "New cell size in meters (number, e.g. 0.5)", true},
};

const std::vector<ParamSpec> kCreateNav3dRegionParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"enabled", "boolean", "Whether the region participates in pathfinding (boolean, default: true)", false},
    {"navigation_layers", "integer", "Navigation layers bitmask (integer, default: 1 — layer 1 only)", false},
};

const std::vector<ParamSpec> kSetNav3dRegionNavigationMeshParams = {
    {"region_rid", "integer", "RID (integer) of the 3D navigation region, from create_nav_3d_region", true},
    {"mesh_path", "string", "Path to a NavigationMesh resource (string, res:// path to a .tres or .obj file)", true},
};

const std::vector<ParamSpec> kCreateNav3dAgentParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"position", "object", "Agent start position (object with x, y and z numbers)", true},
    {"radius", "number", "Agent radius used for avoidance (number)", false},
    {"height", "number", "Agent height used for 3D avoidance (number)", false},
    {"max_speed", "number", "Maximum speed in meters per second used for avoidance (number)", false},
    {"use_3d_avoidance", "boolean", "Use 3D avoidance instead of 2D ground-plane avoidance (boolean, default: false)", false},
};

const std::vector<ParamSpec> kSetNav3dAgentVelocityParams = {
    {"agent_rid", "integer", "RID (integer) of the 3D navigation agent, from create_nav_3d_agent", true},
    {"velocity", "object", "Desired velocity (object with x, y and z numbers)", true},
};

const std::vector<ParamSpec> kCreateNav3dObstacleParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"position", "object", "Obstacle position (object with x, y and z numbers)", true},
    {"radius", "number", "Obstacle radius (number)", false},
    {"height", "number", "Obstacle height (number)", false},
};

const std::vector<ParamSpec> kNavQueryParams = {
    {"mode", "string", "Query to run: path_query, closest_point, closest_point_normal, closest_point_owner, closest_point_to_segment, regions, agents, links, iteration, force_update or agent_state", true},
    {"dimension", "integer", "Navigation dimension: 2 for NavigationServer2D or 3 for NavigationServer3D", true},
    {"map_rid", "integer", "RID (integer) of the navigation map, from create_nav_2d_map/create_nav_3d_map; required by every mode except agent_state", false},
    {"origin", "object", "path_query: path start point ({x,y} for dimension=2, {x,y,z} for dimension=3)", false},
    {"destination", "object", "path_query: path end point ({x,y} or {x,y,z})", false},
    {"metadata", "boolean", "path_query: include path segment metadata (types, RIDs and owner IDs) in the result (boolean, default: false)", false},
    {"navigation_layers", "integer", "path_query: navigation layers the path may traverse (integer bitmask, default: 1)", false},
    {"excluded_regions", "array", "path_query: array of region RIDs (integers) excluded from the query", false},
    {"simplify", "boolean", "path_query: simplify the returned path (boolean, default: true)", false},
    {"simplify_epsilon", "number", "path_query: simplification epsilon in world units (number, engine default when omitted)", false},
    {"path_postprocessing", "string", "path_query: post-processing mode: corridorfunnel (default), edgecentered or none", false},
    {"point", "object", "closest_point/closest_point_normal/closest_point_owner: point to snap to the map ({x,y} or {x,y,z})", false},
    {"start", "object", "closest_point_to_segment: segment start ({x,y,z})", false},
    {"end", "object", "closest_point_to_segment: segment end ({x,y,z})", false},
    {"use_collision", "boolean", "closest_point_to_segment: constrain the result with collision geometry (boolean, default: false)", false},
    {"agent_rid", "integer", "agent_state: RID (integer) of the navigation agent, from create_nav_2d_agent/create_nav_3d_agent", false},
};

const std::vector<ParamSpec> kBakeNavigationMeshParams = {
    {"navmesh_path", "string", "res:// path to the NavigationMesh resource to bake into (e.g. \"res://navmesh/level.tres\"); the resource is updated in memory but never saved", true},
    {"root_path", "string", "Scene-relative path of the root node whose geometry is parsed (e.g. \"Level\"); defaults to the edited scene root", false},
    {"timeout_ms", "integer", "Long-blocking budget in milliseconds (integer, default: 30000); a bake that takes longer returns structured_error.code bake_timeout after the synchronous engine bake finishes", false},
};

const std::vector<ParamSpec> kAstarPathParams = {
    {"kind", "string", "create: AStar kind to instantiate: grid_2d (AStarGrid2D), 2d (AStar2D) or 3d (AStar3D)", false},
    {"handle", "integer", "AStar handle returned by action=create; required by every action except create", false},
    {"region", "object", "grid_2d: rectangle {x, y, width, height} for create, set_region or fill_solid_region; each dimension is capped at 2048 and the cell count at 1048576, rejected before the grid is resized", false},
    {"size", "object", "grid_2d create: grid size {x, y} with positive dimensions (use instead of region); each dimension is capped at 2048 and the cell count at 1048576", false},
    {"cell_size", "object", "grid_2d create: world size of one cell {x, y}", false},
    {"offset", "object", "grid_2d create: world offset of the grid origin {x, y}", false},
    {"diagonal_mode", "string", "grid_2d create: always (default), never, at_least_one_walkable or only_if_no_obstacles", false},
    {"jumping_enabled", "boolean", "grid_2d create: enable jump point search (boolean, default: false)", false},
    {"point", "object", "set_point_solid: grid cell id {x, y}", false},
    {"solid", "boolean", "fill_solid_region/set_point_solid: mark the region or cell solid (boolean, default: true)", false},
    {"from", "json", "Path start: {x,y} cell id for grid_2d, integer point id for kind=2d/3d", false},
    {"to", "json", "Path end: {x,y} cell id for grid_2d, integer point id for kind=2d/3d", false},
    {"id", "integer", "add_point: point id (integer) to add", false},
    {"position", "object", "add_point: point position ({x,y} for kind=2d, {x,y,z} for kind=3d)", false},
    {"weight_scale", "number", "add_point: weight scale multiplying the travel cost through the point (number, default: 1.0)", false},
    {"bidirectional", "boolean", "connect/disconnect: connect or disconnect in both directions (boolean, default: true)", false},
    {"allow_partial_path", "boolean", "get_path/get_id_path/get_point_path: return the path found up to the closest reachable point when the target is unreachable (boolean, default: false)", false},
};

const std::vector<std::string> kAstarPathActions = {
    "create", "free", "set_region", "fill_solid_region", "set_point_solid",
    "get_id_path", "get_point_path", "add_point", "connect", "disconnect",
    "get_path"};

inline mcp::JsonValue nav_query_schema(
    const std::vector<ParamSpec> &params,
    std::initializer_list<const char *> modes, const char *description) {
  mcp::JsonValue raw = schema::build_schema(params);
  mcp::JsonValue mode(mcp::JsonValue::object_tag);
  mode["type"] = mcp::JsonValue("string");
  mode["description"] = mcp::JsonValue(description);
  mcp::JsonValue values(mcp::JsonValue::array_tag);
  for (const char *value : modes) {
    values.PushBack(mcp::JsonValue(value));
  }
  mode["enum"] = std::move(values);
  mcp::JsonValue *properties = raw.Find("properties");
  if (properties != nullptr && properties->IsObject()) {
    (*properties)["mode"] = std::move(mode);
  }
  return raw;
}

inline mcp::JsonValue nav_query_alias_args(const mcp::JsonValue &args,
                                           const char *mode,
                                           int64_t dimension) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["mode"] = mcp::JsonValue(mode);
  out["dimension"] = mcp::JsonValue(dimension);
  return out;
}

inline mcp::JsonValue nav_query_path_alias_args(const mcp::JsonValue &args,
                                                int64_t dimension) {
  mcp::JsonValue out = nav_query_alias_args(args, "path_query", dimension);
  const mcp::JsonValue *optimize = out.Find("optimize");
  if (optimize != nullptr && optimize->IsBool()) {
    out["simplify"] = *optimize;
  }
  return out;
}

inline mcp::JsonValue nav_query_path_alias_project(
    const mcp::JsonValue &canonical) {
  if (canonical.Find("error") != nullptr) {
    return canonical;
  }
  const mcp::JsonValue *result = canonical.Find("result");
  if (result == nullptr || !result->IsObject()) {
    return canonical;
  }
  const mcp::JsonValue *path = result->Find("path");
  if (path == nullptr) {
    return canonical;
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue(mcp::JsonValue::Object{{"path", *path}});
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_2d_map",
      "Create a 2D navigation map on NavigationServer2D. Optional 'active' (default false) marks the map for pathfinding use. Returns the map RID as 'rid' — keep it and pass it to create_nav_2d_region and nav_query mode=path_query (deprecated alias get_nav_2d_map_path). RIDs exist only in the running process; they are never persisted to scene files.",
      "Navigation", {"nav", "2d", "map", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav2dMapParams, nav_ops::handle_2d_map_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_2d_region",
      "Create a 2D navigation region attached to a map created by create_nav_2d_map. Requires 'map_rid'; optional 'enabled' (default true) and 'navigation_layers' bitmask (default 1). Returns the region RID as 'rid'. Regions define the walkable area — a map with no region yields an empty path.",
      "Navigation", {"nav", "2d", "region", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav2dRegionParams, nav_ops::handle_2d_region_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_2d_agent",
      "Create a 2D navigation agent on a map for avoidance-based movement. Requires 'map_rid' and 'position' ({x,y}); optional 'radius', 'max_speed' and 'avoidance_enabled' (default false). Returns the agent RID as 'rid'. Drive it each frame with set_nav_2d_agent_velocity; avoidance only steers while avoidance_enabled is true.",
      "Navigation", {"nav", "2d", "agent", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav2dAgentParams, nav_ops::handle_2d_agent_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_nav_2d_agent_velocity",
      "Set the desired velocity of a 2D navigation agent created by create_nav_2d_agent. Requires 'agent_rid' and 'velocity' ({x,y}). Call this every frame to drive avoidance — the agent adjusts the velocity around obstacles before it is used for movement. Returns true.",
      "Navigation", {"nav", "2d", "agent", "target"}, SideEffect::None, tool_flags::kNone,
      kSetNav2dAgentVelocityParams, nav_ops::handle_2d_agent_set_target}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_map",
      "Create a 3D navigation map on NavigationServer3D. Optional 'active' (default false), 'cell_size' (meters), 'cell_height' (meters) and 'up' ({x,y,z} direction, default Vector3.UP). Returns the map RID as 'rid' — pass it to create_nav_3d_region and nav_query mode=path_query (deprecated alias get_nav_3d_map_path). RIDs exist only in the running process; they are never persisted to scene files.",
      "Navigation", {"nav", "3d", "map", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dMapParams, nav_ops::handle_3d_map_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_nav_3d_map_cell_size",
      "Set the cell size (in meters) of a 3D navigation map created by create_nav_3d_map. Requires 'map_rid' and 'cell_size'. The cell size controls the granularity of the pathfinding grid; smaller values increase precision. Match it with the cell_size baked into region NavigationMeshes. Returns true on success.",
      "Navigation", {"nav", "3d", "map", "cell"}, SideEffect::None, tool_flags::kNone,
      kSetNav3dMapCellSizeParams, nav_ops::handle_3d_map_set_cell_size}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_region",
      "Create a 3D navigation region attached to a map created by create_nav_3d_map. Requires 'map_rid'; optional 'enabled' (default true) and 'navigation_layers' bitmask (default 1). Returns the region RID as 'rid'. Assign a navigation mesh with set_nav_3d_region_navigation_mesh before querying paths. A disabled region is ignored by pathfinding.",
      "Navigation", {"nav", "3d", "region", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dRegionParams, nav_ops::handle_3d_region_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_nav_3d_region_navigation_mesh",
      "Assign a NavigationMesh resource to a 3D navigation region (created by create_nav_3d_region) so the map can build paths over it. Requires 'region_rid' and 'mesh_path' — a res:// path to a NavigationMesh (.tres or .obj). Errors if loading fails or the resource is not a NavigationMesh. Returns true.",
      "Navigation", {"nav", "3d", "region", "navmesh"}, SideEffect::None, tool_flags::kNone,
      kSetNav3dRegionNavigationMeshParams, nav_ops::handle_3d_region_set_nav_mesh}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_agent",
      "Create a 3D navigation agent on a map for avoidance-based movement. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius', 'height', 'max_speed' and 'use_3d_avoidance' (default false). Returns the agent RID as 'rid'. Feed it a desired velocity each frame with set_nav_3d_agent_velocity. Read its state back with nav_query mode=agent_state (deprecated alias get_nav_3d_agent_state).",
      "Navigation", {"nav", "3d", "agent", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dAgentParams, nav_ops::handle_3d_agent_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_nav_3d_agent_velocity",
      "Set the desired velocity of a 3D navigation agent created by create_nav_3d_agent. Requires 'agent_rid' and 'velocity' ({x,y,z}). Call every frame: the agent computes an avoidance-adjusted velocity around obstacles (from create_nav_3d_obstacle) when avoidance is enabled. The stored value is readable via nav_query mode=agent_state. Returns true.",
      "Navigation", {"nav", "3d", "agent", "velocity"}, SideEffect::None, tool_flags::kNone,
      kSetNav3dAgentVelocityParams, nav_ops::handle_3d_agent_set_velocity}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_obstacle",
      "Create a 3D navigation obstacle on a map so avoidance agents steer around it. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius' and 'height' (meters). Returns the obstacle RID as 'rid'. Obstacles only affect agents with avoidance enabled; the RID stays valid until the process exits.",
      "Navigation", {"nav", "3d", "obstacle", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dObstacleParams, nav_ops::handle_3d_obstacle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "nav_query",
      "Query a 2D or 3D navigation map in one mode-based tool. 'dimension' (required) selects NavigationServer2D (2) or NavigationServer3D (3); 'map_rid' comes from create_nav_2d_map/create_nav_3d_map. mode='path_query' runs NavigationServer.query_path with NavigationPathQueryParameters/Result: 'origin' and 'destination' are {x,y} or {x,y,z}, optional 'metadata' (default false) adds path_types, path_rids and path_owner_ids, 'navigation_layers' (default 1) filters region layers, 'excluded_regions' is an array of region RID integers, 'simplify' (default true) and 'simplify_epsilon' tune simplification, and 'path_postprocessing' selects corridorfunnel|edgecentered|none; the result carries 'path' and 'path_length'. mode='closest_point' returns the map point closest to 'point'; 'closest_point_normal' (dimension=3 only) returns its normal; 'closest_point_owner' returns owner_rid and valid. mode='closest_point_to_segment' (dimension=3 only) returns the point closest to the 'start'/'end' segment, with optional 'use_collision'. mode='regions', 'agents' and 'links' return the RIDs registered on the map; mode='iteration' returns the map iteration id; mode='force_update' synchronously updates the map; mode='agent_state' returns the registered 'position' and 'velocity' of 'agent_rid'. Read-only except force_update, which only advances the map. Deprecated aliases: get_nav_2d_map_path (mode=path_query, dimension=2), get_nav_3d_map_path (mode=path_query, dimension=3), get_nav_3d_map_closest_point_to_segment (mode=closest_point_to_segment), get_nav_3d_agent_state (mode=agent_state).",
      "Navigation", {"nav", "query", "path", "closest_point", "closest_point_normal",
                     "closest_point_owner", "regions", "agents", "links",
                     "iteration", "force_update", "get_nav_2d_map_path",
                     "get_nav_3d_map_path",
                     "get_nav_3d_map_closest_point_to_segment",
                     "get_nav_3d_agent_state"},
      SideEffect::None, tool_flags::kNone,
      kNavQueryParams, nav_ops::handle_nav_query,
      nav_query_schema(
          kNavQueryParams,
          {"path_query", "closest_point", "closest_point_normal",
           "closest_point_owner", "closest_point_to_segment", "regions",
           "agents", "links", "iteration", "force_update", "agent_state"},
          "Query mode: path_query|closest_point|closest_point_normal|closest_point_owner|closest_point_to_segment|regions|agents|links|iteration|force_update|agent_state"),
      {
          ToolAlias{"get_nav_2d_map_path", "nav_query",
                    [](const mcp::JsonValue &args) {
                      return nav_query_path_alias_args(args, 2);
                    },
                    nav_query_path_alias_project, "0.4.0", "0.6.0"},
          ToolAlias{"get_nav_3d_map_path", "nav_query",
                    [](const mcp::JsonValue &args) {
                      return nav_query_path_alias_args(args, 3);
                    },
                    nav_query_path_alias_project, "0.4.0", "0.6.0"},
          ToolAlias{"get_nav_3d_map_closest_point_to_segment", "nav_query",
                    [](const mcp::JsonValue &args) {
                      return nav_query_alias_args(
                          args, "closest_point_to_segment", 3);
                    },
                    {}, "0.4.0", "0.6.0"},
          ToolAlias{"get_nav_3d_agent_state", "nav_query",
                    [](const mcp::JsonValue &args) {
                      return nav_query_alias_args(args, "agent_state", 3);
                    },
                    {}, "0.4.0", "0.6.0"},
      },
      {"{\"mode\":\"path_query\",\"dimension\":3,\"map_rid\":1,\"origin\":{\"x\":0,\"y\":0,\"z\":0},\"destination\":{\"x\":10,\"y\":0,\"z\":5},\"simplify\":true}",
       "{\"mode\":\"path_query\",\"dimension\":2,\"map_rid\":1,\"origin\":{\"x\":0,\"y\":0},\"destination\":{\"x\":10,\"y\":5},\"metadata\":true,\"excluded_regions\":[2]}",
       "{\"mode\":\"closest_point\",\"dimension\":2,\"map_rid\":1,\"point\":{\"x\":3,\"y\":4}}",
       "{\"mode\":\"regions\",\"dimension\":3,\"map_rid\":1}",
       "{\"mode\":\"agent_state\",\"dimension\":3,\"agent_rid\":7}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "bake_navigation_mesh",
      "Bake a NavigationMesh resource from the geometry under a scene node with NavigationMeshGenerator.bake(navigation_mesh, root_node). 'navmesh_path' is a required res:// path to a NavigationMesh resource; the loaded resource is baked in memory and is never saved by this tool (non-destructive). 'root_path' is the scene-relative node whose geometry is parsed (default: edited scene root). The call runs synchronously on the editor main thread and is flagged long-blocking; 'timeout_ms' (default 30000) is a post-bake budget — when the bake takes longer, the result is a structured_error with code bake_timeout carrying elapsed_ms, timeout_ms, vertices and polygons (the engine bake cannot be interrupted and the mesh is already updated). Returns navmesh_path, root_path, vertices, polygons, elapsed_ms and timeout_ms on success.",
      "Navigation", {"nav", "bake", "navmesh", "navigation_mesh", "long_blocking"},
      SideEffect::None, tool_flags::kLongBlocking,
      kBakeNavigationMeshParams, nav_ops::handle_bake_navigation_mesh,
      {}, {},
      {"{\"navmesh_path\":\"res://navmesh/level.tres\",\"root_path\":\"Level\",\"timeout_ms\":60000}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "astar_path",
      "Create and query in-memory AStar pathfinding structures in one action tool; nothing is saved to the scene. action='create' instantiates 'kind' (grid_2d = AStarGrid2D, 2d = AStar2D, 3d = AStar3D) and returns a 'handle' that keeps the object alive in a RefCounted registry; grid_2d create requires 'region' {x,y,width,height} or 'size' {x,y} plus optional 'cell_size', 'offset', 'diagonal_mode' and 'jumping_enabled'. action='free' releases a handle. grid_2d actions: 'set_region' and 'fill_solid_region' ('region', optional 'solid'), 'set_point_solid' ('point' {x,y}, optional 'solid'), 'get_id_path' and 'get_point_path' ('from'/'to' cell ids, optional 'allow_partial_path'). kind=2d/3d actions: 'add_point' ('id', 'position', optional 'weight_scale'), 'connect' and 'disconnect' ('from'/'to' integer point ids, optional 'bidirectional'), 'get_path' ('from'/'to', optional 'allow_partial_path'). Path actions return 'path' plus 'count'; create and add_point report the handle and point metadata. Handles live only in the running editor process.",
      "Navigation", {"nav", "astar", "path", "grid", "pathfinding", "handle"},
      SideEffect::None, tool_flags::kNone,
      kAstarPathParams, nav_ops::handle_astar_path,
      schema::build_action_schema(
          kAstarPathActions, kAstarPathParams,
          "AStar action: create|free|set_region|fill_solid_region|set_point_solid|get_id_path|get_point_path|add_point|connect|disconnect|get_path"),
      {},
      {"{\"action\":\"create\",\"kind\":\"grid_2d\",\"region\":{\"x\":0,\"y\":0,\"width\":16,\"height\":16},\"cell_size\":{\"x\":1,\"y\":1}}",
       "{\"action\":\"fill_solid_region\",\"handle\":1,\"region\":{\"x\":4,\"y\":4,\"width\":2,\"height\":2}}",
       "{\"action\":\"get_id_path\",\"handle\":1,\"from\":{\"x\":0,\"y\":0},\"to\":{\"x\":15,\"y\":15}}",
       "{\"action\":\"create\",\"kind\":\"3d\"}",
       "{\"action\":\"add_point\",\"handle\":2,\"id\":1,\"position\":{\"x\":0,\"y\":0,\"z\":0}}",
       "{\"action\":\"connect\",\"handle\":2,\"from\":1,\"to\":2}",
       "{\"action\":\"get_path\",\"handle\":2,\"from\":1,\"to\":2}",
       "{\"action\":\"free\",\"handle\":1}"}}));
  return v;
}

} // namespace nav_tools
} // namespace godot_autopilot

#endif
