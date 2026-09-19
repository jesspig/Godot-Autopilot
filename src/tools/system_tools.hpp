#ifndef GODOT_AUTOPILOT_SYSTEM_TOOLS_HPP
#define GODOT_AUTOPILOT_SYSTEM_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/log_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace system_tools {

namespace {

const std::vector<ParamSpec> kGetGameLogEntriesParams = {
    {"limit", "integer", "Maximum number of tail log entries to return (default: 50, max: 500); the result also includes path (log file path) and total_lines (full file line count). The log is written by the running game process — start the game first with play_editor_current_scene", false},
    {"filter", "string", "Optional case-sensitive substring; when set, the scan widens to the last 2000 log lines and only matching lines are returned (newest ones when limit caps them), with matched_lines reporting the total matches in that scan window. Empty string behaves as no filter", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(make_spec_tool(ToolSpec{
      "get_game_log_entries",
      "Read the tail of the game process log file at user://logs/godot.log. Start the game first with play_editor_current_scene so the log exists and is being written. Returns path (log file path), entries (array of log lines) and total_lines (full file line count); unlike get_debugger_log and get_debugger_output it reads the on-disk file and works without a debug session. Optional 'limit' caps returned entries (default 50, max 500). Optional 'filter' keeps only lines containing that case-sensitive substring (scanning the last 2000 lines) and adds matched_lines with the total matches found.",
      "System", {"log", "game", "read"}, SideEffect::None, tool_flags::kNone,
      kGetGameLogEntriesParams, log_ops::handle_log_get_game_entries}));
  return v;
}

} // namespace system_tools
} // namespace godot_autopilot

#endif