#ifndef GODOT_AUTOPILOT_EDITOR_OPS_HPP
#define GODOT_AUTOPILOT_EDITOR_OPS_HPP

#include <cstdint>
#include "util/json_value.hpp"
#include <string>

namespace godot_autopilot {
namespace editor_ops {

godot_autopilot::util::JsonValue handle_get_selection(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_selection(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_edited_scene_root(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save_all_scenes(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_reload_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_undo_redo_start(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_undo_redo_commit(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_undo_redo_add_do(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_undo_redo_add_undo(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_file_system_get_resources(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_file_system_scan(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_main_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_play_current_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_stop_playing(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_get_resource_filesystem(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_plugin_enabled(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_new_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_open_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_close_scene(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_save_scene_as(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_verify_scene_saved(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_build_csharp_assembly(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_dotnet_environment(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_state(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_files(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_scene_control(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_save(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_selection_set(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_undo(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_script_editor(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_session(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_inspect(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_files_panel(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_info(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_editor_config(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue wait_scene_switch(int64_t watch_id, int64_t timeout_ms);
godot_autopilot::util::JsonValue wait_csharp_build(const godot_autopilot::util::JsonValue &wait_ctx,
                                 int64_t timeout_ms);
bool save_receipt_flag(const godot_autopilot::util::JsonValue &args, const char *name);
std::string hash_hex(uint64_t value);

} // namespace editor_ops
} // namespace godot_autopilot
#endif
