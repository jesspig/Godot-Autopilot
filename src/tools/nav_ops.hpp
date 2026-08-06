#ifndef GODOT_SELF_DRIVING_NAV_OPS_HPP
#define GODOT_SELF_DRIVING_NAV_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace nav_ops {

mcp::JsonValue handle_2d_map_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_region_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_path_query(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_agent_create(const mcp::JsonValue &args);
mcp::JsonValue handle_2d_agent_set_target(const mcp::JsonValue &args);

mcp::JsonValue handle_3d_map_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_region_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_path_query(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_path_query_segment(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_agent_create(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_agent_set_velocity(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_agent_get_next_path(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_map_set_cell_size(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_region_set_nav_mesh(const mcp::JsonValue &args);
mcp::JsonValue handle_3d_obstacle_create(const mcp::JsonValue &args);

} // namespace nav_ops
} // namespace godot_self_driving

#endif
