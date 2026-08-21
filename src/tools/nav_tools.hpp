#ifndef GODOT_AUTOPILOT_NAV_TOOLS_HPP
#define GODOT_AUTOPILOT_NAV_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/nav_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace nav_tools {

GDA_TOOL_CLASS(CreateNav2dMapTool, "create_nav_2d_map",
               "Create a 2D navigation map on NavigationServer2D. Optional 'active' (default false) marks the map for pathfinding use. Returns the map RID as 'rid' — keep it and pass it to create_nav_2d_region and get_nav_2d_map_path. RIDs exist only in the running process; they are never persisted to scene files.",
               "Navigation", std::vector<std::string>({"nav", "2d", "map", "create"}), nav_ops::handle_2d_map_create, false)

GDA_TOOL_CLASS(CreateNav2dRegionTool, "create_nav_2d_region",
               "Create a 2D navigation region attached to a map created by create_nav_2d_map. Requires 'map_rid'; optional 'enabled' (default true) and 'navigation_layers' bitmask (default 1). Returns the region RID as 'rid'. Regions define the walkable area — a map with no region yields an empty path.",
               "Navigation", std::vector<std::string>({"nav", "2d", "region", "create"}), nav_ops::handle_2d_region_create, false)

GDA_TOOL_CLASS(GetNav2dMapPathTool, "get_nav_2d_map_path",
               "Query a navigation path across a 2D map (use after create_nav_2d_map and create_nav_2d_region). Requires 'map_rid', 'origin' and 'destination' ({x,y} objects); optional 'optimize' (default true) and 'navigation_layers' (default 1). Returns 'path' as an array of 2D points — empty when the map has no baked regions.",
               "Navigation", std::vector<std::string>({"nav", "2d", "path", "query"}), nav_ops::handle_2d_path_query, false)

GDA_TOOL_CLASS(CreateNav2dAgentTool, "create_nav_2d_agent",
               "Create a 2D navigation agent on a map for avoidance-based movement. Requires 'map_rid' and 'position' ({x,y}); optional 'radius', 'max_speed' and 'avoidance_enabled' (default false). Returns the agent RID as 'rid'. Drive it each frame with set_nav_2d_agent_velocity; avoidance only steers while avoidance_enabled is true.",
               "Navigation", std::vector<std::string>({"nav", "2d", "agent", "create"}), nav_ops::handle_2d_agent_create, false)

GDA_TOOL_CLASS(SetNav2dAgentVelocityTool, "set_nav_2d_agent_velocity",
               "Set the desired velocity of a 2D navigation agent created by create_nav_2d_agent. Requires 'agent_rid' and 'velocity' ({x,y}). Call this every frame to drive avoidance — the agent adjusts the velocity around obstacles before it is used for movement. Returns true.",
               "Navigation", std::vector<std::string>({"nav", "2d", "agent", "target"}), nav_ops::handle_2d_agent_set_target, false)

GDA_TOOL_CLASS(CreateNav3dMapTool, "create_nav_3d_map",
               "Create a 3D navigation map on NavigationServer3D. Optional 'active' (default false), 'cell_size' (meters), 'cell_height' (meters) and 'up' ({x,y,z} direction, default Vector3.UP). Returns the map RID as 'rid' — pass it to create_nav_3d_region and get_nav_3d_map_path. RIDs exist only in the running process; they are never persisted to scene files.",
               "Navigation", std::vector<std::string>({"nav", "3d", "map", "create"}), nav_ops::handle_3d_map_create, false)

GDA_TOOL_CLASS(SetNav3dMapCellSizeTool, "set_nav_3d_map_cell_size",
               "Set the cell size (in meters) of a 3D navigation map created by create_nav_3d_map. Requires 'map_rid' and 'cell_size'. The cell size controls the granularity of the pathfinding grid; smaller values increase precision. Match it with the cell_size baked into region NavigationMeshes. Returns true on success.",
               "Navigation", std::vector<std::string>({"nav", "3d", "map", "cell"}), nav_ops::handle_3d_map_set_cell_size, false)

GDA_TOOL_CLASS(CreateNav3dRegionTool, "create_nav_3d_region",
               "Create a 3D navigation region attached to a map created by create_nav_3d_map. Requires 'map_rid'; optional 'enabled' (default true) and 'navigation_layers' bitmask (default 1). Returns the region RID as 'rid'. Assign a navigation mesh with set_nav_3d_region_navigation_mesh before querying paths. A disabled region is ignored by pathfinding.",
               "Navigation", std::vector<std::string>({"nav", "3d", "region", "create"}), nav_ops::handle_3d_region_create, false)

GDA_TOOL_CLASS(SetNav3dRegionNavigationMeshTool, "set_nav_3d_region_navigation_mesh",
               "Assign a NavigationMesh resource to a 3D navigation region (created by create_nav_3d_region) so the map can build paths over it. Requires 'region_rid' and 'mesh_path' — a res:// path to a NavigationMesh (.tres or .obj). Errors if loading fails or the resource is not a NavigationMesh. Returns true.",
               "Navigation", std::vector<std::string>({"nav", "3d", "region", "navmesh"}), nav_ops::handle_3d_region_set_nav_mesh, false)

