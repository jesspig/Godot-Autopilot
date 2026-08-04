#ifndef GODOT_SELF_DRIVING_DEBUGGER_OPS_HPP
#define GODOT_SELF_DRIVING_DEBUGGER_OPS_HPP

#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot {

class OutputCaptureLogger : public Logger {
    GDCLASS(OutputCaptureLogger, Logger)
protected:
    static void _bind_methods() {}
public:
    void _log_error(const String& p_function, const String& p_file,
        int32_t p_line, const String& p_code, const String& p_rationale,
        bool p_editor_notify, int32_t p_error_type,
        const TypedArray<Ref<ScriptBacktrace>>& p_script_backtraces) override;
    void _log_message(const String& p_message, bool p_error) override;
};

class DebugCapturePlugin : public EditorDebuggerPlugin {
    GDCLASS(DebugCapturePlugin, EditorDebuggerPlugin)
protected:
    static void _bind_methods() {}
    Ref<EditorDebuggerSession> session_;
public:
    std::vector<int32_t> session_ids_;
    static DebugCapturePlugin* s_instance;
    bool _has_capture(const String& p_name) const override;
    bool _capture(const String& p_message, const Array& p_data, int32_t p_session_id) override;
    void _setup_session(int32_t p_session_id) override;
    Ref<EditorDebuggerSession> get_session_ref() const { return session_; }
    static DebugCapturePlugin* get_instance() { return s_instance; }
};

} // namespace godot

namespace godot_self_driving {
namespace debugger_ops {

void capture_add_log_entry(const std::string& text, bool is_error);
std::string capture_get_log_text(size_t limit);
std::string capture_get_errors_text(size_t limit);
std::string capture_get_game_output_text(size_t limit);
std::string capture_get_stack_dump_text();
std::string capture_get_scene_tree_text();
std::string capture_get_monitors_text(size_t count);
std::string capture_get_session_info_text();
bool capture_session_active();
bool capture_session_breaked();
size_t capture_log_count();
std::string capture_new_error_text(size_t since_count);
std::string capture_new_output_text(size_t since_count);

::godot::Ref<::godot::OutputCaptureLogger> create_output_logger();
::godot::Ref<::godot::DebugCapturePlugin> create_debug_plugin();

mcp::JsonValue handle_output_get_log(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_errors(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_output(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_stack_dump(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_scene_tree(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_monitors(const mcp::JsonValue& args);
mcp::JsonValue handle_debugger_get_session_info(const mcp::JsonValue& args);

void register_classes();

} // namespace debugger_ops
} // namespace godot_self_driving
#endif