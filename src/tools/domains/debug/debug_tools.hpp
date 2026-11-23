#ifndef GODOT_AUTOPILOT_DEBUG_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUG_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/debug/debug_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace debug_tools {

namespace {

const std::vector<ParamSpec> kPrintDebugLogParams = {
    {"message", "string", "Debug text to record in the plugin log at Info level; the message does not reach the game log", true},
};

const std::vector<ParamSpec> kGetDebugStackParams = {
    {"include_variables", "boolean", "Include variable names in the dump: global_variables per backtrace, local_variables and member_variables per frame (default: false)", false},
};

const std::vector<ParamSpec> kSetDebugPhysicsFpsParams = {
    {"fps", "number", "Physics ticks per second (integer; Godot default is 60, typical debug values 10-60). Applies immediately and globally to the engine but is not persisted — restore 60 when done", true},
};

const std::vector<ParamSpec> kGetDebugInfoParams = {
    {"ids", "array", "Only with action=monitors: array of integer monitor ids to return; each entry includes id, name, type and value. Out-of-range ids return an error", false},
    {"include_custom", "boolean", "Only with action=monitors: when true, return {monitors, custom_names} — the built-in monitor values plus registered custom monitor names", false},
    {"id", "string", "Only with action=custom: name of the custom monitor registered via Performance.add_custom_monitor; returns its value serialized as JSON Variant. Omit id to list registered custom monitor names", false},
};

const std::vector<ParamSpec> kRemoveDebugCustomMonitorParams = {
    {"id", "string", "Name of the custom monitor to remove, as registered via Performance.add_custom_monitor by the game or scripts", true},
};

const std::vector<ParamSpec> kSetDebugVisualsParams = {
    {"enabled", "boolean", "True to show the selected debug visual in editor game runs, false to hide it; the flag is written to project metadata, so it persists in project.godot until changed back", true},
};

const std::vector<std::string> kGetDebugInfoActions = {
    "monitors", "catalog", "custom", "object", "node", "memory"};
const std::vector<std::string> kSetDebugVisualsActions = {
    "collision", "navigation", "performance"};

inline mcp::JsonValue debug_monitors_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *catalog = args.Find("catalog");
  if (catalog != nullptr && catalog->IsBool() && catalog->GetBool()) {
    out["action"] = mcp::JsonValue("catalog");
    return out;
  }
  const mcp::JsonValue *ids = args.Find("ids");
  if (ids != nullptr) {
    out["action"] = mcp::JsonValue("monitors");
    out["ids"] = *ids;
    return out;
  }
  const mcp::JsonValue *include_custom = args.Find("include_custom");
  if (include_custom != nullptr && include_custom->IsBool() &&
      include_custom->GetBool()) {
    out["action"] = mcp::JsonValue("custom");
    return out;
  }
  out["action"] = mcp::JsonValue("monitors");
  return out;
}

inline mcp::JsonValue project_monitor_value(const mcp::JsonValue &result) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *inner = result.Find("result");
  if (inner != nullptr && inner->IsArray() && inner->Size() > 0) {
    const mcp::JsonValue *value = (*inner)[0].Find("value");
    if (value != nullptr) {
      out["result"] = *value;
    }
  }
  if (!out.Contains("result")) {
    out["error"] = mcp::JsonValue("monitor value not available");
  }
  return out;
}

inline mcp::JsonValue project_custom_monitor_names(
    const mcp::JsonValue &result) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *inner = result.Find("result");
  const mcp::JsonValue *names =
      inner != nullptr && inner->IsObject() ? inner->Find("custom_names")
                                            : nullptr;
  if (names != nullptr) {
    out["result"] = *names;
  } else {
    out["error"] = mcp::JsonValue("custom monitor names not available");
  }
  return out;
}

