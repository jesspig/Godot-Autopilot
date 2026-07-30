#include "register_all.hpp"
#include "core/log_system.hpp"
#include "tools/property_ops.hpp"
#include "tools/resource_ops.hpp"
#include "tools/script_ops.hpp"
#include "tools/scene_ops.hpp"
#include <mcp/Content.hpp>
#include <mcp/JsonValue.hpp>
#include <unordered_set>
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
#include "tools/tilemap_ops.hpp"
#include "tools/group_ops.hpp"
#include "tools/capture_ops.hpp"
#include "tools/schema_builder.hpp"

namespace godot_self_driving {

namespace {
std::unordered_map<std::string, ToolHandler> g_handlers;
std::unordered_map<std::string, ToolHandler> g_meta_handlers;
const std::unordered_set<std::string> meta_tool_names = {
    "ping", "search_tools", "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"
};

mcp::JsonValue make_schema() {
    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    mcp::JsonValue props(mcp::JsonValue::object_tag);
    s["properties"] = std::move(props);
    mcp::JsonValue req(mcp::JsonValue::array_tag);
    s["required"] = std::move(req);
    return s;
}

void add_required(mcp::JsonValue& schema, const std::string& name) {
    auto* req = schema.Find("required");
    if (req && req->IsArray()) {
        req->PushBack(mcp::JsonValue(name));
    }
}

std::vector<std::string> split_tags(const std::string& csv) {
    if (csv.empty()) return {};
    std::vector<std::string> result;
    size_t start = 0, end;
    while ((end = csv.find(',', start)) != std::string::npos) {
        result.push_back(csv.substr(start, end - start));
        start = end + 1;
    }
    result.push_back(csv.substr(start));
    return result;
}

enum SchemaType { SCHEMA_NONE, SCHEMA_BASIC };

mcp::JsonValue build_schema_for_none_by_name(const std::string& name) {
    if (name.rfind("physics_2d_", 0) == 0 || name.rfind("physics_3d_", 0) == 0) {
        return schema::build_schema({
            {"rid", "integer", "RID of the physics object. Use physics_node_get_rid or resolve_rid to obtain RIDs.", true},
        });
    }

    if (name.rfind("nav_2d_", 0) == 0 || name.rfind("nav_3d_", 0) == 0) {
        return schema::build_schema({
            {"rid", "integer", "RID of the navigation object. Use resolve_rid to look up RIDs.", true},
        });
    }

    if (name.rfind("canvas_item_", 0) == 0 || name.rfind("scenario_", 0) == 0 ||
        name.rfind("camera_", 0) == 0 || name.rfind("light_", 0) == 0 ||
        name.rfind("mesh_", 0) == 0 || name.rfind("material_", 0) == 0 ||
        name.rfind("viewport_", 0) == 0 || name.rfind("particle_", 0) == 0 ||
        name.rfind("environment_", 0) == 0 || name.rfind("fog_", 0) == 0 ||
        name.rfind("reflection_probe_", 0) == 0 || name.rfind("decal_", 0) == 0 ||
        name.rfind("texture_2d_", 0) == 0 || name.rfind("instance_", 0) == 0 ||
        name.rfind("shader_", 0) == 0 || name.rfind("global_shader_", 0) == 0) {
        return schema::build_schema({
            {"rid", "integer", "RID of the render object. Use RenderingServer to create and obtain RIDs.", true},
        });
    }

    if (name.rfind("audio_bus_", 0) == 0 || name.rfind("audio_effect_", 0) == 0 || name.rfind("audio_stream_", 0) == 0) {
        return schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master). Use audio_bus_get_count to list available buses.", false},
            {"rid", "integer", "RID of the audio object.", false},
        });
    }

    if (name.rfind("input_", 0) == 0) {
        return schema::build_schema({
            {"action", "string", "Input action name. Use input_map_get_actions to list available actions.", false},
        });
    }

    if (name.rfind("debug_", 0) == 0) {
        mcp::JsonValue s(mcp::JsonValue::object_tag);
        s["type"] = mcp::JsonValue("object");
        s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
        return s;
    }

    if (name.rfind("display_", 0) == 0) {
        return schema::build_schema({
            {"text", "string", "Display text content.", false},
        });
    }

    if (name.rfind("os_", 0) == 0) {
        return schema::build_schema({
            {"key", "string", "Environment variable name or setting key.", false},
        });
    }

    if (name.rfind("doc_", 0) == 0) {
        return schema::build_schema({
            {"class", "string", "Godot class name (e.g. Node2D, Sprite2D).", true},
        });
    }

    if (name.rfind("scene_tree_", 0) == 0) {
        return schema::build_schema({
            {"group", "string", "Scene group name.", false},
        });
    }

    if (name.rfind("text_", 0) == 0) {
        return schema::build_schema({
            {"rid", "integer", "RID of the text/font object.", false},
        });
    }

    if (name.rfind("tilemap_", 0) == 0) {
        return schema::build_schema({
            {"node_path", "string", "Path to the TileMap node in the scene.", true},
        });
    }

    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    return s;
}

