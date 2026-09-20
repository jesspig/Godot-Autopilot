#ifndef GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/debugger_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace debugger_tools {

namespace {

const std::vector<ParamSpec> kGetDebuggerLogParams = {
    {"limit", "integer", "Maximum number of log entries to return (default: 50); the most recent entries are kept. The buffer always has data from the editor process engine logger, even without a running game", false},
};

const std::vector<ParamSpec> kGetPluginLogParams = {
    {"limit", "integer", "Maximum number of plugin log entries to return (default: 100, max: 1000); the most recent entries are kept unless since_index is given (then the oldest unread are kept)", false},
    {"level", "string", "Minimum log level filter: 'debug' (default, no filtering), 'info', 'warning' or 'error'", false},
    {"category", "string", "Category filter: 'system', 'transport', 'tools', 'resources' or 'prompts' (omit for all categories)", false},
    {"filter", "string", "Case-insensitive substring filter on the message text", false},
    {"since_index", "integer", "Only return entries with serial >= since_index (incremental read); start with 0 and pass the returned next_index on the next call to fetch only new entries", false},
};

const std::vector<ParamSpec> kGetDebuggerErrorsParams = {
    {"limit", "integer", "Maximum number of errors to return (default: 20); with an active debug session the result is a structured list fetched from the running game (time, file, func, line, error, descr, is_warning, stack), otherwise an empty result plus a note — no editor-side fallback; use get_debugger_log for editor-process script errors and output", false},
};

const std::vector<ParamSpec> kGetDebuggerOutputParams = {
    {"limit", "integer", "Maximum number of output entries to return (default: 50); fetched from the running game over the runtime channel when a debug session is active, otherwise an empty result plus a note — no editor-side fallback; use get_debugger_log for editor-process output or get_game_log_entries to read the game process log file", false},
};

const std::vector<ParamSpec> kGetDebuggerSceneTreeParams = {};

const std::vector<ParamSpec> kGetDebuggerSessionInfoParams = {};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(6);
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_log",
      "Read the editor-process engine log buffer, including script errors and print output routed through the engine logger. It always contains data and does not require a running game; for the running game use get_debugger_errors, and for the game process log file on disk use get_game_log_entries. Optional 'limit' (default 50) caps the returned entries.",
      "Debugger", {"debugger", "log", "output"}, SideEffect::None, tool_flags::kNone,
      kGetDebuggerLogParams, debugger_ops::handle_output_get_log}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_plugin_log",
      "Read the plugin's own in-process LogSystem buffer — internal diagnostics such as 'late game response discarded', authorization denials and timeout diagnostics — and does not require a running game. This is a different source from get_debugger_log, which reads the editor-process engine log buffer (OS logger). Optional 'limit' (default 100, max 1000), 'level' (debug/info/warning/error, default debug = no filtering), 'category' (system/transport/tools/resources/prompts), 'filter' (case-insensitive substring) and 'since_index' (incremental read from a previous call's next_index; the response includes next_index).",
      "Debugger", {"debugger", "log", "plugin"}, SideEffect::None, tool_flags::kNone,
      kGetPluginLogParams, debugger_ops::handle_plugin_log_get}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_errors",
      "Read script errors from the running game process as a structured list (time, file, func, line, error, descr, is_warning, stack). Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. Use get_debugger_log for script errors and output of the editor process instead. Optional 'limit' (default 20) caps the result; with an active session it caps raw entries (ungrouped) or returned groups (grouped). Optional 'group' (default false) clusters entries by file/function/line/message and returns {grouped:true, groups:[{count,first_time,last_time,file,func,line,error,descr,is_warning,stack}], total_errors, total_groups} instead of the raw list.",
      "Debugger", {"debugger", "errors", "stack"}, SideEffect::None, tool_flags::kNone,
      kGetDebuggerErrorsParams, debugger_ops::handle_debugger_get_errors}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_output",
      "Read stdout/stderr output captured from the running game process over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. For editor-process output use get_debugger_log; to read the game process log file use get_game_log_entries. Optional 'limit' (default 50) caps the result.",
      "Debugger", {"debugger", "output", "stdout"}, SideEffect::None, tool_flags::kNone,
      kGetDebuggerOutputParams, debugger_ops::handle_debugger_get_output}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_scene_tree",
      "Read the scene tree of the running game process (node names and types) over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. Returns a formatted text tree; takes no parameters.",
      "Debugger", {"debugger", "scene", "tree"}, SideEffect::None, tool_flags::kNone,
      kGetDebuggerSceneTreeParams, debugger_ops::handle_debugger_get_scene_tree}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_session_info",
      "Read the current debug session state of the editor debugger. Use it to check whether a game is running (active) or stopped at a breakpoint (breaked) before using other debugger tools. Returns a text summary with active, breaked, running and session count; takes no parameters.",
      "Debugger", {"debugger", "session", "status"}, SideEffect::None, tool_flags::kNone,
      kGetDebuggerSessionInfoParams, debugger_ops::handle_debugger_get_session_info}));
  return v;
}

} // namespace debugger_tools
} // namespace godot_autopilot

#endif