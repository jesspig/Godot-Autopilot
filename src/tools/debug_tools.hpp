#ifndef GODOT_AUTOPILOT_DEBUG_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUG_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/debug_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace debug_tools {

GDA_TOOL_CLASS(PrintDebugLogTool, "print_debug_log",
               "Log a debug message through the autopilot log system at Info level. Use it to trace tool flow or mark milestones in an automation run; the message appears in the plugin log, not the game log. Requires 'message' (string); returns result 'ok' on success.",
               "Debug", std::vector<std::string>({"debug", "print", "log"}), debug_ops::handle_print, true)

GDA_TOOL_CLASS(GetDebugStackTool, "get_debug_stack",
               "Capture script backtraces for all currently running scripts in the editor process. Use it when a script error occurs to see where execution was at capture time; unlike get_debugger_stack_dump it needs no debug session. Returns per-backtrace language and frame_count, with frames containing function, file and line; include_variables adds global_variables per backtrace plus local_variables and member_variables per frame.",
               "Debug", std::vector<std::string>({"debug", "stack", "trace"}), debug_ops::handle_print_stack, false)

GDA_TOOL_CLASS(GetDebugMonitorTool, "get_debug_monitor",
               "Read the current value of one built-in performance monitor by integer id. Look up the id with get_debug_monitor_catalog first; ids outside the catalog range return an error. Returns the monitor value as a number; for a snapshot of all monitors use get_debug_monitors instead.",
               "Debug", std::vector<std::string>({"debug", "performance", "monitor", "get"}), debug_ops::handle_get_performance_monitor, false)

GDA_TOOL_CLASS(GetDebugMonitorCatalogTool, "get_debug_monitor_catalog",
               "List all built-in performance monitors with their catalog id, name and type (time, memory or quantity). Use it to find valid ids for get_debug_monitor before querying values. Returns an array of {id, name, type} entries; takes no parameters and is the authoritative source of monitor ids.",
               "Debug", std::vector<std::string>({"debug", "performance", "monitor", "list"}), debug_ops::handle_list_performance_monitors, false)

GDA_TOOL_CLASS(GetDebugObjectCountTool, "get_debug_object_count",
               "Read the total live Object count from the Performance singleton. Use it to detect object leaks by comparing counts before and after a scripted operation; pair it with get_debug_node_count for node-specific counts. Returns the count as a number; takes no parameters.",
               "Debug", std::vector<std::string>({"debug", "objects", "count"}), debug_ops::handle_get_object_count, false)

GDA_TOOL_CLASS(GetDebugMemoryUsageTool, "get_debug_memory_usage",
               "Read current static memory usage in bytes from the Performance singleton. Use it to observe memory growth trends while running scripted tests. Returns a byte count as a number; takes no parameters. Note it reflects static engine memory only, not GPU or texture memory.",
               "Debug", std::vector<std::string>({"debug", "memory", "usage"}), debug_ops::handle_get_memory_usage, false)

GDA_TOOL_CLASS(SetDebugPhysicsFpsTool, "set_debug_physics_fps",
               "Set the engine physics ticks per second (physics FPS) globally and immediately. Use it to slow down or speed up physics simulation for debugging; the change is not persisted, so restore 60 after debugging. Requires 'fps' (integer); returns result 'ok' on success.",
               "Debug", std::vector<std::string>({"debug", "physics", "fps", "set"}), debug_ops::handle_set_physics_fps, false)

GDA_TOOL_CLASS(SetDebugCollisionVisualTool, "set_debug_collision_visual",
               "Enable or disable collision shape visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The shapes are drawn as wireframe overlays in the game viewport.",
               "Debug", std::vector<std::string>({"debug", "collision", "visualize"}), debug_ops::handle_collision_debug, true)

GDA_TOOL_CLASS(SetDebugNavigationVisualTool, "set_debug_navigation_visual",
               "Enable or disable navigation geometry visualization while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The polygons and connections are drawn as overlays in the game viewport.",
               "Debug", std::vector<std::string>({"debug", "navigation", "visualize"}), debug_ops::handle_navigation_debug, false)

