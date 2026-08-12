#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_editor_config(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["get_editor_selection"] = schema::build_schema({});
        m["set_editor_selection"] = schema::build_schema({
            {"paths", "array", "List of node paths to select", true},
        });
        m["get_editor_edited_scene_root"] = schema::build_schema({});
        m["save_editor_scene"] = schema::build_schema({});
        m["save_editor_scenes"] = schema::build_schema({});
        m["reload_editor_scene"] = schema::build_schema({
            {"scene_path", "string", "Scene file path to reload (default: current edited scene)", false},
        });
        m["inspect_editor_resource"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID to inspect", true},
        });
        m["create_editor_undo_redo_action"] = schema::build_schema({
            {"name", "string", "Undo/redo action name", true},
        });
        m["commit_editor_undo_redo"] = schema::build_schema({});
        m["add_editor_undo_redo_do"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["add_editor_undo_redo_undo"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["get_editor_file_system_tree"] = schema::build_schema({
            {"path", "string", "Directory path to list", false},
        });
        m["scan_editor_file_system"] = schema::build_schema({});
        m["set_editor_main_scene"] = schema::build_schema({
            {"path", "string", "Scene file path", true},
        });
        m["play_editor_current_scene"] = schema::build_schema({});
        m["stop_editor_playing"] = schema::build_schema({});
        m["get_editor_file_system_status"] = schema::build_schema({});
        m["set_editor_plugin_enabled"] = schema::build_schema({
            {"plugin", "string", "Plugin name", true},
            {"enabled", "boolean", "Whether to enable or disable", true},
        });
        m["create_editor_scene"] = schema::build_schema({
            {"type", "string", "Node class type (default: Node)", false},
            {"name", "string", "Node name (default: NewRoot)", false},
            {"close_current", "boolean", "Close the current scene first if it has no unsaved changes (default: false; errors if the current scene is unsaved)", false},
        });
        m["open_editor_scene"] = schema::build_schema({
            {"path", "string", "Path to the scene file to open (e.g., res://game.tscn)", true},
        });
        m["save_editor_scene_as"] = schema::build_schema({
            {"path", "string", "File path to save scene as", true},
        });
        m["close_editor_scene"] = schema::build_schema({});

        m["get_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"default", "object", "Default value if setting does not exist", false},
        });
        m["set_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["has_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
        });
        m["save_project_settings"] = schema::build_schema({});
        m["get_engine_version"] = schema::build_schema({});
        m["get_engine_fps"] = schema::build_schema({});
        m["get_engine_frames_drawn"] = schema::build_schema({});
        m["set_engine_time_scale"] = schema::build_schema({
            {"scale", "number", "Time scale factor", true},
        });
        m["get_engine_time_scale"] = schema::build_schema({});
        m["set_engine_max_fps"] = schema::build_schema({
            {"fps", "integer", "Maximum FPS", true},
        });
        m["get_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });
        m["set_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["has_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });

        m["add_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier (e.g. \"mario_jump\", \"move_left\", \"ui_accept\") — this is a programmatic identifier, not a display name", true},
            {"deadzone", "number", "Deadzone value (default: 0.5)", false},
        });
        m["erase_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to remove (e.g. \"mario_jump\")", true},
        });
        m["get_input_map_actions"] = schema::build_schema({});
        m["has_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to check for existence (e.g. \"ui_accept\")", true},
        });
        m["add_input_map_action_event"] = schema::build_schema({
            {"action", "string", "Action name identifier to bind event to (e.g. \"mario_jump\")", true},
            {"event", "object", "Input event object (must include \"class\":\"InputEventKey\" etc.) — physical_keycode/keycode accept numeric key codes or KEY_* name strings (e.g. \"KEY_A\" or 65)", true},
        });
        m["erase_input_map_action_event"] = schema::build_schema({
            {"action", "string", "Action name identifier whose event to remove (e.g. \"mario_jump\")", true},
            {"event_index", "integer", "Index of event to remove", true},
        });
        m["set_input_map_action_deadzone"] = schema::build_schema({
            {"action", "string", "Action name identifier to set deadzone for (e.g. \"mario_jump\")", true},
            {"deadzone", "number", "Deadzone value (0.0 to 1.0)", true},
        });
        m["save_input_map"] = schema::build_schema({
            {"actions", "array", "Action whitelist (array of action name strings); when omitted, actions prefixed with \"ui_\" or containing \"/\" are filtered out", false},
        });

        m["press_input_action"] = schema::build_schema({
            {"action", "string", "Input action name to press", true},
            {"strength", "number", "Action strength (default: 1.0)", false},
        });
        m["release_input_action"] = schema::build_schema({
            {"action", "string", "Input action name to release", true},
        });
        m["is_input_action_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["is_input_action_just_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["press_input_key"] = schema::build_schema({
            {"key", "string", "Key name to press (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["release_input_key"] = schema::build_schema({
            {"key", "string", "Key name to release (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["move_input_mouse"] = schema::build_schema({
            {"position", "object", "Mouse position (Vector2 with x and y fields)", true},
            {"relative", "object", "Relative movement (Vector2 with x and y fields)", false},
        });
        m["press_input_mouse_button"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["release_input_mouse_button"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["start_input_gamepad_vibration"] = schema::build_schema({
            {"device", "integer", "Gamepad device index", true},
            {"weak", "number", "Weak motor magnitude 0-1", true},
            {"strong", "number", "Strong motor magnitude 0-1", true},
            {"duration", "number", "Vibration duration in seconds", false},
        });
        m["stop_input_gamepad_vibration"] = schema::build_schema({
            {"device", "integer", "Gamepad device index", true},
        });
}

} // namespace godot_autopilot
