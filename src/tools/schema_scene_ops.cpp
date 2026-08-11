#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_scene(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["scene_node_create"] = schema::build_schema({
            {"parent_path", "string", "Parent node path (omit to create root node)", false},
            {"name", "string", "Node name (default: NewNode)", true},
            {"type", "string", "Node class type (e.g. Node2D, Sprite2D, default: Node)", true},
        });
        m["scene_node_delete"] = schema::build_schema({
            {"path", "string", "Node path to delete", true},
        });
        m["scene_tree_get"] = schema::build_schema({});
        m["scene_get_tree"] = schema::build_schema({
            {"max_depth", "integer", "Maximum tree depth to include (default: 8)", false},
            {"include_properties", "boolean", "Include a summary of node properties (default: false)", false},
        });
        m["scene_instance"] = schema::build_schema({
            {"path", "string", "Path to the .tscn scene file to instantiate", true},
            {"parent_path", "string", "Parent node path (default: edited scene root)", false},
            {"name", "string", "Node name (default: scene root name)", false},
            {"owner", "boolean", "Set owner so nodes are saved with the scene (default: true)", false},
        });


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

        m["property_get"] = schema::build_schema({
            {"path", "string", "Node path", true},
            {"property", "string", "Property name", true},
        });
        m["property_set"] = schema::build_schema({
            {"path", "string", "Node path (scene-relative or absolute). memory:// is a resource namespace and is NOT valid here — memory resources go in the value parameter (e.g. {\"resource\": \"memory://name\"})", true},
            {"property", "string", "Property name", true},
            {"value", "object", "Property value to set — 资源引用支持两种格式: {\"path\": \"res://xxx.tres\"}（磁盘资源，赋节点属性用）或 {\"resource\": \"memory://名称\"}（内存资源，仅资源编辑场景）", true},
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
            {"persist", "boolean", "Persist the connection with CONNECT_PERSIST so it is saved with the scene (default: true); false creates a runtime-only connection that is not saved", false},
        });
        m["signal_disconnect"] = schema::build_schema({
            {"source_path", "string", "Source node path", true},
            {"signal", "string", "Signal name", true},
            {"target_path", "string", "Target node path", true},
            {"method", "string", "Method to call on target", true},
        });

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

        m["script_execute_gdscript"] = schema::build_schema({
            {"expression", "string", "GDScript expression or code to execute. Single expression auto-returns its value; multi-line code needs an explicit return. print() output appears in the output field, errors in the errors field", true},
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

        m["file_write"] = schema::build_schema({
            {"path", "string", "File path to write", true},
            {"content", "string", "Content to write", true},
            {"mode", "string", "Write mode: WRITE or APPEND (default: WRITE)", false},
        });

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
}

} // namespace godot_autopilot
