#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_debug_sys(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["output_get_log"] = schema::build_schema({
            {"limit", "integer", "Maximum number of log entries to return (default: 50)", false},
        });
        m["debugger_get_errors"] = schema::build_schema({
            {"limit", "integer", "Maximum number of errors to return (default: 20)", false},
        });
        m["debugger_get_output"] = schema::build_schema({
            {"limit", "integer", "Maximum number of output entries to return (default: 50); fetched from the running game over the runtime channel when a debug session is active", false},
        });
        m["debugger_get_stack_dump"] = schema::build_schema({});
        m["debugger_get_scene_tree"] = schema::build_schema({});
        m["debugger_get_monitors"] = schema::build_schema({
            {"count", "integer", "Number of recent monitor frames to return (default: 1)", false},
        });
        m["debugger_get_session_info"] = schema::build_schema({});

        m["game_status"] = schema::build_schema({
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000), 返回字段含 paused（SceneTree 暂停状态）与 physics_frame（物理帧计数），用于区分假运行", false},
        });
        m["game_eval"] = schema::build_schema({
            {"action", "string", "Action to run in the game process: \"script\" (execute arbitrary GDScript), \"get_property\", \"set_property\", \"call_method\"", true},
            {"node_path", "string", "Node path for get_property/set_property/call_method (omit to use the current scene root)", false},
            {"property", "string", "Property name for get_property/set_property", false},
            {"value", "object", "Value to set for set_property", false},
            {"method", "string", "Method name for call_method", false},
            {"args", "array", "Arguments for call_method", false},
            {"source_code", "string", "GDScript source for action=\"script\" — must extend Node and define func _run()", false},
            {"persist", "boolean", "Keep the script's temporary node alive after the call (default: false); the node is stored under /root/__gda_runtime and its path is returned in node_path for later get_property/call_method use", false},
            {"persist_name", "string", "Node name under /root/__gda_runtime when persist=true (default: auto-generated)", false},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000)", false},
        });
        m["game_input"] = schema::build_schema({
            {"type", "string", "Input event type: \"key\", \"mouse_button\" or \"action\"", true},
            {"keycode", "string", "Key for type=\"key\": numeric key code (e.g. 65) or key name (e.g. \"A\", \"space\", \"KEY_LEFT\")", false},
            {"pressed", "boolean", "Pressed state (default: true)", false},
            {"button_index", "integer", "Mouse button index for type=\"mouse_button\" (e.g. 1=left, 2=right, 3=middle)", false},
            {"position", "object", "Mouse position {x, y} for type=\"mouse_button\"", false},
            {"action", "string", "Action name; transient states (is_action_just_pressed) are only visible in the next physics frame — use game_input wait/status ops or auto-release (duration_ms) to observe effects", false},
            {"duration_ms", "integer", "Hold duration: auto-release the input after this many ms (0/absent = no auto-release); only meaningful when pressed=true", false},
            {"mode", "string", "Injection mode for type=action: \"event\" (default, via Input.parse_input_event) or \"api\" (via Input.action_press/action_release, immediate)", false},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000)", false},
        });
        m["game_input_wait"] = schema::build_schema({
            {"action", "string", "Action name to wait on", true},
            {"state", "string", "Transient state to wait for: \"just_pressed\" (default), \"just_released\" or \"pressed\"", false},
            {"inject", "object", "Inject an input before waiting, in the same call (fields mirror game_input: type/action/keycode/pressed/mode/duration_ms/button_index/position) — eliminates the inject-then-observe cross-roundtrip frame gap; required for state=just_pressed/just_released, without inject the transient window (1 physics frame) has already expired and the wait will always time out", false},
            {"timeout_ms", "integer", "Wait timeout in milliseconds (default: 2000, max: 30000)", false},
        });
        m["game_input_status"] = schema::build_schema({
            {"action", "string", "Action name to query, 返回字段含 paused 与 physics_frame，用于判断瞬态输入是否因暂停而无法被消费", true},
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000)", false},
        });
        m["game_capture"] = schema::build_schema({
            {"timeout_ms", "integer", "Response timeout in milliseconds (default: 5000, max: 30000)", false},
        });

        m["log_get_game_entries"] = schema::build_schema({
            {"limit", "integer", "Maximum number of log entries to return (default: 50)", false},
        });

        m["debug_print"] = schema::build_schema({
            {"message", "string", "Debug log message to print", true},
        });
        m["debug_print_stack"] = schema::build_schema({
            {"include_variables", "boolean", "Include script local/member variables (default: false)", false},
        });
        m["debug_get_performance_monitor"] = schema::build_schema({
            {"monitor", "number", "Performance monitor ID (integer 0-58, see debug_list_performance_monitors)", true},
        });
        m["debug_list_performance_monitors"] = schema::build_schema({});
        m["debug_get_object_count"] = schema::build_schema({});
        m["debug_get_object_count_by_class"] = schema::build_schema({});
        m["debug_get_memory_usage"] = schema::build_schema({});
        m["debug_profile_start"] = schema::build_schema({});
        m["debug_profile_stop"] = schema::build_schema({});
        m["debug_profile_get_data"] = schema::build_schema({});
        m["debug_set_fps_limit"] = schema::build_schema({
            {"fps", "number", "Maximum FPS limit to set", true},
        });
        m["debug_set_physics_fps"] = schema::build_schema({
            {"fps", "number", "Physics ticks per second to set", true},
        });
        m["debug_collision_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable collision debug visualization", true},
        });
        m["debug_navigation_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable navigation debug visualization", true},
        });
        m["debug_performance_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable performance debug overlay", true},
        });
        m["debug_get_all_monitors"] = schema::build_schema({});
        m["debug_add_custom_monitor"] = schema::build_schema({});
        m["debug_remove_custom_monitor"] = schema::build_schema({
            {"id", "string", "Custom monitor name to remove", true},
        });
        m["debug_get_custom_monitor"] = schema::build_schema({
            {"id", "string", "Custom monitor name to read", true},
        });
        m["debug_list_custom_monitors"] = schema::build_schema({});
        m["debug_query_object_count"] = schema::build_schema({});
        m["debug_query_memory_usage"] = schema::build_schema({});
        m["debug_query_node_count"] = schema::build_schema({});

        m["os_alert"] = schema::build_schema({
            {"text", "string", "Alert message text", true},
            {"title", "string", "Alert dialog title (default: Alert!)", false},
        });
        m["os_create_process"] = schema::build_schema({
            {"path", "string", "Executable path to start", true},
            {"arguments", "array", "Array of command line argument strings", true},
        });
        m["os_execute"] = schema::build_schema({
            {"path", "string", "Executable path to run", true},
            {"arguments", "array", "Array of command line argument strings", true},
            {"output", "boolean", "Capture stdout/stderr (default: false)", false},
        });
        m["os_get_datetime"] = schema::build_schema({
            {"utc", "boolean", "Return UTC time instead of local (default: false)", false},
        });
        m["os_get_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name", true},
        });
        m["os_get_locale"] = schema::build_schema({});
        m["os_get_system_fonts"] = schema::build_schema({});
        m["os_get_system_info"] = schema::build_schema({});
        m["os_get_unique_id"] = schema::build_schema({});
        m["os_get_unix_time"] = schema::build_schema({});
        m["os_get_user_data_dir"] = schema::build_schema({});
        m["os_kill"] = schema::build_schema({
            {"pid", "integer", "Process ID to kill", true},
        });
        m["os_move_to_trash"] = schema::build_schema({
            {"path", "string", "File or folder path to move to trash", true},
        });
        m["os_set_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name", true},
            {"value", "string", "Environment variable value", true},
        });
        m["os_shell_open"] = schema::build_schema({
            {"uri", "string", "URL or file path to open with the default application", true},
        });

        m["doc_get_class"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
        });
        m["doc_search"] = schema::build_schema({
            {"query", "string", "Search text to match against class names", true},
        });
        m["doc_get_method"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
            {"method", "string", "Method name to look up documentation for", true},
        });
        m["doc_get_property"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
            {"property", "string", "Property name to look up documentation for", true},
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
