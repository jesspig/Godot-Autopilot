#ifndef GODOT_AUTOPILOT_SCRIPT_TOOLS_HPP
#define GODOT_AUTOPILOT_SCRIPT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/script_ops.hpp"
#include "tools/script_patch_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace script_tools {

GDA_TOOL_CLASS_SIDE(ExecuteScriptTool, "execute_script",
               "Execute arbitrary GDScript synchronously in the editor process; single-expression input returns its value automatically, multi-line code needs an explicit return statement. print() output appears in the output field, errors in the errors field. Compilation errors include line mapping and the wrapped source. There is no timeout — long code blocks the editor. The environment exposes SceneRoot (edited scene root) for node access; for the running game process use execute_game_script instead.",
               "Scripts", std::vector<std::string>({"script", "execute", "gdscript"}), script_ops::handle_execute_gdscript, true, ::godot_autopilot::SideEffect::CodeExecute)

GDA_TOOL_CLASS(LoadScriptTool, "load_script",
               "Load a GDScript resource from a res:// path. Requires path; errors when the file is missing or the loaded resource is not a Script. Returns the script's class, path and object_id. Use it before get_script_property or reload_script to obtain a valid script. Optional fresh (default false) re-reads the .gd file from disk with CACHE_MODE_IGNORE instead of returning the editor's cached instance — pass fresh:true after the file changed outside the editor, and leave it false for the cache-respecting behaviour.",
               "Scripts", std::vector<std::string>({"script", "load"}), script_ops::handle_load, true)

