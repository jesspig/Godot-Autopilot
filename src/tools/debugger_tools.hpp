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
               "Read the editor-process engine log buffer, including script errors and print output routed through the engine logger. It always contains data and does not require a running game; for the running game use get_debugger_errors, and for the game process log file on disk use get_game_log_entries. Optional 'limit' (default 50) caps the returned entries.",
               "Debugger", std::vector<std::string>({"debugger", "log", "output"}), debugger_ops::handle_output_get_log, false)

GDA_TOOL_CLASS(GetPluginLogTool, "get_plugin_log",
               "Read the plugin's own in-process LogSystem buffer — internal diagnostics such as 'late game response discarded', authorization denials and timeout diagnostics — and does not require a running game. This is a different source from get_debugger_log, which reads the editor-process engine log buffer (OS logger). Optional 'limit' (default 100, max 1000), 'level' (debug/info/warning/error, default debug = no filtering), 'category' (system/transport/tools/resources/prompts), 'filter' (case-insensitive substring) and 'since_index' (incremental read from a previous call's next_index; the response includes next_index).",
               "Debugger", std::vector<std::string>({"debugger", "log", "plugin"}), debugger_ops::handle_plugin_log_get, false)

GDA_TOOL_CLASS(GetDebuggerErrorsTool, "get_debugger_errors",
               "Read script errors from the running game process as a structured list (time, file, func, line, error, descr, is_warning, stack). Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. Use get_debugger_log for script errors and output of the editor process instead. Optional 'limit' (default 20) caps the result; with an active session it caps raw entries (ungrouped) or returned groups (grouped). Optional 'group' (default false) clusters entries by file/function/line/message and returns {grouped:true, groups:[{count,first_time,last_time,file,func,line,error,descr,is_warning,stack}], total_errors, total_groups} instead of the raw list.",
               "Debugger", std::vector<std::string>({"debugger", "errors", "stack"}), debugger_ops::handle_debugger_get_errors, false)

GDA_TOOL_CLASS(GetDebuggerOutputTool, "get_debugger_output",
               "Read stdout/stderr output captured from the running game process over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. For editor-process output use get_debugger_log; to read the game process log file use get_game_log_entries. Optional 'limit' (default 50) caps the result.",
               "Debugger", std::vector<std::string>({"debugger", "output", "stdout"}), debugger_ops::handle_debugger_get_output, false)

GDA_TOOL_CLASS(GetDebuggerSceneTreeTool, "get_debugger_scene_tree",
               "Read the scene tree of the running game process (node names and types) over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. Returns a formatted text tree; takes no parameters.",
               "Debugger", std::vector<std::string>({"debugger", "scene", "tree"}), debugger_ops::handle_debugger_get_scene_tree, false)

GDA_TOOL_CLASS(GetDebuggerSessionInfoTool, "get_debugger_session_info",
               "Read the current debug session state of the editor debugger. Use it to check whether a game is running (active) or stopped at a breakpoint (breaked) before using other debugger tools. Returns a text summary with active, breaked, running and session count; takes no parameters.",
               "Debugger", std::vector<std::string>({"debugger", "session", "status"}), debugger_ops::handle_debugger_get_session_info, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(6);
  v.push_back(std::make_unique<GetDebuggerLogTool>());
  v.push_back(std::make_unique<GetPluginLogTool>());
  v.push_back(std::make_unique<GetDebuggerErrorsTool>());
  v.push_back(std::make_unique<GetDebuggerOutputTool>());
  v.push_back(std::make_unique<GetDebuggerSceneTreeTool>());
  v.push_back(std::make_unique<GetDebuggerSessionInfoTool>());
  return v;
}

} // namespace debugger_tools
} // namespace godot_autopilot

#endif