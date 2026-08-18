#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_debug_sys(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["get_debugger_log"] = schema::build_schema({
            {"limit", "integer", "Maximum number of log entries to return (default: 50); the most recent entries are kept. The buffer always has data from the editor process engine logger, even without a running game", false},
        });
        m["get_debugger_errors"] = schema::build_schema({
            {"limit", "integer", "Maximum number of errors to return (default: 20); the result is a formatted text dump with timestamp, file:line and stack. Fetched from the running game over the runtime channel when a debug session is active, otherwise falls back to editor-process captured errors", false},
        });
        m["get_debugger_output"] = schema::build_schema({
            {"limit", "integer", "Maximum number of output entries to return (default: 50); fetched from the running game over the runtime channel when a debug session is active, otherwise falls back to editor-captured output. Use get_game_log_entries to read the game process log file", false},
        });
        m["get_debugger_stack_dump"] = schema::build_schema({});
        m["get_debugger_scene_tree"] = schema::build_schema({});
        m["get_debugger_monitors"] = schema::build_schema({
            {"count", "integer", "Number of recent monitor frames to return (default: 1); the capture has no data source, so the result is normally empty — use get_debug_monitors for live editor-side values", false},
        });
        m["get_debugger_session_info"] = schema::build_schema({});

        m["get_game_status"] = schema::build_schema({
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000); the response includes paused (SceneTree pause state) and physics_frame (physics frame counter) to distinguish a fake run", false},
        });
        m["execute_game_script"] = schema::build_schema({
            {"action", "string", "Action to run in the game process: 'script' (execute arbitrary GDScript), 'get_property', 'set_property' or 'call_method'", true},
            {"node_path", "string", "Node path for get_property/set_property/call_method (omit to use the current scene root)", false},
            {"property", "string", "Property name for get_property/set_property", false},
            {"value", "object", "Value to set for set_property", false},
            {"method", "string", "Method name for call_method; await methods are awaited to completion (bounded by timeout_ms) and return their final result", false},
            {"args", "array", "Arguments for call_method", false},
            {"source_code", "string", "GDScript source for action='script' — must extend Node and define func _run()", false},
            {"persist", "boolean", "Keep the script's temporary node alive after the call (default: false); the node is stored under /root/__gda_runtime and its path is returned in node_path for later get_property/call_method use", false},
            {"persist_name", "string", "Node name under /root/__gda_runtime when persist=true (default: auto-generated)", false},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000); on expiry an idempotent cancel interrupts the pending in-game await", false},
        });
        m["reload_game_scripts"] = schema::build_schema({
            {"paths", "array", "Non-empty array of res:// GDScript file paths to reload in the running game", true},
        });
        m["queue_game_input"] = schema::build_schema({
            {"type", "string", "Input event type: 'key' (keyboard), 'mouse_button' (mouse click) or 'action' (named InputMap action)", true},
            {"keycode", "string", "Key for type='key': numeric key code (e.g. 65) or key name (e.g. 'A', 'space', 'KEY_LEFT')", false},
            {"pressed", "boolean", "Pressed state (default: true)", false},
            {"button_index", "integer", "Mouse button index for type='mouse_button' (e.g. 1=left, 2=right, 3=middle)", false},
            {"position", "object", "Mouse position {x, y} for type='mouse_button'", false},
            {"action", "string", "Action name for type='action' (e.g. 'ui_accept'); transient states (is_action_just_pressed) are only visible in the next physics frame — use wait_game_input or get_game_input_status to observe effects. Transient states are not visible in _process (render frame)", false},
            {"duration_ms", "integer", "Hold duration: auto-release the input after this many ms (0/absent = no auto-release); only meaningful when pressed=true", false},
            {"mode", "string", "Injection mode: 'event' (default, via Input.parse_input_event), 'api' (via Input.action_press/action_release, immediate) or 'hold' (event-style injection that stays pressed for duration_ms)", false},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000)", false},
        });
        m["wait_game_input"] = schema::build_schema({
            {"action", "string", "Action name to wait on", true},
            {"state", "string", "Transient state to wait for: 'just_pressed' (default), 'just_released' or 'pressed'", false},
            {"inject", "object", "Inject an input before waiting, in the same call (fields mirror queue_game_input: type/action/keycode/pressed/mode/duration_ms/button_index/position) — eliminates the inject-then-observe cross-roundtrip frame gap; required for state=just_pressed/just_released, without inject the transient window (1 physics frame) has already expired and the wait will always time out. Transient states are not visible in _process (render frame)", false},
            {"timeout_ms", "integer", "Wait timeout in milliseconds (default: 2000, max: 30000)", false},
        });
        m["get_game_input_status"] = schema::build_schema({
            {"action", "string", "Action name to query; the response includes paused and physics_frame so transient input consumption can be diagnosed when the game is paused", true},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000); successful responses also include recent_engine_errors (up to 5 recent engine errors) when any exist", false},
        });
        m["capture_game_viewport"] = schema::build_schema({
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000); the capture is returned as a base64-encoded PNG", false},
        });

        m["get_game_log_entries"] = schema::build_schema({
            {"limit", "integer", "Maximum number of tail log entries to return (default: 50, max: 500); the result also includes path (log file path) and total_lines (full file line count). The log is written by the running game process — start the game first with play_editor_current_scene", false},
        });

        m["print_debug_log"] = schema::build_schema({
            {"message", "string", "Debug text to record in the plugin log at Info level; the message does not reach the game log", true},
        });
        m["get_debug_stack"] = schema::build_schema({
            {"include_variables", "boolean", "Include variable names in the dump: global_variables per backtrace, local_variables and member_variables per frame (default: false)", false},
        });
        m["get_debug_monitor"] = schema::build_schema({
            {"monitor", "number", "Performance monitor ID (integer 0-58); list valid ids with get_debug_monitor_catalog first. Out-of-range ids return an error", true},
        });
        m["get_debug_monitor_catalog"] = schema::build_schema({});
        m["get_debug_object_count"] = schema::build_schema({});
        m["get_debug_memory_usage"] = schema::build_schema({});
        m["set_debug_physics_fps"] = schema::build_schema({
            {"fps", "number", "Physics ticks per second (integer; Godot default is 60, typical debug values 10-60). Applies immediately and globally to the engine but is not persisted — restore 60 when done", true},
        });
        m["set_debug_collision_visual"] = schema::build_schema({
            {"enabled", "boolean", "True to show collision shapes during editor game runs, false to hide them; persisted in project.godot via project metadata until changed back", true},
        });
        m["set_debug_navigation_visual"] = schema::build_schema({
            {"enabled", "boolean", "True to show navigation geometry during editor game runs, false to hide it; persisted in project.godot via project metadata until changed back", true},
        });
        m["set_debug_performance_visual"] = schema::build_schema({
            {"enabled", "boolean", "True to show the performance debug overlay during editor game runs, false to hide it; persisted in project.godot via project metadata until changed back", true},
        });
        m["get_debug_monitors"] = schema::build_schema({});
        m["remove_debug_custom_monitor"] = schema::build_schema({
            {"id", "string", "Name of the custom monitor to remove, as registered via Performance.add_custom_monitor by the game or scripts", true},
        });
        m["get_debug_custom_monitor"] = schema::build_schema({
            {"id", "string", "Name of the custom monitor to read, as registered via Performance.add_custom_monitor; the value is serialized as JSON Variant", true},
        });
        m["get_debug_custom_monitor_names"] = schema::build_schema({});
        m["get_debug_node_count"] = schema::build_schema({});

        m["show_os_alert"] = schema::build_schema({
            {"text", "string", "Message to display in the modal alert body; the call blocks the editor UI until the user confirms — avoid in unattended runs", true},
            {"title", "string", "Dialog window title (default: Alert!)", false},
        });
        m["create_os_process"] = schema::build_schema({
            {"path", "string", "Executable to start (full path, or name resolved via PATH)", true},
            {"arguments", "array", "Command line arguments passed to the process; non-string entries are ignored", true},
        });
        m["execute_os_process"] = schema::build_schema({
            {"path", "string", "Executable to run (full path, or name resolved via PATH)", true},
            {"arguments", "array", "Command line arguments; non-string entries are ignored", true},
            {"output", "boolean", "Capture stdout and stderr into the result (default: false); when false, output is discarded. The call blocks until the command exits", false},
        });
        m["get_os_datetime"] = schema::build_schema({
            {"utc", "boolean", "Return UTC date/time instead of local time (default: false); the result dictionary contains year, month, day, weekday, hour, minute, second, dst and related fields", false},
        });
        m["get_os_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name to read from the editor process environment; returns an empty string when unset", true},
        });
        m["get_os_locale"] = schema::build_schema({});
        m["get_os_system_fonts"] = schema::build_schema({});
        m["get_os_system_info"] = schema::build_schema({});
        m["get_os_unique_id"] = schema::build_schema({});
        m["get_os_unix_time"] = schema::build_schema({});
        m["get_os_user_data_dir"] = schema::build_schema({});
        m["kill_os_process"] = schema::build_schema({
            {"pid", "integer", "Process ID to kill, as returned by create_os_process; killing is immediate and unsaved data in the target process is lost", true},
        });
        m["move_os_file_to_trash"] = schema::build_schema({
            {"path", "string", "File or folder path to move to the system trash (recycle bin)", true},
        });
        m["set_os_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name to set in the editor process environment; the change is session-only and not persisted", true},
            {"value", "string", "Environment variable value", true},
        });
        m["open_os_path"] = schema::build_schema({
            {"uri", "string", "URL or file path to open with the default application (browser, file manager, etc.)", true},
        });

        m["get_docs_class"] = schema::build_schema({
            {"class", "string", "Godot class name (e.g. 'Node2D', 'TileMap'); returns reflected signature info with no docstrings", true},
        });
        m["find_docs_class"] = schema::build_schema({
            {"query", "string", "Search text matched case-insensitively against class names (e.g. 'node' matches Node, Node2D, SceneTree); returns up to 50 matches", true},
        });
        m["get_docs_method"] = schema::build_schema({
            {"class", "string", "Godot class name (e.g. 'Node2D')", true},
            {"method", "string", "Method name to look up (e.g. 'queue_free'); returns the reflected signature without docstrings", true},
        });
        m["get_docs_property"] = schema::build_schema({
            {"class", "string", "Godot class name (e.g. 'Node2D')", true},
            {"property", "string", "Property name to look up (e.g. 'position'); returns the reflected property info without docstrings", true},
        });

        m["batch_execute"] = schema::build_schema({
            {"operations", "array", "Ordered list of operations to execute", true},
            {"stop_on_error", "boolean", "Stop on first error (default: true)", false},
        });

        m["call_tool"] = schema::build_schema({
            {"name", "string", "Tool name to execute", true},
            {"arguments", "object", "Tool arguments as JSON object", false},
        });

        m["code_execute"] = schema::build_schema({
            {"source_code", "string", "GDScript source code", true},
            {"function_name", "string", "Function name to call (default: _run)", false},
            {"timeout_ms", "integer", "Execution timeout in milliseconds (max 30000)", false},
            {"auto_owner", "boolean", "Automatically set owner on nodes created during execution so they are saved with the scene (default true)", false},
        });
}

} // namespace godot_autopilot