mcp::JsonValue build_schema_for(SchemaType type, const std::string& name) {
    if (type == SCHEMA_NONE) {
        return build_schema_for_none_by_name(name);
    }

    static const std::unordered_map<std::string, mcp::JsonValue> schemas = []{
        std::unordered_map<std::string, mcp::JsonValue> m;

        // ── Scene ──
        m["scene_node_create"] = schema::build_schema({
            {"parent_path", "string", "Parent node path", true},
            {"name", "string", "Node name", true},
            {"type", "string", "Node class type (e.g. Node2D, Sprite2D)", true},
        });
        m["scene_node_delete"] = schema::build_schema({
            {"path", "string", "Node path to delete", true},
        });
        m["scene_tree_get"] = schema::build_schema({});

        // ── Properties ──
        m["property_get"] = schema::build_schema({
            {"path", "string", "Node path", true},
            {"property", "string", "Property name", true},
        });
        m["property_set"] = schema::build_schema({
            {"path", "string", "Node path", true},
            {"property", "string", "Property name", true},
            {"value", "object", "Property value to set", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["property_get_list"] = schema::build_schema({
            {"path", "string", "Node path", true},
        });
        m["signal_connect"] = schema::build_schema({
            {"source_path", "string", "Source node path", true},
            {"signal", "string", "Signal name", true},
            {"target_path", "string", "Target node path", true},
            {"method", "string", "Method to call on target", true},
        });

        // ── Resources ──
        m["resource_load"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_load_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_load_threaded_get_status"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_load_threaded_wait"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_save"] = schema::build_schema({
            {"path", "string", "Path to load from (if no object_id) or destination path", true},
            {"dest_path", "string", "Optional destination path (defaults to path)", false},
            {"class_type", "string", "Class type to instantiate if loading fails", false},
            {"object_id", "integer", "Object ID of an in-memory resource to save", false},
            {"flags", "integer", "Saver flags as bitfield", false},
        });
        m["resource_create"] = schema::build_schema({
            {"type", "string", "Resource class type (e.g. Resource, PackedScene)", true},
        });
        m["resource_duplicate"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["resource_get_type"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["resource_exists"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["resource_list_types"] = schema::build_schema({});
        m["resource_get_extensions"] = schema::build_schema({
            {"type", "string", "Resource type name", true},
        });
        m["resource_list_dir"] = schema::build_schema({
            {"dir", "string", "Directory path", true},
        });
        m["resource_get_uid"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_set_uid"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
            {"uid", "integer", "UID value", true},
        });
        m["resource_remove"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_rename"] = schema::build_schema({
            {"path", "string", "Current resource path", true},
            {"new_path", "string", "New resource path", true},
        });
        m["resource_get_dependencies"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["resource_has_dependency"] = schema::build_schema({
            {"path", "string", "Resource path", true},
            {"dependency_path", "string", "Dependency file path to check", true},
        });
        m["resource_import"] = schema::build_schema({
            {"path", "string", "Resource file path to import", true},
        });
        m["resource_reimport"] = schema::build_schema({
            {"path", "string", "Resource file path to reimport", true},
        });

        // ── Scripts ──
        m["script_execute_gdscript"] = schema::build_schema({
            {"code", "string", "GDScript code to execute", true},
        });
        m["script_load"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });
        m["script_create"] = schema::build_schema({
            {"path", "string", "Script file path to create", true},
            {"content", "string", "Script content", false},
        });
        m["script_attach_to_node"] = schema::build_schema({
            {"node_path", "string", "Node path to attach script to", true},
            {"script_path", "string", "Resource path of the script (.gd file)", true},
        });
        m["script_detach_from_node"] = schema::build_schema({
            {"node_path", "string", "Node path to detach script from", true},
        });
        m["script_get_property"] = schema::build_schema({
            {"node_path", "string", "Node path", true},
            {"property", "string", "Property name", true},
        });
        m["script_set_property"] = schema::build_schema({
            {"node_path", "string", "Node path", true},
            {"property", "string", "Property name", true},
            {"value", "object", "Property value", true},
        });
        m["script_call_function"] = schema::build_schema({
            {"node_path", "string", "Node path", true},
            {"function", "string", "Function name", true},
            {"args", "array", "Function arguments", false},
        });
        m["script_reload"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });
        m["script_get_variable_list"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });

        // ── Editor ──
        m["editor_get_selection"] = schema::build_schema({});
        m["editor_set_selection"] = schema::build_schema({
            {"paths", "array", "List of node paths to select", true},
        });
        m["editor_get_edited_scene_root"] = schema::build_schema({});
        m["editor_save_scene"] = schema::build_schema({});
        m["editor_save_all_scenes"] = schema::build_schema({});
        m["editor_reload_scene"] = schema::build_schema({});
        m["editor_inspect_object"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID to inspect", true},
        });
        m["editor_undo_redo_start"] = schema::build_schema({
            {"name", "string", "Undo/redo action name", true},
        });
        m["editor_undo_redo_commit"] = schema::build_schema({});
        m["editor_undo_redo_add_do"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["editor_undo_redo_add_undo"] = schema::build_schema({
            {"object", "object", "Object to call method on", true},
            {"method", "string", "Method name", true},
            {"args", "array", "Method arguments", false},
        });
        m["editor_file_system_get_resources"] = schema::build_schema({
            {"path", "string", "Directory path to list", false},
        });
        m["editor_file_system_scan"] = schema::build_schema({});
        m["editor_import_resource"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["editor_set_main_scene"] = schema::build_schema({
            {"path", "string", "Scene file path", true},
        });
        m["editor_play_current_scene"] = schema::build_schema({});
        m["editor_stop_playing"] = schema::build_schema({});
        m["editor_get_resource_filesystem"] = schema::build_schema({});
        m["editor_get_plugin_list"] = schema::build_schema({});
        m["editor_set_plugin_enabled"] = schema::build_schema({
            {"plugin", "string", "Plugin name", true},
            {"enabled", "boolean", "Whether to enable or disable", true},
        });
        m["editor_new_scene"] = schema::build_schema({
            {"type", "string", "Node class type (default: Node)", false},
            {"name", "string", "Node name (default: NewRoot)", false},
        });
        m["editor_open_scene"] = schema::build_schema({
            {"path", "string", "Path to the scene file to open (e.g., res://game.tscn)", true},
        });
        m["editor_save_scene_as"] = schema::build_schema({
            {"path", "string", "File path to save scene as", true},
        });
        m["editor_new_text_resource"] = schema::build_schema({
            {"path", "string", "File path to save the resource", true},
            {"source_code", "string", "Text content to write", true},
        });

        // ── Config ──
        m["project_settings_get"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
        });
        m["project_settings_set"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["project_settings_has"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
        });
        m["project_settings_save"] = schema::build_schema({});
        m["engine_get_version"] = schema::build_schema({});
        m["engine_get_fps"] = schema::build_schema({});
        m["engine_get_frames_drawn"] = schema::build_schema({});
        m["engine_set_time_scale"] = schema::build_schema({
            {"scale", "number", "Time scale factor", true},
        });
        m["engine_get_time_scale"] = schema::build_schema({});
        m["engine_set_max_fps"] = schema::build_schema({
            {"fps", "integer", "Maximum FPS", true},
        });
        m["editor_settings_get"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });
        m["editor_settings_set"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
            {"value", "object", "Setting value", true},
        });
        m["editor_settings_has"] = schema::build_schema({
            {"name", "string", "Editor setting name", true},
        });

        // ── Text (file_write) ──
        m["file_write"] = schema::build_schema({
            {"path", "string", "File path to write", true},
            {"content", "string", "Content to write", true},
            {"mode", "string", "Write mode: WRITE or APPEND (default: WRITE)", false},
        });

        // ── Group ──
        m["group_add_node_to_group"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to add the node to", true},
        });
        m["group_remove_node_from_group"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to remove the node from", true},
        });
        m["group_has_node_in_group"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to check", true},
        });

        // ── Capture ──
        m["editor_capture_viewport"] = schema::build_schema({});

        // ── InputMap ──
        m["input_map_add_action"] = schema::build_schema({
            {"action", "string", "Action name (e.g. mario_jump, move_left)", true},
            {"deadzone", "number", "Deadzone value (default: 0.5)", false},
        });
        m["input_map_erase_action"] = schema::build_schema({
            {"action", "string", "Action name to remove", true},
        });
        m["input_map_get_actions"] = schema::build_schema({});
        m["input_map_has_action"] = schema::build_schema({
            {"action", "string", "Action name to check", true},
        });
        m["input_map_action_add_event"] = schema::build_schema({
            {"action", "string", "Action name to bind event to", true},
            {"event", "object", "Input event object (must include \"class\":\"InputEventKey\" etc.)", true},
        });
        m["input_map_action_erase_event"] = schema::build_schema({
            {"action", "string", "Action name", true},
            {"event_index", "integer", "Index of event to remove", true},
        });
        m["input_map_action_set_deadzone"] = schema::build_schema({
            {"action", "string", "Action name", true},
            {"deadzone", "number", "Deadzone value (0.0 to 1.0)", true},
        });
        m["input_map_persist"] = schema::build_schema({});

        return m;
    }();

    auto it = schemas.find(name);
    if (it != schemas.end()) return it->second;

    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    return s;
}

} // namespace

