#ifndef GODOT_AUTOPILOT_CONFIG_OPS_HPP
#define GODOT_AUTOPILOT_CONFIG_OPS_HPP

#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace config_ops {

bool setting_key_is_protected(const std::string &key);

mcp::JsonValue handle_get_project_settings(const mcp::JsonValue &args);
mcp::JsonValue handle_update_project_settings(const mcp::JsonValue &args);
mcp::JsonValue handle_get_editor_settings(const mcp::JsonValue &args);
mcp::JsonValue handle_update_editor_settings(const mcp::JsonValue &args);
mcp::JsonValue handle_get_engine_state(const mcp::JsonValue &args);
mcp::JsonValue handle_update_engine_state(const mcp::JsonValue &args);
mcp::JsonValue handle_get_autoloads(const mcp::JsonValue &args);
mcp::JsonValue handle_update_autoloads(const mcp::JsonValue &args);
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
mcp::JsonValue handle_autoload_list(const mcp::JsonValue &args);
mcp::JsonValue handle_autoload_add(const mcp::JsonValue &args);
mcp::JsonValue handle_autoload_remove(const mcp::JsonValue &args);
mcp::JsonValue handle_autoload_move(const mcp::JsonValue &args);

} // namespace config_ops
} // namespace godot_autopilot
#endif
