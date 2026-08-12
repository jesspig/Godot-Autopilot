#ifndef GODOT_AUTOPILOT_CONFIG_OPS_HPP
#define GODOT_AUTOPILOT_CONFIG_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace config_ops {

mcp::JsonValue handle_project_settings_get(const mcp::JsonValue &args);
mcp::JsonValue handle_project_settings_set(const mcp::JsonValue &args);
mcp::JsonValue handle_project_settings_has(const mcp::JsonValue &args);
mcp::JsonValue handle_project_settings_save(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_get_version(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_get_fps(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_get_frames_drawn(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_set_time_scale(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_get_time_scale(const mcp::JsonValue &args);
mcp::JsonValue handle_engine_set_max_fps(const mcp::JsonValue &args);
mcp::JsonValue handle_editor_settings_get(const mcp::JsonValue &args);
mcp::JsonValue handle_editor_settings_set(const mcp::JsonValue &args);
mcp::JsonValue handle_editor_settings_has(const mcp::JsonValue &args);

} // namespace config_ops
} // namespace godot_autopilot
#endif
