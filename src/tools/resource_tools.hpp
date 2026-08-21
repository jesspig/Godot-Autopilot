#ifndef GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP
#define GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/resource_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace resource_tools {

GDA_TOOL_CLASS(LoadResourceTool, "load_resource",
               "Load a resource file from disk into memory. Use it to bring a .tres or .tscn file into the editor for inspection or modification, with an optional type_hint to guide loading. Returns the resource reference with class, path, object_id and object_id_str, which set_resource_property and get_resource_property accept as locators. Errors when the file is missing or fails to load.",
               "Resources", std::vector<std::string>({"resource", "load"}), resource_ops::handle_load, true)

GDA_TOOL_CLASS(LoadResourceThreadedTool, "load_resource_threaded",
               "Start an asynchronous background load of a resource file and return immediately. It is the first step of the threaded-load chain: after this call, poll get_resource_load_threaded_status, then fetch the result with get_resource_load_threaded. use_sub_threads enables finer-grained parallel loading. Returns the Godot error code (0 = OK).",
               "Resources", std::vector<std::string>({"resource", "load", "threaded"}), resource_ops::handle_load_threaded, true)

GDA_TOOL_CLASS(GetResourceLoadThreadedStatusTool, "get_resource_load_threaded_status",
               "Poll the status of a threaded load started by load_resource_threaded. Use it between starting the load and fetching the result. Returns status as one of invalid_resource, in_progress, failed or loaded, plus a numeric code; poll until loaded, then call get_resource_load_threaded to retrieve the resource.",
               "Resources", std::vector<std::string>({"resource", "load", "status"}), resource_ops::handle_load_threaded_get_status, true)

GDA_TOOL_CLASS(GetResourceLoadThreadedTool, "get_resource_load_threaded",
               "Blockingly fetch the resource loaded by load_resource_threaded, completing the threaded-load chain: start with load_resource_threaded, poll get_resource_load_threaded_status, then retrieve here. Returns the reference with class, path, object_id and object_id_str; calling it without a pending threaded load returns an error. Use it for large resources so the editor stays responsive during loading.",
               "Resources", std::vector<std::string>({"resource", "load", "wait"}), resource_ops::handle_load_threaded_wait, true)