GDA_TOOL_CLASS_SIDE(CreateScriptTool, "create_script",
               "Create a new GDScript file and save it to disk. Requires path and source_code; source_code is written as UTF-8, so non-ASCII (e.g. CJK) content round-trips byte-exact. The script is compiled before saving and compilation errors abort the call with parser output. overwrite defaults to false — an existing file errors unless overwrite=true. Returns path, class and overwritten; cache_refreshed reports whether the freshly written file was re-read into the resource cache (cache_refresh_error is included when that reload failed); global_class_name is included when the script declares one. The file content is read back from disk after saving and verified against source_code (see verified/readback/disk_bytes/content_hash fields — disk_bytes is the read-back byte length and content_hash is the FNV-1a-64 hex of the bytes actually read from disk, with content_hash_algo naming the algorithm, so a successful response is self-proving without a follow-up read); locked-file write failures surface as verified=false instead of silent success. Do not issue create_script calls for the same file in parallel requests — serial writes to the same path are not synchronized. After changing a script that the running game uses, call reload_game_scripts first and then reload_current_scene (or retry the operation) — reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).",
               "Scripts", std::vector<std::string>({"script", "create"}), script_ops::handle_create, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(PatchScriptTool, "patch_script",
               "Patch one anchored spot in an existing GDScript file, coexisting with create_script (which writes whole files). Requires path, anchor (non-empty literal text located in the current file) and replacement (new text; empty string deletes the anchor in replace mode). Optional mode (default 'replace'): 'replace' swaps the first anchor occurrence for replacement, 'insert' inserts replacement right after the first anchor occurrence; further occurrences are left untouched and reported via anchor_occurrences. A missing anchor fails explicitly with zero disk writes. Optional preview/dry_run (default false; either true) returns a diff preview (before_context/after_context plus byte counts) without writing or compiling. The formal write compiles the patched text first via the same gate as create_script and aborts with zero disk writes on compilation failure; save/readback failures restore the original content best-effort (see rolled_back/rollback_error) and surface as verified=false with write_issue instead of silent success. Success responses mirror create_script self-proof fields (verified/readback/disk_bytes/content_hash/content_hash_algo, all derived from the disk readback). Do not issue patch_script/create_script calls for the same file in parallel requests — serial writes to the same path are not synchronized. After patching a script that the running game uses, call reload_game_scripts first and then reload_current_scene (or retry the operation) — reloading the scene alone does not guarantee the on-disk version is re-read (CACHE_MODE_REUSE).",
               "Scripts", std::vector<std::string>({"script", "patch", "edit"}), script_patch_ops::handle_patch, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(AttachScriptToNodeTool, "attach_script_to_node",
               "Attach a Script resource (e.g. a .gd file or another Script) to a node in the edited scene, registered with the editor undo/redo. Requires node_path and script_path. The script is always re-read from disk (CACHE_MODE_IGNORE) before attaching, so a node never receives a stale cached instance after the file changed outside the editor. Returns 'script attached to <path>' plus instantiated. When the script lacks @tool it cannot be instantiated in the editor, so its methods only run once the game runs — call_script_node reports the same restriction. Replacing an existing script does not migrate exported references (node_paths may be left dangling) — rewire them after attaching.",
               "Scripts", std::vector<std::string>({"script", "attach", "node"}), script_ops::handle_attach_to_node, true)

GDA_TOOL_CLASS(DetachScriptFromNodeTool, "detach_script_from_node",
               "Remove the script from a node in the edited scene, registered with the editor undo/redo. Requires node_path. Returns 'script detached from <path>'. The node keeps its other properties; attach a new script later with attach_script_to_node. Use it before re-attaching an updated script during iteration.",
               "Scripts", std::vector<std::string>({"script", "detach", "node"}), script_ops::handle_detach_from_node, true)

GDA_TOOL_CLASS(GetScriptPropertyTool, "get_script_property",
               "Read a property value from either a script or a scene node. With script_path it returns the script's declared default value for the property; with node_path it reads the node's current property value. Provide exactly one of the two. Optional fresh (default false, script_path only) re-reads the .gd file from disk with CACHE_MODE_IGNORE so a freshly written file is observed instead of the editor's cached instance; an unknown property returns null. Returns the serialized value as 'result'; use get_script_property_list to discover property names.",
               "Scripts", std::vector<std::string>({"script", "property", "get"}), script_ops::handle_get_property, true)

GDA_TOOL_CLASS(SetScriptPropertyTool, "set_script_property",
               "Set a property directly on a node: a lightweight alternative to property_set with no undo entry and no type or read-only validation. Requires node_path, property and value; optional type_hint helps deserialize JSON into the right Variant type. Use property_set when undo support and validation matter; use this tool for quick in-memory tweaks that must not touch the undo stack.",
               "Scripts", std::vector<std::string>({"script", "property", "set"}), script_ops::handle_set_property, true)

GDA_TOOL_CLASS(CallScriptNodeTool, "call_script_node",
               "Call a method on a node in the edited scene. Requires node_path and function; optional args array passes arguments. The node's script must be marked @tool to run in the editor, otherwise the call errors — run the game instead for non-tool scripts. Unlike execute_script it runs inside the existing node instance; unlike execute_game_script it cannot reach the running game process.",
               "Scripts", std::vector<std::string>({"script", "call", "function"}), script_ops::handle_call_function, true)

GDA_TOOL_CLASS(ReloadScriptTool, "reload_script",
               "Reload a GDScript resource from disk: the .gd file is re-read with CACHE_MODE_IGNORE and recompiled, so the editor's cached instance observes the on-disk version instead of its previous source. Requires path; optional keep_state (default false) is passed to the final reload(true/false) — with live script instances (a node already uses the script) keep_state must be true, otherwise the returned Godot Error code is ERR_ALREADY_IN_USE. Returns the Godot Error code as 'result' (0 = OK). Use it after editing a script file externally so the editor picks up the changes.",
               "Scripts", std::vector<std::string>({"script", "reload"}), script_ops::handle_reload, true)

GDA_TOOL_CLASS(GetScriptPropertyListTool, "get_script_property_list",
               "List the declared variables of a GDScript file. Requires path; optional fresh (default false) re-reads the .gd file from disk with CACHE_MODE_IGNORE instead of listing the editor's cached instance — use fresh:true right after create_script or an external edit. Returns an array of entries with name, type_id, type name and usage flags, skipping internal and category entries. Use it to discover which properties get_script_property can read from the script.",
               "Scripts", std::vector<std::string>({"script", "variables", "list"}), script_ops::handle_get_variable_list, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(11);
  v.push_back(std::make_unique<ExecuteScriptTool>());
  v.push_back(std::make_unique<LoadScriptTool>());
  v.push_back(std::make_unique<CreateScriptTool>());
  v.push_back(std::make_unique<PatchScriptTool>());
  v.push_back(std::make_unique<AttachScriptToNodeTool>());
  v.push_back(std::make_unique<DetachScriptFromNodeTool>());
  v.push_back(std::make_unique<GetScriptPropertyTool>());
  v.push_back(std::make_unique<SetScriptPropertyTool>());
  v.push_back(std::make_unique<CallScriptNodeTool>());
  v.push_back(std::make_unique<ReloadScriptTool>());
  v.push_back(std::make_unique<GetScriptPropertyListTool>());
  return v;
}

} // namespace script_tools
} // namespace godot_autopilot

#endif
