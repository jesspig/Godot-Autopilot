#ifndef GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP
#define GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/resource_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace resource_tools {

namespace {

const std::vector<ParamSpec> kLoadResourceParams = {
    {"path", "string", "Resource file path to load, e.g. res://my_resource.tres", true},
    {"type_hint", "string", "Expected resource class to guide loading, e.g. PackedScene, Texture2D; default: none", false},
};

const std::vector<ParamSpec> kReloadResourceParams = {
    {"path", "string", "Resource file path to force-reload from disk, e.g. res://my_resource.tres", true},
};

const std::vector<ParamSpec> kLoadResourceThreadedParams = {
    {"path", "string", "Resource file path to load in the background, e.g. res://my_resource.tres", true},
    {"type_hint", "string", "Expected resource class to guide loading, e.g. PackedScene, Texture2D; default: none", false},
    {"use_sub_threads", "boolean", "Split the load across worker sub-threads for faster loading; default: false", false},
};

const std::vector<ParamSpec> kGetResourceLoadThreadedStatusParams = {
    {"path", "string", "Resource file path whose threaded load was started with load_resource_threaded", true},
};

const std::vector<ParamSpec> kGetResourceLoadThreadedParams = {
    {"path", "string", "Resource file path whose threaded load was started with load_resource_threaded; errors if no such load is pending", true},
};

const std::vector<ParamSpec> kSaveResourceParams = {
    {"path", "string", "Source path to load from, or destination path if object_id is provided; required to identify the resource", true},
    {"dest_path", "string", "Destination file path to write, e.g. res://out/my_resource.tres; default: path. Missing parent directories are created recursively; when dest_path differs from path, the resource is duplicated first so the source cached instance is not modified; the response then includes copy_on_write and source_path", false},
    {"name", "string", "Name of an in-memory resource (as passed to create_resource) to save", false},
    {"class_type", "string", "Resource class to instantiate when no resource resolves, e.g. Curve2D; requires name to register the new instance", false},
    {"object_id", "integer", "Object ID of an in-memory resource (from create_resource) to save; mutually exclusive with object_id_str", false},
    {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
    {"flags", "integer", "ResourceSaver.SaverFlags bitfield, e.g. 1 = RELATIVE_PATHS, 8 = REPLACE_SUBRESOURCE_PATHS; default: 0", false},
};

const std::vector<ParamSpec> kCreateResourceParams = {
    {"type", "string", "Resource class to instantiate, e.g. Resource, Curve2D, PackedScene; accepts both engine classes from ClassDB and global script classes (GDScript class_name / C# [GlobalClass]); abstract or invalid scripts are rejected", true},
    {"name", "string", "Registration name so later calls can reference the instance as memory://name and via the returned object_id; optional but recommended", false},
};

const std::vector<ParamSpec> kDuplicateResourceParams = {
    {"path", "string", "Disk path of the resource to load and duplicate, e.g. res://my_resource.tres; in-memory resources are not supported", true},
    {"deep", "boolean", "Deep duplicate copies sub-resources (true), shallow keeps shared references (false, default)", false},
    {"name", "string", "Registration name so later calls can reference the duplicate as memory://name and via the returned object_id; when omitted only the object_id is registered", false},
};

const std::vector<ParamSpec> kGetResourceTypeParams = {
    {"path", "string", "Resource file path whose class type to report, e.g. res://my_resource.tres", true},
};

const std::vector<ParamSpec> kHasResourceParams = {
    {"path", "string", "Path to check for loadability via the resource loader, e.g. res://icon.svg; not a plain file-existence test", true},
};

const std::vector<ParamSpec> kGetResourceTypesParams = {};

const std::vector<ParamSpec> kGetResourceExtensionsParams = {
    {"type", "string", "Resource class name to list extensions for, e.g. Texture2D, AudioStream; when omitted the loader returns extensions for all known types (contract gap: required in schema but optional in the implementation)", true},
};

const std::vector<ParamSpec> kGetResourceDirFilesParams = {
    {"path", "string", "Directory path to list, e.g. res://assets; returns files and subdirectory names as entry names", true},
};

const std::vector<ParamSpec> kGetResourceUidParams = {
    {"path", "string", "Resource file path whose UID to read, e.g. res://my_resource.tres", true},
};

const std::vector<ParamSpec> kSetResourceUidParams = {
    {"path", "string", "Resource file path to assign the UID to, e.g. res://my_resource.tres", true},
    {"uid", "integer", "UID value to assign; when omitted a fresh UID is auto-generated (contract gap: required in schema but optional in the implementation)", true},
};

const std::vector<ParamSpec> kRemoveResourceFileParams = {
    {"path", "string", "Resource file path to delete from disk, e.g. res://my_resource.tres; without force the call only runs the impact pre-check and deletes nothing", true},
    {"force", "boolean", "Two-stage safety switch (default false): false returns would_delete plus dependents from a reverse-reference scan; true performs the deletion — preferentially moving the file to the OS trash (trashed=true) with fallback to permanent removal (permanent=true), also removing the .uid sidecar (sidecars_removed)", false},
};

const std::vector<ParamSpec> kRenameResourceFileParams = {
    {"path", "string", "Current resource path, e.g. res://old_name.tres; 'from' is accepted as an alias", true},
    {"new_path", "string", "New resource path, e.g. res://new_name.tres; 'to' is accepted as an alias", true},
};

const std::vector<ParamSpec> kMoveResourceFileParams = {
    {"path", "string", "Existing res:// file or directory to move, e.g. res://sfx/jump.wav; directories move recursively preserving the sub-folder layout with *.uid sidecars following their owners", true},
    {"new_directory", "string", "Destination directory inside res:// (absolute like res://assets/sfx or relative like assets/sfx); trailing slashes are trimmed, user:// is rejected", true},
};

const std::vector<ParamSpec> kCopyResourceFileParams = {
    {"path", "string", "Source file path to copy, e.g. res://assets/icon.png; must exist", true},
    {"dest_path", "string", "Destination file path to write, e.g. res://assets/icon_copy.png; missing parent directories are created recursively and an existing file is overwritten", true},
};

const std::vector<ParamSpec> kCreateDirectoryParams = {
    {"path", "string", "Directory to create below res:// (absolute like res://assets/audio or relative like assets/audio); missing parents are created recursively; idempotent when the directory already exists", true},
};

const std::vector<ParamSpec> kGetResourceDependenciesParams = {
    {"path", "string", "Resource file path whose dependencies to list, e.g. res://scene.tscn", true},
};

const std::vector<ParamSpec> kHasResourceDependencyParams = {
    {"path", "string", "Resource file path to inspect, e.g. res://scene.tscn", true},
    {"dependency", "string", "Dependency file path to search for in the resource's dependency list, e.g. res://icon.svg", true},
};

const std::vector<ParamSpec> kGetResourceReferencesParams = {
    {"path", "string", "Resource path to search for as a target reference, e.g. res://assets/icon.png; ignore uid and class_name", false},
    {"uid", "string", "Resource UID to search for in uid:// references, e.g. uid://abc123", false},
    {"class_name", "string", "Script class name to search for in script references, e.g. Player", false},
};

const std::vector<ParamSpec> kReimportResourceFilesParams = {
    {"path", "string", "Single resource file path to reimport; alternative to files", true},
    {"files", "array", "Array of resource file paths to reimport; alternative to a single path", false},
};

const std::vector<ParamSpec> kSetResourcePropertyParams = {
    {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
    {"object_id", "integer", "Object ID of an in-memory resource (from create_resource or load_resource)", false},
    {"name", "string", "Registration name of an in-memory resource (memory://name)", false},
    {"path", "string", "Resource path; loads the file from disk when no object_id or name matches", false},
    {"property", "string", "Property name to set, e.g. curve, texture; must exist on the resource", true},
    {"value", "object", "Serialized JSON value; resource-typed properties accept {'path': 'res://xxx.tres'} or {'resource': 'memory://name'} references", true},
    {"type_hint", "string", "Variant type hint for deserialization, e.g. Vector2, Color, int, float; auto-detected from the property metadata when omitted", false},
};

const std::vector<ParamSpec> kGetResourcePropertyParams = {
    {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
    {"object_id", "integer", "Object ID of an in-memory resource (from create_resource or load_resource)", false},
    {"name", "string", "Registration name of an in-memory resource (memory://name)", false},
    {"path", "string", "Resource path; loads the file from disk when no object_id or name matches", false},
    {"property", "string", "Property name to read, e.g. curve, texture", true},
    {"type_hint", "string", "Accepted for parity with set_resource_property; reading always returns the raw serialized value", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(26);
  v.push_back(make_spec_tool(ToolSpec{
      "load_resource",
      "Load a resource file from disk into memory. Use it to bring a .tres or .tscn file into the editor for inspection or modification, with an optional type_hint to guide loading. Returns the resource reference with class, path, object_id and object_id_str, which set_resource_property and get_resource_property accept as locators. Errors when the file is missing or fails to load.",
      "Resources", {"resource", "load"}, SideEffect::None, tool_flags::kNone,
      kLoadResourceParams, resource_ops::handle_load}));
  v.push_back(make_spec_tool(ToolSpec{
      "reload_resource",
      "Force-reload a resource file from disk, replacing the editor's cached instance via ResourceLoader CACHE_MODE_REPLACE. Use it after the file changed on disk outside the editor so later reads observe the on-disk contents instead of a stale cached instance. Returns result (reloaded), path and replaced (true when the cached instance was swapped for a freshly loaded one). Errors when the path is outside res:// or the file cannot be loaded.",
      "Resources", {"resource", "reload", "cache"}, SideEffect::None, tool_flags::kNone,
      kReloadResourceParams, resource_ops::handle_reload}));
  v.push_back(make_spec_tool(ToolSpec{
      "load_resource_threaded",
      "Start an asynchronous background load of a resource file and return immediately. It is the first step of the threaded-load chain: after this call, poll get_resource_load_threaded_status, then fetch the result with get_resource_load_threaded. use_sub_threads enables finer-grained parallel loading. Returns the Godot error code (0 = OK).",
      "Resources", {"resource", "load", "threaded"}, SideEffect::None, tool_flags::kNone,
      kLoadResourceThreadedParams, resource_ops::handle_load_threaded}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_load_threaded_status",
      "Poll the status of a threaded load started by load_resource_threaded. Use it between starting the load and fetching the result. Returns status as one of invalid_resource, in_progress, failed or loaded, plus a numeric code; poll until loaded, then call get_resource_load_threaded to retrieve the resource.",
      "Resources", {"resource", "load", "status"}, SideEffect::None, tool_flags::kNone,
      kGetResourceLoadThreadedStatusParams, resource_ops::handle_load_threaded_get_status}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_load_threaded",
      "Blockingly fetch the resource loaded by load_resource_threaded, completing the threaded-load chain: start with load_resource_threaded, poll get_resource_load_threaded_status, then retrieve here. Returns the reference with class, path, object_id and object_id_str; calling it without a pending threaded load returns an error. Use it for large resources so the editor stays responsive during loading.",
      "Resources", {"resource", "load", "wait"}, SideEffect::None, tool_flags::kNone,
      kGetResourceLoadThreadedParams, resource_ops::handle_load_threaded_wait}));
  v.push_back(make_spec_tool(ToolSpec{
      "save_resource",
      "Save a resource to disk at dest_path (defaults to path). Locate the resource by object_id/object_id_str (from create_resource), name (in-memory) or path (loaded from disk); if none resolves, class_type plus a name creates and registers a new instance. Missing directories are created recursively and the editor file system is refreshed. Returns the Godot error code, verified and directories_created when applicable. verified means the resource was re-read from disk after saving; on Windows, safe-save briefly replaces the file, so a concurrent reader may transiently fail — re-check/retry if that surfaces.",
      "Resources", {"resource", "save"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveResourceParams, resource_ops::handle_save}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_resource",
      "Instantiate a Resource subclass by class name (type) in memory. type accepts both engine classes from ClassDB and global script classes (GDScript class_name / C# [GlobalClass]); abstract or invalid scripts are rejected. Pass name to register the instance so later calls can reference it as memory://name. Returns the reference with object_id and object_id_str, which serve as locators for set_resource_property, get_resource_property and save_resource. Errors when type is not a known class or not an instantiable Resource subclass.",
      "Resources", {"resource", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateResourceParams, resource_ops::handle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "duplicate_resource",
      "Load a resource from a disk path and return a duplicated instance. deep (default false) controls whether sub-resources are deep-copied. Only file-backed resources are supported; in-memory instances from create_resource must be saved and reloaded first. Pass name to also register the duplicate, so later calls can reference it as memory://name or via the returned object_id; the registered reference is what keeps the duplicate in memory across calls. Returns the duplicate's reference with object_id and name.",
      "Resources", {"resource", "duplicate"}, SideEffect::None, tool_flags::kNone,
      kDuplicateResourceParams, resource_ops::handle_duplicate}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_type",
      "Load the resource at path and report its engine class name, which tells you what the file actually holds. Returns class and path; errors when the file does not exist or fails to load. Use get_resource_types to discover valid class names for create_resource.",
      "Resources", {"resource", "type"}, SideEffect::None, tool_flags::kNone,
      kGetResourceTypeParams, resource_ops::handle_get_type}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_resource",
      "Check whether path is loadable as a resource using the engine's resource loader. This is not a plain file-existence check: files that are not imported or not recognized may return false even though they exist on disk. Returns a boolean.",
      "Resources", {"resource", "exists"}, SideEffect::None, tool_flags::kNone,
      kHasResourceParams, resource_ops::handle_exists}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_types",
      "List every instantiable Resource subclass registered in the engine, including built-in classes and script-based ones. Use it to discover valid type values for create_resource or to confirm that a class exists before instantiating it. Returns an array of class names.",
      "Resources", {"resource", "types", "list"}, SideEffect::None, tool_flags::kNone,
      kGetResourceTypesParams, resource_ops::handle_list_types}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_extensions",
      "List the file extensions recognized for a resource type, e.g. Texture2D returns png, jpg and webp. type is optional: when omitted the loader reports extensions for all known types instead. Returns an array of extension strings without the leading dot.",
      "Resources", {"resource", "extensions"}, SideEffect::None, tool_flags::kNone,
      kGetResourceExtensionsParams, resource_ops::handle_get_extensions}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_dir_files",
      "List the entries of a directory (files and subdirectory names) through the resource loader. Requires path, the res:// or user:// directory to inspect. Returns an array of entry names; subdirectories appear as plain names, so combine with has_resource to tell loadable resources apart from folders.",
      "Resources", {"resource", "directory", "list"}, SideEffect::None, tool_flags::kNone,
      kGetResourceDirFilesParams, resource_ops::handle_list_dir}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_uid",
      "Read the numeric UID of a resource file, the integer behind its uid:// reference. UIDs keep references stable across renames, so they are preferred over raw paths in saved scenes. Returns the UID as an integer, or -1 (INVALID_ID) when the file has none; use set_resource_uid to assign one.",
      "Resources", {"resource", "uid", "get"}, SideEffect::None, tool_flags::kNone,
      kGetResourceUidParams, resource_ops::handle_get_uid}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_resource_uid",
      "Assign a UID to a resource file so it can be referenced as uid://<id>, the counterpart of get_resource_uid. uid is optional: when omitted a fresh UID is auto-generated. Triggers a filesystem reimport so the assignment persists. Returns the UID that was applied.",
      "Resources", {"resource", "uid", "set"}, SideEffect::None, tool_flags::kNone,
      kSetResourceUidParams, resource_ops::handle_set_uid}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_resource_file",
      "Delete a resource file from disk in a two-stage flow. Without force (default) it is a dry-run impact pre-check: a reverse-reference scan over every .tscn/.tres under res:// (path and uid tokens) returns would_delete plus dependents ({file, matched} objects) and a hint — nothing is deleted, even with zero dependents an explicit force=true is required. With force=true the file is preferentially moved to the OS trash (trashed=true; permanent=true when trash is unavailable or fails), its cached reference path is cleared, the .uid sidecar travels with it (sidecars_removed lists removed sidecar paths) and the editor file system is updated. Use rename_resource_file when you want to keep the file. Returns the Godot error code (0 = OK).",
      "Resources", {"resource", "remove", "delete"}, SideEffect::None, tool_flags::kNone,
      kRemoveResourceFileParams, resource_ops::handle_remove}));
  v.push_back(make_spec_tool(ToolSpec{
      "rename_resource_file",
      "Rename or move a resource file. Accepts path and new_path, with from and to accepted as aliases. The .uid sidecar travels with the file (the .import sidecar of imported assets migrates alongside it when present), open scenes that equal the file or reference it are automatically saved before the disk rewrite and reloaded afterwards — their undo history resets — path= and uid= tokens in dependent .tscn/.tres files are rewritten, cached resource paths and the edited scene file path migrate, project.godot entries pointing at the old path (application/run/main_scene and autoload/* values, preserving the leading * singleton marker) are remapped and saved, and the editor file system is notified. Returns the Godot error code, updated_files, stale_references as {file, token} objects (token names the literal leftover reference; script_class tokens need scan_editor_file_system after a class_name rename to re-register global classes — see note), uid_preserved, remapped_settings, saved_scenes, reloaded_scenes, unsupported_binary_references ({file, reason} entries for binary resources such as .scn/.res/.csv/.translation that cannot be rewritten in place) and import_sidecar_warning when the .import move fails.",
      "Resources", {"resource", "rename", "move"}, SideEffect::None, tool_flags::kNone,
      kRenameResourceFileParams, resource_ops::handle_rename}));
  v.push_back(make_spec_tool(ToolSpec{
      "move_resource_file",
      "Move a file to another directory inside res:// while keeping its file name, through the same engine-aware transaction pipeline as rename_resource_file: the .uid sidecar travels with the file (.import sidecars migrate too), open scenes that equal the moved file or reference it are automatically saved before the rewrite and reloaded afterwards — their undo history resets — path= and uid= tokens in dependent .tscn/.tres files are rewritten, project.godot entries pointing at the old path (main_scene and autoload/*, preserving the leading * marker) are remapped and saved, cached resource paths and the edited scene file path migrate, and the editor file system is notified per rewritten file. Parameters: path (an existing res:// file or directory) and new_directory (destination directory inside res://, absolute like res://assets/sfx or relative like assets/sfx; trailing slashes are trimmed; user:// is rejected). When path is a directory, every file under it moves recursively preserving the sub-folder layout, *.uid files follow their owners, empty source folders are left behind, and an editor file system scan runs afterwards. Returns moved (array of {from, to, updated_files, uid_preserved}), failed (array of {from, error}), aggregated stale_references as {file, token} objects whose token names the literal leftover reference (script_class tokens require scan_editor_file_system to re-register renamed global classes; see note on each transaction), aggregated unsupported_binary_references ({file, reason} for binary resources such as .scn/.res/.csv/.translation that cannot be rewritten in place), aggregated saved_scenes and reloaded_scenes (scenes saved before each rewrite and reloaded afterwards) and import_sidecar_warnings (array present only when some .import sidecar move failed). Errors when the source does not exist, new_directory targets user://, or a directory move points into its own sub-folder.",
      "Resources", {"resource", "move", "directory"}, SideEffect::WritesFile, tool_flags::kMutating,
      kMoveResourceFileParams, resource_ops::handle_move}));
  v.push_back(make_spec_tool(ToolSpec{
      "copy_resource_file",
      "Copy a file to dest_path inside res://. Both path (an existing res:// file) and dest_path are required; missing parent directories of dest_path are created recursively and an existing dest_path is overwritten. The copy is byte-for-byte: .import and .uid sidecars are not copied, imported assets need a reimport and a duplicated uid in the copied text resource may be reassigned by the editor scan. The editor file system is notified for dest_path and a scan runs afterwards. Returns result (copied), from and to. Use rename_resource_file/move_resource_file when you want to keep the file or remap references.",
      "Resources", {"resource", "copy", "file"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCopyResourceFileParams, resource_ops::handle_copy}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_directory",
      "Create a directory inside the project, including any missing parent directories. path accepts a res:// absolute path (res://assets/audio) or a relative one (assets/audio); user:// is rejected. Idempotent: when the directory already exists the call succeeds and reports already_existed=true instead of failing. A newly created directory triggers an editor file system scan so subsequent resource operations see it immediately. Returns created (the normalized res:// path) and already_existed; errors when the directory cannot be created (invalid path or permission failure).",
      "Resources", {"resource", "directory", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCreateDirectoryParams, resource_ops::handle_create_directory}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_dependencies",
      "List the dependencies of a resource file, i.e. the external files it references (textures, scenes, scripts). Use it to audit what a resource needs before moving or deleting files. Returns an array of res:// paths; use has_resource_dependency for a targeted single check.",
      "Resources", {"resource", "dependencies", "list"}, SideEffect::None, tool_flags::kNone,
      kGetResourceDependenciesParams, resource_ops::handle_get_dependencies}));
  v.push_back(make_spec_tool(ToolSpec{
      "has_resource_dependency",
      "Check whether a resource file depends on a specific other file. Requires path (the resource to inspect) and dependency (the path to search for in its dependency list). Useful before deleting or moving a file to see who still references it. Returns a boolean result.",
      "Resources", {"resource", "dependency", "check"}, SideEffect::None, tool_flags::kNone,
      kHasResourceDependencyParams, resource_ops::handle_has_dependency}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_references",
      "Reverse-reference lookup: find which .tscn/.tres files reference a given resource. Provide one of path, uid or class_name (at least one is required — supplying none returns an error). Scans every text resource under res:// and returns {result: [{file, matched: [...]}, ...]}, where matched lists the hit reference kinds (path, uid, script_class). This is the reverse direction of get_resource_dependencies: instead of asking what a resource references, it asks who references it. Useful before moving or deleting a file.",
      "Resources", {"resource", "references", "reverse"}, SideEffect::None, tool_flags::kNone,
      kGetResourceReferencesParams, resource_ops::handle_get_references}));
  v.push_back(make_spec_tool(ToolSpec{
      "reimport_resource_files",
      "Queue reimport of one or more files through the editor file system. Accepts files (array of paths) or a single path; editor only, calling it outside the editor returns an error. Reimport runs asynchronously, so the result confirms the queued count, not completion; poll the file system status to know when it finishes.",
      "Resources", {"resource", "reimport"}, SideEffect::None, tool_flags::kNone,
      kReimportResourceFilesParams, resource_ops::handle_reimport}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_resource_property",
      "Set a property on a resource. Locate the resource by object_id/object_id_str (from create_resource or load_resource), name (in-memory) or path (loaded from disk). value is a serialized JSON value; resource-typed properties accept {'path': 'res://...'} or {'resource': 'memory://name'} references, and type_hint guides deserialization. Returns ok with class, path, object_id, resource_attached and possible readback warnings.",
      "Resources", {"resource", "property", "set"}, SideEffect::None, tool_flags::kNone,
      kSetResourcePropertyParams, resource_ops::handle_set_property}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_resource_property",
      "Read a property value from a resource. Locate the resource by object_id/object_id_str, name (in-memory) or path (loaded from disk), and pass the property name. Returns the serialized value; errors when the resource cannot be resolved. Use set_resource_property to modify it.",
      "Resources", {"resource", "property", "get"}, SideEffect::None, tool_flags::kNone,
      kGetResourcePropertyParams, resource_ops::handle_get_property}));
  return v;
}

} // namespace resource_tools
} // namespace godot_autopilot

#endif