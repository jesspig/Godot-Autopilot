#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_editor_config(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["editor_get_selection"] = schema::build_schema({});
        m["editor_set_selection"] = schema::build_schema({
            {"paths", "array", "List of node paths to select", true},
        });
        m["editor_get_edited_scene_root"] = schema::build_schema({});
        m["editor_save_scene"] = schema::build_schema({});
        m["editor_save_all_scenes"] = schema::build_schema({});
        m["editor_reload_scene"] = schema::build_schema({
            {"scene_path", "string", "Scene file path to reload (default: current edited scene)", false},
        });
        m["editor_inspect_object"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID to inspect", true},
        });
        m["editor_undo_redo_start"] = schema::build_schema({
            {"name", "string", "Undo/redo action name", true},
        });
        m["editor_undo_redo_commit"] = schema::build_schema({});
        m["editor_undo_redo_add_do"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["editor_undo_redo_add_undo"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["editor_file_system_get_resources"] = schema::build_schema({
            {"path", "string", "Directory path to list", false},
        });
        m["editor_file_system_scan"] = schema::build_schema({});
        m["editor_import_resource"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["editor_set_main_scene"] = schema::build_schema({
            {"path", "string", "Scene file path", true},
        });
        m["editor_play_current_scene"] = schema::build_schema({});
        m["editor_stop_playing"] = schema::build_schema({});
        m["editor_get_resource_filesystem"] = schema::build_schema({});
        m["editor_get_plugin_list"] = schema::build_schema({});
        m["editor_set_plugin_enabled"] = schema::build_schema({
            {"plugin", "string", "Plugin name", true},
            {"enabled", "boolean", "Whether to enable or disable", true},
        });
        m["editor_new_scene"] = schema::build_schema({
            {"type", "string", "Node class type (default: Node)", false},
            {"name", "string", "Node name (default: NewRoot)", false},
            {"close_current", "boolean", "Close the current scene first if it has no unsaved changes (default: false; errors if the current scene is unsaved)", false},
        });
        m["editor_open_scene"] = schema::build_schema({
            {"path", "string", "Path to the scene file to open (e.g., res://game.tscn)", true},
        });
        m["editor_save_scene_as"] = schema::build_schema({
            {"path", "string", "File path to save scene as", true},
        });
        m["editor_new_text_resource"] = schema::build_schema({
            {"path", "string", "File path to save the resource", true},
            {"source_code", "string", "Text content to write", true},
        });
        m["editor_close_scene"] = schema::build_schema({});

        m["project_settings_get"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"default", "object", "Default value if setting does not exist", false},
        });
        m["project_settings_set"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["project_settings_has"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
        });
        m["project_settings_save"] = schema::build_schema({});
        m["engine_get_version"] = schema::build_schema({});
        m["engine_get_fps"] = schema::build_schema({});
        m["engine_get_frames_drawn"] = schema::build_schema({});
        m["engine_set_time_scale"] = schema::build_schema({
            {"scale", "number", "Time scale factor", true},
        });
        m["engine_get_time_scale"] = schema::build_schema({});
        m["engine_set_max_fps"] = schema::build_schema({
            {"fps", "integer", "Maximum FPS", true},
        });
        m["editor_settings_get"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });
        m["editor_settings_set"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["editor_settings_has"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });

        m["input_map_add_action"] = schema::build_schema({
            {"action", "string", "Action name identifier (e.g. \"mario_jump\", \"move_left\", \"ui_accept\") — this is a programmatic identifier, not a display name", true},
            {"deadzone", "number", "Deadzone value (default: 0.5)", false},
        });
        m["input_map_erase_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to remove (e.g. \"mario_jump\")", true},
        });
        m["input_map_get_actions"] = schema::build_schema({});
        m["input_map_has_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to check for existence (e.g. \"ui_accept\")", true},
        });
        m["input_map_action_add_event"] = schema::build_schema({
            {"action", "string", "Action name identifier to bind event to (e.g. \"mario_jump\")", true},
            {"event", "object", "Input event object (must include \"class\":\"InputEventKey\" etc.) — physical_keycode/keycode accept numeric key codes or KEY_* name strings (e.g. \"KEY_A\" or 65)", true},
        });
        m["input_map_action_erase_event"] = schema::build_schema({
            {"action", "string", "Action name identifier whose event to remove (e.g. \"mario_jump\")", true},
            {"event_index", "integer", "Index of event to remove", true},
        });
        m["input_map_action_set_deadzone"] = schema::build_schema({
            {"action", "string", "Action name identifier to set deadzone for (e.g. \"mario_jump\")", true},
            {"deadzone", "number", "Deadzone value (0.0 to 1.0)", true},
        });
        m["input_map_persist"] = schema::build_schema({
            {"actions", "array", "Action whitelist (array of action name strings); when omitted, actions prefixed with \"ui_\" or containing \"/\" are filtered out", false},
        });

        m["input_action_press"] = schema::build_schema({
            {"action", "string", "Input action name to press", true},
            {"strength", "number", "Action strength (default: 1.0)", false},
        });
        m["input_action_release"] = schema::build_schema({
            {"action", "string", "Input action name to release", true},
        });
        m["input_is_action_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["input_is_action_just_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["input_key_press"] = schema::build_schema({
            {"key", "string", "Key name to press (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["input_key_release"] = schema::build_schema({
            {"key", "string", "Key name to release (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["input_mouse_move"] = schema::build_schema({
            {"position", "object", "Mouse position (Vector2 with x and y fields)", true},
            {"relative", "object", "Relative movement (Vector2 with x and y fields)", false},
        });
        m["input_mouse_button_press"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["input_mouse_button_release"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["input_gamepad_simulate"] = schema::build_schema({
            {"action", "string", "Gamepad action: STOP or VIBRATE", true},
            {"device", "integer", "Gamepad device index (default: 0)", false},
            {"weak_magnitude", "number", "Weak motor magnitude 0-1 (VIBRATE)", false},
            {"strong_magnitude", "number", "Strong motor magnitude 0-1 (VIBRATE)", false},
            {"duration", "number", "Vibration duration in seconds", false},
        });
}

} // namespace godot_autopilot
