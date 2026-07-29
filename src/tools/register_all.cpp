#include "register_all.hpp"
#include "core/log_system.hpp"
#include "tools/property_ops.hpp"
#include "tools/resource_ops.hpp"
#include "tools/script_ops.hpp"
#include "tools/scene_ops.hpp"
#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>
#include "tools/physics_ops.hpp"
#include "tools/render_ops.hpp"
#include "tools/nav_ops.hpp"
#include "tools/audio_ops.hpp"
#include "tools/code_exec_ops.hpp"
#include "tools/input_ops.hpp"
#include "tools/editor_ops.hpp"
#include "tools/config_ops.hpp"
#include "tools/debug_ops.hpp"
#include "tools/display_ops.hpp"
#include "tools/doc_ops.hpp"
#include "tools/input_map_ops.hpp"
#include "tools/os_ops.hpp"
#include "tools/scene_tree_ops.hpp"
#include "tools/text_ops.hpp"

namespace godot_self_driving {

namespace {
std::unordered_map<std::string, ToolHandler> g_handlers;
} // namespace

mcp::JsonValue call_handler(const std::string& name, const mcp::JsonValue& args) {
    auto it = g_handlers.find(name);
    if (it == g_handlers.end()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("tool not found: " + name);
        return e;
    }
    return it->second(args);
}

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port) {
    // ── 1. Populate handler registry ──
    g_handlers["system_status"] = [port, start = std::chrono::steady_clock::now()](const mcp::JsonValue&) -> mcp::JsonValue {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        mcp::JsonValue status(mcp::JsonValue::object_tag);
        status["version"] = mcp::JsonValue("0.1.0");
        status["port"] = mcp::JsonValue(static_cast<int64_t>(port));
        status["uptime_seconds"] = mcp::JsonValue(static_cast<int64_t>(uptime));
        status["running"] = mcp::JsonValue(true);
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = std::move(status);
        return r;
    };
    g_handlers["scene_node_create"] = scene_ops::handle_create;
    g_handlers["scene_node_delete"] = scene_ops::handle_delete;
    g_handlers["scene_tree_get"] = scene_ops::handle_get_tree;
    g_handlers["property_get"] = property_ops::handle_get;
    g_handlers["property_set"] = property_ops::handle_set;
    g_handlers["property_get_list"] = property_ops::handle_get_list;
    g_handlers["signal_connect"] = property_ops::handle_signal_connect;
    g_handlers["physics_2d_space_get_direct_state"] = physics_ops::handle_2d_space_get_direct_state;
    g_handlers["physics_2d_ray_cast"] = physics_ops::handle_2d_ray_cast;
    g_handlers["physics_2d_shape_cast"] = physics_ops::handle_2d_shape_cast;
    g_handlers["physics_2d_point_query"] = physics_ops::handle_2d_point_query;
    g_handlers["physics_2d_intersect_shape"] = physics_ops::handle_2d_intersect_shape;
    g_handlers["physics_2d_intersect_point"] = physics_ops::handle_2d_intersect_point;
    g_handlers["physics_2d_body_create"] = physics_ops::handle_2d_body_create;
    g_handlers["physics_2d_body_set_mode"] = physics_ops::handle_2d_body_set_mode;
    g_handlers["physics_2d_body_apply_force"] = physics_ops::handle_2d_body_apply_force;
    g_handlers["physics_2d_body_apply_impulse"] = physics_ops::handle_2d_body_apply_impulse;
    g_handlers["physics_2d_body_set_state"] = physics_ops::handle_2d_body_set_state;
    g_handlers["physics_2d_body_get_state"] = physics_ops::handle_2d_body_get_state;
    g_handlers["physics_2d_joint_create"] = physics_ops::handle_2d_joint_create;
    g_handlers["physics_2d_area_create"] = physics_ops::handle_2d_area_create;
    g_handlers["physics_2d_area_set_monitorable"] = physics_ops::handle_2d_area_set_monitorable;
    g_handlers["physics_3d_space_get_direct_state"] = physics_ops::handle_3d_space_get_direct_state;
    g_handlers["physics_3d_ray_cast"] = physics_ops::handle_3d_ray_cast;
    g_handlers["physics_3d_shape_cast"] = physics_ops::handle_3d_shape_cast;
    g_handlers["physics_3d_point_query"] = physics_ops::handle_3d_point_query;
    g_handlers["physics_3d_intersect_shape"] = physics_ops::handle_3d_intersect_shape;
    g_handlers["physics_3d_intersect_point"] = physics_ops::handle_3d_intersect_point;
    g_handlers["physics_3d_body_create"] = physics_ops::handle_3d_body_create;
    g_handlers["physics_3d_body_set_mode"] = physics_ops::handle_3d_body_set_mode;
    g_handlers["physics_3d_body_apply_force"] = physics_ops::handle_3d_body_apply_force;
    g_handlers["physics_3d_body_apply_impulse"] = physics_ops::handle_3d_body_apply_impulse;
    g_handlers["physics_3d_body_set_state"] = physics_ops::handle_3d_body_set_state;
    g_handlers["physics_3d_body_get_state"] = physics_ops::handle_3d_body_get_state;
    g_handlers["physics_3d_joint_create"] = physics_ops::handle_3d_joint_create;
    g_handlers["physics_3d_area_create"] = physics_ops::handle_3d_area_create;
    g_handlers["physics_3d_area_set_monitorable"] = physics_ops::handle_3d_area_set_monitorable;
    g_handlers["physics_3d_body_apply_torque"] = physics_ops::handle_3d_body_apply_torque;
    g_handlers["physics_3d_body_set_axis_lock"] = physics_ops::handle_3d_body_set_axis_lock;
    g_handlers["physics_3d_body_add_collision_exception"] = physics_ops::handle_3d_body_add_collision_exception;
    g_handlers["physics_3d_body_remove_collision_exception"] = physics_ops::handle_3d_body_remove_collision_exception;
    g_handlers["physics_3d_joint_set_param"] = physics_ops::handle_3d_joint_set_param;
    g_handlers["physics_3d_area_set_space_override"] = physics_ops::handle_3d_area_set_space_override;
    g_handlers["physics_3d_space_set_gravity"] = physics_ops::handle_3d_space_set_gravity;
    g_handlers["physics_3d_space_set_debug"] = physics_ops::handle_3d_space_set_debug;
    g_handlers["physics_3d_soft_body_create"] = physics_ops::handle_3d_soft_body_create;
    g_handlers["physics_3d_soft_body_set_mesh"] = physics_ops::handle_3d_soft_body_set_mesh;
    g_handlers["canvas_item_create"] = render_ops::handle_canvas_item_create;
    g_handlers["canvas_item_draw_rect"] = render_ops::handle_canvas_item_draw_rect;
    g_handlers["canvas_item_draw_circle"] = render_ops::handle_canvas_item_draw_circle;
    g_handlers["canvas_item_draw_texture"] = render_ops::handle_canvas_item_draw_texture;
    g_handlers["canvas_item_draw_line"] = render_ops::handle_canvas_item_draw_line;
    g_handlers["canvas_item_set_transform"] = render_ops::handle_canvas_item_set_transform;
    g_handlers["canvas_item_set_visible"] = render_ops::handle_canvas_item_set_visible;
    g_handlers["scenario_create"] = render_ops::handle_scenario_create;
    g_handlers["scenario_set_environment"] = render_ops::handle_scenario_set_environment;
    g_handlers["camera_create"] = render_ops::handle_camera_create;
    g_handlers["camera_set_transform"] = render_ops::handle_camera_set_transform;
    g_handlers["camera_set_perspective"] = render_ops::handle_camera_set_perspective;
    g_handlers["camera_set_orthogonal"] = render_ops::handle_camera_set_orthogonal;
    g_handlers["light_create"] = render_ops::handle_light_create;
    g_handlers["light_set_param"] = render_ops::handle_light_set_param;
    g_handlers["light_set_color"] = render_ops::handle_light_set_color;
    g_handlers["mesh_create"] = render_ops::handle_mesh_create;
    g_handlers["mesh_add_surface"] = render_ops::handle_mesh_add_surface;
    g_handlers["mesh_set_material"] = render_ops::handle_mesh_set_material;
    g_handlers["material_create"] = render_ops::handle_material_create;
    g_handlers["material_set_param"] = render_ops::handle_material_set_param;
    g_handlers["viewport_create"] = render_ops::handle_viewport_create;
    g_handlers["viewport_set_size"] = render_ops::handle_viewport_set_size;
    g_handlers["viewport_set_clear_mode"] = render_ops::handle_viewport_set_clear_mode;
    g_handlers["particle_create"] = render_ops::handle_particle_create;
    g_handlers["environment_set_bg_color"] = render_ops::handle_environment_set_bg_color;
    g_handlers["environment_set_ambient"] = render_ops::handle_environment_set_ambient;
    g_handlers["fog_create"] = render_ops::handle_fog_create;
    g_handlers["shader_create"] = render_ops::handle_shader_create;
    g_handlers["nav_2d_map_create"] = nav_ops::handle_2d_map_create;
    g_handlers["nav_2d_region_create"] = nav_ops::handle_2d_region_create;
    g_handlers["nav_2d_path_query"] = nav_ops::handle_2d_path_query;
    g_handlers["nav_2d_agent_create"] = nav_ops::handle_2d_agent_create;
    g_handlers["nav_2d_agent_set_target"] = nav_ops::handle_2d_agent_set_target;
    g_handlers["nav_3d_map_create"] = nav_ops::handle_3d_map_create;
    g_handlers["nav_3d_region_create"] = nav_ops::handle_3d_region_create;
    g_handlers["nav_3d_path_query"] = nav_ops::handle_3d_path_query;
    g_handlers["nav_3d_path_query_segment"] = nav_ops::handle_3d_path_query_segment;
    g_handlers["nav_3d_agent_create"] = nav_ops::handle_3d_agent_create;
    g_handlers["nav_3d_agent_set_velocity"] = nav_ops::handle_3d_agent_set_velocity;
    g_handlers["nav_3d_agent_get_next_path"] = nav_ops::handle_3d_agent_get_next_path;
    g_handlers["nav_3d_map_set_cell_size"] = nav_ops::handle_3d_map_set_cell_size;
    g_handlers["nav_3d_region_set_nav_mesh"] = nav_ops::handle_3d_region_set_nav_mesh;
    g_handlers["nav_3d_obstacle_create"] = nav_ops::handle_3d_obstacle_create;
    g_handlers["audio_bus_get_layout"] = audio_ops::handle_bus_get_layout;
    g_handlers["audio_bus_set_layout"] = audio_ops::handle_bus_set_layout;
    g_handlers["audio_bus_get_count"] = audio_ops::handle_bus_get_count;
    g_handlers["audio_bus_get_name"] = audio_ops::handle_bus_get_name;
    g_handlers["audio_bus_set_volume"] = audio_ops::handle_bus_set_volume;
    g_handlers["audio_bus_set_mute"] = audio_ops::handle_bus_set_mute;
    g_handlers["audio_bus_set_bypass"] = audio_ops::handle_bus_set_bypass;
    g_handlers["audio_effect_add"] = audio_ops::handle_effect_add;
    g_handlers["audio_effect_remove"] = audio_ops::handle_effect_remove;
    g_handlers["audio_stream_play"] = audio_ops::handle_stream_play;
    g_handlers["audio_stream_stop"] = audio_ops::handle_stream_stop;
    g_handlers["audio_stream_set_volume"] = audio_ops::handle_stream_set_volume;
    g_handlers["audio_stream_set_pitch"] = audio_ops::handle_stream_set_pitch;
    g_handlers["audio_stream_get_playback_position"] = audio_ops::handle_stream_get_playback_position;
    g_handlers["audio_stream_seek"] = audio_ops::handle_stream_seek;
    g_handlers["input_action_press"] = input_ops::handle_action_press;
    g_handlers["input_action_release"] = input_ops::handle_action_release;
    g_handlers["input_is_action_pressed"] = input_ops::handle_is_action_pressed;
    g_handlers["input_is_action_just_pressed"] = input_ops::handle_is_action_just_pressed;
    g_handlers["input_key_press"] = input_ops::handle_key_press;
    g_handlers["input_key_release"] = input_ops::handle_key_release;
    g_handlers["input_mouse_move"] = input_ops::handle_mouse_move;
    g_handlers["input_mouse_button_press"] = input_ops::handle_mouse_button_press;
    g_handlers["input_mouse_button_release"] = input_ops::handle_mouse_button_release;
    g_handlers["input_gamepad_simulate"] = input_ops::handle_gamepad_simulate;
    g_handlers["editor_get_selection"] = editor_ops::handle_get_selection;
    g_handlers["editor_set_selection"] = editor_ops::handle_set_selection;
    g_handlers["editor_get_edited_scene_root"] = editor_ops::handle_get_edited_scene_root;
    g_handlers["editor_save_scene"] = editor_ops::handle_save_scene;
    g_handlers["editor_save_all_scenes"] = editor_ops::handle_save_all_scenes;
    g_handlers["editor_reload_scene"] = editor_ops::handle_reload_scene;
    g_handlers["editor_inspect_object"] = editor_ops::handle_inspect_object;
    g_handlers["editor_undo_redo_start"] = editor_ops::handle_undo_redo_start;
    g_handlers["editor_undo_redo_commit"] = editor_ops::handle_undo_redo_commit;
    g_handlers["editor_undo_redo_add_do"] = editor_ops::handle_undo_redo_add_do;
    g_handlers["editor_undo_redo_add_undo"] = editor_ops::handle_undo_redo_add_undo;
    g_handlers["editor_file_system_get_resources"] = editor_ops::handle_file_system_get_resources;
    g_handlers["editor_file_system_scan"] = editor_ops::handle_file_system_scan;
    g_handlers["editor_import_resource"] = editor_ops::handle_import_resource;
    g_handlers["editor_set_main_scene"] = editor_ops::handle_set_main_scene;
    g_handlers["editor_play_current_scene"] = editor_ops::handle_play_current_scene;
    g_handlers["editor_stop_playing"] = editor_ops::handle_stop_playing;
    g_handlers["editor_get_resource_filesystem"] = editor_ops::handle_get_resource_filesystem;
    g_handlers["editor_get_plugin_list"] = editor_ops::handle_get_plugin_list;
    g_handlers["editor_set_plugin_enabled"] = editor_ops::handle_set_plugin_enabled;
    g_handlers["project_settings_get"] = config_ops::handle_project_settings_get;
    g_handlers["project_settings_set"] = config_ops::handle_project_settings_set;
    g_handlers["project_settings_has"] = config_ops::handle_project_settings_has;
    g_handlers["project_settings_save"] = config_ops::handle_project_settings_save;
    g_handlers["engine_get_version"] = config_ops::handle_engine_get_version;
    g_handlers["engine_get_fps"] = config_ops::handle_engine_get_fps;
    g_handlers["engine_get_frames_drawn"] = config_ops::handle_engine_get_frames_drawn;
    g_handlers["engine_set_time_scale"] = config_ops::handle_engine_set_time_scale;
    g_handlers["engine_get_time_scale"] = config_ops::handle_engine_get_time_scale;
    g_handlers["engine_set_max_fps"] = config_ops::handle_engine_set_max_fps;
    g_handlers["editor_settings_get"] = config_ops::handle_editor_settings_get;
    g_handlers["editor_settings_set"] = config_ops::handle_editor_settings_set;
    g_handlers["editor_settings_has"] = config_ops::handle_editor_settings_has;
    g_handlers["debug_print"] = debug_ops::handle_print;
    g_handlers["debug_print_stack"] = debug_ops::handle_print_stack;
    g_handlers["debug_get_performance_monitor"] = debug_ops::handle_get_performance_monitor;
    g_handlers["debug_list_performance_monitors"] = debug_ops::handle_list_performance_monitors;
    g_handlers["debug_get_object_count"] = debug_ops::handle_get_object_count;
    g_handlers["debug_get_object_count_by_class"] = debug_ops::handle_get_object_count_by_class;
    g_handlers["debug_get_memory_usage"] = debug_ops::handle_get_memory_usage;
    g_handlers["debug_profile_start"] = debug_ops::handle_profile_start;
    g_handlers["debug_profile_stop"] = debug_ops::handle_profile_stop;
    g_handlers["debug_profile_get_data"] = debug_ops::handle_profile_get_data;
    g_handlers["debug_set_fps_limit"] = debug_ops::handle_set_fps_limit;
    g_handlers["debug_set_physics_fps"] = debug_ops::handle_set_physics_fps;
    g_handlers["debug_collision_debug"] = debug_ops::handle_collision_debug;
    g_handlers["debug_navigation_debug"] = debug_ops::handle_navigation_debug;
    g_handlers["debug_performance_debug"] = debug_ops::handle_performance_debug;
    g_handlers["doc_get_class"] = doc_ops::handle_get_class;
    g_handlers["doc_search"] = doc_ops::handle_search;
    g_handlers["doc_get_method"] = doc_ops::handle_get_method;
    g_handlers["doc_get_property"] = doc_ops::handle_get_property;
    g_handlers["resource_load"] = resource_ops::handle_load;
    g_handlers["resource_load_threaded"] = resource_ops::handle_load_threaded;
    g_handlers["resource_load_threaded_get_status"] = resource_ops::handle_load_threaded_get_status;
    g_handlers["resource_load_threaded_wait"] = resource_ops::handle_load_threaded_wait;
    g_handlers["resource_save"] = resource_ops::handle_save;
    g_handlers["resource_create"] = resource_ops::handle_create;
    g_handlers["resource_duplicate"] = resource_ops::handle_duplicate;
    g_handlers["resource_get_type"] = resource_ops::handle_get_type;
    g_handlers["resource_exists"] = resource_ops::handle_exists;
    g_handlers["resource_list_types"] = resource_ops::handle_list_types;
    g_handlers["resource_get_extensions"] = resource_ops::handle_get_extensions;
    g_handlers["resource_list_dir"] = resource_ops::handle_list_dir;
    g_handlers["resource_get_uid"] = resource_ops::handle_get_uid;
    g_handlers["resource_set_uid"] = resource_ops::handle_set_uid;
    g_handlers["resource_remove"] = resource_ops::handle_remove;
    g_handlers["resource_rename"] = resource_ops::handle_rename;
    g_handlers["resource_get_dependencies"] = resource_ops::handle_get_dependencies;
    g_handlers["resource_has_dependency"] = resource_ops::handle_has_dependency;
    g_handlers["resource_import"] = resource_ops::handle_import;
    g_handlers["resource_reimport"] = resource_ops::handle_reimport;
    g_handlers["script_execute_gdscript"] = script_ops::handle_execute_gdscript;
    g_handlers["script_load"] = script_ops::handle_load;
    g_handlers["script_create"] = script_ops::handle_create;
    g_handlers["script_attach_to_node"] = script_ops::handle_attach_to_node;
    g_handlers["script_detach_from_node"] = script_ops::handle_detach_from_node;
    g_handlers["script_get_property"] = script_ops::handle_get_property;
    g_handlers["script_set_property"] = script_ops::handle_set_property;
    g_handlers["script_call_function"] = script_ops::handle_call_function;
    g_handlers["script_reload"] = script_ops::handle_reload;
    g_handlers["script_get_variable_list"] = script_ops::handle_get_variable_list;

    // display_ops
    g_handlers["display_clipboard_get"] = display_ops::handle_clipboard_get;
    g_handlers["display_clipboard_set"] = display_ops::handle_clipboard_set;
    g_handlers["display_dialog_show"] = display_ops::handle_dialog_show;
    g_handlers["display_mouse_get_position"] = display_ops::handle_mouse_get_position;
    g_handlers["display_mouse_set_mode"] = display_ops::handle_mouse_set_mode;
    g_handlers["display_mouse_warp"] = display_ops::handle_mouse_warp;
    g_handlers["display_screen_capture"] = display_ops::handle_screen_capture;
    g_handlers["display_screen_get_count"] = display_ops::handle_screen_get_count;
    g_handlers["display_screen_get_dpi"] = display_ops::handle_screen_get_dpi;
    g_handlers["display_screen_get_position"] = display_ops::handle_screen_get_position;
    g_handlers["display_screen_get_refresh_rate"] = display_ops::handle_screen_get_refresh_rate;
    g_handlers["display_screen_get_size"] = display_ops::handle_screen_get_size;
    g_handlers["display_tts_get_voices"] = display_ops::handle_tts_get_voices;
    g_handlers["display_tts_speak"] = display_ops::handle_tts_speak;
    g_handlers["display_tts_stop"] = display_ops::handle_tts_stop;
    g_handlers["display_window_create"] = display_ops::handle_window_create;
    g_handlers["display_window_delete"] = display_ops::handle_window_delete;
    g_handlers["display_window_move_to_foreground"] = display_ops::handle_window_move_to_foreground;
    g_handlers["display_window_request_attention"] = display_ops::handle_window_request_attention;
    g_handlers["display_window_set_flag"] = display_ops::handle_window_set_flag;
    g_handlers["display_window_set_mode"] = display_ops::handle_window_set_mode;
    g_handlers["display_window_set_position"] = display_ops::handle_window_set_position;
    g_handlers["display_window_set_size"] = display_ops::handle_window_set_size;
    g_handlers["display_window_set_title"] = display_ops::handle_window_set_title;

    // os_ops
    g_handlers["os_alert"] = os_ops::handle_os_alert;
    g_handlers["os_create_process"] = os_ops::handle_os_create_process;
    g_handlers["os_execute"] = os_ops::handle_os_execute;
    g_handlers["os_get_datetime"] = os_ops::handle_os_get_datetime;
    g_handlers["os_get_environment"] = os_ops::handle_os_get_environment;
    g_handlers["os_get_locale"] = os_ops::handle_os_get_locale;
    g_handlers["os_get_system_fonts"] = os_ops::handle_os_get_system_fonts;
    g_handlers["os_get_system_info"] = os_ops::handle_os_get_system_info;
    g_handlers["os_get_unique_id"] = os_ops::handle_os_get_unique_id;
    g_handlers["os_get_unix_time"] = os_ops::handle_os_get_unix_time;
    g_handlers["os_get_user_data_dir"] = os_ops::handle_os_get_user_data_dir;
    g_handlers["os_kill"] = os_ops::handle_os_kill;
    g_handlers["os_move_to_trash"] = os_ops::handle_os_move_to_trash;
    g_handlers["os_set_environment"] = os_ops::handle_os_set_environment;
    g_handlers["os_shell_open"] = os_ops::handle_os_shell_open;

    // render_ops — new
    g_handlers["render_texture_create_2d"] = render_ops::handle_texture_create_2d;
    g_handlers["render_shader_set_code"] = render_ops::handle_shader_set_code;
    g_handlers["render_shader_get_parameter_list"] = render_ops::handle_shader_get_parameter_list;
    g_handlers["render_environment_set_glow"] = render_ops::handle_environment_set_glow;
    g_handlers["render_environment_set_ssr"] = render_ops::handle_environment_set_ssr;
    g_handlers["render_environment_set_tonemap"] = render_ops::handle_environment_set_tonemap;
    g_handlers["render_environment_set_sdfgi"] = render_ops::handle_environment_set_sdfgi;
    g_handlers["render_environment_set_volumetric_fog"] = render_ops::handle_environment_set_volumetric_fog;
    g_handlers["render_sky_create"] = render_ops::handle_sky_create;
    g_handlers["render_sky_set_material"] = render_ops::handle_sky_set_material;
    g_handlers["render_particles_set_emitting"] = render_ops::handle_particles_set_emitting;
    g_handlers["render_particles_restart"] = render_ops::handle_particles_restart;
    g_handlers["render_particles_set_lifetime"] = render_ops::handle_particles_set_lifetime;
    g_handlers["render_reflection_probe_create"] = render_ops::handle_reflection_probe_create;
    g_handlers["render_decal_create"] = render_ops::handle_decal_create;
    g_handlers["render_fog_volume_set_shape"] = render_ops::handle_fog_volume_set_shape;
    g_handlers["render_instance_set_visible"] = render_ops::handle_instance_set_visible;
    g_handlers["render_instance_set_layer_mask"] = render_ops::handle_instance_set_layer_mask;
    g_handlers["render_global_shader_parameter_set"] = render_ops::handle_global_shader_parameter_set;

    // scene_tree_ops
    g_handlers["scene_tree_call_group"] = scene_tree_ops::handle_call_group;
    g_handlers["scene_tree_create_timer"] = scene_tree_ops::handle_create_timer;
    g_handlers["scene_tree_get_nodes_in_group"] = scene_tree_ops::handle_get_nodes_in_group;
    g_handlers["scene_tree_is_paused"] = scene_tree_ops::handle_is_paused;
    g_handlers["scene_tree_notify_group"] = scene_tree_ops::handle_notify_group;
    g_handlers["scene_tree_reload_current_scene"] = scene_tree_ops::handle_reload_current_scene;
    g_handlers["scene_tree_set_debug_collisions"] = scene_tree_ops::handle_set_debug_collisions;
    g_handlers["scene_tree_set_pause"] = scene_tree_ops::handle_set_pause;

    // input_map_ops
    g_handlers["input_map_action_add_event"] = input_map_ops::handle_action_add_event;
    g_handlers["input_map_action_erase_event"] = input_map_ops::handle_action_erase_event;
    g_handlers["input_map_action_set_deadzone"] = input_map_ops::handle_action_set_deadzone;
    g_handlers["input_map_add_action"] = input_map_ops::handle_add_action;
    g_handlers["input_map_erase_action"] = input_map_ops::handle_erase_action;
    g_handlers["input_map_get_actions"] = input_map_ops::handle_get_actions;
    g_handlers["input_map_has_action"] = input_map_ops::handle_has_action;

    // text_ops
    g_handlers["text_create_font"] = text_ops::handle_create_font;
    g_handlers["text_create_shaped_text"] = text_ops::handle_create_shaped_text;
    g_handlers["text_font_set_antialiasing"] = text_ops::handle_font_set_antialiasing;
    g_handlers["text_font_set_data"] = text_ops::handle_font_set_data;
    g_handlers["text_font_set_hinting"] = text_ops::handle_font_set_hinting;
    g_handlers["text_get_system_font_path"] = text_ops::handle_get_system_font_path;
    g_handlers["text_has_feature"] = text_ops::handle_has_feature;
    g_handlers["text_is_locale_right_to_left"] = text_ops::handle_is_locale_right_to_left;
    g_handlers["text_shaped_text_add_string"] = text_ops::handle_shaped_text_add_string;
    g_handlers["text_shaped_text_get_size"] = text_ops::handle_shaped_text_get_size;

    // audio_ops — new
    g_handlers["audio_bus_set_solo"] = audio_ops::handle_bus_set_solo;
    g_handlers["audio_get_output_device_list"] = audio_ops::handle_get_output_device_list;
    g_handlers["audio_set_output_device"] = audio_ops::handle_set_output_device;
    g_handlers["audio_get_input_device_list"] = audio_ops::handle_get_input_device_list;
    g_handlers["audio_set_input_device"] = audio_ops::handle_set_input_device;

    // debug_ops — new
    g_handlers["debug_get_all_monitors"] = debug_ops::handle_get_all_monitors;
    g_handlers["debug_add_custom_monitor"] = debug_ops::handle_add_custom_monitor;
    g_handlers["debug_remove_custom_monitor"] = debug_ops::handle_remove_custom_monitor;
    g_handlers["debug_get_custom_monitor"] = debug_ops::handle_get_custom_monitor;
    g_handlers["debug_list_custom_monitors"] = debug_ops::handle_list_custom_monitors;
    g_handlers["debug_query_object_count"] = debug_ops::handle_query_object_count;
    g_handlers["debug_query_memory_usage"] = debug_ops::handle_query_memory_usage;
    g_handlers["debug_query_node_count"] = debug_ops::handle_query_node_count;

    // physics_ops — new
    g_handlers["physics_3d_shape_create"] = physics_ops::handle_3d_shape_create;
    g_handlers["physics_3d_shape_set_data"] = physics_ops::handle_3d_shape_set_data;
    g_handlers["physics_3d_body_add_shape"] = physics_ops::handle_3d_body_add_shape;
    g_handlers["physics_3d_body_set_param"] = physics_ops::handle_3d_body_set_param;
    g_handlers["physics_3d_area_set_param"] = physics_ops::handle_3d_area_set_param;
    g_handlers["physics_3d_space_set_param"] = physics_ops::handle_3d_space_set_param;
    g_handlers["physics_3d_area_set_transform"] = physics_ops::handle_3d_area_set_transform;
    g_handlers["physics_3d_body_set_transform"] = physics_ops::handle_3d_body_set_transform;
    g_handlers["physics_2d_shape_create"] = physics_ops::handle_2d_shape_create;
    g_handlers["physics_2d_shape_set_data"] = physics_ops::handle_2d_shape_set_data;

    // ── 2. Populate catalog ──
    catalog.populate_default_tools();

    // ── 3. Register directly-called meta-tools ──
    // ping
    {
        mcp::ToolOptions opts;
        opts.Description("Health check ping");
        server.RegisterTool("ping", opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                auto body = queue.submit([]() -> std::string {
                    mcp::JsonValue r(mcp::JsonValue::object_tag);
                    r["result"] = mcp::JsonValue("pong");
                    return r.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // search_tools
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");

        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue q_prop(mcp::JsonValue::object_tag);
        q_prop["type"] = mcp::JsonValue("string");
        q_prop["description"] = mcp::JsonValue("Search query");
        props["query"] = std::move(q_prop);

        mcp::JsonValue c_prop(mcp::JsonValue::object_tag);
        c_prop["type"] = mcp::JsonValue("string");
        c_prop["description"] = mcp::JsonValue("Filter by category");
        props["category"] = std::move(c_prop);

        mcp::JsonValue t_prop(mcp::JsonValue::object_tag);
        t_prop["type"] = mcp::JsonValue("array");
        mcp::JsonValue ti(mcp::JsonValue::object_tag);
        ti["type"] = mcp::JsonValue("string");
        t_prop["items"] = std::move(ti);
        t_prop["description"] = mcp::JsonValue("Filter by tags");
        props["tags"] = std::move(t_prop);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("query"));
        s["required"] = std::move(req);

        mcp::ToolOptions s_opts;
        s_opts.Description("Search available tools by query").InputSchema(std::move(s));
        server.RegisterTool("search_tools", s_opts,
            [&queue, &index](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &index]() -> std::string {
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools called");
                    if (args.Find("query") == nullptr) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("missing required parameter: query");
                        return err.Dump();
                    }

                    Bm25Index::SearchQuery query;
                    query.text = args["query"].GetString();

                    if (auto* cat = args.Find("category")) {
                        if (!cat->IsNull()) {
                            query.category = cat->GetString();
                        }
                    }

                    if (auto* tags_val = args.Find("tags")) {
                        if (!tags_val->IsNull() && tags_val->IsArray()) {
                            std::vector<std::string> tags;
                            for (auto& t : tags_val->GetArray()) {
                                tags.push_back(t.GetString());
                            }
                            query.tags = std::move(tags);
                        }
                    }

                    auto results = index.search(query);

                    mcp::JsonValue j(mcp::JsonValue::object_tag);
                    mcp::JsonValue arr(mcp::JsonValue::array_tag);
                    for (auto& r : results) {
                        mcp::JsonValue item(mcp::JsonValue::object_tag);
                        item["name"] = mcp::JsonValue(r.name);
                        item["score"] = mcp::JsonValue(r.score);
                        arr.PushBack(std::move(item));
                    }
                    j["results"] = std::move(arr);
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools completed");
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // list_categories
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);

        mcp::ToolOptions l_opts;
        l_opts.Description("List all tool categories").InputSchema(std::move(s));
        server.RegisterTool("list_categories", l_opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>&) -> mcp::CallToolResult {
                auto body = queue.submit([&catalog]() -> std::string {
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "list_categories called");
                    auto cats = catalog.get_categories();
                    mcp::JsonValue j(mcp::JsonValue::object_tag);
                    mcp::JsonValue arr(mcp::JsonValue::array_tag);
                    for (auto& c : cats) {
                        mcp::JsonValue item(mcp::JsonValue::object_tag);
                        item["id"] = mcp::JsonValue(c);
                        item["name"] = mcp::JsonValue(c);
                        int count = 0;
                        for (auto* t : catalog.get_all_tools()) {
                            if (t->category == c) {
                                ++count;
                            }
                        }
                        item["tool_count"] = mcp::JsonValue(static_cast<int64_t>(count));
                        arr.PushBack(std::move(item));
                    }
                    j["categories"] = std::move(arr);
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "list_categories completed");
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // get_tool_detail
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue n(mcp::JsonValue::object_tag);
        n["type"] = mcp::JsonValue("string");
        n["description"] = mcp::JsonValue("Tool name (e.g. ping, search_tools)");
        props["name"] = std::move(n);
        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(req);

        mcp::ToolOptions g_opts;
        g_opts.Description("Get complete schema for one tool").InputSchema(std::move(s));
        server.RegisterTool("get_tool_detail", g_opts,
            [&queue, &catalog](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args_copy = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args_copy), &catalog]() -> std::string {
                    if (args.Find("name") == nullptr) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("missing required parameter: name");
                        return err.Dump();
                    }

                    std::string name = args["name"].GetString();
                    auto* info = catalog.get_tool(name);
                    if (!info) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("tool not found: " + name);
                        return err.Dump();
                    }

                    mcp::JsonValue j(mcp::JsonValue::object_tag);
                    mcp::JsonValue t(mcp::JsonValue::object_tag);
                    t["name"] = mcp::JsonValue(info->name);
                    t["description"] = mcp::JsonValue(info->description);
                    t["category"] = mcp::JsonValue(info->category);
                    mcp::JsonValue tags(mcp::JsonValue::array_tag);
                    for (auto& tg : info->tags) {
                        tags.PushBack(mcp::JsonValue(tg));
                    }
                    t["tags"] = std::move(tags);
                    t["input_schema"] = info->input_schema;
                    j["tool"] = std::move(t);
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "get_tool_detail completed");
                    return j.Dump();
                }).get();

                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // call_tool — single entry point for all non-meta tools
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue name_p(mcp::JsonValue::object_tag);
        name_p["type"] = mcp::JsonValue("string");
        name_p["description"] = mcp::JsonValue("Tool name to execute");
        props["name"] = std::move(name_p);
        mcp::JsonValue a(mcp::JsonValue::object_tag);
        a["type"] = mcp::JsonValue("object");
        a["description"] = mcp::JsonValue("Tool arguments as JSON object");
        props["arguments"] = std::move(a);
        s["properties"] = std::move(props);
        mcp::JsonValue creq(mcp::JsonValue::array_tag);
        creq.PushBack(mcp::JsonValue("name"));
        s["required"] = std::move(creq);

        mcp::ToolOptions c_opts;
        c_opts.Description("Execute any tool by name. Use this to call all non-meta tools (scene_*, property_*, signal_*, system_status).").InputSchema(std::move(s));
        server.RegisterTool("call_tool", c_opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                if (args.Find("name") == nullptr) {
                    mcp::CallToolResult err;
                    err.is_error = true;
                    err.content.push_back(mcp::TextContent{"text", R"({"error":"missing required parameter: name"})"});
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "call_tool failed: missing name");
                    return err;
                }

                std::string name = args["name"].GetString();
                mcp::JsonValue tool_args = [&]{
                    if (auto* a = args.Find("arguments")) return *a;
                    return mcp::JsonValue(mcp::JsonValue::object_tag);
                }();

                auto result = queue.submit([name, tool_args = std::move(tool_args)]() -> std::string {
                    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " called via call_tool");

                    auto it = g_handlers.find(name);
                    if (it == g_handlers.end()) {
                        mcp::JsonValue err(mcp::JsonValue::object_tag);
                        err["error"] = mcp::JsonValue("tool not found: " + name);
                        return err.Dump();
                    }

                    return it->second(tool_args).Dump();
                });

                std::string body = result.get();
                mcp::JsonValue j = mcp::JsonValue::Parse(body);

                mcp::CallToolResult mcp_result;
                if (j.Find("error") != nullptr) mcp_result.is_error = true;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, name + " completed");
                return mcp_result;
            });
    }

    // batch_execute
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["type"] = mcp::JsonValue("object");
        mcp::JsonValue iprops(mcp::JsonValue::object_tag);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        tp["description"] = mcp::JsonValue("Tool name to execute");
        iprops["tool"] = std::move(tp);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("object");
        ap["description"] = mcp::JsonValue("Tool arguments");
        iprops["args"] = std::move(ap);
        item["properties"] = std::move(iprops);
        mcp::JsonValue ireq(mcp::JsonValue::array_tag);
        ireq.PushBack(mcp::JsonValue("tool"));
        item["required"] = std::move(ireq);

        mcp::JsonValue ops_prop(mcp::JsonValue::object_tag);
        ops_prop["type"] = mcp::JsonValue("array");
        ops_prop["items"] = std::move(item);
        ops_prop["description"] = mcp::JsonValue("Ordered list of operations to execute");
        props["operations"] = std::move(ops_prop);

        mcp::JsonValue stop_prop(mcp::JsonValue::object_tag);
        stop_prop["type"] = mcp::JsonValue("boolean");
        stop_prop["description"] = mcp::JsonValue("Stop on first error");
        stop_prop["default"] = mcp::JsonValue(true);
        props["stop_on_error"] = std::move(stop_prop);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("operations"));
        s["required"] = std::move(req);

        mcp::ToolOptions b_opts;
        b_opts.Description("Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped.").InputSchema(std::move(s));
        server.RegisterTool("batch_execute", b_opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args)]() -> std::string {
                    return code_exec_ops::handle_batch_execute(args).Dump();
                }).get();
                mcp::CallToolResult mcp_result;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // code_execute
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue sc(mcp::JsonValue::object_tag);
        sc["type"] = mcp::JsonValue("string");
        sc["description"] = mcp::JsonValue("GDScript source code");
        props["source_code"] = std::move(sc);

        mcp::JsonValue fn(mcp::JsonValue::object_tag);
        fn["type"] = mcp::JsonValue("string");
        fn["description"] = mcp::JsonValue("Function name to call (default: _run)");
        props["function_name"] = std::move(fn);

        mcp::JsonValue tm(mcp::JsonValue::object_tag);
        tm["type"] = mcp::JsonValue("integer");
        tm["description"] = mcp::JsonValue("Execution timeout in milliseconds (max 30000)");
        tm["default"] = mcp::JsonValue(static_cast<int64_t>(5000));
        tm["maximum"] = mcp::JsonValue(static_cast<int64_t>(30000));
        props["timeout_ms"] = std::move(tm);

        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("source_code"));
        s["required"] = std::move(req);

        mcp::ToolOptions c_opts;
        c_opts.Description("Execute arbitrary GDScript code. The source code is wrapped in a script that extends Node, compiled, attached to a temporary node, and executed. Returns the function result serialized as JSON.").InputSchema(std::move(s));
        server.RegisterTool("code_execute", c_opts,
            [&queue](const mcp::RequestContext<mcp::CallToolRequestParams>& ctx) -> mcp::CallToolResult {
                mcp::JsonValue args = ctx.Params().arguments
                    ? *ctx.Params().arguments : mcp::JsonValue(mcp::JsonValue::object_tag);
                auto body = queue.submit([args = std::move(args)]() -> std::string {
                    return code_exec_ops::handle_code_execute(args).Dump();
                }).get();
                mcp::CallToolResult mcp_result;
                mcp::JsonValue j = mcp::JsonValue::Parse(body);
                if (j.Find("error") != nullptr) mcp_result.is_error = true;
                mcp_result.content.push_back(mcp::TextContent{"text", body});
                return mcp_result;
            });
    }

    // ── 4. Catalog entries (full schemas) ──
    // Meta-tools (overwrite basic entries from populate_default_tools)
    {
        catalog.add_tool({"call_tool", "Execute any tool by name", "Meta", {"proxy", "execute"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);

        mcp::JsonValue item(mcp::JsonValue::object_tag);
        item["type"] = mcp::JsonValue("object");
        mcp::JsonValue iprops(mcp::JsonValue::object_tag);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        iprops["tool"] = std::move(tp);
        mcp::JsonValue ap(mcp::JsonValue::object_tag);
        ap["type"] = mcp::JsonValue("object");
        iprops["args"] = std::move(ap);
        item["properties"] = std::move(iprops);
        mcp::JsonValue ireq(mcp::JsonValue::array_tag);
        ireq.PushBack(mcp::JsonValue("tool"));
        item["required"] = std::move(ireq);

        mcp::JsonValue ops_prop(mcp::JsonValue::object_tag);
        ops_prop["type"] = mcp::JsonValue("array");
        ops_prop["items"] = std::move(item);
        props["operations"] = std::move(ops_prop);
        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("operations"));
        s["required"] = std::move(req);
        catalog.add_tool({"batch_execute", "Execute multiple tools in batch", "Meta", {"batch", "execute"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue sc(mcp::JsonValue::object_tag);
        sc["type"] = mcp::JsonValue("string");
        props["source_code"] = std::move(sc);
        s["properties"] = std::move(props);
        mcp::JsonValue req(mcp::JsonValue::array_tag);
        req.PushBack(mcp::JsonValue("source_code"));
        s["required"] = std::move(req);
        catalog.add_tool({"code_execute", "Execute arbitrary GDScript code", "Meta", {"code", "execute"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["parent_path"] = std::move(pp);
        mcp::JsonValue np(mcp::JsonValue::object_tag);
        np["type"] = mcp::JsonValue("string");
        props["name"] = std::move(np);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        props["type"] = std::move(tp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("parent_path"));
        s["required"] = std::move(rq);
        catalog.add_tool({"scene_node_create", "Create a new scene node as child of a parent", "Scene", {"node", "create"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        catalog.add_tool({"scene_node_delete", "Delete a scene node by path", "Scene", {"node", "delete"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        catalog.add_tool({"scene_tree_get", "Get the full scene tree", "Scene", {"tree", "structure"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        mcp::JsonValue prp(mcp::JsonValue::object_tag);
        prp["type"] = mcp::JsonValue("string");
        props["property"] = std::move(prp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("property"));
        s["required"] = std::move(rq);
        catalog.add_tool({"property_get", "Get a property value from a scene node", "Properties", {"property", "get"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        mcp::JsonValue prp(mcp::JsonValue::object_tag);
        prp["type"] = mcp::JsonValue("string");
        props["property"] = std::move(prp);
        mcp::JsonValue vl(mcp::JsonValue::object_tag);
        vl["type"] = mcp::JsonValue("object");
        props["value"] = std::move(vl);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        rq.PushBack(mcp::JsonValue("property"));
        rq.PushBack(mcp::JsonValue("value"));
        s["required"] = std::move(rq);
        catalog.add_tool({"property_set", "Set a property value on a scene node", "Properties", {"property", "set"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue pp(mcp::JsonValue::object_tag);
        pp["type"] = mcp::JsonValue("string");
        props["path"] = std::move(pp);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("path"));
        s["required"] = std::move(rq);
        catalog.add_tool({"property_get_list", "List all properties of a scene node", "Properties", {"property", "list"}, std::move(s)});
    }
    {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        mcp::JsonValue props(mcp::JsonValue::object_tag);
        mcp::JsonValue sp(mcp::JsonValue::object_tag);
        sp["type"] = mcp::JsonValue("string");
        props["source_path"] = std::move(sp);
        mcp::JsonValue sg(mcp::JsonValue::object_tag);
        sg["type"] = mcp::JsonValue("string");
        props["signal"] = std::move(sg);
        mcp::JsonValue tp(mcp::JsonValue::object_tag);
        tp["type"] = mcp::JsonValue("string");
        props["target_path"] = std::move(tp);
        mcp::JsonValue mt(mcp::JsonValue::object_tag);
        mt["type"] = mcp::JsonValue("string");
        props["method"] = std::move(mt);
        s["properties"] = std::move(props);
        mcp::JsonValue rq(mcp::JsonValue::array_tag);
        rq.PushBack(mcp::JsonValue("source_path"));
        rq.PushBack(mcp::JsonValue("signal"));
        rq.PushBack(mcp::JsonValue("target_path"));
        rq.PushBack(mcp::JsonValue("method"));
        s["required"] = std::move(rq);
        catalog.add_tool({"signal_connect", "Connect a signal from one node to another", "Properties", {"signal", "connect"}, std::move(s)});
    }

    // resource_ops
    catalog.add_tool({"resource_load", "Load a resource from file path", "Resources", {"resource", "load"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_load_threaded", "Start threaded resource load", "Resources", {"resource", "load", "threaded"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_load_threaded_get_status", "Get status of threaded resource load", "Resources", {"resource", "load", "status"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_load_threaded_wait", "Wait for threaded resource load to complete", "Resources", {"resource", "load", "wait"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_save", "Save a resource to file", "Resources", {"resource", "save"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_create", "Create a new resource instance by class type", "Resources", {"resource", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_duplicate", "Duplicate/instance a loaded resource", "Resources", {"resource", "duplicate"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_get_type", "Get the class type of a resource", "Resources", {"resource", "type"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_exists", "Check if a resource exists at path", "Resources", {"resource", "exists"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_list_types", "List all instantiable Resource subclass types", "Resources", {"resource", "types", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_get_extensions", "Get recognized file extensions for a resource type", "Resources", {"resource", "extensions"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_list_dir", "List resource files in a directory", "Resources", {"resource", "directory", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_get_uid", "Get the UID of a resource file", "Resources", {"resource", "uid", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_set_uid", "Set or assign a UID to a resource file", "Resources", {"resource", "uid", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_remove", "Delete a resource file from disk", "Resources", {"resource", "remove", "delete"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_rename", "Rename/move a resource file", "Resources", {"resource", "rename", "move"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_get_dependencies", "List all dependencies of a resource", "Resources", {"resource", "dependencies", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_has_dependency", "Check if a resource depends on another file", "Resources", {"resource", "dependency", "check"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_import", "Import a single resource file (editor only)", "Resources", {"resource", "import"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"resource_reimport", "Reimport one or more resource files (editor only)", "Resources", {"resource", "reimport"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // script_ops
    catalog.add_tool({"script_execute_gdscript", "Execute an arbitrary GDScript expression", "Scripts", {"script", "execute", "gdscript"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_load", "Load a script from file path", "Scripts", {"script", "load"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_create", "Create and save a new GDScript file", "Scripts", {"script", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_attach_to_node", "Attach a script to a scene node", "Scripts", {"script", "attach", "node"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_detach_from_node", "Detach script from a scene node", "Scripts", {"script", "detach", "node"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_get_property", "Get a script property's default value or a node's property", "Scripts", {"script", "property", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_set_property", "Set a property on a node via script", "Scripts", {"script", "property", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_call_function", "Call a function on a node via its script", "Scripts", {"script", "call", "function"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_reload", "Reload a script from disk", "Scripts", {"script", "reload"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"script_get_variable_list", "List all script variables and their types", "Scripts", {"script", "variables", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // physics_ops — 2D
    catalog.add_tool({"physics_2d_space_get_direct_state", "Get the direct state of a 2D physics space", "Physics", {"physics", "2d", "space"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_ray_cast", "Cast a ray in 2D physics space", "Physics", {"physics", "2d", "ray"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_shape_cast", "Cast a shape in 2D physics space", "Physics", {"physics", "2d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_point_query", "Query a point in 2D physics space", "Physics", {"physics", "2d", "point"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_intersect_shape", "Intersect a shape in 2D physics space", "Physics", {"physics", "2d", "intersect", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_intersect_point", "Intersect a point in 2D physics space", "Physics", {"physics", "2d", "intersect", "point"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_create", "Create a 2D physics body", "Physics", {"physics", "2d", "body", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_set_mode", "Set the mode of a 2D physics body", "Physics", {"physics", "2d", "body", "mode"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_apply_force", "Apply force to a 2D physics body", "Physics", {"physics", "2d", "body", "force"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_apply_impulse", "Apply impulse to a 2D physics body", "Physics", {"physics", "2d", "body", "impulse"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_set_state", "Set state of a 2D physics body", "Physics", {"physics", "2d", "body", "state"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_body_get_state", "Get state of a 2D physics body", "Physics", {"physics", "2d", "body", "state"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_joint_create", "Create a 2D physics joint", "Physics", {"physics", "2d", "joint", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_area_create", "Create a 2D physics area", "Physics", {"physics", "2d", "area", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_area_set_monitorable", "Set monitorable flag on a 2D area", "Physics", {"physics", "2d", "area", "monitorable"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // physics_ops — 3D
    catalog.add_tool({"physics_3d_space_get_direct_state", "Get the direct state of a 3D physics space", "Physics", {"physics", "3d", "space"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_ray_cast", "Cast a ray in 3D physics space", "Physics", {"physics", "3d", "ray"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_shape_cast", "Cast a shape in 3D physics space", "Physics", {"physics", "3d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_point_query", "Query a point in 3D physics space", "Physics", {"physics", "3d", "point"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_intersect_shape", "Intersect a shape in 3D physics space", "Physics", {"physics", "3d", "intersect", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_intersect_point", "Intersect a point in 3D physics space", "Physics", {"physics", "3d", "intersect", "point"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_create", "Create a 3D physics body", "Physics", {"physics", "3d", "body", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_set_mode", "Set the mode of a 3D physics body", "Physics", {"physics", "3d", "body", "mode"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_apply_force", "Apply force to a 3D physics body", "Physics", {"physics", "3d", "body", "force"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_apply_impulse", "Apply impulse to a 3D physics body", "Physics", {"physics", "3d", "body", "impulse"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_apply_torque", "Apply torque to a 3D physics body", "Physics", {"physics", "3d", "body", "torque"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_set_axis_lock", "Set axis lock on a 3D physics body", "Physics", {"physics", "3d", "body", "axis_lock"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_set_state", "Set state of a 3D physics body", "Physics", {"physics", "3d", "body", "state"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_get_state", "Get state of a 3D physics body", "Physics", {"physics", "3d", "body", "state"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_add_collision_exception", "Add collision exception to a 3D body", "Physics", {"physics", "3d", "collision", "exception"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_remove_collision_exception", "Remove collision exception from a 3D body", "Physics", {"physics", "3d", "collision", "exception"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_joint_create", "Create a 3D physics joint", "Physics", {"physics", "3d", "joint", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_joint_set_param", "Set parameter on a 3D physics joint", "Physics", {"physics", "3d", "joint", "param"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_area_create", "Create a 3D physics area", "Physics", {"physics", "3d", "area", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_area_set_monitorable", "Set monitorable flag on a 3D area", "Physics", {"physics", "3d", "area", "monitorable"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_area_set_space_override", "Set space override mode on a 3D area", "Physics", {"physics", "3d", "area", "space_override"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_space_set_gravity", "Set gravity on a 3D physics space", "Physics", {"physics", "3d", "space", "gravity"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_space_set_debug", "Toggle debug visualization on a 3D space", "Physics", {"physics", "3d", "space", "debug"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_soft_body_create", "Create a 3D soft body", "Physics", {"physics", "3d", "soft_body", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_soft_body_set_mesh", "Set soft body mesh", "Physics", {"physics", "3d", "soft_body", "mesh"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // render_ops
    catalog.add_tool({"canvas_item_create", "Create a canvas item", "Render", {"render", "canvas", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_draw_rect", "Draw a rectangle on a canvas item", "Render", {"render", "canvas", "draw", "rect"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_draw_circle", "Draw a circle on a canvas item", "Render", {"render", "canvas", "draw", "circle"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_draw_texture", "Draw a texture on a canvas item", "Render", {"render", "canvas", "draw", "texture"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_draw_line", "Draw a line on a canvas item", "Render", {"render", "canvas", "draw", "line"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_set_transform", "Set transform on a canvas item", "Render", {"render", "canvas", "transform"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"canvas_item_set_visible", "Set visibility on a canvas item", "Render", {"render", "canvas", "visible"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scenario_create", "Create a render scenario", "Render", {"render", "scenario", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scenario_set_environment", "Set environment on a render scenario", "Render", {"render", "scenario", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"camera_create", "Create a render camera", "Render", {"render", "camera", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"camera_set_transform", "Set transform on a render camera", "Render", {"render", "camera", "transform"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"camera_set_perspective", "Set perspective projection on camera", "Render", {"render", "camera", "perspective"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"camera_set_orthogonal", "Set orthogonal projection on camera", "Render", {"render", "camera", "orthogonal"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"light_create", "Create a render light", "Render", {"render", "light", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"light_set_param", "Set a parameter on a render light", "Render", {"render", "light", "param"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"light_set_color", "Set color on a render light", "Render", {"render", "light", "color"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"mesh_create", "Create a render mesh", "Render", {"render", "mesh", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"mesh_add_surface", "Add a surface to a render mesh", "Render", {"render", "mesh", "surface"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"mesh_set_material", "Set material on a render mesh", "Render", {"render", "mesh", "material"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"material_create", "Create a render material", "Render", {"render", "material", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"material_set_param", "Set a parameter on a render material", "Render", {"render", "material", "param"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"viewport_create", "Create a render viewport", "Render", {"render", "viewport", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"viewport_set_size", "Set size on a render viewport", "Render", {"render", "viewport", "size"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"viewport_set_clear_mode", "Set clear mode on a render viewport", "Render", {"render", "viewport", "clear"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"particle_create", "Create a particle system", "Render", {"render", "particle", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"environment_set_bg_color", "Set background color on environment", "Render", {"render", "environment", "bg"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"environment_set_ambient", "Set ambient light on environment", "Render", {"render", "environment", "ambient"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"fog_create", "Create a fog volume", "Render", {"render", "fog", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"shader_create", "Create a shader", "Render", {"render", "shader", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // nav_ops — 2D
    catalog.add_tool({"nav_2d_map_create", "Create a 2D navigation map", "Navigation", {"nav", "2d", "map", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_2d_region_create", "Create a 2D navigation region", "Navigation", {"nav", "2d", "region", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_2d_path_query", "Query a path in 2D navigation", "Navigation", {"nav", "2d", "path", "query"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_2d_agent_create", "Create a 2D navigation agent", "Navigation", {"nav", "2d", "agent", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_2d_agent_set_target", "Set target for a 2D navigation agent", "Navigation", {"nav", "2d", "agent", "target"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // nav_ops — 3D
    catalog.add_tool({"nav_3d_map_create", "Create a 3D navigation map", "Navigation", {"nav", "3d", "map", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_map_set_cell_size", "Set cell size on a 3D nav map", "Navigation", {"nav", "3d", "map", "cell"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_region_create", "Create a 3D navigation region", "Navigation", {"nav", "3d", "region", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_region_set_nav_mesh", "Set nav mesh on a 3D region", "Navigation", {"nav", "3d", "region", "navmesh"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_path_query", "Query a path in 3D navigation", "Navigation", {"nav", "3d", "path", "query"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_path_query_segment", "Query a path segment in 3D navigation", "Navigation", {"nav", "3d", "path", "segment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_agent_create", "Create a 3D navigation agent", "Navigation", {"nav", "3d", "agent", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_agent_set_velocity", "Set velocity on a 3D nav agent", "Navigation", {"nav", "3d", "agent", "velocity"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_agent_get_next_path", "Get next path position for a 3D nav agent", "Navigation", {"nav", "3d", "agent", "next_path"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"nav_3d_obstacle_create", "Create a 3D navigation obstacle", "Navigation", {"nav", "3d", "obstacle", "create"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // audio_ops
    catalog.add_tool({"audio_bus_get_layout", "Get the current audio bus layout", "Audio", {"audio", "bus", "layout", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_set_layout", "Set the audio bus layout", "Audio", {"audio", "bus", "layout", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_get_count", "Get the number of audio buses", "Audio", {"audio", "bus", "count"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_get_name", "Get the name of an audio bus by index", "Audio", {"audio", "bus", "name"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_set_volume", "Set volume on an audio bus", "Audio", {"audio", "bus", "volume"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_set_mute", "Set mute on an audio bus", "Audio", {"audio", "bus", "mute"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_bus_set_bypass", "Set bypass effects on an audio bus", "Audio", {"audio", "bus", "bypass"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_effect_add", "Add an audio effect to a bus", "Audio", {"audio", "effect", "add"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_effect_remove", "Remove an audio effect from a bus", "Audio", {"audio", "effect", "remove"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_play", "Play an audio stream", "Audio", {"audio", "stream", "play"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_stop", "Stop the currently playing audio stream", "Audio", {"audio", "stream", "stop"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_set_volume", "Set volume on the audio stream", "Audio", {"audio", "stream", "volume"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_set_pitch", "Set pitch on the audio stream", "Audio", {"audio", "stream", "pitch"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_get_playback_position", "Get the current playback position", "Audio", {"audio", "stream", "position"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_stream_seek", "Seek the audio stream to a position", "Audio", {"audio", "stream", "seek"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // input_ops
    catalog.add_tool({"input_action_press", "Press an input action", "Input", {"input", "action", "press"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_action_release", "Release an input action", "Input", {"input", "action", "release"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_is_action_pressed", "Check if an input action is pressed", "Input", {"input", "action", "pressed"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_is_action_just_pressed", "Check if an input action was just pressed", "Input", {"input", "action", "just_pressed"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_key_press", "Simulate a key press", "Input", {"input", "key", "press"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_key_release", "Simulate a key release", "Input", {"input", "key", "release"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_mouse_move", "Simulate mouse movement", "Input", {"input", "mouse", "move"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_mouse_button_press", "Simulate mouse button press", "Input", {"input", "mouse", "press"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_mouse_button_release", "Simulate mouse button release", "Input", {"input", "mouse", "release"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_gamepad_simulate", "Simulate gamepad input", "Input", {"input", "gamepad", "simulate"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // editor_ops
    catalog.add_tool({"editor_get_selection", "Get currently selected nodes", "Editor", {"editor", "selection", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_set_selection", "Set selected nodes by path", "Editor", {"editor", "selection", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_get_edited_scene_root", "Get the edited scene root node", "Editor", {"editor", "scene", "root"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_save_scene", "Save the current scene", "Editor", {"editor", "scene", "save"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_save_all_scenes", "Save all open scenes", "Editor", {"editor", "scene", "save_all"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_reload_scene", "Reload the current scene from disk", "Editor", {"editor", "scene", "reload"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_inspect_object", "Inspect an object in the editor", "Editor", {"editor", "inspect"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_undo_redo_start", "Start an undo/redo action", "Editor", {"editor", "undo", "start"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_undo_redo_commit", "Commit the current undo/redo action", "Editor", {"editor", "undo", "commit"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_undo_redo_add_do", "Add a do method to the undo/redo action", "Editor", {"editor", "undo", "do"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_undo_redo_add_undo", "Add an undo method to the undo/redo action", "Editor", {"editor", "undo", "undo"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_file_system_get_resources", "Get resources from the file system", "Editor", {"editor", "filesystem", "resources"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_file_system_scan", "Scan the file system for changes", "Editor", {"editor", "filesystem", "scan"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_import_resource", "Import a resource into the project", "Editor", {"editor", "import", "resource"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_set_main_scene", "Set the main scene", "Editor", {"editor", "scene", "main"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_play_current_scene", "Play the current scene", "Editor", {"editor", "play", "scene"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_stop_playing", "Stop the running scene", "Editor", {"editor", "stop", "playing"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_get_resource_filesystem", "Get the editor file system", "Editor", {"editor", "filesystem", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_get_plugin_list", "List all plugins", "Editor", {"editor", "plugin", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_set_plugin_enabled", "Enable or disable a plugin", "Editor", {"editor", "plugin", "enable"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // config_ops
    catalog.add_tool({"project_settings_get", "Get a project setting by name", "Config", {"config", "project", "settings", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"project_settings_set", "Set a project setting by name", "Config", {"config", "project", "settings", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"project_settings_has", "Check if a project setting exists", "Config", {"config", "project", "settings", "has"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"project_settings_save", "Save all project settings to disk", "Config", {"config", "project", "settings", "save"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_get_version", "Get engine version info", "Config", {"config", "engine", "version"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_get_fps", "Get current FPS", "Config", {"config", "engine", "fps", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_get_frames_drawn", "Get total frames drawn", "Config", {"config", "engine", "frames"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_set_time_scale", "Set the engine time scale", "Config", {"config", "engine", "time", "scale", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_get_time_scale", "Get the engine time scale", "Config", {"config", "engine", "time", "scale", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"engine_set_max_fps", "Set the maximum FPS", "Config", {"config", "engine", "fps", "max"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_settings_get", "Get an editor setting by name", "Config", {"config", "editor", "settings", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_settings_set", "Set an editor setting by name", "Config", {"config", "editor", "settings", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"editor_settings_has", "Check if an editor setting exists", "Config", {"config", "editor", "settings", "has"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // debug_ops
    catalog.add_tool({"debug_print", "Print a debug log message", "Debug", {"debug", "print", "log"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_print_stack", "Print the current script call stack", "Debug", {"debug", "stack", "trace"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_get_performance_monitor", "Get a specific performance monitor value by ID", "Debug", {"debug", "performance", "monitor", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_list_performance_monitors", "List all available performance monitors", "Debug", {"debug", "performance", "monitor", "list"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_get_object_count", "Get total live object count", "Debug", {"debug", "objects", "count"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_get_object_count_by_class", "Get object count by class (limited availability)", "Debug", {"debug", "objects", "class"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_get_memory_usage", "Get current static memory usage in bytes", "Debug", {"debug", "memory", "usage"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_profile_start", "Start profiling (not available via godot-cpp)", "Debug", {"debug", "profile", "start"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_profile_stop", "Stop profiling (not available via godot-cpp)", "Debug", {"debug", "profile", "stop"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_profile_get_data", "Get profiling data (not available via godot-cpp)", "Debug", {"debug", "profile", "data"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_set_fps_limit", "Set a dynamic FPS limit for the engine", "Debug", {"debug", "fps", "limit", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_set_physics_fps", "Set the physics FPS (ticks per second)", "Debug", {"debug", "physics", "fps", "set"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_collision_debug", "Toggle collision debug visualization", "Debug", {"debug", "collision", "visualize"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_navigation_debug", "Toggle navigation debug visualization", "Debug", {"debug", "navigation", "visualize"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_performance_debug", "Toggle performance debug overlay in editor", "Debug", {"debug", "performance", "overlay"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // doc_ops
    catalog.add_tool({"doc_get_class", "Get detailed documentation for a Godot class", "Docs", {"docs", "class", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"doc_search", "Search Godot classes by name", "Docs", {"docs", "search", "class"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"doc_get_method", "Get signature info for a specific method on a class", "Docs", {"docs", "method", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"doc_get_property", "Get property info for a specific property on a class", "Docs", {"docs", "property", "get"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // display_ops
    catalog.add_tool({"display_clipboard_get", "Get clipboard text", "Display", {"display", "clipboard"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_clipboard_set", "Set clipboard text", "Display", {"display", "clipboard"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_dialog_show", "Show a native dialog", "Display", {"display", "dialog"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_mouse_get_position", "Get mouse cursor position", "Display", {"display", "mouse"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_mouse_set_mode", "Set mouse cursor mode", "Display", {"display", "mouse"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_mouse_warp", "Warp mouse cursor to position", "Display", {"display", "mouse"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_capture", "Capture screen image", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_get_count", "Get number of screens", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_get_dpi", "Get screen DPI", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_get_position", "Get screen position", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_get_refresh_rate", "Get screen refresh rate", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_screen_get_size", "Get screen resolution", "Display", {"display", "screen"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_tts_get_voices", "Get available TTS voices", "Display", {"display", "tts"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_tts_speak", "Speak text via TTS", "Display", {"display", "tts"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_tts_stop", "Stop TTS playback", "Display", {"display", "tts"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_create", "Create a sub-window", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_delete", "Delete a sub-window", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_move_to_foreground", "Move window to foreground", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_request_attention", "Flash window taskbar", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_set_flag", "Set a window flag", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_set_mode", "Set window mode", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_set_position", "Set window position", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_set_size", "Set window size", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"display_window_set_title", "Set window title", "Display", {"display", "window"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // os_ops
    catalog.add_tool({"os_alert", "Show a modal alert dialog", "OS", {"os", "alert"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_create_process", "Start a process asynchronously", "OS", {"os", "process"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_execute", "Execute a command and wait for output", "OS", {"os", "execute"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_datetime", "Get current system date and time", "OS", {"os", "datetime"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_environment", "Get an environment variable", "OS", {"os", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_locale", "Get system locale", "OS", {"os", "locale"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_system_fonts", "Get list of system fonts", "OS", {"os", "fonts"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_system_info", "Get system information", "OS", {"os", "system"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_unique_id", "Get machine unique ID", "OS", {"os", "unique_id"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_unix_time", "Get current Unix timestamp", "OS", {"os", "time"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_get_user_data_dir", "Get user data directory", "OS", {"os", "data_dir"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_kill", "Kill a process by PID", "OS", {"os", "kill"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_move_to_trash", "Move file or folder to trash", "OS", {"os", "trash"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_set_environment", "Set an environment variable", "OS", {"os", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"os_shell_open", "Open URL or file in default application", "OS", {"os", "shell"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // render_ops — new
    catalog.add_tool({"render_texture_create_2d", "Create a 2D texture from an image", "Render", {"render", "texture"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_shader_set_code", "Set shader source code", "Render", {"render", "shader"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_shader_get_parameter_list", "Get shader parameter list", "Render", {"render", "shader"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_environment_set_glow", "Set glow effect on environment", "Render", {"render", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_environment_set_ssr", "Set screen-space reflections", "Render", {"render", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_environment_set_tonemap", "Set tone mapping on environment", "Render", {"render", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_environment_set_sdfgi", "Set SDFGI global illumination", "Render", {"render", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_environment_set_volumetric_fog", "Set volumetric fog on environment", "Render", {"render", "environment"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_sky_create", "Create a sky", "Render", {"render", "sky"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_sky_set_material", "Set material on sky", "Render", {"render", "sky"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_particles_set_emitting", "Set particle emitting state", "Render", {"render", "particles"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_particles_restart", "Restart particle system", "Render", {"render", "particles"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_particles_set_lifetime", "Set particle lifetime", "Render", {"render", "particles"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_reflection_probe_create", "Create a reflection probe", "Render", {"render", "reflection_probe"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_decal_create", "Create a decal", "Render", {"render", "decal"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_fog_volume_set_shape", "Set fog volume shape", "Render", {"render", "fog"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_instance_set_visible", "Set instance visibility", "Render", {"render", "instance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_instance_set_layer_mask", "Set instance layer mask", "Render", {"render", "instance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"render_global_shader_parameter_set", "Set global shader parameter", "Render", {"render", "shader"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // scene_tree_ops
    catalog.add_tool({"scene_tree_call_group", "Call a method on all nodes in a group", "Scene", {"scene", "group"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_create_timer", "Create a scene tree timer", "Scene", {"scene", "timer"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_get_nodes_in_group", "Get all nodes in a group", "Scene", {"scene", "group"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_is_paused", "Check if scene tree is paused", "Scene", {"scene", "pause"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_notify_group", "Send a notification to a group", "Scene", {"scene", "group"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_reload_current_scene", "Reload the current scene", "Scene", {"scene", "reload"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_set_debug_collisions", "Toggle collision debug visualization", "Scene", {"scene", "debug"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"scene_tree_set_pause", "Pause or unpause the scene tree", "Scene", {"scene", "pause"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // input_map_ops
    catalog.add_tool({"input_map_action_add_event", "Bind an input event to an action", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_action_erase_event", "Remove an input event from an action", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_action_set_deadzone", "Set deadzone for an action", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_add_action", "Add a new input action", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_erase_action", "Remove an input action", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_get_actions", "Get all input actions", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"input_map_has_action", "Check if an input action exists", "Input", {"input", "map"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // text_ops
    catalog.add_tool({"text_create_font", "Create a font object", "Text", {"text", "font"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_create_shaped_text", "Create a shaped text object", "Text", {"text", "shaped"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_font_set_antialiasing", "Set font antialiasing mode", "Text", {"text", "font"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_font_set_data", "Set font data from file", "Text", {"text", "font"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_font_set_hinting", "Set font hinting mode", "Text", {"text", "font"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_get_system_font_path", "Get system font file path", "Text", {"text", "font"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_has_feature", "Check if a text feature is supported", "Text", {"text", "feature"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_is_locale_right_to_left", "Check if locale is RTL", "Text", {"text", "locale"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_shaped_text_add_string", "Add a string to shaped text", "Text", {"text", "shaped"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"text_shaped_text_get_size", "Get shaped text size", "Text", {"text", "shaped"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // audio_ops — new
    catalog.add_tool({"audio_bus_set_solo", "Set solo on an audio bus", "Audio", {"audio", "bus"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_get_output_device_list", "List audio output devices", "Audio", {"audio", "device"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_set_output_device", "Set audio output device", "Audio", {"audio", "device"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_get_input_device_list", "List audio input devices", "Audio", {"audio", "device"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"audio_set_input_device", "Set audio input device", "Audio", {"audio", "device"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // debug_ops — new
    catalog.add_tool({"debug_get_all_monitors", "Get all performance monitor values", "Debug", {"debug", "performance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_add_custom_monitor", "Add a custom performance monitor", "Debug", {"debug", "performance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_remove_custom_monitor", "Remove a custom performance monitor", "Debug", {"debug", "performance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_get_custom_monitor", "Get custom performance monitor value", "Debug", {"debug", "performance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_list_custom_monitors", "List all custom performance monitors", "Debug", {"debug", "performance"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_query_object_count", "Get total object count", "Debug", {"debug", "objects"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_query_memory_usage", "Get static memory usage", "Debug", {"debug", "memory"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"debug_query_node_count", "Get current node count", "Debug", {"debug", "objects"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // physics_ops — new
    catalog.add_tool({"physics_3d_shape_create", "Create a 3D physics shape", "Physics", {"physics", "3d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_shape_set_data", "Set data on a 3D physics shape", "Physics", {"physics", "3d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_add_shape", "Add a shape to a 3D physics body", "Physics", {"physics", "3d", "body"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_set_param", "Set a parameter on a 3D physics body", "Physics", {"physics", "3d", "body"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_area_set_param", "Set a parameter on a 3D physics area", "Physics", {"physics", "3d", "area"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_space_set_param", "Set a parameter on a 3D physics space", "Physics", {"physics", "3d", "space"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_area_set_transform", "Set transform on a 3D physics area", "Physics", {"physics", "3d", "area"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_3d_body_set_transform", "Set transform on a 3D physics body", "Physics", {"physics", "3d", "body"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_shape_create", "Create a 2D physics shape", "Physics", {"physics", "2d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});
    catalog.add_tool({"physics_2d_shape_set_data", "Set data on a 2D physics shape", "Physics", {"physics", "2d", "shape"}, mcp::JsonValue(mcp::JsonValue::object_tag)});

    // ── 5. Populate BM25 index ──
    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");
}

} // namespace godot_self_driving
