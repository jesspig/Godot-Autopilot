#ifndef GODOT_AUTOPILOT_ANALYZE_TOOLS_HPP
#define GODOT_AUTOPILOT_ANALYZE_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/analyze/analyze_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace analyze_tools {

namespace {

const std::vector<std::string> kAnalyzeProjectActions = {
    "validate_scene", "unused_resources", "signal_flow"};

const std::vector<ParamSpec> kAnalyzeProjectParams = {
    {"path", "string", "validate_scene: res:// path to a .tscn/.scn scene file on disk to dry-run validate (e.g. \"res://levels/level_01.tscn\"); signal_flow: scene-relative node path of the node whose signal wiring is traced (e.g. \"Player\")", false},
    {"directory", "string", "unused_resources: project directory whose files are scanned recursively for unreferenced resources (string, default: \"res://\", e.g. \"res://assets\")", false},
    {"direction", "string", "signal_flow: which connections to report: 'outgoing' (signals the node emits), 'incoming' (connections targeting it) or 'both' (default)", false},
    {"max_depth", "integer", "signal_flow: how many hops to walk from the start node, >= 1 (default: 3); cycles are cut by a visited set", false},
    {"include_state", "boolean", "validate_scene: also read PackedScene.get_state() from the file and return the state section {node_count, nodes, connections, truncated} with node paths, types, owners, properties and groups (boolean, default: false); read-only", false},
    {"max_nodes", "integer", "validate_scene: maximum number of state nodes to return when include_state is true, >= 1 (default: 200); extra nodes set truncated true", false},
};

inline godot_autopilot::util::JsonValue analyze_alias_args_with_action(
    const godot_autopilot::util::JsonValue &args, const char *action) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  if (args.IsObject()) {
    out = args;
  }
  out["action"] = godot_autopilot::util::JsonValue(action);
  return out;
}

inline ToolAlias analyze_action_alias(const char *name, const char *canonical,
                                      const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const godot_autopilot::util::JsonValue &args) {
    return analyze_alias_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(ToolSpec{
      "analyze_project",
      "Run read-only project analysis in one action tool; no action touches the edited scene or writes files. action='validate_scene' dry-run validates the scene file at 'path' (res:// .tscn/.scn): it loads it via ResourceLoader with CACHE_MODE_IGNORE, checks every dependency reported by get_dependencies for existence on disk, then calls PackedScene::instantiate() and immediately frees the instance, returning {valid, problems:[{kind,message}], missing_dependencies, dependency_count} with problem kinds load_failed / missing_dependency / instantiate_failed. With 'include_state' true it also reads PackedScene.get_state() and returns state {node_count, nodes:[{path,type,owner,properties,groups}], connections:[{source,signal,target,method,flags}], returned_nodes, truncated, max_nodes} together with the same node_count/nodes/connections/truncated fields mirrored at the result top level; nodes are capped by 'max_nodes' (default 200) and truncated marks dropped nodes. action='unused_resources' finds project files never referenced by any other file under 'directory' (default \"res://\"): it scans the editor filesystem recursively, collects each file's get_dependencies(), resolves uid:// entries through ResourceUID (unresolvable ones are listed in 'unresolved_uids' instead of being misreported), then subtracts the referenced set plus exemptions (icon.svg, main_scene and autoload paths from ProjectSettings, *.import sidecars and project.godot), returning {scanned, unused:[{path,type}], unresolved_uids, exempted}; large projects may take a few seconds. action='signal_flow' traces signal wiring around 'path' in the edited scene with optional 'direction' (outgoing/incoming/both, default both) and 'max_depth' (int >= 1, default 3; cycles are cut by a visited set): each edge is {from,signal,to,method,persisted} where persisted marks CONNECT_PERSIST connections, non-node targets appear as Class#instance_id, and the result is {root, edges, edge_count}. Deprecated aliases: validate_scene_file (action=validate_scene), find_unused_resources (action=unused_resources), trace_signal_flow (action=signal_flow).",
      "Analysis",
      {"analysis", "validation", "scene", "dependencies", "resources", "unused",
       "signals", "connections", "graph", "scene_state", "state",
       "validate_scene_file", "find_unused_resources", "trace_signal_flow"},
      SideEffect::None, tool_flags::kNone,
      kAnalyzeProjectParams, analyze_ops::handle_analyze_project,
      schema::build_action_schema(
          kAnalyzeProjectActions, kAnalyzeProjectParams,
          "Analysis action: validate_scene|unused_resources|signal_flow"),
      {
          analyze_action_alias("validate_scene_file", "analyze_project",
                               "validate_scene"),
          analyze_action_alias("find_unused_resources", "analyze_project",
                               "unused_resources"),
          analyze_action_alias("trace_signal_flow", "analyze_project",
                               "signal_flow"),
      },
      {"{\"action\":\"validate_scene\",\"path\":\"res://levels/level_01.tscn\"}",
       "{\"action\":\"validate_scene\",\"path\":\"res://levels/level_01.tscn\",\"include_state\":true,\"max_nodes\":100}",
       "{\"action\":\"unused_resources\",\"directory\":\"res://assets\"}",
       "{\"action\":\"signal_flow\",\"path\":\"Player\",\"direction\":\"both\",\"max_depth\":3}"}}));
  return v;
}

} // namespace analyze_tools
} // namespace godot_autopilot

#endif
