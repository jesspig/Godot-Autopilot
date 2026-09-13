#ifndef GODOT_AUTOPILOT_DEBUGGER_OPS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_OPS_HPP

#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <algorithm>
#include <atomic>
#include <mcp/JsonValue.hpp>
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace debugger_ops {

class OutputCaptureLogger : public godot::Logger {
  GDCLASS(OutputCaptureLogger, Logger)
protected:
  static void _bind_methods() {}

public:
  void _log_error(
      const godot::String &p_function, const godot::String &p_file,
      int32_t p_line, const godot::String &p_code,
      const godot::String &p_rationale, bool p_editor_notify,
      int32_t p_error_type,
      const godot::TypedArray<godot::Ref<godot::ScriptBacktrace>>
          &p_script_backtraces) override;
  void _log_message(const godot::String &p_message, bool p_error) override;
};

class DebugCapturePlugin : public godot::EditorDebuggerPlugin {
  GDCLASS(DebugCapturePlugin, EditorDebuggerPlugin)
protected:
  static void _bind_methods() {}
  godot::Ref<godot::EditorDebuggerSession> session_;

public:
  static std::atomic<DebugCapturePlugin *> s_instance;
  bool _has_capture(const godot::String &p_name) const override;
  bool _capture(const godot::String &p_message, const godot::Array &p_data,
                int32_t p_session_id) override;
  void _setup_session(int32_t p_session_id) override;
  godot::Ref<godot::EditorDebuggerSession> get_session_ref() const {
    return session_;
  }
  static DebugCapturePlugin *get_instance() {
    return s_instance.load(std::memory_order_relaxed);
  }
  std::vector<int32_t> get_session_ids() const {
    std::lock_guard<std::mutex> lock(session_mtx_);
    return session_ids_;
  }
  bool has_session(int32_t id) const {
    std::lock_guard<std::mutex> lock(session_mtx_);
    return std::find(session_ids_.begin(), session_ids_.end(), id) !=
           session_ids_.end();
  }
  bool is_session_ready(int32_t id) const {
    std::lock_guard<std::mutex> lock(session_mtx_);
    return std::find(ready_session_ids_.begin(), ready_session_ids_.end(),
                     id) != ready_session_ids_.end();
  }

private:
  std::vector<int32_t> session_ids_;
  std::vector<int32_t> ready_session_ids_;
  mutable std::mutex session_mtx_;
};

void capture_add_log_entry(const std::string &text, bool is_error);
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

::godot::Ref<OutputCaptureLogger> create_output_logger();
::godot::Ref<DebugCapturePlugin> create_debug_plugin();

mcp::JsonValue handle_output_get_log(const mcp::JsonValue &args);
mcp::JsonValue handle_plugin_log_get(const mcp::JsonValue &args);
mcp::JsonValue handle_debugger_get_errors(const mcp::JsonValue &args);
mcp::JsonValue handle_debugger_get_output(const mcp::JsonValue &args);
mcp::JsonValue handle_debugger_get_scene_tree(const mcp::JsonValue &args);
mcp::JsonValue handle_debugger_get_session_info(const mcp::JsonValue &args);

void register_classes();

} // namespace debugger_ops
} // namespace godot_autopilot
#endif