#ifndef GODOT_AUTOPILOT_NAV_TOOLS_HPP
#define GODOT_AUTOPILOT_NAV_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/nav_ops.hpp"
#include <tools/tool_spec.hpp>

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

const std::vector<ParamSpec> kGetNav2dMapPathParams = {
    {"map_rid", "integer", "RID (integer) of the 2D navigation map, from create_nav_2d_map", true},
    {"origin", "object", "Path start point (object with x and y numbers, e.g. {\"x\": 0, \"y\": 0})", true},
    {"destination", "object", "Path end point (object with x and y numbers, e.g. {\"x\": 100, \"y\": 50})", true},
    {"optimize", "boolean", "Simplify the path (boolean, default: true)", false},
    {"navigation_layers", "integer", "Layers the path may traverse (integer bitmask, default: 1)", false},
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

const std::vector<ParamSpec> kGetNav3dMapPathParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"origin", "object", "Path start point (object with x, y and z numbers, e.g. {\"x\": 0, \"y\": 0, \"z\": 0})", true},
    {"destination", "object", "Path end point (object with x, y and z numbers)", true},
    {"optimize", "boolean", "Simplify the path (boolean, default: true)", false},
    {"navigation_layers", "integer", "Layers the path may traverse (integer bitmask, default: 1)", false},
};

const std::vector<ParamSpec> kGetNav3dMapClosestPointToSegmentParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"start", "object", "Segment start point (object with x, y and z numbers)", true},
    {"end", "object", "Segment end point (object with x, y and z numbers)", true},
    {"use_collision", "boolean", "Consider collision geometry when finding the closest point (boolean, default: false)", false},
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

const std::vector<ParamSpec> kGetNav3dAgentStateParams = {
    {"agent_rid", "integer", "RID (integer) of the 3D navigation agent, from create_nav_3d_agent", true},
};

const std::vector<ParamSpec> kCreateNav3dObstacleParams = {
    {"map_rid", "integer", "RID (integer) of the 3D navigation map, from create_nav_3d_map", true},
    {"position", "object", "Obstacle position (object with x, y and z numbers)", true},
    {"radius", "number", "Obstacle radius (number)", false},
    {"height", "number", "Obstacle height (number)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_2d_map",
      "Create a 2D navigation map on NavigationServer2D. Optional 'active' (default false) marks the map for pathfinding use. Returns the map RID as 'rid' — keep it and pass it to create_nav_2d_region and get_nav_2d_map_path. RIDs exist only in the running process; they are never persisted to scene files.",
      "Navigation", {"nav", "2d", "map", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav2dMapParams, nav_ops::handle_2d_map_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_2d_region",
      "Create a 2D navigation region attached to a map created by create_nav_2d_map. Requires 'map_rid'; optional 'enabled' (default true) and 'navigation_layers' bitmask (default 1). Returns the region RID as 'rid'. Regions define the walkable area — a map with no region yields an empty path.",
      "Navigation", {"nav", "2d", "region", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav2dRegionParams, nav_ops::handle_2d_region_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_nav_2d_map_path",
      "Query a navigation path across a 2D map (use after create_nav_2d_map and create_nav_2d_region). Requires 'map_rid', 'origin' and 'destination' ({x,y} objects); optional 'optimize' (default true) and 'navigation_layers' (default 1). Returns 'path' as an array of 2D points — empty when the map has no baked regions.",
      "Navigation", {"nav", "2d", "path", "query"}, SideEffect::None, tool_flags::kNone,
      kGetNav2dMapPathParams, nav_ops::handle_2d_path_query}));
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
      "Create a 3D navigation map on NavigationServer3D. Optional 'active' (default false), 'cell_size' (meters), 'cell_height' (meters) and 'up' ({x,y,z} direction, default Vector3.UP). Returns the map RID as 'rid' — pass it to create_nav_3d_region and get_nav_3d_map_path. RIDs exist only in the running process; they are never persisted to scene files.",
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
      "get_nav_3d_map_path",
      "Query a navigation path across a 3D map (use after create_nav_3d_map, create_nav_3d_region and set_nav_3d_region_navigation_mesh). Requires 'map_rid', 'origin' and 'destination' ({x,y,z} objects); optional 'optimize' (default true) and 'navigation_layers' (default 1). Returns 'path' as an array of 3D points — empty when the map has no baked mesh.",
      "Navigation", {"nav", "3d", "path", "query"}, SideEffect::None, tool_flags::kNone,
      kGetNav3dMapPathParams, nav_ops::handle_3d_path_query}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_nav_3d_map_closest_point_to_segment",
      "Get the point on a 3D navigation map closest to a line segment. Requires 'map_rid', 'start' and 'end' ({x,y,z} segment endpoints); optional 'use_collision' (default false) constrains the result to the segment when true. Returns the closest point as 'point'. Useful for snapping a position onto the navigation mesh.",
      "Navigation", {"nav", "3d", "path", "segment"}, SideEffect::None, tool_flags::kNone,
      kGetNav3dMapClosestPointToSegmentParams, nav_ops::handle_3d_path_query_segment}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_agent",
      "Create a 3D navigation agent on a map for avoidance-based movement. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius', 'height', 'max_speed' and 'use_3d_avoidance' (default false). Returns the agent RID as 'rid'. Feed it a desired velocity each frame with set_nav_3d_agent_velocity. Read its state back with get_nav_3d_agent_state.",
      "Navigation", {"nav", "3d", "agent", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dAgentParams, nav_ops::handle_3d_agent_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_nav_3d_agent_velocity",
      "Set the desired velocity of a 3D navigation agent created by create_nav_3d_agent. Requires 'agent_rid' and 'velocity' ({x,y,z}). Call every frame: the agent computes an avoidance-adjusted velocity around obstacles (from create_nav_3d_obstacle) when avoidance is enabled. The stored value is readable via get_nav_3d_agent_state. Returns true.",
      "Navigation", {"nav", "3d", "agent", "velocity"}, SideEffect::None, tool_flags::kNone,
      kSetNav3dAgentVelocityParams, nav_ops::handle_3d_agent_set_velocity}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_nav_3d_agent_state",
      "Read the current state of a 3D navigation agent created by create_nav_3d_agent. Requires 'agent_rid'. Returns 'position' and 'velocity' — the registered position and the velocity last supplied via set_nav_3d_agent_velocity. Use it to verify the agent is registered and responding. Both values are returned as {x,y,z} objects in 'result'.",
      "Navigation", {"nav", "3d", "agent", "next_path"}, SideEffect::None, tool_flags::kNone,
      kGetNav3dAgentStateParams, nav_ops::handle_3d_agent_get_next_path}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_nav_3d_obstacle",
      "Create a 3D navigation obstacle on a map so avoidance agents steer around it. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius' and 'height' (meters). Returns the obstacle RID as 'rid'. Obstacles only affect agents with avoidance enabled; the RID stays valid until the process exits.",
      "Navigation", {"nav", "3d", "obstacle", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateNav3dObstacleParams, nav_ops::handle_3d_obstacle_create}));
  return v;
}

} // namespace nav_tools
} // namespace godot_autopilot

#endif