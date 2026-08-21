#ifndef GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/debugger_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace debugger_tools {

GDA_TOOL_CLASS(GetDebuggerLogTool, "get_debugger_log",
               "Read the captured engine log buffer of the editor process, including script errors and messages routed through the engine logger. It always contains data and does not require a running game; use get_game_log_entries to read the game process log file instead. Optional 'limit' (default 50) caps the returned entries.",
               "Debugger", std::vector<std::string>({"debugger", "log", "output"}), debugger_ops::handle_output_get_log, false)

GDA_TOOL_CLASS(GetDebuggerErrorsTool, "get_debugger_errors",
               "Read script errors captured in the running game process, with time, file, line, error text and stack. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without a session it falls back to editor-process captured errors. Optional 'limit' (default 20) caps the result; returns a formatted text dump.",
               "Debugger", std::vector<std::string>({"debugger", "errors", "stack"}), debugger_ops::handle_debugger_get_errors, false)

GDA_TOOL_CLASS(GetDebuggerOutputTool, "get_debugger_output",
               "Read stdout/stderr output captured from the running game process over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without a session it returns editor-captured output. Use get_game_log_entries to read the game process log file instead. Optional 'limit' (default 50) caps the result.",
               "Debugger", std::vector<std::string>({"debugger", "output", "stdout"}), debugger_ops::handle_debugger_get_output, false)

GDA_TOOL_CLASS(GetDebuggerStackDumpTool, "get_debugger_stack_dump",
               "Read the stack dump held by the editor debugger capture. The current implementation has no data source, so it always returns an empty dump even inside a breakpoint session; use get_game_log_entries or execute_game_script to diagnose runtime issues instead. No parameters are accepted.",
               "Debugger", std::vector<std::string>({"debugger", "stack", "dump"}), debugger_ops::handle_debugger_get_stack_dump, false)

GDA_TOOL_CLASS(GetDebuggerSceneTreeTool, "get_debugger_scene_tree",
               "Read the scene tree of the running game process (node names and types) over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without a session it returns the last editor-captured tree. Returns a formatted text tree; takes no parameters.",
               "Debugger", std::vector<std::string>({"debugger", "scene", "tree"}), debugger_ops::handle_debugger_get_scene_tree, false)

GDA_TOOL_CLASS(GetDebuggerMonitorsTool, "get_debugger_monitors",
               "Read performance monitor frames captured by the editor debugger. The current implementation has no data source, so it always returns an empty dump; for live editor-side values use get_debug_monitors instead. Optional 'count' (integer, default 1) caps the number of frames.",
               "Debugger", std::vector<std::string>({"debugger", "monitors", "performance"}), debugger_ops::handle_debugger_get_monitors, false)

GDA_TOOL_CLASS(GetDebuggerSessionInfoTool, "get_debugger_session_info",
               "Read the current debug session state of the editor debugger. Use it to check whether a game is running (active) or stopped at a breakpoint (breaked) before using other debugger tools. Returns a text summary with active, breaked, running and session count; takes no parameters.",
               "Debugger", std::vector<std::string>({"debugger", "session", "status"}), debugger_ops::handle_debugger_get_session_info, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(7);
  v.push_back(std::make_unique<GetDebuggerLogTool>());
  v.push_back(std::make_unique<GetDebuggerErrorsTool>());
  v.push_back(std::make_unique<GetDebuggerOutputTool>());
  v.push_back(std::make_unique<GetDebuggerStackDumpTool>());
  v.push_back(std::make_unique<GetDebuggerSceneTreeTool>());
  v.push_back(std::make_unique<GetDebuggerMonitorsTool>());
  v.push_back(std::make_unique<GetDebuggerSessionInfoTool>());
  return v;
}

} // namespace debugger_tools
} // namespace godot_autopilot

#endif