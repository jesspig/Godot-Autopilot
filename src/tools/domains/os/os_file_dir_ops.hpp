#ifndef GODOT_AUTOPILOT_FILE_DIR_OPS_HPP
#define GODOT_AUTOPILOT_FILE_DIR_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace file_dir_ops {

godot_autopilot::util::JsonValue handle_os_file_text(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_file_write(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_os_file_manager(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_list_directory(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_file_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_copy_file_generic(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_ensure_directory(const godot_autopilot::util::JsonValue &args);

} // namespace file_dir_ops
} // namespace godot_autopilot

#endif
