#ifndef GODOT_AUTOPILOT_SCRIPT_TOOLS_HPP
#define GODOT_AUTOPILOT_SCRIPT_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/script_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace script_tools {

GDA_TOOL_CLASS(ExecuteScriptTool, "execute_script",
               "Execute arbitrary GDScript synchronously in the editor process; single-expression input returns its value automatically, multi-line code needs an explicit return statement. print() output appears in the output field, errors in the errors field. Compilation errors include line mapping and the wrapped source. There is no timeout — long code blocks the editor. The environment exposes SceneRoot (edited scene root) for node access; for the running game process use execute_game_script instead.",
               "Scripts", std::vector<std::string>({"script", "execute", "gdscript"}), script_ops::handle_execute_gdscript, true)

GDA_TOOL_CLASS(LoadScriptTool, "load_script",
               "Load a GDScript resource from a res:// path. Requires path; errors when the file is missing or the loaded resource is not a Script. Returns the script's class, path and object_id. Use it before get_script_property or reload_script to obtain a valid script.",
               "Scripts", std::vector<std::string>({"script", "load"}), script_ops::handle_load, true)

GDA_TOOL_CLASS_SIDE(CreateScriptTool, "create_script",
               "Create a new GDScript file and save it to disk. Requires path and source_code; the script is compiled before saving and compilation errors abort the call with parser output. overwrite defaults to false — an existing file errors unless overwrite=true. Returns path, class and overwritten; global_class_name is included when the script declares one. The file content is read back from disk after saving and verified against source_code (see verified/readback fields); locked-file write failures surface as verified=false instead of silent success. Do not issue create_script calls for the same file in parallel requests — serial writes to the same path are not synchronized.",
               "Scripts", std::vector<std::string>({"script", "create"}), script_ops::handle_create, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(AttachScriptToNodeTool, "attach_script_to_node",
               "Attach a GDScript file to a node in the edited scene, registered with the editor undo/redo. Requires node_path and script_path. Returns 'script attached to <path>' plus instantiated. When the script lacks @tool it cannot be instantiated in the editor, so its methods only run once the game runs — call_script_node reports the same restriction.",
               "Scripts", std::vector<std::string>({"script", "attach", "node"}), script_ops::handle_attach_to_node, true)

GDA_TOOL_CLASS(DetachScriptFromNodeTool, "detach_script_from_node",
               "Remove the script from a node in the edited scene, registered with the editor undo/redo. Requires node_path. Returns 'script detached from <path>'. The node keeps its other properties; attach a new script later with attach_script_to_node. Use it before re-attaching an updated script during iteration.",
               "Scripts", std::vector<std::string>({"script", "detach", "node"}), script_ops::handle_detach_from_node, true)

GDA_TOOL_CLASS(GetScriptPropertyTool, "get_script_property",
               "Read a property value from either a script or a scene node. With script_path it returns the script's declared default value for the property; with node_path it reads the node's current property value. Provide exactly one of the two. Returns the serialized value as 'result'; use get_script_property_list to discover property names.",
               "Scripts", std::vector<std::string>({"script", "property", "get"}), script_ops::handle_get_property, true)

GDA_TOOL_CLASS(SetScriptPropertyTool, "set_script_property",
               "Set a property directly on a node: a lightweight alternative to property_set with no undo entry and no type or read-only validation. Requires node_path, property and value; optional type_hint helps deserialize JSON into the right Variant type. Use property_set when undo support and validation matter; use this tool for quick in-memory tweaks that must not touch the undo stack.",
               "Scripts", std::vector<std::string>({"script", "property", "set"}), script_ops::handle_set_property, true)

GDA_TOOL_CLASS(CallScriptNodeTool, "call_script_node",
               "Call a method on a node in the edited scene. Requires node_path and function; optional args array passes arguments. The node's script must be marked @tool to run in the editor, otherwise the call errors — run the game instead for non-tool scripts. Unlike execute_script it runs inside the existing node instance; unlike execute_game_script it cannot reach the running game process.",
               "Scripts", std::vector<std::string>({"script", "call", "function"}), script_ops::handle_call_function, true)

GDA_TOOL_CLASS(ReloadScriptTool, "reload_script",
               "Reload a GDScript resource from disk. Requires path; optional keep_state (default false) preserves instance state across the reload. Returns the Godot Error code as 'result'. Use it after editing a script file externally so the editor picks up the changes.",
               "Scripts", std::vector<std::string>({"script", "reload"}), script_ops::handle_reload, true)

GDA_TOOL_CLASS(GetScriptPropertyListTool, "get_script_property_list",
               "List the declared variables of a GDScript file. Requires path; returns an array of entries with name, type_id, type name and usage flags, skipping internal and category entries. Use it to discover which properties get_script_property can read from the script.",
               "Scripts", std::vector<std::string>({"script", "variables", "list"}), script_ops::handle_get_variable_list, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(std::make_unique<ExecuteScriptTool>());
  v.push_back(std::make_unique<LoadScriptTool>());
  v.push_back(std::make_unique<CreateScriptTool>());
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