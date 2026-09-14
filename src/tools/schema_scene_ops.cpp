#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_scene(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_scene_node"] = schema::build_schema({
            {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Player' or 'Player/Weapon'); omit only while the scene has no root — required once the scene already has a root", false},
            {"name", "string", "Node name (string, default: NewNode)", true},
            {"type", "string", "Node class type (string, default: Node). Must be a Node subclass, e.g. Node2D, Sprite2D — other classes error", true},
            {"properties", "object", "Optional map of property names to values applied right after creation via the same conversion chain as property_set (object, e.g. {\"visible\": false, \"position\": [10, 20]}); any failing property frees the new node and reports the failing name", false},
        });
        m["delete_scene_node"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene (e.g. 'Enemies/Enemy1'); the scene root cannot be deleted", true},
        });
        m["rename_scene_node"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene (e.g. 'Player/Sprite2D')", true},
            {"new_name", "string", "New node name; must be unique among siblings and cannot contain '/' or ':' (string, e.g. 'Hero')", true},
        });
        m["reparent_node"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene to reparent (e.g. 'Player/Weapon')", true},
            {"new_parent_path", "string", "Target parent node path in the edited scene; must not be the node itself or one of its descendants (string, e.g. 'Inventory')", true},
            {"keep_world_position", "boolean", "Preserve the world transform when the node and the new parent are both Node2D or both Node3D; other type combinations ignore it (boolean, default: false)", false},
        });
        m["get_scene_tree"] = schema::build_schema({
            {"max_depth", "integer", "Maximum tree depth to include (integer, default: 8; -1 means unlimited)", false},
            {"include_properties", "boolean", "Add up to 20 properties per node, skipping metadata/* keys, names starting with '_' and Object-typed values (boolean, default: false)", false},
        });
        m["instantiate_scene"] = schema::build_schema({
            {"path", "string", "Path to the .tscn scene file to instantiate (string, e.g. 'res://scenes/Enemy.tscn')", true},
            {"parent_path", "string", "Parent node path (string, default: edited scene root)", false},
            {"name", "string", "Node name to override the instance root (string, default: scene root name)", false},
            {"owner", "boolean", "Set scene ownership so nodes are saved with the scene (boolean, default: true)", false},
        });


        m["call_scene_tree_group"] = schema::build_schema({
            {"group_name", "string", "Name of the group whose nodes receive the call (string, e.g. 'enemies')", true},
            {"method", "string", "Method name to call on every group node (string, e.g. 'take_damage')", true},
            {"arguments", "array", "Optional arguments array passed to the method (array, e.g. [10, 'fire'])", false},
        });
        m["create_scene_tree_timer"] = schema::build_schema({
            {"delay_sec", "number", "Timer delay in seconds (number, e.g. 1.5)", true},
            {"process_always", "boolean", "Process even while the scene tree is paused (boolean, default: true)", false},
            {"process_in_physics", "boolean", "Count down in the physics step instead of the process step (boolean, default: false)", false},
        });
        m["get_scene_tree_nodes_in_group"] = schema::build_schema({
            {"group_name", "string", "Name of the group to look up (string, e.g. 'enemies')", true},
        });
        m["is_scene_tree_paused"] = schema::build_schema({});
        m["notify_scene_tree_group"] = schema::build_schema({
            {"group_name", "string", "Name of the group whose nodes receive the notification (string, e.g. 'enemies')", true},
            {"notification", "integer", "Notification constant as integer (e.g. NOTIFICATION_READY=13, NOTIFICATION_PROCESS=3, NOTIFICATION_ENTER_TREE=10, NOTIFICATION_EXIT_TREE=11)", true},
        });
        m["reload_scene_tree_current_scene"] = schema::build_schema({});
        m["set_scene_tree_debug_collisions_hint"] = schema::build_schema({
            {"enabled", "boolean", "Draw collision shapes while the scene runs (boolean, e.g. true)", true},
        });
        m["set_scene_tree_pause"] = schema::build_schema({
            {"paused", "boolean", "Pause state: true pauses the scene tree, false resumes (boolean)", true},
        });

        m["property_get"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene (string, e.g. 'Player' or 'Player/Camera2D')", true},
            {"property", "string", "Property name to read (string, e.g. 'position', 'visible')", true},
        });
        m["property_set"] = schema::build_schema({
            {"path", "string", "Node path (scene-relative or absolute). memory:// is a resource namespace and is NOT valid here — memory resources go in the value parameter (e.g. {\"resource\": \"memory://name\"})", true},
            {"property", "string", "Property name to set (string, e.g. 'position', 'modulate')", true},
            {"value", "object", "Property value to set. Resource references use two forms: {\"path\": \"res://xxx.tres\"} for a disk resource assigned to a node property, or {\"resource\": \"memory://name\"} for an in-memory resource (only valid in resource editing scenarios). Assigning a memory:// resource to a node property is rejected because it would corrupt the saved scene file", true},
            {"type_hint", "string", "Variant type hint (string, e.g. Vector2, Color, int, float). Omit to infer the type automatically from the property metadata", false},
        });
        m["property_get_list"] = schema::build_schema({
            {"path", "string", "Node path in the edited scene (string, e.g. 'Player')", true},
            {"only_script_variables", "boolean", "Only return properties with PROPERTY_USAGE_SCRIPT_VARIABLE (usage bit 4096), i.e. script-declared variables (default: false)", false},
            {"property_filter", "string", "Case-sensitive substring filter on property names (default: empty = no filter)", false},
        });
        m["signal_connect"] = schema::build_schema({
            {"source_path", "string", "Node path of the signal emitter (string, e.g. 'Player')", true},
            {"signal", "string", "Signal name on the source node (string, e.g. 'health_changed')", true},
            {"target_path", "string", "Node path of the receiver (string, e.g. 'UI/HealthBar')", true},
            {"method", "string", "Method name to call on the target node (string, e.g. 'set_health')", true},
            {"persist", "boolean", "Persist the connection with CONNECT_PERSIST so it is saved with the scene and inherited by instantiations (boolean, default: true); false creates a runtime-only connection that is not saved", false},
        });
        m["signal_disconnect"] = schema::build_schema({
            {"source_path", "string", "Node path of the signal emitter (string, e.g. 'Player')", true},
            {"signal", "string", "Signal name on the source node (string, e.g. 'health_changed')", true},
            {"target_path", "string", "Node path of the receiver (string, e.g. 'UI/HealthBar')", true},
            {"method", "string", "Method name of the connected callable (string, e.g. 'set_health')", true},
        });

        m["load_resource"] = schema::build_schema({
            {"path", "string", "Resource file path to load, e.g. res://my_resource.tres", true},
            {"type_hint", "string", "Expected resource class to guide loading, e.g. PackedScene, Texture2D; default: none", false},
        });
        m["reload_resource"] = schema::build_schema({
            {"path", "string", "Resource file path to force-reload from disk, e.g. res://my_resource.tres", true},
        });
        m["load_resource_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path to load in the background, e.g. res://my_resource.tres", true},
            {"type_hint", "string", "Expected resource class to guide loading, e.g. PackedScene, Texture2D; default: none", false},
            {"use_sub_threads", "boolean", "Split the load across worker sub-threads for faster loading; default: false", false},
        });
        m["get_resource_load_threaded_status"] = schema::build_schema({
            {"path", "string", "Resource file path whose threaded load was started with load_resource_threaded", true},
        });
        m["get_resource_load_threaded"] = schema::build_schema({
            {"path", "string", "Resource file path whose threaded load was started with load_resource_threaded; errors if no such load is pending", true},
        });
        m["save_resource"] = schema::build_schema({
            {"path", "string", "Source path to load from, or destination path if object_id is provided; required to identify the resource", true},
            {"dest_path", "string", "Destination file path to write, e.g. res://out/my_resource.tres; default: path. Missing parent directories are created recursively; when dest_path differs from path, the resource is duplicated first so the source cached instance is not modified; the response then includes copy_on_write and source_path", false},
            {"name", "string", "Name of an in-memory resource (as passed to create_resource) to save", false},
            {"class_type", "string", "Resource class to instantiate when no resource resolves, e.g. Curve2D; requires name to register the new instance", false},
            {"object_id", "integer", "Object ID of an in-memory resource (from create_resource) to save; mutually exclusive with object_id_str", false},
            {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
            {"flags", "integer", "ResourceSaver.SaverFlags bitfield, e.g. 1 = RELATIVE_PATHS, 8 = REPLACE_SUBRESOURCE_PATHS; default: 0", false},
        });
        m["create_resource"] = schema::build_schema({
            {"type", "string", "Resource class to instantiate, e.g. Resource, Curve2D, PackedScene; accepts both engine classes from ClassDB and global script classes (GDScript class_name / C# [GlobalClass]); abstract or invalid scripts are rejected", true},
            {"name", "string", "Registration name so later calls can reference the instance as memory://name and via the returned object_id; optional but recommended", false},
        });
        m["duplicate_resource"] = schema::build_schema({
            {"path", "string", "Disk path of the resource to load and duplicate, e.g. res://my_resource.tres; in-memory resources are not supported", true},
            {"deep", "boolean", "Deep duplicate copies sub-resources (true), shallow keeps shared references (false, default)", false},
            {"name", "string", "Registration name so later calls can reference the duplicate as memory://name and via the returned object_id; when omitted only the object_id is registered", false},
        });
        m["get_resource_type"] = schema::build_schema({
            {"path", "string", "Resource file path whose class type to report, e.g. res://my_resource.tres", true},
        });
        m["has_resource"] = schema::build_schema({
            {"path", "string", "Path to check for loadability via the resource loader, e.g. res://icon.svg; not a plain file-existence test", true},
        });
        m["get_resource_types"] = schema::build_schema({});
        m["get_resource_extensions"] = schema::build_schema({
            {"type", "string", "Resource class name to list extensions for, e.g. Texture2D, AudioStream; when omitted the loader returns extensions for all known types (contract gap: required in schema but optional in the implementation)", true},
        });
        m["get_resource_dir_files"] = schema::build_schema({
            {"path", "string", "Directory path to list, e.g. res://assets; returns files and subdirectory names as entry names", true},
        });
        m["get_resource_uid"] = schema::build_schema({
            {"path", "string", "Resource file path whose UID to read, e.g. res://my_resource.tres", true},
        });
        m["set_resource_uid"] = schema::build_schema({
            {"path", "string", "Resource file path to assign the UID to, e.g. res://my_resource.tres", true},
            {"uid", "integer", "UID value to assign; when omitted a fresh UID is auto-generated (contract gap: required in schema but optional in the implementation)", true},
        });
        m["remove_resource_file"] = schema::build_schema({
            {"path", "string", "Resource file path to delete from disk, e.g. res://my_resource.tres; without force the call only runs the impact pre-check and deletes nothing", true},
            {"force", "boolean", "Two-stage safety switch (default false): false returns would_delete plus dependents from a reverse-reference scan; true performs the deletion — preferentially moving the file to the OS trash (trashed=true) with fallback to permanent removal (permanent=true), also removing the .uid sidecar (sidecars_removed)", false},
        });
        m["rename_resource_file"] = schema::build_schema({
            {"path", "string", "Current resource path, e.g. res://old_name.tres; 'from' is accepted as an alias", true},
            {"new_path", "string", "New resource path, e.g. res://new_name.tres; 'to' is accepted as an alias", true},
        });
        m["move_resource_file"] = schema::build_schema({
            {"path", "string", "Existing res:// file or directory to move, e.g. res://sfx/jump.wav; directories move recursively preserving the sub-folder layout with *.uid sidecars following their owners", true},
            {"new_directory", "string", "Destination directory inside res:// (absolute like res://assets/sfx or relative like assets/sfx); trailing slashes are trimmed, user:// is rejected", true},
        });
        m["copy_resource_file"] = schema::build_schema({
            {"path", "string", "Source file path to copy, e.g. res://assets/icon.png; must exist", true},
            {"dest_path", "string", "Destination file path to write, e.g. res://assets/icon_copy.png; missing parent directories are created recursively and an existing file is overwritten", true},
        });
        m["create_directory"] = schema::build_schema({
            {"path", "string", "Directory to create below res:// (absolute like res://assets/audio or relative like assets/audio); missing parents are created recursively; idempotent when the directory already exists", true},
        });
        m["get_resource_dependencies"] = schema::build_schema({
            {"path", "string", "Resource file path whose dependencies to list, e.g. res://scene.tscn", true},
        });
        m["has_resource_dependency"] = schema::build_schema({
            {"path", "string", "Resource file path to inspect, e.g. res://scene.tscn", true},
            {"dependency", "string", "Dependency file path to search for in the resource's dependency list, e.g. res://icon.svg", true},
        });
        m["get_resource_references"] = schema::build_schema({
            {"path", "string", "Resource path to search for as a target reference, e.g. res://assets/icon.png; ignore uid and class_name", false},
            {"uid", "string", "Resource UID to search for in uid:// references, e.g. uid://abc123", false},
            {"class_name", "string", "Script class name to search for in script references, e.g. Player", false},
        });
        m["reimport_resource_files"] = schema::build_schema({
            {"path", "string", "Single resource file path to reimport; alternative to files", true},
            {"files", "array", "Array of resource file paths to reimport; alternative to a single path", false},
        });
        m["set_resource_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
            {"object_id", "integer", "Object ID of an in-memory resource (from create_resource or load_resource)", false},
            {"name", "string", "Registration name of an in-memory resource (memory://name)", false},
            {"path", "string", "Resource path; loads the file from disk when no object_id or name matches", false},
            {"property", "string", "Property name to set, e.g. curve, texture; must exist on the resource", true},
            {"value", "object", "Serialized JSON value; resource-typed properties accept {'path': 'res://xxx.tres'} or {'resource': 'memory://name'} references", true},
            {"type_hint", "string", "Variant type hint for deserialization, e.g. Vector2, Color, int, float; auto-detected from the property metadata when omitted", false},
        });
        m["get_resource_property"] = schema::build_schema({
            {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
            {"object_id", "integer", "Object ID of an in-memory resource (from create_resource or load_resource)", false},
            {"name", "string", "Registration name of an in-memory resource (memory://name)", false},
            {"path", "string", "Resource path; loads the file from disk when no object_id or name matches", false},
            {"property", "string", "Property name to read, e.g. curve, texture", true},
            {"type_hint", "string", "Accepted for parity with set_resource_property; reading always returns the raw serialized value", false},
        });

        m["execute_script"] = schema::build_schema({
            {"expression", "string", "GDScript expression or code to execute. Single expression auto-returns its value; multi-line code needs an explicit return. print() output appears in the output field, errors in the errors field. Runs synchronously in the editor process with no timeout — long code blocks the editor", true},
        });
        m["load_script"] = schema::build_schema({
            {"path", "string", "Script file path (e.g. res://player.gd)", true},
        });
        m["create_script"] = schema::build_schema({
            {"path", "string", "Script file path to create (e.g. res://scripts/enemy.gd)", true},
            {"source_code", "string", "GDScript source code; compiled before saving, compilation errors abort the call", false},
            {"overwrite", "boolean", "Overwrite an existing file (default: false); false errors when the file already exists", false},
        });
        m["attach_script_to_node"] = schema::build_schema({
            {"node_path", "string", "Node path in the edited scene to attach the script to", true},
            {"script_path", "string", "Resource path of a Script resource (e.g. a .gd file or another Script); the resource must be a Script", true},
        });
        m["detach_script_from_node"] = schema::build_schema({
            {"node_path", "string", "Node path in the edited scene to detach the script from", true},
        });
        m["get_script_property"] = schema::build_schema({
            {"script_path", "string", "Resource path of the script (.gd file) to read a declared default property from; exactly one of script_path or node_path is required", false},
            {"node_path", "string", "Node path to read the current property value from; exactly one of script_path or node_path is required", false},
            {"property", "string", "Property name", true},
        });
        m["set_script_property"] = schema::build_schema({
            {"node_path", "string", "Node path in the edited scene", true},
            {"property", "string", "Property name", true},
            {"value", "object", "Property value as JSON; the tool performs no type or read-only validation and creates no undo entry — use property_set for validated, undoable changes", true},
        });
        m["call_script_node"] = schema::build_schema({
            {"node_path", "string", "Node path in the edited scene", true},
            {"function", "string", "Method name to call; the node's script must be @tool to run in the editor", true},
            {"args", "array", "Function arguments (serialized as Variants)", false},
        });
        m["reload_script"] = schema::build_schema({
            {"path", "string", "Script file path (e.g. res://player.gd)", true},
        });
        m["get_script_property_list"] = schema::build_schema({
            {"path", "string", "Script file path (e.g. res://player.gd)", true},
        });

        m["write_file"] = schema::build_schema({
            {"path", "string", "File path to write; res:// paths are engine-managed (automatic reimport/update_file sync and script diagnostics), user:// or absolute paths are plain I/O", true},
            {"content", "string", "Text content to store in the file", true},
            {"mode", "string", "Write mode: 'WRITE' overwrites existing content (default), 'APPEND' appends to the end of the file", false},
        });
        m["read_file"] = schema::build_schema({
            {"path", "string", "File path to read (absolute path, or res:// / user://-relative); plain file I/O with no editor resource tracking", true},
        });
        m["find_in_files"] = schema::build_schema({
            {"query", "string", "Text to search for within each file; files that contain no occurrence are skipped", true},
            {"dir", "string", "Directory to search recursively (string, default: res://)", false},
            {"extensions", "array", "File extensions to include, without leading dot (array, e.g. ['gd','tscn','cs']); default: ['gd','tscn','tres','cs','md','json','h','cpp']", false},
            {"case_sensitive", "boolean", "Match the query with case sensitivity (boolean, default: false)", false},
            {"max_results", "integer", "Maximum number of matching files to return (integer, default: 500); stops searching early and sets truncated: true when exceeded", false},
        });

        m["add_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
            {"group_name", "string", "Group name to add the node to (string, e.g. 'enemies')", true},
        });
        m["remove_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
            {"group_name", "string", "Group name to remove the node from (string, e.g. 'enemies')", true},
        });
        m["has_group_node"] = schema::build_schema({
            {"node_path", "string", "Path to the scene node (string, e.g. 'Enemies/Enemy1')", true},
            {"group_name", "string", "Group name to check (string, e.g. 'enemies')", true},
        });
}

} // namespace godot_autopilot
