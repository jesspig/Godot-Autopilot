#ifndef GODOT_AUTOPILOT_OS_OPS_HPP
#define GODOT_AUTOPILOT_OS_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace os_ops {

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
mcp::JsonValue handle_os_move_to_trash(const mcp::JsonValue &args);
mcp::JsonValue handle_os_set_environment(const mcp::JsonValue &args);
mcp::JsonValue handle_os_shell_open(const mcp::JsonValue &args);

} // namespace os_ops
} // namespace godot_autopilot
#endif
