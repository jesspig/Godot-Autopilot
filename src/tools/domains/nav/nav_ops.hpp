#ifndef GODOT_AUTOPILOT_NAV_OPS_HPP
#define GODOT_AUTOPILOT_NAV_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace nav_ops {

godot_autopilot::util::JsonValue handle_2d_map_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_region_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_path_query(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_agent_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_2d_agent_set_target(const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue handle_3d_map_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_region_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_path_query(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_path_query_segment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_agent_create(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_agent_set_velocity(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_agent_get_next_path(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_map_set_cell_size(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_region_set_nav_mesh(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_3d_obstacle_create(const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue handle_nav_query(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_bake_navigation_mesh(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_astar_path(const godot_autopilot::util::JsonValue &args);

} // namespace nav_ops
} // namespace godot_autopilot

#endif
