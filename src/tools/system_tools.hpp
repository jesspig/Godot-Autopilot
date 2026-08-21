#ifndef GODOT_AUTOPILOT_SYSTEM_TOOLS_HPP
#define GODOT_AUTOPILOT_SYSTEM_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/log_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace system_tools {

GDA_TOOL_CLASS(GetGameLogEntriesTool, "get_game_log_entries",
               "Read the tail of the game process log file at user://logs/godot.log. Start the game first with play_editor_current_scene so the log exists and is being written. Returns path (log file path), entries (array of log lines) and total_lines (full file line count); unlike get_debugger_log and get_debugger_output it reads the on-disk file and works without a debug session. Optional 'limit' caps returned entries (default 50, max 500).",
               "System", std::vector<std::string>({"log", "game", "read"}), log_ops::handle_log_get_game_entries, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(1);
  v.push_back(std::make_unique<GetGameLogEntriesTool>());
  return v;
}

} // namespace system_tools
} // namespace godot_autopilot

#endif