#ifndef GODOT_AUTOPILOT_ANALYZE_TOOLS_HPP
#define GODOT_AUTOPILOT_ANALYZE_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/analyze_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace analyze_tools {

GDA_TOOL_CLASS(ValidateSceneFileTool, "validate_scene_file",
               "Dry-run validate a scene file on disk without touching the "
               "currently edited scene. Required 'path' (string, res:// path "
               "to a .tscn/.scn file, e.g. \"res://levels/level_01.tscn\"). "
               "Loads it via ResourceLoader with CACHE_MODE_IGNORE (bypasses "
               "the resource cache), checks every dependency reported by "
               "get_dependencies for existence on disk, then calls "
               "PackedScene::instantiate() and immediately frees the instance. "
               "Returns {valid, problems:[{kind,message}], "
               "missing_dependencies, dependency_count}; problem kinds: "
               "load_failed / missing_dependency / instantiate_failed. Use "
               "after bulk edits or before committing scene changes.",
               "Analysis",
               std::vector<std::string>(
                   {"analysis", "validation", "scene", "dependencies"}),
               analyze_ops::handle_validate_scene_file, true)

GDA_TOOL_CLASS(FindUnusedResourcesTool, "find_unused_resources",
               "Find project files that are never referenced by any other "
               "file's dependencies under 'directory' (optional string, "
               "default \"res://\", e.g. \"res://assets\"). Scans the editor "
               "filesystem recursively (EditorFileSystemDirectory), collects "
               "each file's get_dependencies(), resolves uid:// entries "
               "through ResourceUID (unresolvable ones are listed in "
               "'unresolved_uids' instead of being misreported), then "
               "subtracts the referenced set plus exemptions: icon.svg, "
               "main_scene and autoload paths read from ProjectSettings, "
               "*.import sidecars and project.godot. Returns {scanned, "
               "unused:[{path,type}], unresolved_uids, exempted}. Read-only; "
               "large projects may take a few seconds.",
               "Analysis",
               std::vector<std::string>(
                   {"analysis", "resources", "unused", "dependencies"}),
               analyze_ops::handle_find_unused_resources, true)

GDA_TOOL_CLASS(TraceSignalFlowTool, "trace_signal_flow",
               "Trace signal wiring around a node of the edited scene. "
               "Required 'path' (string, scene-relative node path, e.g. "
               "\"Player\"); optional 'direction' (string: \"outgoing\" for "
               "signals the node emits, \"incoming\" for connections targeting "
               "it, default \"both\") and 'max_depth' (int >= 1, how many hops "
               "to walk from the start node, default 3; cycles are cut by a "
               "visited set). Each edge is {from,signal,to,method,persisted} "
               "where 'persisted' marks connections saved in the scene file "
               "(CONNECT_PERSIST). Non-node targets such as autoloads are "
               "shown as Class#instance_id. Returns {root, edges, edge_count}.",
               "Analysis",
               std::vector<std::string>(
                   {"analysis", "signals", "connections", "graph"}),
               analyze_ops::handle_trace_signal_flow, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(3);
  v.push_back(std::make_unique<ValidateSceneFileTool>());
  v.push_back(std::make_unique<FindUnusedResourcesTool>());
  v.push_back(std::make_unique<TraceSignalFlowTool>());
  return v;
}

} // namespace analyze_tools
} // namespace godot_autopilot

#endif
