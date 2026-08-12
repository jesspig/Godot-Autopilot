#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_scene(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_scene_node"] = schema::build_schema({
            {"parent_path", "string", "Parent node path (omit to create root node)", false},
            {"name", "string", "Node name (default: NewNode)", true},
            {"type", "string", "Node class type (e.g. Node2D, Sprite2D, default: Node)", true},
        });
        m["delete_scene_node"] = schema::build_schema({
            {"path", "string", "Node path to delete", true},
        });
        m["get_scene_tree"] = schema::build_schema({
            {"max_depth", "integer", "Maximum tree depth to include (default: 8)", false},
            {"include_properties", "boolean", "Include a summary of node properties (default: false)", false},
        });
        m["instantiate_scene"] = schema::build_schema({
            {"path", "string", "Path to the .tscn scene file to instantiate", true},
            {"parent_path", "string", "Parent node path (default: edited scene root)", false},
            {"name", "string", "Node name (default: scene root name)", false},
            {"owner", "boolean", "Set owner so nodes are saved with the scene (default: true)", false},
        });


        m["call_scene_tree_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
            {"method", "string", "Method name to call on group nodes", true},
            {"arguments", "array", "Optional arguments to pass to the method", false},
        });
        m["create_scene_tree_timer"] = schema::build_schema({
            {"delay_sec", "number", "Timer delay in seconds", true},
            {"process_always", "boolean", "Process when paused (default: true)", false},
            {"process_in_physics", "boolean", "Process in physics step (default: false)", false},
        });
        m["get_scene_tree_nodes_in_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
        });
        m["is_scene_tree_paused"] = schema::build_schema({});
        m["notify_scene_tree_group"] = schema::build_schema({
            {"group_name", "string", "Scene group name", true},
            {"notification", "integer", "Notification constant (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3)", true},
        });
        m["reload_scene_tree_current_scene"] = schema::build_schema({});
        m["set_scene_tree_debug_collisions_hint"] = schema::build_schema({
            {"enabled", "boolean", "Enable collision debug visualization", true},
        });
        m["set_scene_tree_pause"] = schema::build_schema({
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

        m["load_resource"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
            {"type_hint", "string", "Resource type hint (e.g. PackedScene, Texture2D)", false},
        });
        m["load_resource_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
            {"type_hint", "string", "Resource type hint", false},
            {"use_sub_threads", "boolean", "Use sub-threads for background loading", false},
        });
        m["get_resource_load_threaded_status"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["get_resource_load_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["save_resource"] = schema::build_schema({
            {"path", "string", "Source path to load from, or destination path if object_id provided", true},
            {"dest_path", "string", "Destination path (defaults to path)", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"class_type", "string", "Class type to instantiate if resource not found", false},
            {"object_id", "integer", "Object ID of an in-memory resource to save", false},
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"flags", "integer", "Saver flags as bitfield (see ResourceSaver.SaverFlags)", false},
        });
        m["create_resource"] = schema::build_schema({
            {"type", "string", "Resource class type (e.g. Resource, PackedScene)", true},
            {"name", "string", "Optional resource name for identification", false},
        });
        m["duplicate_resource"] = schema::build_schema({
            {"path", "string", "Resource path to load and duplicate", true},
            {"deep", "boolean", "Deep duplicate (true) or shallow (false, default)", false},
        });
        m["get_resource_type"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["has_resource"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["get_resource_types"] = schema::build_schema({});
        m["get_resource_extensions"] = schema::build_schema({
            {"type", "string", "Resource type name", true},
        });
        m["get_resource_dir_files"] = schema::build_schema({
            {"dir", "string", "Directory path", true},
        });
        m["get_resource_uid"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["set_resource_uid"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
            {"uid", "integer", "UID value", true},
        });
        m["remove_resource_file"] = schema::build_schema({
            {"path", "string", "Resource file path", true},
        });
        m["rename_resource_file"] = schema::build_schema({
            {"path", "string", "Current resource path", true},
            {"new_path", "string", "New resource path", true},
        });
        m["get_resource_dependencies"] = schema::build_schema({
            {"path", "string", "Resource path", true},
        });
        m["has_resource_dependency"] = schema::build_schema({
            {"path", "string", "Resource path", true},
            {"dependency_path", "string", "Dependency file path to check", true},
        });
        m["reimport_resource_files"] = schema::build_schema({
            {"path", "string", "Resource file path to reimport", true},
        });
        m["set_resource_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"object_id", "integer", "Object ID of an in-memory resource", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"path", "string", "Resource path to load and modify", false},
            {"property", "string", "Property name to set", true},
            {"value", "object", "Property value to set", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });
        m["get_resource_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as string (alternative to object_id)", false},
            {"object_id", "integer", "Object ID of an in-memory resource", false},
            {"name", "string", "Resource name for in-memory resources", false},
            {"path", "string", "Resource path to load and read", false},
            {"property", "string", "Property name to read", true},
            {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
        });

        m["execute_script"] = schema::build_schema({
            {"expression", "string", "GDScript expression or code to execute. Single expression auto-returns its value; multi-line code needs an explicit return. print() output appears in the output field, errors in the errors field", true},
        });
        m["load_script"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });
        m["create_script"] = schema::build_schema({
            {"path", "string", "Script file path to create", true},
            {"source_code", "string", "Script content", false},
            {"overwrite", "boolean", "Overwrite existing file (default: false)", false},
        });
        m["attach_script_to_node"] = schema::build_schema({
            {"node_path", "string", "Node path to attach script to", true},
            {"script_path", "string", "Resource path of the script (.gd file)", true},
        });
        m["detach_script_from_node"] = schema::build_schema({
            {"node_path", "string", "Node path to detach script from", true},
        });
        m["get_script_property"] = schema::build_schema({
            {"script_path", "string", "Resource path of the script (.gd file) to read a default property from", false},
            {"node_path", "string", "Node path", false},
            {"property", "string", "Property name", true},
        });
        m["set_script_property"] = schema::build_schema({
            {"node_path", "string", "Node path", true},
            {"property", "string", "Property name", true},
            {"value", "object", "Property value", true},
        });
        m["call_script_node"] = schema::build_schema({
            {"node_path", "string", "Node path", true},
            {"function", "string", "Function name", true},
            {"args", "array", "Function arguments", false},
        });
        m["reload_script"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });
        m["get_script_property_list"] = schema::build_schema({
            {"path", "string", "Script file path", true},
        });

        m["write_file"] = schema::build_schema({
            {"path", "string", "File path to write", true},
            {"content", "string", "Content to write", true},
            {"mode", "string", "Write mode: WRITE or APPEND (default: WRITE)", false},
        });

        m["add_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to add the node to", true},
        });
        m["remove_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to remove the node from", true},
        });
        m["has_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node", true},
            {"group_name", "string", "Group name to check", true},
        });
}

} // namespace godot_autopilot
