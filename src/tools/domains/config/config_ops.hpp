#ifndef GODOT_AUTOPILOT_CONFIG_OPS_HPP
#define GODOT_AUTOPILOT_CONFIG_OPS_HPP

#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace config_ops {

bool setting_key_is_protected(const std::string &key);

godot_autopilot::util::JsonValue handle_get_project_settings(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_project_settings(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_editor_settings(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_editor_settings(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_engine_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_engine_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_autoloads(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_autoloads(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_project_settings_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_project_settings_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_project_settings_has(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_project_settings_save(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_get_version(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_get_fps(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_get_frames_drawn(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_set_time_scale(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_get_time_scale(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_engine_set_max_fps(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_settings_get(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_settings_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_settings_has(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_autoload_list(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_autoload_add(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_autoload_remove(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_autoload_move(const godot_autopilot::util::JsonValue &args);

} // namespace config_ops
} // namespace godot_autopilot
#endif
