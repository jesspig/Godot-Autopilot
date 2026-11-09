#ifndef GODOT_AUTOPILOT_FILE_DIR_OPS_HPP
#define GODOT_AUTOPILOT_FILE_DIR_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace file_dir_ops {

mcp::JsonValue handle_list_directory(const mcp::JsonValue &args);
mcp::JsonValue handle_get_file_info(const mcp::JsonValue &args);
mcp::JsonValue handle_copy_file_generic(const mcp::JsonValue &args);
mcp::JsonValue handle_ensure_directory(const mcp::JsonValue &args);

} // namespace file_dir_ops
} // namespace godot_autopilot

#endif