GDA_TOOL_CLASS(GetNav3dMapPathTool, "get_nav_3d_map_path",
               "Query a navigation path across a 3D map (use after create_nav_3d_map, create_nav_3d_region and set_nav_3d_region_navigation_mesh). Requires 'map_rid', 'origin' and 'destination' ({x,y,z} objects); optional 'optimize' (default true) and 'navigation_layers' (default 1). Returns 'path' as an array of 3D points — empty when the map has no baked mesh.",
               "Navigation", std::vector<std::string>({"nav", "3d", "path", "query"}), nav_ops::handle_3d_path_query, false)

GDA_TOOL_CLASS(GetNav3dMapClosestPointToSegmentTool, "get_nav_3d_map_closest_point_to_segment",
               "Get the point on a 3D navigation map closest to a line segment. Requires 'map_rid', 'start' and 'end' ({x,y,z} segment endpoints); optional 'use_collision' (default false) constrains the result to the segment when true. Returns the closest point as 'point'. Useful for snapping a position onto the navigation mesh.",
               "Navigation", std::vector<std::string>({"nav", "3d", "path", "segment"}), nav_ops::handle_3d_path_query_segment, false)

GDA_TOOL_CLASS(CreateNav3dAgentTool, "create_nav_3d_agent",
               "Create a 3D navigation agent on a map for avoidance-based movement. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius', 'height', 'max_speed' and 'use_3d_avoidance' (default false). Returns the agent RID as 'rid'. Feed it a desired velocity each frame with set_nav_3d_agent_velocity. Read its state back with get_nav_3d_agent_state.",
               "Navigation", std::vector<std::string>({"nav", "3d", "agent", "create"}), nav_ops::handle_3d_agent_create, false)

GDA_TOOL_CLASS(SetNav3dAgentVelocityTool, "set_nav_3d_agent_velocity",
               "Set the desired velocity of a 3D navigation agent created by create_nav_3d_agent. Requires 'agent_rid' and 'velocity' ({x,y,z}). Call every frame: the agent computes an avoidance-adjusted velocity around obstacles (from create_nav_3d_obstacle) when avoidance is enabled. The stored value is readable via get_nav_3d_agent_state. Returns true.",
               "Navigation", std::vector<std::string>({"nav", "3d", "agent", "velocity"}), nav_ops::handle_3d_agent_set_velocity, false)

GDA_TOOL_CLASS(GetNav3dAgentStateTool, "get_nav_3d_agent_state",
               "Read the current state of a 3D navigation agent created by create_nav_3d_agent. Requires 'agent_rid'. Returns 'position' and 'velocity' — the registered position and the velocity last supplied via set_nav_3d_agent_velocity. Use it to verify the agent is registered and responding. Both values are returned as {x,y,z} objects in 'result'.",
               "Navigation", std::vector<std::string>({"nav", "3d", "agent", "next_path"}), nav_ops::handle_3d_agent_get_next_path, false)

GDA_TOOL_CLASS(CreateNav3dObstacleTool, "create_nav_3d_obstacle",
               "Create a 3D navigation obstacle on a map so avoidance agents steer around it. Requires 'map_rid' and 'position' ({x,y,z}); optional 'radius' and 'height' (meters). Returns the obstacle RID as 'rid'. Obstacles only affect agents with avoidance enabled; the RID stays valid until the process exits.",
               "Navigation", std::vector<std::string>({"nav", "3d", "obstacle", "create"}), nav_ops::handle_3d_obstacle_create, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(std::make_unique<CreateNav2dMapTool>());
  v.push_back(std::make_unique<CreateNav2dRegionTool>());
  v.push_back(std::make_unique<GetNav2dMapPathTool>());
  v.push_back(std::make_unique<CreateNav2dAgentTool>());
  v.push_back(std::make_unique<SetNav2dAgentVelocityTool>());
  v.push_back(std::make_unique<CreateNav3dMapTool>());
  v.push_back(std::make_unique<SetNav3dMapCellSizeTool>());
  v.push_back(std::make_unique<CreateNav3dRegionTool>());
  v.push_back(std::make_unique<SetNav3dRegionNavigationMeshTool>());
  v.push_back(std::make_unique<GetNav3dMapPathTool>());
  v.push_back(std::make_unique<GetNav3dMapClosestPointToSegmentTool>());
  v.push_back(std::make_unique<CreateNav3dAgentTool>());
  v.push_back(std::make_unique<SetNav3dAgentVelocityTool>());
  v.push_back(std::make_unique<GetNav3dAgentStateTool>());
  v.push_back(std::make_unique<CreateNav3dObstacleTool>());
  return v;
}

} // namespace nav_tools
} // namespace godot_autopilot

#endif