GDA_TOOL_CLASS_SIDE(SaveResourceTool, "save_resource",
               "Save a resource to disk at dest_path (defaults to path). Locate the resource by object_id/object_id_str (from create_resource), name (in-memory) or path (loaded from disk); if none resolves, class_type plus a name creates and registers a new instance. Missing directories are created recursively and the editor file system is refreshed. Returns the Godot error code, verified (a read-back load check) and directories_created when applicable.",
               "Resources", std::vector<std::string>({"resource", "save"}), resource_ops::handle_save, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(CreateResourceTool, "create_resource",
               "Instantiate a Resource subclass by class name (type) in memory. Pass name to register the instance so later calls can reference it as memory://name. Returns the reference with object_id and object_id_str, which serve as locators for set_resource_property, get_resource_property and save_resource. Errors when type is not an instantiable Resource subclass.",
               "Resources", std::vector<std::string>({"resource", "create"}), resource_ops::handle_create, true)

GDA_TOOL_CLASS(DuplicateResourceTool, "duplicate_resource",
               "Load a resource from a disk path and return a duplicated instance. deep (default false) controls whether sub-resources are deep-copied. Only file-backed resources are supported; in-memory instances from create_resource must be saved and reloaded first. Returns the duplicate's reference with object_id and name.",
               "Resources", std::vector<std::string>({"resource", "duplicate"}), resource_ops::handle_duplicate, true)

GDA_TOOL_CLASS(GetResourceTypeTool, "get_resource_type",
               "Load the resource at path and report its engine class name, which tells you what the file actually holds. Returns class and path; errors when the file does not exist or fails to load. Use get_resource_types to discover valid class names for create_resource.",
               "Resources", std::vector<std::string>({"resource", "type"}), resource_ops::handle_get_type, true)

GDA_TOOL_CLASS(HasResourceTool, "has_resource",
               "Check whether path is loadable as a resource using the engine's resource loader. This is not a plain file-existence check: files that are not imported or not recognized may return false even though they exist on disk. Returns a boolean.",
               "Resources", std::vector<std::string>({"resource", "exists"}), resource_ops::handle_exists, true)

GDA_TOOL_CLASS(GetResourceTypesTool, "get_resource_types",
               "List every instantiable Resource subclass registered in the engine, including built-in classes and script-based ones. Use it to discover valid type values for create_resource or to confirm that a class exists before instantiating it. Returns an array of class names.",
               "Resources", std::vector<std::string>({"resource", "types", "list"}), resource_ops::handle_list_types, true)

GDA_TOOL_CLASS(GetResourceExtensionsTool, "get_resource_extensions",
               "List the file extensions recognized for a resource type, e.g. Texture2D returns png, jpg and webp. type is optional: when omitted the loader reports extensions for all known types instead. Returns an array of extension strings without the leading dot.",
               "Resources", std::vector<std::string>({"resource", "extensions"}), resource_ops::handle_get_extensions, true)

GDA_TOOL_CLASS(GetResourceDirFilesTool, "get_resource_dir_files",
               "List the entries of a directory (files and subdirectory names) through the resource loader. Requires path, the res:// or user:// directory to inspect. Returns an array of entry names; subdirectories appear as plain names, so combine with has_resource to tell loadable resources apart from folders.",
               "Resources", std::vector<std::string>({"resource", "directory", "list"}), resource_ops::handle_list_dir, true)

GDA_TOOL_CLASS(GetResourceUidTool, "get_resource_uid",
               "Read the numeric UID of a resource file, the integer behind its uid:// reference. UIDs keep references stable across renames, so they are preferred over raw paths in saved scenes. Returns the UID as an integer, or -1 (INVALID_ID) when the file has none; use set_resource_uid to assign one.",
               "Resources", std::vector<std::string>({"resource", "uid", "get"}), resource_ops::handle_get_uid, true)

GDA_TOOL_CLASS(SetResourceUidTool, "set_resource_uid",
               "Assign a UID to a resource file so it can be referenced as uid://<id>, the counterpart of get_resource_uid. uid is optional: when omitted a fresh UID is auto-generated. Triggers a filesystem reimport so the assignment persists. Returns the UID that was applied.",
               "Resources", std::vector<std::string>({"resource", "uid", "set"}), resource_ops::handle_set_uid, true)

GDA_TOOL_CLASS(RemoveResourceFileTool, "remove_resource_file",
               "Delete a resource file from disk and clear its cached reference so the resource no longer reports a path. This is destructive and irreversible; use rename_resource_file when you want to keep the file. Returns the Godot error code (0 = OK).",
               "Resources", std::vector<std::string>({"resource", "remove", "delete"}), resource_ops::handle_remove, true)

GDA_TOOL_CLASS(RenameResourceFileTool, "rename_resource_file",
               "Rename or move a resource file. Accepts path and new_path, with from and to accepted as aliases. The cached resource path, the edited scene file path (when affected) and the editor file system are updated. Returns the Godot error code.",
               "Resources", std::vector<std::string>({"resource", "rename", "move"}), resource_ops::handle_rename, true)

GDA_TOOL_CLASS(GetResourceDependenciesTool, "get_resource_dependencies",
               "List the dependencies of a resource file, i.e. the external files it references (textures, scenes, scripts). Use it to audit what a resource needs before moving or deleting files. Returns an array of res:// paths; use has_resource_dependency for a targeted single check.",
               "Resources", std::vector<std::string>({"resource", "dependencies", "list"}), resource_ops::handle_get_dependencies, true)

GDA_TOOL_CLASS(HasResourceDependencyTool, "has_resource_dependency",
               "Check whether a resource file depends on a specific other file. Requires path (the resource to inspect) and dependency (the path to search for in its dependency list). Useful before deleting or moving a file to see who still references it. Returns a boolean result.",
               "Resources", std::vector<std::string>({"resource", "dependency", "check"}), resource_ops::handle_has_dependency, true)

GDA_TOOL_CLASS(GetResourceReferencesTool, "get_resource_references",
               "Reverse-reference lookup: find which .tscn/.tres files reference a given resource. Provide one of path, uid or class_name (at least one is required — supplying none returns an error). Scans every text resource under res:// and returns {result: [{file, matched: [...]}, ...]}, where matched lists the hit reference kinds (path, uid, script_class). This is the reverse direction of get_resource_dependencies: instead of asking what a resource references, it asks who references it. Useful before moving or deleting a file.",
               "Resources", std::vector<std::string>({"resource", "references", "reverse"}), resource_ops::handle_get_references, true)

GDA_TOOL_CLASS(ReimportResourceFilesTool, "reimport_resource_files",
               "Queue reimport of one or more files through the editor file system. Accepts files (array of paths) or a single path; editor only, calling it outside the editor returns an error. Reimport runs asynchronously, so the result confirms the queued count, not completion; poll the file system status to know when it finishes.",
               "Resources", std::vector<std::string>({"resource", "reimport"}), resource_ops::handle_reimport, true)

GDA_TOOL_CLASS(SetResourcePropertyTool, "set_resource_property",
               "Set a property on a resource. Locate the resource by object_id/object_id_str (from create_resource or load_resource), name (in-memory) or path (loaded from disk). value is a serialized JSON value; resource-typed properties accept {'path': 'res://...'} or {'resource': 'memory://name'} references, and type_hint guides deserialization. Returns ok with class, path, object_id, resource_attached and possible readback warnings.",
               "Resources", std::vector<std::string>({"resource", "property", "set"}), resource_ops::handle_set_property, true)

GDA_TOOL_CLASS(GetResourcePropertyTool, "get_resource_property",
               "Read a property value from a resource. Locate the resource by object_id/object_id_str, name (in-memory) or path (loaded from disk), and pass the property name. Returns the serialized value; errors when the resource cannot be resolved. Use set_resource_property to modify it.",
               "Resources", std::vector<std::string>({"resource", "property", "get"}), resource_ops::handle_get_property, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(22);
  v.push_back(std::make_unique<LoadResourceTool>());
  v.push_back(std::make_unique<LoadResourceThreadedTool>());
  v.push_back(std::make_unique<GetResourceLoadThreadedStatusTool>());
  v.push_back(std::make_unique<GetResourceLoadThreadedTool>());
  v.push_back(std::make_unique<SaveResourceTool>());
  v.push_back(std::make_unique<CreateResourceTool>());
  v.push_back(std::make_unique<DuplicateResourceTool>());
  v.push_back(std::make_unique<GetResourceTypeTool>());
  v.push_back(std::make_unique<HasResourceTool>());
  v.push_back(std::make_unique<GetResourceTypesTool>());
  v.push_back(std::make_unique<GetResourceExtensionsTool>());
  v.push_back(std::make_unique<GetResourceDirFilesTool>());
  v.push_back(std::make_unique<GetResourceUidTool>());
  v.push_back(std::make_unique<SetResourceUidTool>());
  v.push_back(std::make_unique<RemoveResourceFileTool>());
  v.push_back(std::make_unique<RenameResourceFileTool>());
  v.push_back(std::make_unique<GetResourceDependenciesTool>());
  v.push_back(std::make_unique<HasResourceDependencyTool>());
  v.push_back(std::make_unique<GetResourceReferencesTool>());
  v.push_back(std::make_unique<ReimportResourceFilesTool>());
  v.push_back(std::make_unique<SetResourcePropertyTool>());
  v.push_back(std::make_unique<GetResourcePropertyTool>());
  return v;
}

} // namespace resource_tools
} // namespace godot_autopilot

#endif