#ifndef GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/debugger/debugger_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace debugger_tools {

namespace {

const std::vector<ParamSpec> kGetDebuggerStreamParams = {
    {"source", "string", "Where the data comes from: 'editor' (default) reads the editor-process engine log buffer, 'game' reads the running game over the runtime channel, 'plugin' reads the plugin's own LogSystem buffer. action=output and action=errors only support source 'game'", false},
    {"limit", "integer", "Maximum number of entries to return; action=log editor default 50, action=log plugin default 100 (max 1000), action=output default 50, action=errors default 20. The most recent entries are kept", false},
    {"group", "boolean", "Only with action=errors: cluster entries by file/function/line/message and return {grouped:true, groups:[...], total_errors, total_groups, next_seq} instead of the raw list", false},
    {"since_seq", "integer", "Only with action=errors: return errors with seq > since_seq (exclusive incremental read); pass the previous response's top-level next_seq", false},
    {"level", "string", "Only with source=plugin: minimum log level filter: 'debug' (default, no filtering), 'info', 'warning' or 'error'", false},
    {"category", "string", "Only with source=plugin: category filter 'system', 'transport', 'tools', 'resources' or 'prompts' (omit for all)", false},
    {"filter", "string", "Case-insensitive substring filter on the message text; supported by source=plugin", false},
    {"since_index", "integer", "Only with source=plugin: return entries with serial >= since_index (incremental read); the response carries next_index for the next call", false},
};

const std::vector<ParamSpec> kGetPluginLogParams = {
    {"limit", "integer", "Maximum number of plugin log entries to return (default: 100, max: 1000); the most recent entries are kept unless since_index is given (then the oldest unread are kept)", false},
    {"level", "string", "Minimum log level filter: 'debug' (default, no filtering), 'info', 'warning' or 'error'", false},
    {"category", "string", "Category filter: 'system', 'transport', 'tools', 'resources' or 'prompts' (omit for all categories)", false},
    {"filter", "string", "Case-insensitive substring filter on the message text", false},
    {"since_index", "integer", "Only return entries with serial >= since_index (incremental read); start with 0 and pass the returned next_index on the next call to fetch only new entries", false},
};

const std::vector<ParamSpec> kGetDebuggerSceneTreeParams = {};

const std::vector<std::string> kDebuggerBreakpointsActions = {"list", "set",
                                                               "clear"};

const std::vector<ParamSpec> kDebuggerBreakpointsParams = {
    {"path", "string", "set/clear: script file path of the breakpoint, e.g. 'res://scripts/player.gd'", false},
    {"line", "integer", "set/clear: 1-based line number of the breakpoint", false},
};

const std::vector<std::string> kGetDebuggerStreamActions = {
    "log", "output", "errors", "session"};

inline godot_autopilot::util::JsonValue stream_args(const godot_autopilot::util::JsonValue &args,
                                  const char *action, const char *source) {
  godot_autopilot::util::JsonValue out(godot_autopilot::util::JsonValue::object_tag);
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (entry.first != "action" && entry.first != "source" &&
          entry.first != "confirm") {
        out[entry.first] = entry.second;
      }
    }
  }
  out["action"] = godot_autopilot::util::JsonValue(action);
  out["source"] = godot_autopilot::util::JsonValue(source);
  return out;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "get_debugger_stream",
      "Read one debugger stream in one tool. action selects the stream: 'log' reads a log stream, 'output' reads stdout/stderr of the running game, 'errors' reads script errors of the running game, 'session' reports the debug session state. source selects where action=log reads from: 'editor' (default) is the editor-process engine log buffer captured by the OS logger — it always has data and needs no running game; 'plugin' is the plugin's own LogSystem buffer with internal diagnostics (authorization denials, timeouts) and supports level/category/filter/since_index; 'game' reads the running game over the runtime channel. action=output and action=errors only accept source='game' and require a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active session they return an empty result plus a note. action=session returns a text summary with active, breaked, running and session count and ignores source. The on-disk game log is a different source: use get_game_log_entries. Optional limit caps returned entries; action=errors also accepts group and since_seq. Example: {\"action\": \"log\"} or {\"action\": \"errors\", \"since_seq\": 0} or {\"action\": \"log\", \"source\": \"plugin\", \"level\": \"warning\"}.",
      "Debugger", {"debugger", "log", "output", "errors", "session", "stream", "stdout"},
      SideEffect::None, tool_flags::kNone,
      kGetDebuggerStreamParams, debugger_ops::handle_debugger_stream,
      schema::build_action_schema(
          kGetDebuggerStreamActions, kGetDebuggerStreamParams,
          "log: log stream selected by source (editor engine log, plugin LogSystem, or running game output); output: running game stdout/stderr; errors: running game script errors; session: debugger session state"),
      {},
      {"get_debugger_stream action=log source=editor",
       "get_debugger_stream action=errors group=true",
       "get_debugger_stream action=session"}},
      {
          ToolAlias{
              "get_debugger_log", "get_debugger_stream",
              [](const godot_autopilot::util::JsonValue &args) {
                return stream_args(args, "log", "editor");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debugger_output", "get_debugger_stream",
              [](const godot_autopilot::util::JsonValue &args) {
                return stream_args(args, "output", "game");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debugger_errors", "get_debugger_stream",
              [](const godot_autopilot::util::JsonValue &args) {
                return stream_args(args, "errors", "game");
              },
              {}, "0.4.0", "0.6.0"},
          ToolAlias{
              "get_debugger_session_info", "get_debugger_stream",
              [](const godot_autopilot::util::JsonValue &args) {
                return stream_args(args, "session", "editor");
              },
              {}, "0.4.0", "0.6.0"},
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "debugger_breakpoints",
      "List, set and clear script breakpoints in the editor debugger in one action tool. action='list' reads ScriptEditor.get_breakpoints() and returns each entry as {path, line, raw, enabled}; action='set' enables the breakpoint at path/line and action='clear' disables it through the active EditorDebuggerSession.set_breakpoint(path, line, enabled). list works without a running game; set/clear return the structured error 'no active debugger session' when no debug session is attached. set/clear modify editor window state but never ask for confirmation. Breakpoints only bind to res:// scripts.",
      "Debugger", {"debugger", "breakpoint", "breakpoints", "script", "debug"},
      SideEffect::ModifiesWindow, tool_flags::kMutating,
      kDebuggerBreakpointsParams, debugger_ops::handle_debugger_breakpoints,
      schema::build_action_schema(kDebuggerBreakpointsActions,
                                  kDebuggerBreakpointsParams,
                                  "Breakpoint action: list|set|clear"),
      {},
      {"{\"action\":\"list\"}",
       "{\"action\":\"set\",\"path\":\"res://scripts/player.gd\",\"line\":12}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_plugin_log",
      "Read the plugin's own in-process LogSystem buffer — internal diagnostics such as 'late game response discarded', authorization denials and timeout diagnostics — and does not require a running game. This is the dedicated tool for the plugin source of get_debugger_stream(action='log', source='plugin'); it is a different source from the editor engine log (get_debugger_stream action='log' source='editor') and from the game process log file on disk (get_game_log_entries). Optional 'limit' (default 100, max 1000), 'level' (debug/info/warning/error, default debug = no filtering), 'category' (system/transport/tools/resources/prompts), 'filter' (case-insensitive substring) and 'since_index' (incremental read from a previous call's next_index; the response includes next_index). Example: {\"level\": \"warning\", \"limit\": 20}.",
      "Debugger", {"debugger", "log", "plugin", "source"},
      SideEffect::None, tool_flags::kNone,
      kGetPluginLogParams, debugger_ops::handle_plugin_log_get,
      {},
      {},
      {"{\"level\": \"warning\"}", "{\"since_index\": 0}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_debugger_scene_tree",
      "Read the scene tree of the running game process (node names and types) over the runtime channel. Requires a game launched from the editor (play_editor_current_scene) whose project loads the godot-autopilot extension; without an active debug session it returns an empty result plus a note — no editor-side fallback. Returns a formatted text tree; takes no parameters.",
      "Debugger", {"debugger", "scene", "tree", "game"},
      SideEffect::None, tool_flags::kNone,
      kGetDebuggerSceneTreeParams, debugger_ops::handle_debugger_get_scene_tree}));
  return v;
}

} // namespace debugger_tools
} // namespace godot_autopilot

#endif