GDA_TOOL_CLASS(SetDebugPerformanceVisualTool, "set_debug_performance_visual",
               "Enable or disable the performance debug overlay while the game runs from the editor. Requires 'enabled' (bool); the flag is written to project metadata, so it persists in project.godot until changed back. Returns result 'ok' on success. The overlay shows FPS and frame times in the game viewport.",
               "Debug", std::vector<std::string>({"debug", "performance", "overlay"}), debug_ops::handle_performance_debug, false)

GDA_TOOL_CLASS(GetDebugMonitorsTool, "get_debug_monitors",
               "Read current values of all built-in performance monitors in one call. Use it for a full performance snapshot without repeated get_debug_monitor calls. Returns an array of {name, type, value} entries covering 58 monitors; takes no parameters. Values are captured at the moment of the call.",
               "Debug", std::vector<std::string>({"debug", "performance"}), debug_ops::handle_get_all_monitors, false)

GDA_TOOL_CLASS(RemoveDebugCustomMonitorTool, "remove_debug_custom_monitor",
               "Remove a custom performance monitor registered via add_custom_monitor by its string id. Use it to clean up monitors no longer needed so they stop consuming per-frame overhead during game runs. Requires 'id' (string) matching the registered name; returns result 'ok' on success.",
               "Debug", std::vector<std::string>({"debug", "performance"}), debug_ops::handle_remove_custom_monitor, false)

GDA_TOOL_CLASS(GetDebugCustomMonitorTool, "get_debug_custom_monitor",
               "Read the current value of a custom performance monitor by its string id. Use it after registering a monitor with Performance.add_custom_monitor from game code or scripts. Returns the monitor value serialized as JSON Variant, whose shape depends on what the registration returned.",
               "Debug", std::vector<std::string>({"debug", "performance"}), debug_ops::handle_get_custom_monitor, false)

GDA_TOOL_CLASS(GetDebugCustomMonitorNamesTool, "get_debug_custom_monitor_names",
               "List the names of all registered custom performance monitors. Use it to discover monitor ids before calling get_debug_custom_monitor or to verify that game code registered the expected monitors. Returns an array of name strings; takes no parameters. The list is empty when no custom monitor is registered.",
               "Debug", std::vector<std::string>({"debug", "performance"}), debug_ops::handle_list_custom_monitors, false)

GDA_TOOL_CLASS(GetDebugNodeCountTool, "get_debug_node_count",
               "Read the current live Node count from the Performance singleton. Use it with get_debug_object_count to detect node leaks by comparing counts before and after scene operations or scripted loops. Returns the count as a number and takes no parameters; a steadily rising count suggests leaked nodes.",
               "Debug", std::vector<std::string>({"debug", "objects"}), debug_ops::handle_query_node_count, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(15);
  v.push_back(std::make_unique<PrintDebugLogTool>());
  v.push_back(std::make_unique<GetDebugStackTool>());
  v.push_back(std::make_unique<GetDebugMonitorTool>());
  v.push_back(std::make_unique<GetDebugMonitorCatalogTool>());
  v.push_back(std::make_unique<GetDebugObjectCountTool>());
  v.push_back(std::make_unique<GetDebugMemoryUsageTool>());
  v.push_back(std::make_unique<SetDebugPhysicsFpsTool>());
  v.push_back(std::make_unique<SetDebugCollisionVisualTool>());
  v.push_back(std::make_unique<SetDebugNavigationVisualTool>());
  v.push_back(std::make_unique<SetDebugPerformanceVisualTool>());
  v.push_back(std::make_unique<GetDebugMonitorsTool>());
  v.push_back(std::make_unique<RemoveDebugCustomMonitorTool>());
  v.push_back(std::make_unique<GetDebugCustomMonitorTool>());
  v.push_back(std::make_unique<GetDebugCustomMonitorNamesTool>());
  v.push_back(std::make_unique<GetDebugNodeCountTool>());
  return v;
}

} // namespace debug_tools
} // namespace godot_autopilot

#endif