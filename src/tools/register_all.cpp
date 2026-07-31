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
#include "tools/log_ops.hpp"
#include "tools/os_ops.hpp"
#include "tools/scene_tree_ops.hpp"
#include "tools/spriteframes_ops.hpp"
#include "tools/text_ops.hpp"
#include "tools/tilemap_ops.hpp"
#include "tools/tileset_ops.hpp"
#include "tools/group_ops.hpp"
#include "tools/capture_ops.hpp"
#include "tools/debugger_ops.hpp"
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
    if (name.rfind("scene_tree_", 0) == 0) {
        return schema::build_schema({
            {"group", "string", "Scene group name.", false},
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
    static const std::unordered_map<std::string, mcp::JsonValue> schemas = []{
        std::unordered_map<std::string, mcp::JsonValue> m;

        // ── Scene ──
        m["scene_node_create"] = schema::build_schema({
            {"parent_path", "string", "Parent node path (omit to create root node)", false},
            {"name", "string", "Node name (default: NewNode)", true},
            {"type", "string", "Node class type (e.g. Node2D, Sprite2D, default: Node)", true},
        });
        m["scene_node_delete"] = schema::build_schema({
            {"path", "string", "Node path to delete", true},
        });
        m["scene_tree_get"] = schema::build_schema({});
        m["scene_instance"] = schema::build_schema({
            {"path", "string", "Path to the .tscn scene file to instantiate", true},
            {"parent_path", "string", "Parent node path (default: edited scene root)", false},
            {"name", "string", "Node name (default: scene root name)", false},
            {"owner", "boolean", "Set owner so nodes are saved with the scene (default: true)", false},
        });

        // ── SceneTree ──
        m["scene_tree_call_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
            {"method", "string", "Method name to call on group nodes", true},
            {"arguments", "array", "Optional arguments to pass to the method", false},
        });
        m["scene_tree_create_timer"] = schema::build_schema({
            {"delay_sec", "number", "Timer delay in seconds", true},
            {"process_always", "boolean", "Process when paused (default: true)", false},
            {"process_in_physics", "boolean", "Process in physics step (default: false)", false},
        });
        m["scene_tree_get_nodes_in_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
        });
        m["scene_tree_is_paused"] = schema::build_schema({});
        m["scene_tree_notify_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
            {"notification", "integer", "Notification constant (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3)", true},
        });
        m["scene_tree_reload_current_scene"] = schema::build_schema({});
        m["scene_tree_set_debug_collisions"] = schema::build_schema({
            {"enabled", "boolean", "Enable collision debug visualization", true},
        });
        m["scene_tree_set_pause"] = schema::build_schema({
            {"paused", "boolean", "Pause state", true},
        });

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
            {"type_hint", "string", "Resource type hint (e.g. PackedScene, Texture2D)", false},
        });
        m["resource_load_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
            {"type_hint", "string", "Resource type hint", false},
            {"use_sub_threads", "boolean", "Use sub-threads for background loading", false},
        });
        m["resource_load_threaded_get_status"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_load_threaded_wait"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["resource_save"] = schema::build_schema({
            {"path", "string", "Source path to load from, or destination path if object_id provided", true},
            {"dest_path", "string", "Destination path (defaults to path)", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"class_type", "string", "Class type to instantiate if resource not found", false},
            {"object_id", "integer", "Object ID of an in-memory resource to save", false},
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"flags", "integer", "Saver flags as bitfield (see ResourceSaver.SaverFlags)", false},
        });
        m["resource_create"] = schema::build_schema({
            {"type", "string", "Resource class type (e.g. Resource, PackedScene)", true},
            {"name", "string", "Optional resource name for identification", false},
        });
        m["resource_duplicate"] = schema::build_schema({
            {"path", "string", "Resource path to load and duplicate", true},
            {"deep", "boolean", "Deep duplicate (true) or shallow (false, default)", false},
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
        m["resource_set_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"object_id", "integer", "Object ID of an in-memory resource", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"path", "string", "Resource path to load and modify", false},
            {"property", "string", "Property name to set", true},
            {"value", "object", "Property value to set", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["resource_get_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"object_id", "integer", "Object ID of an in-memory resource", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"path", "string", "Resource path to load and read", false},
            {"property", "string", "Property name to read", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });

        // ── Scripts ──
        m["script_execute_gdscript"] = schema::build_schema({
            {"expression", "string", "GDScript expression or code to execute", true},
        });
        m["script_load"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });
        m["script_create"] = schema::build_schema({
            {"path", "string", "Script file path to create", true},
            {"source_code", "string", "Script content", false},
            {"overwrite", "boolean", "Overwrite existing file (default: false)", false},
        });
        m["script_attach_to_node"] = schema::build_schema({
            {"node_path", "string", "Node path to attach script to", true},
            {"script_path", "string", "Resource path of the script (.gd file)", true},
        });
        m["script_detach_from_node"] = schema::build_schema({
            {"node_path", "string", "Node path to detach script from", true},
        });
        m["script_get_property"] = schema::build_schema({
            {"script_path", "string", "Resource path of the script (.gd file) to read a default property from", false},
            {"node_path", "string", "Node path", false},
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
        m["editor_close_scene"] = schema::build_schema({});

        // ── Config ──
        m["project_settings_get"] = schema::build_schema({
            {"name", "string", "Project setting name", true},
            {"default", "object", "Default value if setting does not exist", false},
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

        // ── Text (explicit) ──
        m["text_create_font"] = schema::build_schema({});
        m["text_create_shaped_text"] = schema::build_schema({
            {"direction", "integer", "Text direction: 0=auto, 1=ltr, 2=rtl (default: 0)", false},
            {"orientation", "integer", "Text orientation: 0=horizontal, 1=vertical (default: 0)", false},
        });
        m["text_font_set_antialiasing"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"antialiasing", "integer", "Font antialiasing mode (0=none, 1=gray, 2=grayscale, 3=subpixel)", true},
        });
        m["text_font_set_data"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"data", "string", "Path to a font file (.ttf, .otf, .woff2)", true},
        });
        m["text_font_set_hinting"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"hinting", "integer", "Font hinting mode (0=none, 1=light, 2=normal)", true},
        });
        m["text_get_system_font_path"] = schema::build_schema({
            {"font_name", "string", "System font name (e.g. Arial)", true},
            {"weight", "integer", "Font weight (default: 400)", false},
            {"stretch", "integer", "Font stretch percentage (default: 100)", false},
            {"italic", "boolean", "Request italic variant (default: false)", false},
        });
        m["text_has_feature"] = schema::build_schema({
            {"feature", "integer", "TextServer feature flag (TextServer.Feature enum: 1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, 32=font_lcd_subpixel, 64=font_autohinter, 128=font_subpixel_positioning, 256=font_system, 512=font_variable, 1024=context_sensitive_cleartype, 2048=fast_path, 4096=shaping_fallback, 8192=unicode_security, 16384=has_rid)", true},
        });
        m["text_is_locale_right_to_left"] = schema::build_schema({
            {"locale", "string", "Locale code (e.g. ar, he, en)", true},
        });
        m["text_shaped_text_add_string"] = schema::build_schema({
            {"shaped_rid", "integer", "RID of the shaped text (from text_create_shaped_text)", true},
            {"text", "string", "Text string to add", true},
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"size", "integer", "Font size in pixels", true},
            {"language", "string", "Text language code (optional)", false},
        });
        m["text_shaped_text_get_size"] = schema::build_schema({
            {"shaped_rid", "integer", "RID of the shaped text (from text_create_shaped_text)", true},
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
            {"action", "string", "Action name identifier (e.g. \"mario_jump\", \"move_left\", \"ui_accept\") — this is a programmatic identifier, not a display name", true},
            {"deadzone", "number", "Deadzone value (default: 0.5)", false},
        });
        m["input_map_erase_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to remove (e.g. \"mario_jump\")", true},
        });
        m["input_map_get_actions"] = schema::build_schema({});
        m["input_map_has_action"] = schema::build_schema({
            {"action", "string", "Action name identifier to check for existence (e.g. \"ui_accept\")", true},
        });
        m["input_map_action_add_event"] = schema::build_schema({
            {"action", "string", "Action name identifier to bind event to (e.g. \"mario_jump\")", true},
            {"event", "object", "Input event object (must include \"class\":\"InputEventKey\" etc.) — physical_keycode/keycode accept numeric key codes or KEY_* name strings (e.g. \"KEY_A\" or 65)", true},
        });
        m["input_map_action_erase_event"] = schema::build_schema({
            {"action", "string", "Action name identifier whose event to remove (e.g. \"mario_jump\")", true},
            {"event_index", "integer", "Index of event to remove", true},
        });
        m["input_map_action_set_deadzone"] = schema::build_schema({
            {"action", "string", "Action name identifier to set deadzone for (e.g. \"mario_jump\")", true},
            {"deadzone", "number", "Deadzone value (0.0 to 1.0)", true},
        });
        m["input_map_persist"] = schema::build_schema({});

        // ── Input (explicit schemas) ──
        m["input_action_press"] = schema::build_schema({
            {"action", "string", "Input action name to press", true},
            {"strength", "number", "Action strength (default: 1.0)", false},
        });
        m["input_action_release"] = schema::build_schema({
            {"action", "string", "Input action name to release", true},
        });
        m["input_is_action_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["input_is_action_just_pressed"] = schema::build_schema({
            {"action", "string", "Input action name to check", true},
        });
        m["input_key_press"] = schema::build_schema({
            {"key", "string", "Key name to press (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["input_key_release"] = schema::build_schema({
            {"key", "string", "Key name to release (e.g. \"space\", \"A\", \"Shift\")", true},
        });
        m["input_mouse_move"] = schema::build_schema({
            {"position", "object", "Mouse position (Vector2 with x and y fields)", true},
            {"relative", "object", "Relative movement (Vector2 with x and y fields)", false},
        });
        m["input_mouse_button_press"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["input_mouse_button_release"] = schema::build_schema({
            {"button", "string", "Mouse button: left, right, or middle", true},
            {"position", "object", "Mouse position (Vector2 with x and y fields)", false},
        });
        m["input_gamepad_simulate"] = schema::build_schema({
            {"action", "string", "Gamepad action: STOP or VIBRATE", true},
            {"device", "integer", "Gamepad device index (default: 0)", false},
            {"weak_magnitude", "number", "Weak motor magnitude 0-1 (VIBRATE)", false},
            {"strong_magnitude", "number", "Strong motor magnitude 0-1 (VIBRATE)", false},
            {"duration", "number", "Vibration duration in seconds", false},
        });

        // ── Physics (explicit) ──
        m["physics_2d_ray_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space (omit to auto-detect from editor scene)", false},
            {"from", "object", "Ray origin (Vector2 with x and y fields)", true},
            {"to", "object", "Ray destination (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"hit_from_inside", "boolean", "Should hit shapes the ray starts inside of (default: false)", false},
        });
        m["physics_2d_space_get_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
        });
        m["physics_2d_shape_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"shape_rid", "integer", "RID of the shape to cast (from physics_2d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform2D: origin/rotation/scale)", false},
            {"motion", "object", "Motion vector (Vector2 with x and y fields)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_point_query"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"position", "object", "Query position (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_intersect_shape"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"shape_rid", "integer", "RID of the shape to intersect (from physics_2d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform2D: origin/rotation/scale)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_intersect_point"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 2D physics space", true},
            {"position", "object", "Query position (Vector2 with x and y fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_2d_body_create"] = schema::build_schema({});
        m["physics_2d_body_set_mode"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"mode", "number", "Body mode: 0=static,1=kinematic,2=rigid,3=rigid_linear", true},
        });
        m["physics_2d_body_apply_force"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"force", "object", "Force vector (Vector2 with x and y fields)", true},
            {"position", "object", "Force application point (Vector2 with x and y fields)", false},
        });
        m["physics_2d_body_apply_impulse"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"impulse", "object", "Impulse vector (Vector2 with x and y fields)", true},
            {"position", "object", "Impulse application point (Vector2 with x and y fields)", false},
        });
        m["physics_2d_body_set_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
            {"value", "object", "State value (transform object, Vector2, or bool)", true},
        });
        m["physics_2d_body_get_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
        });
        m["physics_2d_joint_create"] = schema::build_schema({
            {"type", "string", "Joint type: pin, groove, damped_spring", true},
            {"anchor", "object", "Pin joint anchor (Vector2 with x and y fields)", false},
            {"body_a", "integer", "RID of the first body", false},
            {"body_b", "integer", "RID of the second body", false},
            {"groove1_a", "object", "Groove joint first groove point (Vector2)", false},
            {"groove2_a", "object", "Groove joint second groove point (Vector2)", false},
            {"anchor_b", "object", "Groove joint anchor on body B (Vector2)", false},
            {"anchor_a", "object", "Damped spring anchor on body A (Vector2)", false},
        });
        m["physics_2d_area_create"] = schema::build_schema({});
        m["physics_2d_area_set_monitorable"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics area", true},
            {"monitorable", "boolean", "Whether the area can be monitored by other areas", true},
        });
        m["physics_2d_shape_create"] = schema::build_schema({});
        m["physics_2d_shape_set_data"] = schema::build_schema({
            {"rid", "integer", "RID of the 2D physics shape", true},
            {"data", "object", "Shape data (e.g. {\"radius\": 10})", true},
        });

        // ── Physics 3D (explicit) ──
        m["physics_3d_space_get_direct_state"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
        });
        m["physics_3d_ray_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"from", "object", "Ray origin (Vector3 with x, y and z fields)", true},
            {"to", "object", "Ray destination (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"hit_from_inside", "boolean", "Should hit shapes the ray starts inside of (default: false)", false},
            {"hit_back_faces", "boolean", "Should hit back faces (default: false)", false},
        });
        m["physics_3d_shape_cast"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"shape_rid", "integer", "RID of the shape to cast (from physics_3d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"motion", "object", "Motion vector (Vector3 with x, y and z fields)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_point_query"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"position", "object", "Query position (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_intersect_shape"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"shape_rid", "integer", "RID of the shape to intersect (from physics_3d_shape_create)", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_intersect_point"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"position", "object", "Query position (Vector3 with x, y and z fields)", true},
            {"collision_mask", "integer", "Collision layer mask", false},
            {"exclude", "array", "Array of RIDs to exclude from collision", false},
            {"collide_with_bodies", "boolean", "Should collide with physics bodies (default: true)", false},
            {"collide_with_areas", "boolean", "Should collide with areas (default: false)", false},
            {"max_results", "integer", "Maximum number of results (default: 32)", false},
        });
        m["physics_3d_body_create"] = schema::build_schema({});
        m["physics_3d_body_set_mode"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"mode", "number", "Body mode: 0=static,1=kinematic,2=rigid,3=rigid_linear", true},
        });
        m["physics_3d_body_apply_force"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"force", "object", "Force vector (Vector3 with x, y and z fields)", true},
            {"position", "object", "Force application point (Vector3 with x, y and z fields)", false},
        });
        m["physics_3d_body_apply_impulse"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"impulse", "object", "Impulse vector (Vector3 with x, y and z fields)", true},
            {"position", "object", "Impulse application point (Vector3 with x, y and z fields)", false},
        });
        m["physics_3d_body_set_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
            {"value", "object", "State value (transform object, Vector3, or bool)", true},
        });
        m["physics_3d_body_get_state"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"state", "number", "Body state: 0=transform,1=linear_vel,2=angular_vel,3=sleeping,4=can_sleep", true},
        });
        m["physics_3d_joint_create"] = schema::build_schema({
            {"type", "string", "Joint type: pin, hinge, slider, cone_twist, generic_6dof", true},
            {"body_a_rid", "integer", "RID of the first body", true},
            {"body_b_rid", "integer", "RID of the second body", false},
            {"local_a", "object", "Pin joint local anchor on body A (Vector3)", false},
            {"local_b", "object", "Pin joint local anchor on body B (Vector3)", false},
            {"hinge_a", "object", "Hinge joint transform on body A (Transform3D)", false},
            {"hinge_b", "object", "Hinge joint transform on body B (Transform3D)", false},
            {"ref_a", "object", "Reference transform on body A (Transform3D)", false},
            {"ref_b", "object", "Reference transform on body B (Transform3D)", false},
        });
        m["physics_3d_area_create"] = schema::build_schema({});
        m["physics_3d_area_set_monitorable"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"monitorable", "boolean", "Whether the area can be monitored by other areas", true},
        });
        m["physics_3d_body_apply_torque"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"torque", "object", "Torque vector (Vector3 with x, y and z fields)", true},
        });
        m["physics_3d_body_set_axis_lock"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"axis", "number", "Axis: 1=linear_x,2=linear_y,4=linear_z,8=angular_x,16=angular_y,32=angular_z", true},
            {"lock", "boolean", "Lock (true) or unlock (false) the axis", true},
        });
        m["physics_3d_body_add_collision_exception"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"excepted_body_rid", "integer", "RID of the body to exclude from collision", true},
        });
        m["physics_3d_body_remove_collision_exception"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"excepted_body_rid", "integer", "RID of the body to remove from collision exceptions", true},
        });
        m["physics_3d_joint_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics joint", true},
            {"solver_priority", "integer", "Joint solver priority", false},
            {"disable_collision", "boolean", "Disable collisions between the jointed bodies", false},
        });
        m["physics_3d_area_set_space_override"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"space_rid", "integer", "RID of the 3D physics space to attach the area to", true},
        });
        m["physics_3d_space_set_gravity"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Solver iterations", false},
        });
        m["physics_3d_space_set_debug"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics space", true},
            {"solver_iterations", "integer", "Solver iterations", false},
            {"contact_max_allowed_penetration", "number", "Maximum allowed penetration depth", false},
        });
        m["physics_3d_soft_body_create"] = schema::build_schema({});
        m["physics_3d_soft_body_set_mesh"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D soft body", true},
            {"mesh_rid", "integer", "RID of the mesh to assign to the soft body", true},
        });
        m["physics_3d_shape_create"] = schema::build_schema({});
        m["physics_3d_shape_set_data"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics shape", true},
            {"data", "object", "Shape data (e.g. {\"radius\": 0.5})", true},
        });
        m["physics_3d_body_add_shape"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"shape_rid", "integer", "RID of the shape to add", true},
            {"transform", "object", "Shape transform (Transform3D with basis array and origin)", false},
            {"disabled", "boolean", "Add the shape as disabled (default: false)", false},
        });
        m["physics_3d_body_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"param", "number", "Body parameter index (see PhysicsServer3D.BodyParameter)", true},
            {"value", "number", "Parameter value", true},
        });
        m["physics_3d_area_set_param"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"param", "number", "Area parameter index (see PhysicsServer3D.AreaParameter)", true},
            {"value", "object", "Parameter value", true},
        });
        m["physics_3d_space_set_param"] = schema::build_schema({
            {"space_rid", "integer", "RID of the 3D physics space", true},
            {"param", "number", "Space parameter index (see PhysicsServer3D.SpaceParameter)", true},
            {"value", "number", "Parameter value", true},
        });
        m["physics_3d_area_set_transform"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics area", true},
            {"transform", "object", "Transform3D with basis array and origin", true},
        });
        m["physics_3d_body_set_transform"] = schema::build_schema({
            {"rid", "integer", "RID of the 3D physics body", true},
            {"transform", "object", "Transform3D with basis array and origin", true},
        });
        m["physics_node_get_rid"] = schema::build_schema({
            {"path", "string", "Node path of a CollisionObject2D or CollisionObject3D node", true},
        });
        m["resolve_object"] = schema::build_schema({
            {"object_id", "integer", "Object instance ID (ObjectID) to resolve", true},
        });

        // ── Navigation (explicit) ──
        m["nav_2d_map_create"] = schema::build_schema({
            {"active", "boolean", "Set the map active (default: false)", false},
        });
        m["nav_2d_region_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"enabled", "boolean", "Whether the region is enabled (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_2d_path_query"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"origin", "object", "Path origin (Vector2 with x and y fields)", true},
            {"destination", "object", "Path destination (Vector2 with x and y fields)", true},
            {"optimize", "boolean", "Optimize the path (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_2d_agent_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 2D navigation map", true},
            {"position", "object", "Agent position (Vector2 with x and y fields)", true},
            {"radius", "number", "Agent avoidance radius", false},
            {"max_speed", "number", "Agent maximum speed", false},
            {"avoidance_enabled", "boolean", "Enable avoidance (default: false)", false},
        });
        m["nav_2d_agent_set_target"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 2D navigation agent", true},
            {"velocity", "object", "Agent velocity (Vector2 with x and y fields)", true},
        });
        m["nav_3d_map_create"] = schema::build_schema({
            {"active", "boolean", "Set the map active (default: false)", false},
            {"cell_size", "number", "Map cell size in meters", false},
            {"cell_height", "number", "Map cell height in meters", false},
            {"up", "object", "Up direction (Vector3 with x, y and z fields)", false},
        });
        m["nav_3d_map_set_cell_size"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"cell_size", "number", "Cell size in meters", true},
        });
        m["nav_3d_region_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"enabled", "boolean", "Whether the region is enabled (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_3d_region_set_nav_mesh"] = schema::build_schema({
            {"region_rid", "integer", "RID of the 3D navigation region", true},
            {"mesh_path", "string", "Path to a NavigationMesh resource (.tres, .obj)", true},
        });
        m["nav_3d_path_query"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"origin", "object", "Path origin (Vector3 with x, y and z fields)", true},
            {"destination", "object", "Path destination (Vector3 with x, y and z fields)", true},
            {"optimize", "boolean", "Optimize the path (default: true)", false},
            {"navigation_layers", "integer", "Navigation layers bitmask (default: 1)", false},
        });
        m["nav_3d_path_query_segment"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"start", "object", "Segment start (Vector3 with x, y and z fields)", true},
            {"end", "object", "Segment end (Vector3 with x, y and z fields)", true},
            {"use_collision", "boolean", "Use collision when finding closest point (default: false)", false},
        });
        m["nav_3d_agent_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"position", "object", "Agent position (Vector3 with x, y and z fields)", true},
            {"radius", "number", "Agent avoidance radius", false},
            {"height", "number", "Agent height for avoidance", false},
            {"max_speed", "number", "Agent maximum speed", false},
            {"use_3d_avoidance", "boolean", "Use 3D avoidance (default: false)", false},
        });
        m["nav_3d_agent_set_velocity"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 3D navigation agent", true},
            {"velocity", "object", "Agent velocity (Vector3 with x, y and z fields)", true},
        });
        m["nav_3d_agent_get_next_path"] = schema::build_schema({
            {"agent_rid", "integer", "RID of the 3D navigation agent", true},
        });
        m["nav_3d_obstacle_create"] = schema::build_schema({
            {"map_rid", "integer", "RID of the 3D navigation map", true},
            {"position", "object", "Obstacle position (Vector3 with x, y and z fields)", true},
            {"radius", "number", "Obstacle radius", false},
            {"height", "number", "Obstacle height", false},
        });

        // ── Render (explicit) ──
        m["canvas_item_create"] = schema::build_schema({});
        m["canvas_item_draw_rect"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"rect", "object", "Rectangle (Rect2 with position and size Vector2 fields)", true},
            {"color", "object", "Fill color (Color with r, g, b, a fields)", true},
            {"antialiased", "boolean", "Antialias the rectangle edges (default: false)", false},
        });
        m["canvas_item_draw_circle"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"position", "object", "Circle center (Vector2 with x and y fields)", true},
            {"color", "object", "Fill color (Color with r, g, b, a fields)", true},
            {"radius", "number", "Circle radius (default: 1.0)", false},
            {"antialiased", "boolean", "Antialias the circle edges (default: false)", false},
        });
        m["canvas_item_draw_texture"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"texture_rid", "integer", "RID of the texture to draw", true},
            {"rect", "object", "Destination rectangle (Rect2 with position and size Vector2 fields)", true},
            {"tile", "boolean", "Tile the texture (default: false)", false},
            {"modulate", "object", "Modulate color (Color with r, g, b, a fields)", false},
            {"transpose", "boolean", "Transpose the texture (default: false)", false},
        });
        m["canvas_item_draw_line"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"from", "object", "Line start point (Vector2 with x and y fields)", true},
            {"to", "object", "Line end point (Vector2 with x and y fields)", true},
            {"color", "object", "Line color (Color with r, g, b, a fields)", true},
            {"width", "number", "Line width (default: -1.0, use project setting)", false},
            {"antialiased", "boolean", "Antialias the line (default: false)", false},
        });
        m["canvas_item_set_transform"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"x", "number", "Transform X axis scale (default: 1.0)", false},
            {"y", "number", "Transform Y axis scale (default: 1.0)", false},
            {"origin_x", "number", "Transform origin X offset (default: 0.0)", false},
            {"origin_y", "number", "Transform origin Y offset (default: 0.0)", false},
        });
        m["canvas_item_set_visible"] = schema::build_schema({
            {"canvas_item_rid", "integer", "RID of the canvas item", true},
            {"visible", "boolean", "Visibility state (default: true)", false},
        });
        m["canvas_item_get_rid"] = schema::build_schema({
            {"path", "string", "Node path of a CanvasItem node", true},
        });
        m["scenario_create"] = schema::build_schema({});
        m["scenario_set_environment"] = schema::build_schema({
            {"scenario_rid", "integer", "RID of the render scenario", true},
            {"environment_rid", "integer", "RID of the environment to attach", true},
        });
        m["camera_create"] = schema::build_schema({});
        m["camera_set_transform"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"origin_x", "number", "Camera origin X (default: 0.0)", false},
            {"origin_y", "number", "Camera origin Y (default: 0.0)", false},
            {"origin_z", "number", "Camera origin Z (default: 0.0)", false},
        });
        m["camera_set_perspective"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"fovy_degrees", "number", "Vertical field of view in degrees (default: 75)", false},
            {"z_near", "number", "Near clip distance (default: 0.01)", false},
            {"z_far", "number", "Far clip distance (default: 4000)", false},
        });
        m["camera_set_orthogonal"] = schema::build_schema({
            {"camera_rid", "integer", "RID of the render camera", true},
            {"size", "number", "Orthogonal viewport size (default: 10)", false},
            {"z_near", "number", "Near clip distance (default: 0.01)", false},
            {"z_far", "number", "Far clip distance (default: 4000)", false},
        });
        m["light_create"] = schema::build_schema({
            {"type", "string", "Light type: directional, omni, or spot (default: directional)", false},
        });
        m["light_set_param"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light", true},
            {"param", "number", "Light parameter index (see RenderingServer.LightParam)", true},
            {"value", "number", "Parameter value", true},
        });
        m["light_set_color"] = schema::build_schema({
            {"light_rid", "integer", "RID of the render light", true},
            {"color", "object", "Light color (Color with r, g, b, a fields)", true},
        });
        m["mesh_create"] = schema::build_schema({});
        m["mesh_add_surface"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh", true},
            {"primitive", "integer", "Primitive type (default: triangles)", false},
            {"arrays", "object", "Surface arrays: vertices, normals, tangents, colors, uvs, indices", false},
        });
        m["mesh_set_material"] = schema::build_schema({
            {"mesh_rid", "integer", "RID of the render mesh", true},
            {"surface", "integer", "Surface index", true},
            {"material_rid", "integer", "RID of the material to assign", true},
        });
        m["material_create"] = schema::build_schema({});
        m["material_set_param"] = schema::build_schema({
            {"material_rid", "integer", "RID of the render material", true},
            {"parameter", "string", "Material parameter name", true},
            {"value", "object", "Parameter value", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["viewport_create"] = schema::build_schema({});
        m["viewport_set_size"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport", true},
            {"width", "integer", "Viewport width (default: 640)", false},
            {"height", "integer", "Viewport height (default: 480)", false},
        });
        m["viewport_set_clear_mode"] = schema::build_schema({
            {"viewport_rid", "integer", "RID of the render viewport", true},
            {"clear_mode", "number", "Clear mode: 0=always, 1=never, 2=only_next_frame (default: 0)", false},
        });
        m["particle_create"] = schema::build_schema({
            {"mode", "integer", "Particle mode: 0=2D, 1=3D (default: 1)", false},
        });
        m["environment_set_bg_color"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"color", "object", "Background color (Color with r, g, b, a fields)", true},
        });
        m["environment_set_ambient"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"color", "object", "Ambient light color (Color with r, g, b, a fields)", true},
            {"source", "number", "Ambient source: 0=bg, 1=disabled, 2=color, 3=sky (default: 0)", false},
            {"energy", "number", "Ambient light energy (default: 1.0)", false},
        });
        m["fog_create"] = schema::build_schema({
            {"shape", "number", "Fog volume shape (see RenderingServer.FogVolumeShape)", false},
            {"size", "object", "Fog volume size (Vector3 with x, y and z fields)", false},
        });
        m["shader_create"] = schema::build_schema({
            {"code", "string", "Shader source code (optional)", false},
        });
        m["render_texture_create_2d"] = schema::build_schema({
            {"image_path", "string", "Image file path to create the texture from", true},
        });
        m["render_shader_set_code"] = schema::build_schema({
            {"shader_rid", "integer", "RID of the shader", true},
            {"code", "string", "Shader source code", true},
        });
        m["render_shader_get_parameter_list"] = schema::build_schema({});
        m["render_environment_set_glow"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable glow (default: true)", false},
            {"level", "number", "Glow level strength (default: 0.85)", false},
            {"intensity", "number", "Glow intensity (default: 0.8)", false},
            {"strength", "number", "Glow strength (default: 1.0)", false},
            {"mix", "number", "Glow mix with scene (default: 0.05)", false},
            {"bloom_threshold", "number", "Bloom threshold (default: 0.0)", false},
            {"blend_mode", "integer", "Glow blend mode (see EnvironmentGlowBlendMode)", false},
            {"hdr_bleed_threshold", "number", "HDR bleed threshold (default: 0.5)", false},
            {"hdr_bleed_scale", "number", "HDR bleed scale (default: 2.0)", false},
            {"hdr_luminance_cap", "number", "HDR luminance cap (default: 2.0)", false},
            {"glow_map_strength", "number", "Glow map strength (default: 1.0)", false},
        });
        m["render_environment_set_ssr"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable screen-space reflections (default: true)", false},
            {"max_steps", "integer", "Maximum reflection steps (default: 64)", false},
            {"fade_in", "number", "Fade-in distance (default: 0.1)", false},
            {"fade_out", "number", "Fade-out distance (default: 0.1)", false},
            {"depth_tolerance", "number", "Depth tolerance (default: 0.1)", false},
        });
        m["render_environment_set_tonemap"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"tone_mapper", "integer", "Tone mapper: 0=linear, 1=reinhard, 2=filmic, 3=aces (default: 0)", false},
            {"exposure", "number", "Exposure (default: 1.0)", false},
            {"white", "number", "White reference (default: 1.0)", false},
        });
        m["render_environment_set_sdfgi"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable SDFGI (default: true)", false},
            {"cascades", "integer", "Number of cascades (default: 4)", false},
            {"min_cell_size", "number", "Minimum cell size (default: 0.1)", false},
            {"y_scale", "integer", "Y scale mode (see EnvironmentSDFGIYScale)", false},
            {"use_occlusion", "boolean", "Use occlusion culling (default: true)", false},
            {"bounce_feedback", "number", "Bounce feedback (default: 0.5)", false},
            {"read_sky", "boolean", "Read sky for lighting (default: true)", false},
            {"energy", "number", "GI energy (default: 1.0)", false},
            {"normal_bias", "number", "Normal bias (default: 1.0)", false},
            {"probe_bias", "number", "Probe bias (default: 1.0)", false},
        });
        m["render_environment_set_volumetric_fog"] = schema::build_schema({
            {"environment_rid", "integer", "RID of the render environment", true},
            {"enabled", "boolean", "Enable volumetric fog (default: true)", false},
            {"density", "number", "Fog density (default: 0.05)", false},
            {"albedo", "object", "Fog albedo color (Color with r, g, b, a fields)", false},
            {"emission", "object", "Fog emission color (Color with r, g, b, a fields)", false},
            {"emission_energy", "number", "Emission energy (default: 1.0)", false},
            {"anisotropy", "number", "Light anisotropy (default: 0.0)", false},
            {"length", "number", "Fog length (default: 0.0)", false},
            {"detail_spread", "number", "Detail spread (default: 0.0)", false},
            {"gi_inject", "number", "GI injection (default: 0.0)", false},
            {"temporal_reprojection", "boolean", "Temporal reprojection (default: false)", false},
            {"temporal_reprojection_amount", "number", "Temporal reprojection amount (default: 0.5)", false},
            {"ambient_inject", "number", "Ambient injection (default: 0.0)", false},
            {"sky_affect", "number", "Sky affect (default: 0.0)", false},
        });
        m["render_sky_create"] = schema::build_schema({});
        m["render_sky_set_material"] = schema::build_schema({
            {"sky_rid", "integer", "RID of the sky", true},
            {"material_rid", "integer", "RID of the material to assign", true},
        });
        m["render_particles_set_emitting"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
            {"emitting", "boolean", "Emit particles (default: true)", false},
        });
        m["render_particles_restart"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
        });
        m["render_particles_set_lifetime"] = schema::build_schema({
            {"particles_rid", "integer", "RID of the particle system", true},
            {"lifetime", "number", "Particle lifetime in seconds (default: 1.0)", false},
        });
        m["render_reflection_probe_create"] = schema::build_schema({});
        m["render_decal_create"] = schema::build_schema({});
        m["render_fog_volume_set_shape"] = schema::build_schema({
            {"fog_rid", "integer", "RID of the fog volume", true},
            {"shape", "integer", "Fog volume shape (see RenderingServer.FogVolumeShape)", true},
        });
        m["render_instance_set_visible"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance", true},
            {"visible", "boolean", "Visibility state (default: true)", false},
        });
        m["render_instance_set_layer_mask"] = schema::build_schema({
            {"instance_rid", "integer", "RID of the render instance", true},
            {"mask", "integer", "Layer mask bitfield", true},
        });
        m["render_global_shader_parameter_set"] = schema::build_schema({
            {"name", "string", "Global shader parameter name", true},
            {"value", "object", "Parameter value", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["resolve_rid"] = schema::build_schema({
            {"rid", "integer", "RID to resolve", true},
        });

        // ── Audio (explicit) ──
        m["audio_stream_play"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"stream_path", "string", "Path to an audio resource file (.ogg, .mp3, .wav) to load and play", false},
            {"from_position", "number", "Start playback position in seconds", false},
        });
        m["audio_bus_get_layout"] = schema::build_schema({});
        m["audio_bus_set_layout"] = schema::build_schema({
            {"layout", "object", "Audio bus layout (from audio_bus_get_layout)", true},
        });
        m["audio_bus_get_count"] = schema::build_schema({});
        m["audio_bus_get_name"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
        });
        m["audio_bus_set_volume"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"volume_db", "number", "Volume in decibels", true},
        });
        m["audio_bus_set_mute"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"muted", "boolean", "Mute state", true},
        });
        m["audio_bus_set_bypass"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"bypass", "boolean", "Bypass effects state", true},
        });
        m["audio_bus_set_solo"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"solo", "boolean", "Solo state", true},
        });
        m["audio_effect_add"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"effect_type", "string", "AudioEffect class name (e.g. AudioEffectReverb)", true},
            {"at_position", "integer", "Effect slot position (default: -1, append)", false},
        });
        m["audio_effect_remove"] = schema::build_schema({
            {"bus_index", "integer", "Audio bus index (0 = Master)", true},
            {"effect_index", "integer", "Index of the effect to remove", true},
        });
        m["audio_stream_stop"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
        });
        m["audio_stream_set_volume"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"volume_db", "number", "Volume in decibels", true},
        });
        m["audio_stream_set_pitch"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"pitch_scale", "number", "Pitch scale (1.0 = normal)", true},
        });
        m["audio_stream_get_playback_position"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
        });
        m["audio_stream_seek"] = schema::build_schema({
            {"node_path", "string", "Path to the AudioStreamPlayer node", true},
            {"to_position", "number", "Playback position in seconds", true},
        });
        m["audio_get_output_device_list"] = schema::build_schema({});
        m["audio_set_output_device"] = schema::build_schema({
            {"device", "string", "Audio output device name", true},
        });
        m["audio_get_input_device_list"] = schema::build_schema({});
        m["audio_set_input_device"] = schema::build_schema({
            {"device", "string", "Audio input device name", true},
        });

        // ── Debugger / Output ──
        m["output_get_log"] = schema::build_schema({
            {"limit", "integer", "Maximum number of log entries to return (default: 50)", false},
        });
        m["debugger_get_errors"] = schema::build_schema({
            {"limit", "integer", "Maximum number of errors to return (default: 20)", false},
        });
        m["debugger_get_output"] = schema::build_schema({
            {"limit", "integer", "Maximum number of output entries to return (default: 50)", false},
        });
        m["debugger_get_stack_dump"] = schema::build_schema({});
        m["debugger_get_scene_tree"] = schema::build_schema({});
        m["debugger_get_monitors"] = schema::build_schema({
            {"count", "integer", "Number of recent monitor frames to return (default: 1)", false},
        });
        m["debugger_get_session_info"] = schema::build_schema({});

        // ── System ──
        m["log_get_game_entries"] = schema::build_schema({
            {"limit", "integer", "Maximum number of log entries to return (default: 50)", false},
        });

        // ── Debug (explicit) ──
        m["debug_print"] = schema::build_schema({
            {"message", "string", "Debug log message to print", true},
        });
        m["debug_print_stack"] = schema::build_schema({
            {"include_variables", "boolean", "Include script local/member variables (default: false)", false},
        });
        m["debug_get_performance_monitor"] = schema::build_schema({
            {"monitor", "number", "Performance monitor ID (integer 0-58, see debug_list_performance_monitors)", true},
        });
        m["debug_list_performance_monitors"] = schema::build_schema({});
        m["debug_get_object_count"] = schema::build_schema({});
        m["debug_get_object_count_by_class"] = schema::build_schema({});
        m["debug_get_memory_usage"] = schema::build_schema({});
        m["debug_profile_start"] = schema::build_schema({});
        m["debug_profile_stop"] = schema::build_schema({});
        m["debug_profile_get_data"] = schema::build_schema({});
        m["debug_set_fps_limit"] = schema::build_schema({
            {"fps", "number", "Maximum FPS limit to set", true},
        });
        m["debug_set_physics_fps"] = schema::build_schema({
            {"fps", "number", "Physics ticks per second to set", true},
        });
        m["debug_collision_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable collision debug visualization", true},
        });
        m["debug_navigation_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable navigation debug visualization", true},
        });
        m["debug_performance_debug"] = schema::build_schema({
            {"enabled", "boolean", "Enable performance debug overlay", true},
        });
        m["debug_get_all_monitors"] = schema::build_schema({});
        m["debug_add_custom_monitor"] = schema::build_schema({});
        m["debug_remove_custom_monitor"] = schema::build_schema({
            {"id", "string", "Custom monitor name to remove", true},
        });
        m["debug_get_custom_monitor"] = schema::build_schema({
            {"id", "string", "Custom monitor name to read", true},
        });
        m["debug_list_custom_monitors"] = schema::build_schema({});
        m["debug_query_object_count"] = schema::build_schema({});
        m["debug_query_memory_usage"] = schema::build_schema({});
        m["debug_query_node_count"] = schema::build_schema({});

        // ── Display (explicit) ──
        m["display_clipboard_get"] = schema::build_schema({});
        m["display_clipboard_set"] = schema::build_schema({
            {"text", "string", "Text to copy to the clipboard", true},
        });
        m["display_dialog_show"] = schema::build_schema({
            {"title", "string", "Dialog title", true},
            {"description", "string", "Dialog description text", true},
            {"buttons", "array", "Array of button label strings", true},
        });
        m["display_mouse_get_position"] = schema::build_schema({});
        m["display_mouse_set_mode"] = schema::build_schema({
            {"mode", "integer", "Mouse mode: 0=visible, 1=hidden, 2=captured, 3=confined, 4=confined_hidden", true},
        });
        m["display_mouse_warp"] = schema::build_schema({
            {"x", "integer", "Mouse X position (screen coordinates)", true},
            {"y", "integer", "Mouse Y position (screen coordinates)", true},
        });
        m["display_screen_capture"] = schema::build_schema({
            {"screen", "integer", "Screen index to capture (default: 0)", false},
        });
        m["display_screen_get_count"] = schema::build_schema({});
        m["display_screen_get_dpi"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_position"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_refresh_rate"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_screen_get_size"] = schema::build_schema({
            {"screen", "integer", "Screen index (default: 0)", false},
        });
        m["display_tts_get_voices"] = schema::build_schema({});
        m["display_tts_speak"] = schema::build_schema({
            {"text", "string", "Text to speak aloud", true},
            {"voice", "string", "Voice ID (from display_tts_get_voices)", false},
            {"volume", "integer", "Volume 0-100 (default: 50)", false},
            {"pitch", "number", "Speech pitch (default: 1.0)", false},
            {"rate", "number", "Speech rate (default: 1.0)", false},
        });
        m["display_tts_stop"] = schema::build_schema({});
        m["display_window_create"] = schema::build_schema({
            {"mode", "integer", "Window mode (see Window.Mode, default: 0)", false},
            {"rect", "object", "Window rect {\"x\":int,\"y\":int,\"w\":int,\"h\":int} (default: 800x600 at 0,0)", false},
        });
        m["display_window_delete"] = schema::build_schema({
            {"window_id", "integer", "Window ID of the sub-window to delete", true},
        });
        m["display_window_move_to_foreground"] = schema::build_schema({
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_request_attention"] = schema::build_schema({
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_flag"] = schema::build_schema({
            {"flag", "integer", "Window flag (see DisplayServer.WindowFlags)", true},
            {"enabled", "boolean", "Flag state", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_mode"] = schema::build_schema({
            {"mode", "integer", "Window mode (see DisplayServer.WindowMode)", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_position"] = schema::build_schema({
            {"x", "integer", "Window X position (screen coordinates)", true},
            {"y", "integer", "Window Y position (screen coordinates)", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_size"] = schema::build_schema({
            {"width", "integer", "Window width in pixels", true},
            {"height", "integer", "Window height in pixels", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });
        m["display_window_set_title"] = schema::build_schema({
            {"title", "string", "Window title text", true},
            {"window_id", "integer", "Window ID (default: main window)", false},
        });

        // ── OS (explicit) ──
        m["os_alert"] = schema::build_schema({
            {"text", "string", "Alert message text", true},
            {"title", "string", "Alert dialog title (default: Alert!)", false},
        });
        m["os_create_process"] = schema::build_schema({
            {"path", "string", "Executable path to start", true},
            {"arguments", "array", "Array of command line argument strings", true},
        });
        m["os_execute"] = schema::build_schema({
            {"path", "string", "Executable path to run", true},
            {"arguments", "array", "Array of command line argument strings", true},
            {"output", "boolean", "Capture stdout/stderr (default: false)", false},
        });
        m["os_get_datetime"] = schema::build_schema({
            {"utc", "boolean", "Return UTC time instead of local (default: false)", false},
        });
        m["os_get_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name", true},
        });
        m["os_get_locale"] = schema::build_schema({});
        m["os_get_system_fonts"] = schema::build_schema({});
        m["os_get_system_info"] = schema::build_schema({});
        m["os_get_unique_id"] = schema::build_schema({});
        m["os_get_unix_time"] = schema::build_schema({});
        m["os_get_user_data_dir"] = schema::build_schema({});
        m["os_kill"] = schema::build_schema({
            {"pid", "integer", "Process ID to kill", true},
        });
        m["os_move_to_trash"] = schema::build_schema({
            {"path", "string", "File or folder path to move to trash", true},
        });
        m["os_set_environment"] = schema::build_schema({
            {"variable", "string", "Environment variable name", true},
            {"value", "string", "Environment variable value", true},
        });
        m["os_shell_open"] = schema::build_schema({
            {"uri", "string", "URL or file path to open with the default application", true},
        });

        // ── Docs (explicit) ──
        m["doc_get_class"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
        });
        m["doc_search"] = schema::build_schema({
            {"query", "string", "Search text to match against class names", true},
        });
        m["doc_get_method"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
            {"method", "string", "Method name to look up documentation for", true},
        });
        m["doc_get_property"] = schema::build_schema({
            {"class", "string", "Godot class name", true},
            {"property", "string", "Property name to look up documentation for", true},
        });

        // ── TileMap (explicit schemas) ──
        m["tilemap_create"] = schema::build_schema({
            {"name", "string", "TileMap node name (default: TileMap)", false},
            {"tile_size", "integer", "Tile size in pixels (default: 16)", false},
            {"format", "integer", "Tile map cell format (0=square, 1=isometric, default: 0)", false},
            {"parent_path", "string", "Parent node path (omit to add to root)", false},
        });
        m["tilemap_set_cell"] = schema::build_schema({
            {"path", "string", "Path to the TileMap node", true},
            {"x", "integer", "Cell X coordinate", true},
            {"y", "integer", "Cell Y coordinate", true},
            {"layer", "integer", "Tile layer index (default: 0)", false},
            {"source_id", "integer", "TileSet source ID (default: 0)", false},
            {"atlas_coords", "object", "Atlas coordinates Vector2i (e.g. {\"x\":0,\"y\":0})", false},
        });
        m["tilemap_set_cells"] = schema::build_schema({
            {"node_path", "string", "Path to the TileMap node", true},
            {"cells", "array", "Array of cells, each {\"x\":int,\"y\":int,\"source_id\":int,\"atlas_coords\":{\"x\":int,\"y\":int}}", true},
            {"layer", "integer", "Tile layer index (default: 0)", false},
        });
        m["tileset_create"] = schema::build_schema({
            {"name", "string", "Resource name used as memory:// reference", false},
            {"tile_size", "integer", "Base tile size in pixels (default 16)", false},
        });
        m["tileset_add_atlas_source"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"source_id", "integer", "Source ID to assign (0-255)", true},
            {"texture", "string", "Texture file path (e.g. res://tiles.png)", true},
            {"tile_size", "object", "Tile size in pixels (e.g. {\"x\":16,\"y\":16})", true},
            {"margin", "integer", "Margin in pixels around the atlas texture (default: 0)", false},
            {"spacing", "integer", "Spacing between tiles in pixels (default: 0)", false},
        });
        m["tileset_add_physics_layer"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"layer_id", "integer", "Physics layer index to add", true},
            {"collision_layer", "integer", "Collision layer bitmask (default: 1)", false},
            {"collision_mask", "integer", "Collision mask bitmask (default: 1)", false},
        });
        m["tileset_set_tile_collision"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"source_id", "integer", "Source ID of the atlas source", true},
            {"atlas_coords", "object", "Atlas coordinates of the tile (e.g. {\"x\":0,\"y\":0})", true},
            {"physics_layer", "integer", "Physics layer index to set collision on", true},
            {"polygon", "array", "Collision polygon points, each {\"x\":float,\"y\":float}", true},
        });

        // ── SpriteFrames ──
        m["spriteframes_create"] = schema::build_schema({
            {"name", "string", "Resource name used as memory:// reference", true},
        });
        m["spriteframes_add_animation"] = schema::build_schema({
            {"name", "string", "SpriteFrames resource name (memory:// reference)", true},
            {"animation", "string", "Animation name to add", true},
            {"fps", "number", "Animation playback speed in frames per second (default: 5)", false},
            {"loop", "boolean", "Loop the animation (default: true)", false},
        });
        m["spriteframes_add_frame"] = schema::build_schema({
            {"name", "string", "SpriteFrames resource name (memory:// reference)", true},
            {"animation", "string", "Animation name to add the frame to", true},
            {"texture", "string", "Texture file path (e.g. res://frame.png)", true},
            {"duration", "number", "Frame duration in seconds (default: 1.0)", false},
            {"hframes", "integer", "Horizontal frame count for spritesheet splitting (default: 1)", false},
            {"vframes", "integer", "Vertical frame count for spritesheet splitting (default: 1)", false},
        });

        // ── Batch (for catalog discoverability) ──
        m["batch_execute"] = schema::build_schema({
            {"operations", "array", "Ordered list of operations to execute", true},
            {"stop_on_error", "boolean", "Stop on first error (default: true)", false},
        });

        return m;
    }();

    auto it = schemas.find(name);
    if (it != schemas.end()) return it->second;

    if (type == SCHEMA_NONE) {
        return build_schema_for_none_by_name(name);
    }

    mcp::JsonValue s(mcp::JsonValue::object_tag);
    s["type"] = mcp::JsonValue("object");
    s["properties"] = mcp::JsonValue(mcp::JsonValue::object_tag);
    return s;
}

} // namespace

