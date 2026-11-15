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

constexpr int64_t kMonitorMemoryStatic = 4;
constexpr int64_t kMonitorObjectCount = 7;
constexpr int64_t kMonitorNodeCount = 9;

const std::vector<ParamSpec> kSetDebugPhysicsFpsParams = {
    {"fps", "number", "Physics ticks per second (integer; Godot default is 60, typical debug values 10-60). Applies immediately and globally to the engine but is not persisted — restore 60 when done", true},
};

const std::vector<ParamSpec> kSetDebugCollisionVisualParams = {
    {"enabled", "boolean", "True to show collision shapes during editor game runs, false to hide them; persisted in project.godot via project metadata until changed back", true},
};

const std::vector<ParamSpec> kSetDebugNavigationVisualParams = {
    {"enabled", "boolean", "True to show navigation geometry during editor game runs, false to hide it; persisted in project.godot via project metadata until changed back", true},
};

const std::vector<ParamSpec> kSetDebugPerformanceVisualParams = {
    {"enabled", "boolean", "True to show the performance debug overlay during editor game runs, false to hide it; persisted in project.godot via project metadata until changed back", true},
};

const std::vector<ParamSpec> kGetDebugMonitorsParams = {
    {"ids", "array", "Optional array of integer monitor ids to return; each entry includes id, name, type and value. Out-of-range ids return an error", false},
    {"catalog", "boolean", "When true, return the monitor catalog ({id, name, type}) instead of values", false},
    {"include_custom", "boolean", "When true, return {monitors, custom_names}: the built-in monitor values plus registered custom monitor names", false},
};

const std::vector<ParamSpec> kRemoveDebugCustomMonitorParams = {
    {"id", "string", "Name of the custom monitor to remove, as registered via Performance.add_custom_monitor by the game or scripts", true},
};

const std::vector<ParamSpec> kGetDebugCustomMonitorParams = {
    {"id", "string", "Name of the custom monitor to read, as registered via Performance.add_custom_monitor; the value is serialized as JSON Variant", true},
};

inline mcp::JsonValue single_monitor_id_args(int64_t id) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  mcp::JsonValue ids(mcp::JsonValue::array_tag);
  ids.PushBack(mcp::JsonValue(id));
  out["ids"] = std::move(ids);
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

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(9);
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
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_collision_visual",
      "Enable or disable collision shape visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The shapes are drawn as wireframe overlays in the game viewport.",
      "Debug", {"debug", "collision", "visualize"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetDebugCollisionVisualParams, debug_ops::handle_collision_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_navigation_visual",
      "Enable or disable navigation geometry visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The polygons and connections are drawn as overlays in the game viewport.",
      "Debug", {"debug", "navigation", "visualize"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetDebugNavigationVisualParams, debug_ops::handle_navigation_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_performance_visual",
      "Enable or disable the performance debug overlay while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The overlay shows FPS and frame times in the game viewport.",
      "Debug", {"debug", "performance", "overlay"}, SideEffect::WritesConfig, tool_flags::kMutating,
      kSetDebugPerformanceVisualParams, debug_ops::handle_performance_debug}));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_debug_monitors",
      "Read performance monitor values in one call. Default returns all built-in monitors as {name, type, value} entries. 'ids' selects integer monitor ids and returns {id, name, type, value} per entry; 'catalog' returns {id, name, type} without values; 'include_custom' returns {monitors, custom_names} with the built-in values plus registered custom monitor names. Values are captured at call time.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kGetDebugMonitorsParams, debug_ops::handle_get_all_monitors},
      {
          ToolAlias{
              "get_debug_monitor", "get_debug_monitors",
              [](const mcp::JsonValue &args) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
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
              "get_debug_monitor_catalog", "get_debug_monitors",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["catalog"] = mcp::JsonValue(true);
                return out;
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_object_count", "get_debug_monitors",
              [](const mcp::JsonValue &) {
                return single_monitor_id_args(kMonitorObjectCount);
              },
              project_monitor_value, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_memory_usage", "get_debug_monitors",
              [](const mcp::JsonValue &) {
                return single_monitor_id_args(kMonitorMemoryStatic);
              },
              project_monitor_value, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_node_count", "get_debug_monitors",
              [](const mcp::JsonValue &) {
                return single_monitor_id_args(kMonitorNodeCount);
              },
              project_monitor_value, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debug_custom_monitor_names", "get_debug_monitors",
              [](const mcp::JsonValue &) {
                mcp::JsonValue out(mcp::JsonValue::object_tag);
                out["include_custom"] = mcp::JsonValue(true);
                return out;
              },
              project_custom_monitor_names, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_debug_custom_monitor",
      "Remove a custom performance monitor registered via add_custom_monitor by its string id. Use it to clean up monitors no longer needed so they stop consuming per-frame overhead during game runs. Requires 'id' (string) matching the registered name; returns result 'ok' on success. This tool is classified as a destructive side effect because it unregisters and destroys the monitor object that already exists inside the host, and no undo path is provided.",
      "Debug", {"debug", "performance"}, SideEffect::Destructive, tool_flags::kMutating,
      kRemoveDebugCustomMonitorParams, debug_ops::handle_remove_custom_monitor}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_custom_monitor",
      "Read the current value of a custom performance monitor by its string id. Use it after registering a monitor with Performance.add_custom_monitor from game code or scripts. Returns the monitor value serialized as JSON Variant, whose shape depends on what the registration returned.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kGetDebugCustomMonitorParams, debug_ops::handle_get_custom_monitor}));
  return v;
}

} // namespace debug_tools
} // namespace godot_autopilot

#endif