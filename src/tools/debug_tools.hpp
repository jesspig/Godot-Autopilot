#ifndef GODOT_AUTOPILOT_DEBUG_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUG_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/debug_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace debug_tools {

namespace {

const std::vector<ParamSpec> kPrintDebugLogParams = {
    {"message", "string", "Debug text to record in the plugin log at Info level; the message does not reach the game log", true},
};

const std::vector<ParamSpec> kGetDebugStackParams = {
    {"include_variables", "boolean", "Include variable names in the dump: global_variables per backtrace, local_variables and member_variables per frame (default: false)", false},
};

const std::vector<ParamSpec> kGetDebugMonitorParams = {
    {"monitor", "number", "Performance monitor ID (integer 0-58); list valid ids with get_debug_monitor_catalog first. Out-of-range ids return an error", true},
};

const std::vector<ParamSpec> kGetDebugMonitorCatalogParams = {};

const std::vector<ParamSpec> kGetDebugObjectCountParams = {};

const std::vector<ParamSpec> kGetDebugMemoryUsageParams = {};

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

const std::vector<ParamSpec> kGetDebugMonitorsParams = {};

const std::vector<ParamSpec> kRemoveDebugCustomMonitorParams = {
    {"id", "string", "Name of the custom monitor to remove, as registered via Performance.add_custom_monitor by the game or scripts", true},
};

const std::vector<ParamSpec> kGetDebugCustomMonitorParams = {
    {"id", "string", "Name of the custom monitor to read, as registered via Performance.add_custom_monitor; the value is serialized as JSON Variant", true},
};

const std::vector<ParamSpec> kGetDebugCustomMonitorNamesParams = {};

const std::vector<ParamSpec> kGetDebugNodeCountParams = {};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
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
      "get_debug_monitor",
      "Read the current value of one built-in performance monitor by integer id. Look up the id with get_debug_monitor_catalog first; ids outside the catalog range return an error. Returns the monitor value as a number; for a snapshot of all monitors use get_debug_monitors instead.",
      "Debug", {"debug", "performance", "monitor", "get"}, SideEffect::None, tool_flags::kNone,
      kGetDebugMonitorParams, debug_ops::handle_get_performance_monitor}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_monitor_catalog",
      "List all built-in performance monitors with their catalog id, name and type (time, memory or quantity). Use it to find valid ids for get_debug_monitor before querying values. Returns an array of {id, name, type} entries; takes no parameters and is the authoritative source of monitor ids.",
      "Debug", {"debug", "performance", "monitor", "list"}, SideEffect::None, tool_flags::kNone,
      kGetDebugMonitorCatalogParams, debug_ops::handle_list_performance_monitors}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_object_count",
      "Read the total live Object count from the Performance singleton. Use it to detect object leaks by comparing counts before and after a scripted operation; pair it with get_debug_node_count for node-specific counts. Returns the count as a number; takes no parameters.",
      "Debug", {"debug", "objects", "count"}, SideEffect::None, tool_flags::kNone,
      kGetDebugObjectCountParams, debug_ops::handle_get_object_count}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_memory_usage",
      "Read current static memory usage in bytes from the Performance singleton. Use it to observe memory growth trends while running scripted tests. Returns a byte count as a number; takes no parameters. Note it reflects static engine memory only, not GPU or texture memory.",
      "Debug", {"debug", "memory", "usage"}, SideEffect::None, tool_flags::kNone,
      kGetDebugMemoryUsageParams, debug_ops::handle_get_memory_usage}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_physics_fps",
      "Set the engine physics ticks per second (physics FPS) globally and immediately. Use it to slow down or speed up physics simulation for debugging; the change is not persisted, so restore 60 after debugging. Requires 'fps' (integer); returns result 'ok' on success.",
      "Debug", {"debug", "physics", "fps", "set"}, SideEffect::None, tool_flags::kNone,
      kSetDebugPhysicsFpsParams, debug_ops::handle_set_physics_fps}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_collision_visual",
      "Enable or disable collision shape visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The shapes are drawn as wireframe overlays in the game viewport.",
      "Debug", {"debug", "collision", "visualize"}, SideEffect::None, tool_flags::kNone,
      kSetDebugCollisionVisualParams, debug_ops::handle_collision_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_navigation_visual",
      "Enable or disable navigation geometry visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The polygons and connections are drawn as overlays in the game viewport.",
      "Debug", {"debug", "navigation", "visualize"}, SideEffect::None, tool_flags::kNone,
      kSetDebugNavigationVisualParams, debug_ops::handle_navigation_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_debug_performance_visual",
      "Enable or disable the performance debug overlay while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The overlay shows FPS and frame times in the game viewport.",
      "Debug", {"debug", "performance", "overlay"}, SideEffect::None, tool_flags::kNone,
      kSetDebugPerformanceVisualParams, debug_ops::handle_performance_debug}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_monitors",
      "Read current values of all built-in performance monitors in one call. Use it for a full performance snapshot without repeated get_debug_monitor calls. Returns an array of {name, type, value} entries covering 59 monitors; takes no parameters. Values are captured at the moment of the call.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kGetDebugMonitorsParams, debug_ops::handle_get_all_monitors}));
  v.push_back(make_spec_tool(ToolSpec{
      "remove_debug_custom_monitor",
      "Remove a custom performance monitor registered via add_custom_monitor by its string id. Use it to clean up monitors no longer needed so they stop consuming per-frame overhead during game runs. Requires 'id' (string) matching the registered name; returns result 'ok' on success.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kRemoveDebugCustomMonitorParams, debug_ops::handle_remove_custom_monitor}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_custom_monitor",
      "Read the current value of a custom performance monitor by its string id. Use it after registering a monitor with Performance.add_custom_monitor from game code or scripts. Returns the monitor value serialized as JSON Variant, whose shape depends on what the registration returned.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kGetDebugCustomMonitorParams, debug_ops::handle_get_custom_monitor}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_custom_monitor_names",
      "List the names of all registered custom performance monitors. Use it to discover monitor ids before calling get_debug_custom_monitor or to verify that game code registered the expected monitors. Returns an array of name strings; takes no parameters. The list is empty when no custom monitor is registered.",
      "Debug", {"debug", "performance"}, SideEffect::None, tool_flags::kNone,
      kGetDebugCustomMonitorNamesParams, debug_ops::handle_list_custom_monitors}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debug_node_count",
      "Read the current live Node count from the Performance singleton. Use it with get_debug_object_count to detect node leaks by comparing counts before and after scene operations or scripted loops. Returns the count as a number and takes no parameters; a steadily rising count suggests leaked nodes.",
      "Debug", {"debug", "objects"}, SideEffect::None, tool_flags::kNone,
      kGetDebugNodeCountParams, debug_ops::handle_query_node_count}));
  return v;
}

} // namespace debug_tools
} // namespace godot_autopilot

#endif