#ifndef GODOT_AUTOPILOT_OS_TOOLS_HPP
#define GODOT_AUTOPILOT_OS_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/os_ops.hpp"
#include "tools/text_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace os_tools {

namespace {

const std::vector<ParamSpec> kShowOsAlertParams = {
    {"text", "string", "Message to display in the modal alert body; the call blocks the editor UI until the user confirms — avoid in unattended runs", true},
    {"title", "string", "Dialog window title (default: Alert!)", false},
};

const std::vector<ParamSpec> kCreateOsProcessParams = {
    {"path", "string", "Executable to start (full path, or name resolved via PATH)", true},
    {"arguments", "array", "Command line arguments passed to the process; non-string entries are ignored", true},
};

const std::vector<ParamSpec> kExecuteOsProcessParams = {
    {"path", "string", "Executable to run (full path, or name resolved via PATH)", true},
    {"arguments", "array", "Command line arguments; non-string entries are ignored", true},
    {"output", "boolean", "Capture stdout and stderr into the result (default: false); when false, output is discarded. The call blocks until the command exits", false},
};

const std::vector<ParamSpec> kGetOsDatetimeParams = {
    {"utc", "boolean", "Return UTC date/time instead of local time (default: false); the result dictionary contains year, month, day, weekday, hour, minute, second, dst and related fields", false},
};

const std::vector<ParamSpec> kGetOsEnvironmentParams = {
    {"variable", "string", "Environment variable name to read from the editor process environment; returns an empty string when unset", true},
};

const std::vector<ParamSpec> kGetOsLocaleParams = {};

const std::vector<ParamSpec> kGetOsSystemFontsParams = {};

const std::vector<ParamSpec> kGetOsSystemInfoParams = {};

const std::vector<ParamSpec> kGetOsUniqueIdParams = {};

const std::vector<ParamSpec> kGetOsUnixTimeParams = {};

const std::vector<ParamSpec> kGetOsUserDataDirParams = {};

const std::vector<ParamSpec> kKillOsProcessParams = {
    {"pid", "integer", "Process ID to kill, as returned by create_os_process; killing is immediate and unsaved data in the target process is lost", true},
};

const std::vector<ParamSpec> kMoveOsFileToTrashParams = {
    {"path", "string", "File or folder path to move to the system trash (recycle bin)", true},
};

const std::vector<ParamSpec> kSetOsEnvironmentParams = {
    {"variable", "string", "Environment variable name to set in the editor process environment; the change is session-only and not persisted", true},
    {"value", "string", "Environment variable value", true},
};

const std::vector<ParamSpec> kOpenOsPathParams = {
    {"uri", "string", "URL or file path to open with the default application (browser, file manager, etc.)", true},
};

const std::vector<ParamSpec> kWriteFileParams = {
    {"path", "string", "File path to write; res:// paths are engine-managed (automatic reimport/update_file sync and script diagnostics), user:// or absolute paths are plain I/O", true},
    {"content", "string", "Text content to store in the file", true},
    {"mode", "string", "Write mode: 'WRITE' overwrites existing content (default), 'APPEND' appends to the end of the file", false},
};

const std::vector<ParamSpec> kReadFileParams = {
    {"path", "string", "File path to read (absolute path, or res:// / user://-relative); plain file I/O with no editor resource tracking", true},
};

const std::vector<ParamSpec> kFindInFilesParams = {
    {"query", "string", "Text to search for within each file; files that contain no occurrence are skipped", true},
    {"dir", "string", "Directory to search recursively (string, default: res://)", false},
    {"extensions", "array", "File extensions to include, without leading dot (array, e.g. ['gd','tscn','cs']); default: ['gd','tscn','tres','cs','md','json','h','cpp']", false},
    {"case_sensitive", "boolean", "Match the query with case sensitivity (boolean, default: false)", false},
    {"max_results", "integer", "Maximum number of matching files to return (integer, default: 500); stops searching early and sets truncated: true when exceeded", false},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(18);
  v.push_back(make_spec_tool(ToolSpec{
      "show_os_alert",
      "Show a modal alert dialog with the given text and optional title. The call blocks the editor UI until the user dismisses the dialog, so avoid it in unattended or automated runs where it would hang. Requires 'text' (string); returns result 'ok' once dismissed.",
      "OS", {"os", "alert"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kShowOsAlertParams, os_ops::handle_os_alert}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_os_process",
      "Start an executable as a detached background process and return immediately. Use it for long-running or fire-and-forget programs; keep the returned PID to kill it later with kill_os_process. Unlike execute_os_process it does not wait for the process or capture output. Requires 'path' (string) and 'arguments' (array of strings); returns the process PID.",
      "OS", {"os", "process"}, SideEffect::Process, tool_flags::kMutating,
      kCreateOsProcessParams, os_ops::handle_os_create_process}));
  v.push_back(make_spec_tool(ToolSpec{
      "execute_os_process",
      "Run a command synchronously and wait until it exits, so the call may block for the full runtime of the command. Use it for short commands whose result is needed immediately. Returns exit_code, plus stdout and stderr (empty unless 'output' is true). Requires 'path' and 'arguments'; 'output' (bool, default false) enables capturing output instead of discarding it.",
      "OS", {"os", "execute"}, SideEffect::Process, tool_flags::kMutating,
      kExecuteOsProcessParams, os_ops::handle_os_execute}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_datetime",
      "Read the current system date and time as a dictionary with year, month, day, weekday, hour, minute, second, dst and other fields. Use it when calendar fields are needed; for a raw second-based timestamp use get_os_unix_time instead. Optional 'utc' (bool, default false) selects UTC over local time.",
      "OS", {"os", "datetime"}, SideEffect::None, tool_flags::kNone,
      kGetOsDatetimeParams, os_ops::handle_os_get_datetime}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_environment",
      "Read an environment variable from the editor process environment. Use it to adapt behavior to configuration set outside the editor or to inspect configuration injected by the launch script. Returns the variable value as a string, or an empty string if it is not set. Requires 'variable' (string).",
      "OS", {"os", "environment"}, SideEffect::None, tool_flags::kNone,
      kGetOsEnvironmentParams, os_ops::handle_os_get_environment}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_locale",
      "Read the system locale string (for example 'en' or 'zh_CN'). Use it to localize tool output or UI decisions and to reproduce locale-dependent formatting issues. Returns a language code string; takes no parameters. The format follows the BCP 47 convention used by Godot.",
      "OS", {"os", "locale"}, SideEffect::None, tool_flags::kNone,
      kGetOsLocaleParams, os_ops::handle_os_get_locale}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_system_fonts",
      "List the names of fonts installed on the system. Use it to pick a font for UI or rendering experiments before loading actual font resources. Returns an array of font name strings; takes no parameters. Font names can be loaded with FontFile or used by UI themes.",
      "OS", {"os", "fonts"}, SideEffect::None, tool_flags::kNone,
      kGetOsSystemFontsParams, os_ops::handle_os_get_system_fonts}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_system_info",
      "Read basic information about the host machine and operating system. Use it to report the environment in diagnostics or adapt behavior to the platform. Returns name (OS name), version, processor_count and processor_name; takes no parameters. The OS name matches the platform Godot was built for, such as Windows or Linux.",
      "OS", {"os", "system"}, SideEffect::None, tool_flags::kNone,
      kGetOsSystemInfoParams, os_ops::handle_os_get_system_info}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_unique_id",
      "Read the machine unique identifier string. Use it to distinguish machines or persist device-bound state across sessions. Returns the unique ID string, which is stable for the machine; takes no parameters. It is derived from hardware characteristics and should not be treated as a secret.",
      "OS", {"os", "unique_id"}, SideEffect::None, tool_flags::kNone,
      kGetOsUniqueIdParams, os_ops::handle_os_get_unique_id}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_unix_time",
      "Read the current Unix time as a floating-point seconds timestamp since the epoch. Use it for elapsed-time measurement and timing comparisons; for calendar fields such as year or month use get_os_datetime instead. Returns a number; takes no parameters. Fractional seconds preserve sub-second precision.",
      "OS", {"os", "time"}, SideEffect::None, tool_flags::kNone,
      kGetOsUnixTimeParams, os_ops::handle_os_get_unix_time}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_user_data_dir",
      "Read the user data directory path where the project stores persistent files. Use it to locate game logs (user://logs) or write user files from scripts. Returns an absolute path string; takes no parameters. The directory is platform-dependent and shared across project instances.",
      "OS", {"os", "data_dir"}, SideEffect::None, tool_flags::kNone,
      kGetOsUserDataDirParams, os_ops::handle_os_get_user_data_dir}));
  v.push_back(make_spec_tool(ToolSpec{
      "kill_os_process",
      "Forcefully kill a running process by its PID. Use it to stop processes started with create_os_process; killing is immediate and cannot be undone, and unsaved data in the target process is lost. Returns a Godot error code (0 means OK); requires 'pid' (integer).",
      "OS", {"os", "kill"}, SideEffect::Process, tool_flags::kMutating,
      kKillOsProcessParams, os_ops::handle_os_kill}));
  v.push_back(make_spec_tool(ToolSpec{
      "move_os_file_to_trash",
      "Move a file or folder to the system trash (recycle bin). Use it to delete files safely without permanent removal, leaving a recovery option. Supports res:// and user:// project paths (auto-globalized). Returns a Godot error code (0 means OK); requires 'path' (string). On systems without a trash, deletion may fail.",
      "OS", {"os", "trash"}, SideEffect::WritesFile, tool_flags::kMutating,
      kMoveOsFileToTrashParams, os_ops::handle_os_move_to_trash}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_os_environment",
      "Set an environment variable in the editor process environment. Use it to influence the editor or child processes started afterwards, for example to override tooling paths; the change is not persisted and only affects the current session. Requires 'variable' and 'value' (strings); returns result 'ok'.",
      "OS", {"os", "environment"}, SideEffect::Process, tool_flags::kMutating,
      kSetOsEnvironmentParams, os_ops::handle_os_set_environment}));
  v.push_back(make_spec_tool(ToolSpec{
      "open_os_path",
      "Open a URL or file path with the default application (for example a browser or file manager). Use it to show documentation, folders or web resources to the user. Returns a Godot error code (0 means OK); requires 'uri' (string).",
      "OS", {"os", "shell"}, SideEffect::Process, tool_flags::kMutating,
      kOpenOsPathParams, os_ops::handle_os_shell_open}));
  v.push_back(make_spec_tool(ToolSpec{
      "write_file",
      "Write or append text content to a file. Paths inside the project (res://) are engine-managed: after writing, the file is synced with the editor file system — imported asset types (images, audio, fonts, 3D models, translations, or any file that already has a .import sidecar) are queued for reimport while plain text files get update_file; script-like files (.gd/.gdshader/.gdshaderinc/.cs) additionally return a diagnostics object reporting whether the written script still loads. External paths (user:// or absolute) are written as plain I/O outside editor tracking and report engine_managed: false. Requires 'path' and 'content' (strings); optional 'mode' is 'WRITE' (overwrite, default) or 'APPEND'. Returns result 'ok', plus engine_managed and action ('reimport' or 'update_file') for managed writes.",
      "OS", {"file", "write"}, SideEffect::WritesFile, tool_flags::kMutating,
      kWriteFileParams, text_ops::handle_file_write}));
  v.push_back(make_spec_tool(ToolSpec{
      "read_file",
      "Read a text file from disk (absolute path, or res:// / user://-relative) and return its full text content. This is the read-side counterpart of write_file: plain file I/O with no editor resource tracking. Requires 'path'. Returns result {'path', 'content'} on success; errors when the file cannot be opened.",
      "OS", {"file", "read"}, SideEffect::None, tool_flags::kNone,
      kReadFileParams, text_ops::handle_file_read}));
  v.push_back(make_spec_tool(ToolSpec{
      "find_in_files",
      "Recursively search text files under a directory (default res://) for a query string, returning each matching file with its occurrence count. Optional 'dir', 'extensions' (array, default gd/tscn/tres/cs/md/json/h/cpp), 'case_sensitive' (default false) and 'max_results' (default 500). Stops early once max_results files are collected and reports truncated: true; the result also carries the files array and total count.",
      "OS", {"file", "search"}, SideEffect::None, tool_flags::kNone,
      kFindInFilesParams, text_ops::handle_find_in_files}));
  return v;
}

} // namespace os_tools
} // namespace godot_autopilot

#endif