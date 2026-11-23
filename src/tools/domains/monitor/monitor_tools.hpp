#ifndef GODOT_AUTOPILOT_MONITOR_TOOLS_HPP
#define GODOT_AUTOPILOT_MONITOR_TOOLS_HPP

#include <memory>
#include <string>
#include <vector>

#include <tools/base/tool_spec.hpp>

#include "tools/domains/monitor/monitor_ops.hpp"

namespace godot_autopilot {
namespace monitor_tools {

namespace {

const std::vector<std::string> kGameMonitorActions = {
    "status", "sessions", "report", "export"};

const std::vector<ParamSpec> kGameMonitorParams = {
    {"session", "string",
     "report/export: id of a recorded monitor session from action=sessions; only digits, letters, '-' and '_' are accepted", false},
    {"slow_ms", "number",
     "report: frame time threshold in milliseconds for counting slow frames (default 33.3)", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(ToolSpec{
      "game_monitor",
      "Read per-frame performance recordings captured from a game launched from the editor. GDA records every frame into JSONL in the game process, including all 59 built-in Performance monitors and any custom monitors the game registered with Performance.add_custom_monitor; custom monitors are discovered automatically and require no game-side changes. action='status' reports the supported scope and recording directory. action='sessions' lists recorded session ids newest first. action='report' reads one session and returns frame time distribution (avg/p95/p99/max, slow frame count), duration, and per-monitor min/max/avg/p50/p95/p99/first/last/delta; optional slow_ms sets the slow-frame threshold. action='export' copies one session JSONL into the GDA home monitor-sessions directory and returns its path. Requires the game project to load the godot-autopilot extension; exported games without the extension are not supported.",
      "Monitor",
      {"monitor", "performance", "profiling", "frames", "memory", "objects",
       "custom_monitor", "session", "jsonl"},
      SideEffect::None, tool_flags::kObserve,
      kGameMonitorParams, monitor_ops::handle_game_monitor,
      schema::build_action_schema(
          kGameMonitorActions, kGameMonitorParams,
          "Monitor action: status|sessions|report|export"),
      {},
      {"{\"action\":\"sessions\"}",
       "{\"action\":\"report\",\"session\":\"123456-1\",\"slow_ms\":25}",
       "{\"action\":\"export\",\"session\":\"123456-1\"}"}}));
  return v;
}

} // namespace monitor_tools
} // namespace godot_autopilot

#endif