mcp::JsonValue call_handler(const std::string& name, const mcp::JsonValue& args) {
    auto it = g_handlers.find(name);
    if (it != g_handlers.end()) {
        try {
            return it->second(args);
        } catch (...) {
            std::string args_dump = args.Dump();
            if (args_dump.size() > 256) {
                args_dump.resize(256);
            }
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("internal error in tool '" + name + "': unexpected C++ exception (args: " + args_dump + ")");
            return e;
        }
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

        mcp::JsonValue ao(mcp::JsonValue::object_tag);
        ao["type"] = mcp::JsonValue("boolean");
        ao["description"] = mcp::JsonValue("Automatically set owner on nodes created during execution so they are saved with the scene (default true)");
        props["auto_owner"] = std::move(ao);

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

    // ── 7. Register meta-tools in catalog for discoverability ──
    if (catalog.get_tool("batch_execute") == nullptr) {
        catalog.add_tool({"batch_execute",
            "Execute multiple tools in batch. Each operation runs in sequence; if stop_on_error is true and any operation fails, remaining operations are skipped.",
            "System", {"batch", "execute", "multi"},
            build_schema_for(SCHEMA_BASIC, "batch_execute")
        });
    }

    // ── 8. Populate BM25 index ──
    for (auto* tool : catalog.get_all_tools()) {
        index.add_entry(tool->name, tool->description, tool->category, tool->tags);
    }

    auto count = catalog.size();
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
        std::to_string(count) + " tools registered via catalog");

    // ── 9. Sync check: g_handlers vs catalog ──
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
            LogSystem::instance().log(LogLevel::Error, LogCategory::Tools,
                "tool '" + ctool->name + "' exists in catalog but has no handler in g_handlers");
        }
    }
}

} // namespace godot_self_driving
