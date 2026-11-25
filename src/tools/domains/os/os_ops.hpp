#ifndef GODOT_AUTOPILOT_OS_OPS_HPP
#define GODOT_AUTOPILOT_OS_OPS_HPP

#include <cstdint>
#include "util/json_value.hpp"
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot_autopilot {
namespace os_ops {

godot_autopilot::util::JsonValue handle_os_process(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_time(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_system_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_has_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_alert(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_create_process(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_execute(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_datetime(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_locale(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_system_fonts(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_system_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_unique_id(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_unix_time(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_get_user_data_dir(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_kill(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_process_status(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_process_exit_code(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_process_is_running(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_memory_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_static_memory(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_video_adapter(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_cmdline_args(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_move_to_trash(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_set_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_shell_open(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue wait_os_process(int64_t wait_id, int64_t timeout_ms);
int64_t start_csharp_build(const godot::String &path,
                           const godot::PackedStringArray &build_args);

} // namespace os_ops
} // namespace godot_autopilot
#endif
