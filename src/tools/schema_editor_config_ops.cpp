#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_editor_config(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["get_editor_selection"] = schema::build_schema({});
        m["set_editor_selection"] = schema::build_schema({
            {"paths", "array", "Array of node paths in the edited scene to select, e.g. ['Player', 'Level1/Enemy'] or absolute like '/root/Level1/Enemy'; the previous selection is cleared first and unresolvable paths are skipped", true},
        });
        m["get_editor_edited_scene_root"] = schema::build_schema({});
        m["save_editor_scene"] = schema::build_schema({});
        m["save_editor_scenes"] = schema::build_schema({});
        m["reload_editor_scene"] = schema::build_schema({
            {"scene_path", "string", "Scene file path to reload from disk, e.g. 'res://game.tscn'; when omitted the currently edited scene is reloaded. All unsaved changes are discarded without confirmation", false},
        });
        m["inspect_editor_resource"] = schema::build_schema({
            {"resource_path", "string", "Path to a resource file to open in the editor's Inspector panel, e.g. 'res://player.tres'; the file is loaded via ResourceLoader and errors when missing or unloadable", true},
        });
        m["create_editor_undo_redo_action"] = schema::build_schema({
            {"name", "string", "Human-readable name shown in the editor's undo history, e.g. 'Move player'; group related changes under one name so they revert together", true},
        });
        m["commit_editor_undo_redo"] = schema::build_schema({});
        m["add_editor_undo_redo_do"] = schema::build_schema({
            {"node_path", "string", "Path to the node whose method will be called on redo, e.g. 'Player' or '/root/Player'; resolved against the edited scene root", true},
            {"method", "string", "Name of the node method to call, e.g. 'set_position' or 'queue_free'", true},
            {"value", "object", "Optional serialized JSON value passed as the single method argument; omit for methods without arguments", false},
        });
        m["add_editor_undo_redo_undo"] = schema::build_schema({
            {"node_path", "string", "Path to the node whose method will be called on undo, e.g. 'Player' or '/root/Player'; resolved against the edited scene root", true},
            {"method", "string", "Name of the node method to call for the inverse operation, e.g. 'set_position'", true},
            {"value", "object", "Optional serialized JSON value passed as the single method argument; omit for methods without arguments", false},
        });
        m["get_editor_file_system_tree"] = schema::build_schema({
            {"path", "string", "Directory path to root the tree at, e.g. 'res://scenes' or 'res://'; when omitted the whole project tree is returned. An invalid path returns null", false},
        });
        m["scan_editor_file_system"] = schema::build_schema({});
        m["set_editor_main_scene"] = schema::build_schema({
            {"path", "string", "Path of the scene to launch when the project runs, e.g. 'res://game.tscn'; persisted to project.godot", true},
        });
        m["play_editor_current_scene"] = schema::build_schema({});
        m["stop_editor_playing"] = schema::build_schema({});
        m["get_editor_file_system_status"] = schema::build_schema({});
        m["set_editor_plugin_enabled"] = schema::build_schema({
            {"plugin", "string", "Name of the editor plugin to enable or disable, e.g. 'godot_autopilot'", true},
            {"enabled", "boolean", "true activates the plugin, false deactivates it", true},
        });
        m["create_editor_scene"] = schema::build_schema({
            {"type", "string", "Godot class name of the root node to create, e.g. 'Node2D' or 'Node3D'; must be a Node subclass, defaults to 'Node'", false},
            {"name", "string", "Name of the new root node, defaults to 'NewRoot'", false},
            {"close_current", "boolean", "Close the current scene tab first if it has no unsaved changes (default: false; errors if the current scene is unsaved). Only the current tab is closed: with multiple scene tabs open, repeat close_editor_scene until no scene is open, otherwise create_editor_scene fails fast without entering the switch wait", false},
            {"timeout_ms", "integer", "Maximum time in milliseconds to wait for the editor to observe the new scene root after add_root_node (default: 2000, min: 50, max: 30000); on timeout the call errors with waited_ms, timeout_ms, node_released and editor_state diagnostics", false},
        });
        m["open_editor_scene"] = schema::build_schema({
            {"path", "string", "Path to the scene file to open, e.g. 'res://game.tscn'; errors when the current scene has unsaved changes", true},
        });
        m["save_editor_scene_as"] = schema::build_schema({
            {"path", "string", "Target file path for the save, e.g. 'res://levels/level1.tscn'; missing parent directories are created automatically", true},
        });
        m["close_editor_scene"] = schema::build_schema({});

        m["get_editor_viewport_geometry"] = schema::build_schema({
            {"viewport", "string", "Viewport to report: '2d' (default) or '3d'", false},
            {"index", "integer", "3D viewport index, 0-3 (default: 0); ignored when viewport='2d'", false},
        });
        m["get_editor_ui_elements"] = schema::build_schema({
            {"query", "string", "Case-insensitive substring filter matched against path, name, text, tooltip and placeholder", false},
            {"type_filter", "string", "Exact class name to restrict results to, e.g. 'Button' or 'LineEdit'", false},
            {"interactive_only", "boolean", "Keep only actionable controls such as buttons, line edits, trees and sliders (default: false)", false},
            {"max_elements", "integer", "Maximum number of elements to return, an integer from 1 to 1000 (default: 100); the result reports truncated when the cap is hit", false},
        });
        m["hit_test_editor_point"] = schema::build_schema({
            {"position", "object", "Point to test, object with numeric x and y fields, in pixels relative to the client area of window_id", true},
            {"window_id", "integer", "Window to test against (default: 0 = main editor window), e.g. an id from get_editor_ui_elements", false},
            {"max_results", "integer", "Maximum number of hit controls to return, an integer from 1 to 64 (default: 10)", false},
            {"include_scene_nodes", "boolean", "Append scene context (default: false): scene_tree_item, the scene-tree row under the point when a row is hit, and scene_nodes, the topmost-first scene node paths with name/type under the point inside the 2D editor viewport, plus scene_node_count; scene_tree_error/scene_nodes_error report unavailable context", false},
        });
        m["scene_tree_items"] = schema::build_schema({
            {"filter", "string", "Case-insensitive substring filter matched against each row's scene-relative path or name", false},
            {"selected_only", "boolean", "Keep only rows currently selected in the scene tree (default: false)", false},
            {"max_items", "integer", "Maximum number of rows to return, an integer from 1 to 1000 (default: 200); the result reports truncated when the cap is hit", false},
        });
        m["select_scene_tree_node"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene: scene-relative such as 'Player' or 'Level1/Enemy', or absolute such as '/root/Level1/Enemy'; the previous selection is cleared first unless add is true", true},
            {"add", "boolean", "true appends the node to the current selection instead of replacing it (default: false)", false},
            {"inspect", "boolean", "Open the node in the Inspector through EditorInterface.edit_node (default: true)", false},
            {"focus", "boolean", "Scroll the scene tree to the node row when the row is found (default: true)", false},
        });
        m["click_editor_element"] = schema::build_schema({
            {"path", "string", "Element path as returned by get_editor_ui_elements or hit_test_editor_point", true},
            {"button", "string", "Mouse button to click: left, right or middle (default: left)", false},
            {"double_click", "boolean", "Send a second press/release pair carrying the double-click flag (default: false)", false},
            {"warp", "boolean", "Warp the physical cursor onto the element center before clicking (default: true)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["type_editor_element_text"] = schema::build_schema({
            {"path", "string", "Element path as returned by get_editor_ui_elements or hit_test_editor_point", true},
            {"text", "string", "Text to write into the focused element; may contain any UTF-8 text", true},
            {"submit", "boolean", "Append an Enter key press and release after the text (default: false)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["run_editor_shortcut"] = schema::build_schema({
            {"shortcut", "string", "Shortcut string with '+'-separated modifiers (ctrl/control, shift, alt, meta/super) followed by the main key, e.g. 'ctrl+s', 'ctrl+shift+z' or 'f5'; the key accepts the input tool key names plus F1-F12", true},
        });

        m["build_csharp_assembly"] = schema::build_schema({});

        m["get_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name, e.g. display/window/size/viewport_width; settings come from project.godot", true},
            {"default", "object", "Serialized JSON value returned when the setting does not exist; otherwise a null value is returned", false},
        });
        m["set_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name, e.g. application/config/name; must already exist or be added by this call", true},
            {"value", "object", "Serialized JSON value; when a string, the engine keeps the existing setting's type if one is stored, e.g. 1920 for an int setting", true},
        });
        m["has_project_settings"] = schema::build_schema({
            {"name", "string", "Project setting name to check for existence in project.godot", true},
        });
        m["save_project_settings"] = schema::build_schema({});
        m["get_engine_version"] = schema::build_schema({});
        m["get_engine_fps"] = schema::build_schema({});
        m["get_engine_frames_drawn"] = schema::build_schema({});
        m["set_engine_time_scale"] = schema::build_schema({
            {"scale", "number", "Time scale multiplier: 1.0 = real time, 0.5 = half speed, 2.0 = double speed; 0.0 stops gameplay processing", true},
        });
        m["get_engine_time_scale"] = schema::build_schema({});
        m["set_engine_max_fps"] = schema::build_schema({
            {"fps", "integer", "Frame rate cap in frames per second; 0 disables the cap (uncapped)", true},
        });
        m["get_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name, e.g. interface/theme/base_color; settings come from the editor's own preferences. Alongside 'result' (the raw value, unchanged) the response carries the property list metadata: 'hint' (integer PropertyHint) plus 'hint_string' when non-empty; enum settings (hint 2) also include 'enum_options', an array of {'label', 'value'} entries that decodes bare integers such as run/window_placement/game_embed_mode", true},
        });
        m["set_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name to write, e.g. interface/theme/base_color", true},
            {"value", "object", "Serialized JSON value to store; editor settings persist automatically", true},
        });
        m["has_editor_settings"] = schema::build_schema({
            {"name", "string", "Editor setting name to check for existence in the editor's preferences", true},
        });

        m["add_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier, e.g. 'mario_jump' or 'move_left'; a programmatic identifier, not a display name", true},
            {"deadzone", "number", "Deadzone 0.0 to 1.0, the analog threshold below which input is ignored (default: 0.5)", false},
        });
        m["erase_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to remove, e.g. 'mario_jump'; erased from the editor project InputMap and its persisted setting", true},
        });
        m["get_input_map_actions"] = schema::build_schema({});
        m["has_input_map_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to check for existence, e.g. 'ui_accept'", true},
        });
        m["add_input_map_action_event"] = schema::build_schema({
            {"action", "string", "Action name identifier to bind the event to, e.g. 'mario_jump'", true},
            {"event", "object", "Input event object with a concrete 'class' field, e.g. 'InputEventKey' with 'keycode' 65; keycode and physical_keycode accept numeric key codes or KEY_* names like 'KEY_A'", true},
        });
        m["erase_input_map_action_event"] = schema::build_schema({
            {"action", "string", "Action name identifier whose event to remove, e.g. 'mario_jump'", true},
            {"event_index", "integer", "Zero-based index of the event to remove within the action's event list", true},
        });
        m["set_input_map_action_deadzone"] = schema::build_schema({
            {"action", "string", "Action name identifier to set the deadzone for, e.g. 'mario_jump'", true},
            {"deadzone", "number", "Deadzone 0.0 to 1.0, the analog threshold below which input is ignored", true},
        });
        m["save_input_map"] = schema::build_schema({
            {"actions", "array", "Optional whitelist of action names to persist; when omitted, actions prefixed with 'ui_' or containing '/' are skipped", false},
        });

        m["press_input_action"] = schema::build_schema({
            {"action", "string", "Input action name to press, e.g. 'ui_accept'", true},
            {"strength", "number", "Analog strength 0.0 to 1.0 (default: 1.0)", false},
        });
        m["release_input_action"] = schema::build_schema({
            {"action", "string", "Input action name to release, e.g. 'ui_accept'", true},
        });
        m["is_input_action_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check, e.g. 'ui_accept'; true while the action is held down", true},
        });
        m["is_input_action_just_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check, e.g. 'ui_accept'; true only on the frame in which the press is injected", true},
        });
        m["press_input_key"] = schema::build_schema({
            {"key", "string", "Key to press: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP, DOWN; other values return 'invalid key'", true},
        });
        m["release_input_key"] = schema::build_schema({
            {"key", "string", "Key to release: a single character A-Z or 0-9, or SPACE, ENTER, ESCAPE, SHIFT, CTRL, CONTROL, ALT, TAB, BACKSPACE, DELETE, LEFT, RIGHT, UP, DOWN; other values return 'invalid key'", true},
        });
        m["move_input_mouse"] = schema::build_schema({
            {"position", "object", "Mouse position, object with numeric x and y fields", true},
            {"relative", "object", "Relative movement delta since the last event, object with numeric x and y fields (default: 0, 0)", false},
        });
        m["press_input_mouse_button"] = schema::build_schema({
            {"button", "string", "Mouse button to press: left, right or middle only", true},
            {"position", "object", "Position of the press, object with numeric x and y fields (default: 0, 0)", false},
        });
        m["release_input_mouse_button"] = schema::build_schema({
            {"button", "string", "Mouse button to release: left, right or middle only", true},
            {"position", "object", "Position of the release, object with numeric x and y fields (default: 0, 0)", false},
        });
        m["click_input_mouse"] = schema::build_schema({
            {"position", "object", "Click position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
            {"button", "string", "Mouse button to click: left, right or middle (default: left)", false},
            {"double_click", "boolean", "Send a second press/release pair to form a double click (default: false)", false},
            {"warp", "boolean", "Warp the physical cursor onto the position before clicking (default: true)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["scroll_input_mouse"] = schema::build_schema({
            {"direction", "string", "Wheel direction: up, down, left or right", true},
            {"position", "object", "Pointer position the wheel event is routed to, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
            {"amount", "integer", "Number of wheel detents, an integer from 1 to 10 (default: 1)", false},
            {"warp", "boolean", "Warp the physical cursor onto the position first (default: true)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["drag_input_mouse"] = schema::build_schema({
            {"from", "object", "Drag start position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
            {"to", "object", "Drag end position, object with numeric x and y fields, in pixels relative to the client area of the focused window", true},
            {"button", "string", "Mouse button to drag with: left, right or middle (default: left)", false},
            {"steps", "integer", "Number of interpolated motion events between from and to, an integer from 1 to 64 (default: 8)", false},
            {"warp", "boolean", "Warp the physical cursor onto the start position first (default: true)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["type_input_text"] = schema::build_schema({
            {"text", "string", "Text to write into the focused control; may contain any UTF-8 text", true},
            {"submit", "boolean", "Append an Enter key press and release after the text (default: false)", false},
            {"observe", "boolean", "Append a fresh editor viewport capture to the result (default: false)", false},
        });
        m["start_input_gamepad_vibration"] = schema::build_schema({
            {"device", "integer", "Gamepad device index (default: 0)", true},
            {"weak", "number", "Weak motor magnitude 0.0 to 1.0 (default: 0.5)", true},
            {"strong", "number", "Strong motor magnitude 0.0 to 1.0 (default: 0.5)", true},
            {"duration", "number", "Vibration duration in seconds; 0 (default) vibrates indefinitely until stop_input_gamepad_vibration", false},
        });
        m["stop_input_gamepad_vibration"] = schema::build_schema({
            {"device", "integer", "Gamepad device index (default: 0)", true},
        });
}

} // namespace godot_autopilot