mcp::JsonValue call_handler(const std::string& name, const mcp::JsonValue& args) {
    auto it = g_handlers.find(name);
    if (it != g_handlers.end()) {
        return it->second(args);
    }
    if (meta_tool_names.count(name)) {
        auto meta_it = g_meta_handlers.find(name);
        if (meta_it != g_meta_handlers.end()) {
            return meta_it->second(args);
        }
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("meta tool '" + name + "' cannot be invoked via this path — try calling it directly as a top-level tool instead");
        return e;
    }
    mcp::JsonValue e(mcp::JsonValue::object_tag);
    e["error"] = mcp::JsonValue("domain tool '" + name + "' not found — use search_tools to discover available tools");
    return e;
}

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port) {
    // ── 1. system_status handler (inline — unique lambda captures port/start) ──
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

    // ── 2. Populate handler registry via X-macro ──
    #define TOOL_ENTRY(id, name_str, desc, cat, tags_csv, handler_fn, schema_type) \
        g_handlers[name_str] = handler_fn;
    #include "tool_defs.def"
    #undef TOOL_ENTRY

    // ── 3. Populate catalog defaults ──
    catalog.populate_default_tools();

    // ── 4. Register directly-called meta-tools ──
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

                    mcp::JsonValue handler_result = call_handler(name, tool_args);
                    return handler_result.Dump();
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

    // ── 5. Catalog entries via X-macro ──
    #define TOOL_ENTRY(id, name_str, desc, cat, tags_csv, handler_fn, schema_type) \
        catalog.add_tool({name_str, desc, cat, split_tags(tags_csv), build_schema_for(schema_type, name_str)});
    #include "tool_defs.def"
    #undef TOOL_ENTRY

    // ── 6. Populate meta handlers (for call_handler fallback) ──
    g_meta_handlers["ping"] = [](const mcp::JsonValue&) -> mcp::JsonValue {
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = mcp::JsonValue("pong");
        return r;
    };
    g_meta_handlers["search_tools"] = [&index](const mcp::JsonValue& args) -> mcp::JsonValue {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools, "search_tools called via meta handler");
        if (args.Find("query") == nullptr) {
            mcp::JsonValue err(mcp::JsonValue::object_tag);
            err["error"] = mcp::JsonValue("missing required parameter: query");
            return err;
        }
        Bm25Index::SearchQuery query;
        query.text = args["query"].GetString();
        if (auto* cat = args.Find("category")) {
            if (!cat->IsNull()) query.category = cat->GetString();
        }
        if (auto* tags_val = args.Find("tags")) {
            if (!tags_val->IsNull() && tags_val->IsArray()) {
                std::vector<std::string> tags;
                for (auto& t : tags_val->GetArray()) tags.push_back(t.GetString());
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
        return j;
    };
    g_meta_handlers["list_categories"] = [&catalog](const mcp::JsonValue&) -> mcp::JsonValue {
        auto cats = catalog.get_categories();
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        mcp::JsonValue arr(mcp::JsonValue::array_tag);
        for (auto& c : cats) {
            mcp::JsonValue item(mcp::JsonValue::object_tag);
            item["id"] = mcp::JsonValue(c);
            item["name"] = mcp::JsonValue(c);
            int count = 0;
            for (auto* t : catalog.get_all_tools()) {
                if (t->category == c) ++count;
            }
            item["tool_count"] = mcp::JsonValue(static_cast<int64_t>(count));
            arr.PushBack(std::move(item));
        }
        j["categories"] = std::move(arr);
        return j;
    };
    g_meta_handlers["get_tool_detail"] = [&catalog](const mcp::JsonValue& args) -> mcp::JsonValue {
        if (args.Find("name") == nullptr) {
            mcp::JsonValue err(mcp::JsonValue::object_tag);
            err["error"] = mcp::JsonValue("missing required parameter: name");
            return err;
        }
        std::string name = args["name"].GetString();
        auto* info = catalog.get_tool(name);
        if (!info) {
            mcp::JsonValue err(mcp::JsonValue::object_tag);
            err["error"] = mcp::JsonValue("tool not found: " + name);
            return err;
        }
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        mcp::JsonValue t(mcp::JsonValue::object_tag);
        t["name"] = mcp::JsonValue(info->name);
        t["description"] = mcp::JsonValue(info->description);
        t["category"] = mcp::JsonValue(info->category);
        mcp::JsonValue tags(mcp::JsonValue::array_tag);
        for (auto& tg : info->tags) tags.PushBack(mcp::JsonValue(tg));
        t["tags"] = std::move(tags);
        t["input_schema"] = info->input_schema;
        j["tool"] = std::move(t);
        return j;
    };
    g_meta_handlers["call_tool"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        if (args.Find("name") == nullptr) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("missing required parameter: name");
            return e;
        }
        std::string name = args["name"].GetString();
        mcp::JsonValue tool_args = [&]{
            if (auto* a = args.Find("arguments")) return *a;
            return mcp::JsonValue(mcp::JsonValue::object_tag);
        }();
        return call_handler(name, tool_args);
    };
    g_meta_handlers["batch_execute"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        return code_exec_ops::handle_batch_execute(args);
    };
    g_meta_handlers["code_execute"] = [](const mcp::JsonValue& args) -> mcp::JsonValue {
        return code_exec_ops::handle_code_execute(args);
    };

    // ── 7. Populate BM25 index ──
    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");

    // ── 8. Sync check: g_handlers vs catalog ──
    int missing_from_catalog = 0;
    for (auto& [name, _] : g_handlers) {
        if (catalog.get_tool(name) == nullptr) {
            LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                "handler '" + name + "' missing from catalog — adding");
            catalog.add_tool({name, name, "Auto", {"auto"}, make_schema()});
            ++missing_from_catalog;
        }
    }
    if (missing_from_catalog > 0) {
        LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
            std::to_string(missing_from_catalog) + " handlers auto-added to catalog");
    }

    // Reverse sync check: catalog → g_handlers
    for (auto* ctool : catalog.get_all_tools()) {
        if (g_handlers.find(ctool->name) == g_handlers.end() && meta_tool_names.find(ctool->name) == meta_tool_names.end()) {
            LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                "WARNING: tool '" + ctool->name + "' exists in catalog but has no handler in g_handlers");
        }
    }
}

} // namespace godot_self_driving
