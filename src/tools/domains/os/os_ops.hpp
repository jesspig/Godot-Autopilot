#ifndef GODOT_AUTOPILOT_OS_OPS_HPP
#define GODOT_AUTOPILOT_OS_OPS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_autopilot {
namespace os_ops {

mcp::JsonValue handle_os_process(const mcp::JsonValue &args);
mcp::JsonValue handle_os_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_os_time(const mcp::JsonValue &args);
mcp::JsonValue handle_os_system_info(const mcp::JsonValue &args);
mcp::JsonValue handle_os_has_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_os_alert(const mcp::JsonValue &args);
mcp::JsonValue handle_os_create_process(const mcp::JsonValue &args);
mcp::JsonValue handle_os_execute(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_datetime(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_locale(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_system_fonts(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_system_info(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_unique_id(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_unix_time(const mcp::JsonValue &args);
mcp::JsonValue handle_os_get_user_data_dir(const mcp::JsonValue &args);
mcp::JsonValue handle_os_kill(const mcp::JsonValue &args);
mcp::JsonValue handle_os_process_status(const mcp::JsonValue &args);
mcp::JsonValue handle_os_process_exit_code(const mcp::JsonValue &args);
mcp::JsonValue handle_os_process_is_running(const mcp::JsonValue &args);
mcp::JsonValue handle_os_memory_info(const mcp::JsonValue &args);
mcp::JsonValue handle_os_static_memory(const mcp::JsonValue &args);
mcp::JsonValue handle_os_video_adapter(const mcp::JsonValue &args);
mcp::JsonValue handle_os_cmdline_args(const mcp::JsonValue &args);
mcp::JsonValue handle_os_move_to_trash(const mcp::JsonValue &args);
mcp::JsonValue handle_os_set_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_os_shell_open(const mcp::JsonValue &args);
mcp::JsonValue wait_os_process(int64_t wait_id, int64_t timeout_ms);
int64_t start_csharp_build(const godot::String &path,
                           const godot::PackedStringArray &build_args);

} // namespace os_ops
} // namespace godot_autopilot
#endif
