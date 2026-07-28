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
#include "tools/input_ops.hpp"
#include "tools/editor_ops.hpp"
#include "tools/config_ops.hpp"
#include "tools/debug_ops.hpp"
#include "tools/doc_ops.hpp"

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

    // ── 4. Populate BM25 index ──
    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");
}

} // namespace godot_self_driving
