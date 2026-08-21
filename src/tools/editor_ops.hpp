#ifndef GODOT_AUTOPILOT_EDITOR_OPS_HPP
#define GODOT_AUTOPILOT_EDITOR_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace editor_ops {

mcp::JsonValue handle_get_selection(const mcp::JsonValue &args);
mcp::JsonValue handle_set_selection(const mcp::JsonValue &args);
mcp::JsonValue handle_get_edited_scene_root(const mcp::JsonValue &args);
mcp::JsonValue handle_save_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_save_all_scenes(const mcp::JsonValue &args);
mcp::JsonValue handle_reload_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_inspect_object(const mcp::JsonValue &args);
mcp::JsonValue handle_undo_redo_start(const mcp::JsonValue &args);
mcp::JsonValue handle_undo_redo_commit(const mcp::JsonValue &args);
mcp::JsonValue handle_undo_redo_add_do(const mcp::JsonValue &args);
mcp::JsonValue handle_undo_redo_add_undo(const mcp::JsonValue &args);
mcp::JsonValue handle_file_system_get_resources(const mcp::JsonValue &args);
mcp::JsonValue handle_file_system_scan(const mcp::JsonValue &args);
mcp::JsonValue handle_set_main_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_play_current_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_stop_playing(const mcp::JsonValue &args);
mcp::JsonValue handle_get_resource_filesystem(const mcp::JsonValue &args);
mcp::JsonValue handle_set_plugin_enabled(const mcp::JsonValue &args);
mcp::JsonValue handle_new_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_open_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_close_scene(const mcp::JsonValue &args);
mcp::JsonValue handle_save_scene_as(const mcp::JsonValue &args);
mcp::JsonValue handle_build_csharp_assembly(const mcp::JsonValue &args);

} // namespace editor_ops
} // namespace godot_autopilot
#endif
