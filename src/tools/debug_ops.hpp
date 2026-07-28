#ifndef GODOT_SELF_DRIVING_DEBUG_OPS_HPP
#define GODOT_SELF_DRIVING_DEBUG_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace debug_ops {

mcp::JsonValue handle_print(const mcp::JsonValue& args);
mcp::JsonValue handle_print_stack(const mcp::JsonValue& args);
mcp::JsonValue handle_get_performance_monitor(const mcp::JsonValue& args);
mcp::JsonValue handle_list_performance_monitors(const mcp::JsonValue& args);
mcp::JsonValue handle_get_object_count(const mcp::JsonValue& args);
mcp::JsonValue handle_get_object_count_by_class(const mcp::JsonValue& args);
mcp::JsonValue handle_get_memory_usage(const mcp::JsonValue& args);
mcp::JsonValue handle_profile_start(const mcp::JsonValue& args);
mcp::JsonValue handle_profile_stop(const mcp::JsonValue& args);
mcp::JsonValue handle_profile_get_data(const mcp::JsonValue& args);
mcp::JsonValue handle_set_fps_limit(const mcp::JsonValue& args);
mcp::JsonValue handle_set_physics_fps(const mcp::JsonValue& args);
mcp::JsonValue handle_collision_debug(const mcp::JsonValue& args);
mcp::JsonValue handle_navigation_debug(const mcp::JsonValue& args);
mcp::JsonValue handle_performance_debug(const mcp::JsonValue& args);

} // namespace debug_ops
} // namespace godot_self_driving
#endif