inline mcp::JsonValue visual_args(const mcp::JsonValue &args,
                                  const char *action) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  const mcp::JsonValue *enabled = args.Find("enabled");
  if (enabled != nullptr) {
    out["enabled"] = *enabled;
  }
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(6);
  v.push_back(make_spec_tool(ToolSpec{
      "print_debug_log",
      "Log a debug message through the autopilot log system at Info level. Use it to trace tool flow or mark milestones in an automation run; the message appears in the plugin log, not the game log. Requires 'message' (string); returns result 'ok' on success.",
      "Debug", {"debug", "print", "log"}, SideEffect::None, tool_flags::kNone,
      kPrintDebugLogParams, debug_ops::handle_print}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_stack",
      "Capture script backtraces for all currently running scripts in the editor process. Use it when a script error occurs to see where execution was at capture time; it needs no debug session. Returns per-backtrace language and frame_count, with frames containing function, file and line; include_variables adds global_variables per backtrace plus local_variables and member_variables per frame.",
      "Debug", {"debug", "stack", "trace"}, SideEffect::None, tool_flags::kNone,
      kGetDebugStackParams, debug_ops::handle_print_stack}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_physics_fps",
      "Set the engine physics ticks per second (physics FPS) globally and immediately. Use it to slow down or speed up physics simulation for debugging; the change is not persisted, so restore 60 after debugging. Requires 'fps' (integer); returns result 'ok' on success.",
      "Debug", {"debug", "physics", "fps", "set"}, SideEffect::ModifiesWindow, tool_flags::kMutating,
      kSetDebugPhysicsFpsParams, debug_ops::handle_set_physics_fps}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "set_debug_visuals",
      "Toggle debug overlays for editor game runs in one tool. action selects the overlay: 'collision' draws collision shapes as wireframes, 'navigation' draws navigation polygons and connections, 'performance' shows the FPS/frame-time overlay. Requires action and enabled (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok'. Example: {\"action\": \"collision\", \"enabled\": true}.",
      "Debug", {"debug", "collision", "navigation", "performance", "overlay", "visualize"},
      SideEffect::WritesConfig, tool_flags::kMutating,
      kSetDebugVisualsParams, debug_ops::handle_set_visuals,
      schema::build_action_schema(
          kSetDebugVisualsActions, kSetDebugVisualsParams,
          "collision: collision shape wireframes; navigation: navigation geometry overlays; performance: FPS and frame-time overlay"),
      {},
      {"set_debug_visuals action=collision enabled=true",
       "set_debug_visuals action=performance enabled=false"}},
      {
          ToolAlias{
              "set_debug_collision_visual", "set_debug_visuals",
              [](const mcp::JsonValue &args) {
                return visual_args(args, "collision");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_debug_navigation_visual", "set_debug_visuals",
              [](const mcp::JsonValue &args) {
                return visual_args(args, "navigation");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "set_debug_performance_visual", "set_debug_visuals",
              [](const mcp::JsonValue &args) {
                return visual_args(args, "performance");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_debug_info",
      "Read performance monitor data in one tool. action selects the view: 'monitors' returns the built-in monitor values as {name, type, value} entries for all monitors, or only the given integer ids as {id, name, type, value}; optional include_custom=true returns {monitors, custom_names} with the built-in values plus registered custom monitor names. 'catalog' returns the monitor id table {id, name, type} without values. 'custom' reads one custom monitor by string id and returns its value serialized as JSON Variant; without id it returns the same {monitors, custom_names} view. 'object' returns the object count, 'node' the node count and 'memory' the static memory value, each as a single number. Values are captured at call time. Example: {\"action\": \"object\"} or {\"action\": \"custom\", \"id\": \"enemies_alive\"}.",
      "Debug", {"debug", "performance", "monitor", "object", "node", "memory", "custom"},
      SideEffect::None, tool_flags::kNone,
      kGetDebugInfoParams, debug_ops::handle_get_info,
      schema::build_action_schema(
          kGetDebugInfoActions, kGetDebugInfoParams,
          "monitors: built-in monitor values (optionally narrowed by ids or include_custom); catalog: monitor id table; custom: one named custom monitor value (names when id is omitted); object: object count; node: node count; memory: static memory"),
      {},
      {"get_debug_info action=monitors",
       "get_debug_info action=memory",
       "get_debug_info action=custom id=enemies_alive"}},
      {
          ToolAlias{
              "get_debug_monitors", "get_debug_info",
              debug_monitors_args, {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_monitor", "get_debug_info",
              [](const mcp::JsonValue &args) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("monitors");
                mcp::JsonValue ids(mcp::JsonValue::array_tag);
                const mcp::JsonValue *monitor = args.Find("monitor");
                if (monitor != nullptr) {
                  ids.PushBack(*monitor);
                }
                out["ids"] = std::move(ids);
                return out;
              },
              project_monitor_value, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                const mcp::JsonValue *monitor = args.Find("monitor");
                if (monitor == nullptr || !monitor->IsNumber()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] = mcp::JsonValue(
                      "missing required parameter: monitor (integer 0-58)");
                  return error;
                }
                return mcp::JsonValue();
              }},
          ToolAlias{
              "get_debug_monitor_catalog", "get_debug_info",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("catalog");
                return out;
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_object_count", "get_debug_info",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("object");
                return out;
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_memory_usage", "get_debug_info",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("memory");
                return out;
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_node_count", "get_debug_info",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("node");
                return out;
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_custom_monitor_names", "get_debug_info",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("custom");
                return out;
              },
              project_custom_monitor_names, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_custom_monitor", "get_debug_info",
              [](const mcp::JsonValue &args) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["action"] = mcp::JsonValue("custom");
                const mcp::JsonValue *id = args.Find("id");
                if (id != nullptr) {
                  out["id"] = *id;
                }
                return out;
              },
              {}, "0.4.0", "0.6.0",
              [](const mcp::JsonValue &args) {
                const mcp::JsonValue *id = args.Find("id");
                if (id == nullptr || !id->IsString()) {
                  mcp::JsonValue error(mcp::JsonValue::object_tag);
                  error["error"] =
                      mcp::JsonValue("missing required parameter: id");
                  return error;
                }
                return mcp::JsonValue();
              }},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_debug_custom_monitor",
      "Remove a custom performance monitor registered via add_custom_monitor by its string id. Use it to clean up monitors no longer needed so they stop consuming per-frame overhead during game runs. Requires 'id' (string) matching the registered name; returns result 'ok' on success. This tool is classified as a destructive side effect because it unregisters and destroys the monitor object that already exists inside the host, and no undo path is provided; confirm=true is required. Example: {\"id\": \"enemies_alive\", \"confirm\": true}.",
      "Debug", {"debug", "performance", "monitor", "remove", "custom"},
      SideEffect::Destructive, tool_flags::kMutating,
      kRemoveDebugCustomMonitorParams,
      debug_ops::handle_remove_custom_monitor,
      {}, {}, {"{\"id\": \"enemies_alive\", \"confirm\": true}"}}));
  return v;
}

} // namespace debug_tools
} // namespace godot_autopilot

#endif
