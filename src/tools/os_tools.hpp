#ifndef GODOT_AUTOPILOT_OS_TOOLS_HPP
#define GODOT_AUTOPILOT_OS_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/file_dir_ops.hpp"
#include "tools/os_ops.hpp"
#include "tools/text_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace os_tools {

namespace {

const std::vector<ParamSpec> kShowOsAlertParams = {
    {"text", "string", "Message to show in the alert body; it is only displayed when modal=true, otherwise it is returned as part of the confirmation notice", true},
    {"title", "string", "Dialog window title (default: Alert!)", false},
    {"modal", "boolean", "Actually open the modal dialog (default: false). With false the call returns a structured notice (shown=false, requires_confirmation=true) instead of a dialog, because a modal alert blocks the editor main thread until a human dismisses it and freezes unattended runs. Set true only when a human is watching the editor", false},
};

const std::vector<ParamSpec> kCreateOsProcessParams = {
    {"path", "string", "Executable to start (full path, or name resolved via PATH)", true},
    {"arguments", "array", "Command line arguments passed to the process; non-string entries are ignored", true},
};

const std::vector<ParamSpec> kExecuteOsProcessParams = {
    {"path", "string", "Executable to run (full path, or name resolved via PATH)", true},
    {"arguments", "array", "Command line arguments; non-string entries are ignored", true},
    {"output", "boolean", "Capture stdout and stderr into the result (default: false); when false, output is discarded (the pipes are still drained, just not kept in memory). The process is started without blocking the editor and the call waits for it to exit without freezing the editor. Captured output is capped at 2 MiB per stream (GDA_SCAN_MAX_FILE_BYTES): collection stops at the cap and stdout_truncated / stderr_truncated report true alongside max_output_bytes", false},
    {"timeout_ms", "integer", "Deadline for the external process in milliseconds (default: 5000, min: 100, max: 25000); when the deadline passes the process is terminated and the call fails with code main_thread_timeout (cancelled, not retryable)", false},
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

const std::vector<ParamSpec> kCreateCSharpScriptParams = {
    {"path", "string", "C# file path to create under res:// (e.g. res://scripts/Player.cs); the file stem must equal the declared class name", true},
    {"source_code", "string", "C# source code; must declare one partial class deriving from a Godot base class, covered by the project .csproj", true},
    {"overwrite", "boolean", "Overwrite an existing file (default: false); false errors when the file already exists", false},
};

const std::vector<ParamSpec> kReadFileParams = {
    {"path", "string", "File path to read (res:// or user:// below the namespace root; an absolute path is accepted only inside the project root and is normalized to res://); plain file I/O with no editor resource tracking", true},
};

const std::vector<ParamSpec> kFindInFilesParams = {
    {"query", "string", "Text to search for within each file; files that contain no occurrence are skipped", true},
    {"dir", "string", "Directory to search recursively (string, default: res://)", false},
    {"extensions", "array", "File extensions to include, without leading dot (array, e.g. ['gd','tscn','cs']); default: ['gd','tscn','tres','cs','md','json','h','cpp']", false},
    {"case_sensitive", "boolean", "Match the query with case sensitivity (boolean, default: false)", false},
    {"max_results", "integer", "Maximum number of matching files to return (integer, default: 500); stops searching early and sets truncated: true when exceeded", false},
};

const std::vector<ParamSpec> kListDirectoryParams = {
    {"path", "string", "Directory to list (res:// or user://; absolute paths inside the project root are normalized to res://)", true},
    {"include_hidden", "boolean", "Include dotfile entries starting with '.' (boolean, default: false); when false hidden entries are skipped", false},
    {"max_entries", "integer", "Maximum number of entries to return (integer, default: 500, minimum: 1); stops collecting early and sets truncated: true when exceeded", false},
};

const std::vector<ParamSpec> kGetFileInfoParams = {
    {"path", "string", "File or directory path to inspect (res:// or user://; absolute paths inside the project root are normalized to res://)", true},
};

const std::vector<ParamSpec> kCopyFileGenericParams = {
    {"path", "string", "Source file path to copy (res:// or user://; must exist as a file)", true},
    {"dest_path", "string", "Destination file path to write (res:// or user://); missing parent directories are created recursively and an existing file is overwritten", true},
};

const std::vector<ParamSpec> kEnsureDirectoryParams = {
    {"path", "string", "Directory to create below the namespace root (res:// or user://); missing parents are created recursively; idempotent when the directory already exists", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(23);
  v.push_back(make_spec_tool(ToolSpec{
      "show_os_alert",
      "Ask the editor to show an alert to the user. By default the call does NOT open a dialog: it returns a structured notice (shown=false, requires_confirmation=true) carrying the text and title and explaining that the alert needs human confirmation. A modal alert blocks the editor main thread until the user dismisses it, so unattended or automated runs would hang — only pass modal=true when a human is watching the editor. Requires 'text' (string); optional 'title' (default: Alert!) and 'modal' (boolean, default false). Returns result 'ok' plus shown.",
      "OS", {"os", "alert"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kShowOsAlertParams, os_ops::handle_os_alert}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_os_process",
      "Start an executable as a detached background process and return immediately. Use it for long-running or fire-and-forget programs; keep the returned PID to kill it later with kill_os_process. Unlike execute_os_process it does not wait for the process or capture output. Requires 'path' (string) and 'arguments' (array of strings); returns the process PID.",
      "OS", {"os", "process"}, SideEffect::Process, tool_flags::kMutating,
      kCreateOsProcessParams, os_ops::handle_os_create_process}));
  v.push_back(make_spec_tool(ToolSpec{
      "execute_os_process",
      "Run an external command and return its exit code, optionally with its stdout and stderr. The process is started without blocking the editor: the editor main thread stays responsive while the call waits for the command to finish on the transport thread, and the pipes are drained frame by frame. Use it for short commands whose result is needed immediately; for long-running or fire-and-forget programs use create_os_process instead. Optional 'timeout_ms' (default 5000, min 100, max 25000) bounds the wait: when the deadline passes the process is terminated and the call fails with code main_thread_timeout (cancelled, not retryable). Returns exit_code, plus stdout and stderr (empty unless 'output' is true); each captured stream is capped at 2 MiB and the result reports stdout_truncated / stderr_truncated with max_output_bytes when the cap was reached. Requires 'path' and 'arguments'.",
      "OS", {"os", "execute"}, SideEffect::Process, tool_flags::kMutating,
      kExecuteOsProcessParams, os_ops::handle_os_execute}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_datetime",
      "Read the current system date and time as a dictionary with year, month, day, weekday, hour, minute, second, dst and other fields. Use it when calendar fields are needed; for a raw second-based timestamp use get_os_unix_time instead. Optional 'utc' (bool, default false) selects UTC over local time.",
      "OS", {"os", "datetime"}, SideEffect::None, tool_flags::kNone,
      kGetOsDatetimeParams, os_ops::handle_os_get_datetime}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_environment",
      "Read an environment variable from the editor process environment. Use it to adapt behavior to configuration set outside the editor or to inspect configuration injected by the launch script. Returns the variable value as a string, or an empty string if it is not set. Requires 'variable' (string). The editor process environment inherits the launch shell and can carry cloud credentials or tokens, so the call is gated by the 'process' capability: without 'process' in the allow list (GODOT_AUTOPILOT_ALLOW=process) it is rejected with code authorization_required.",
      "OS", {"os", "environment"}, SideEffect::Process, tool_flags::kNone,
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
      "Read the machine unique identifier string. Use it to distinguish machines or persist device-bound state across sessions. Returns the unique ID string, which is stable for the machine; takes no parameters. It is derived from hardware characteristics and identifies the machine uniquely, so the call is gated by the 'process' capability: without 'process' in the allow list (GODOT_AUTOPILOT_ALLOW=process) it is rejected with code authorization_required.",
      "OS", {"os", "unique_id"}, SideEffect::Process, tool_flags::kNone,
      kGetOsUniqueIdParams, os_ops::handle_os_get_unique_id}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_unix_time",
      "Read the current Unix time as a floating-point seconds timestamp since the epoch. Use it for elapsed-time measurement and timing comparisons; for calendar fields such as year or month use get_os_datetime instead. Returns a number; takes no parameters. Fractional seconds preserve sub-second precision.",
      "OS", {"os", "time"}, SideEffect::None, tool_flags::kNone,
      kGetOsUnixTimeParams, os_ops::handle_os_get_unix_time}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_user_data_dir",
      "Read the user data directory path where the project stores persistent files. Use it to locate game logs (user://logs) or write user files from scripts. Returns an absolute path string; takes no parameters. The directory is platform-dependent and shared across project instances. The path exposes the host layout of the editor process, so the call is gated by the 'process' capability: without 'process' in the allow list (GODOT_AUTOPILOT_ALLOW=process) it is rejected with code authorization_required.",
      "OS", {"os", "data_dir"}, SideEffect::Process, tool_flags::kNone,
      kGetOsUserDataDirParams, os_ops::handle_os_get_user_data_dir}));
  v.push_back(make_spec_tool(ToolSpec{
      "kill_os_process",
      "Forcefully kill a running process by its PID. Use it to stop processes started with create_os_process; killing is immediate and cannot be undone, and unsaved data in the target process is lost. Returns a Godot error code (0 means OK); requires 'pid' (integer).",
      "OS", {"os", "kill"}, SideEffect::Process, tool_flags::kMutating,
      kKillOsProcessParams, os_ops::handle_os_kill}));
  v.push_back(make_spec_tool(ToolSpec{
      "move_os_file_to_trash",
      "Move a file or folder to the system trash (recycle bin). Use it to delete files safely without permanent removal, leaving a recovery option. Accepts res:// and user:// paths below the namespace root (auto-globalized); an absolute path is accepted only inside the project root and is normalized to res://; a path that escapes the root, an unsupported scheme or a bare namespace root is rejected with a path-rejected error before the trash call. Returns a Godot error code (0 means OK); requires 'path' (string). On systems without a trash, deletion may fail.",
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
      "create_csharp_script",
      "Create a new C# script file under res:// with pre-write validation. Requires path (must end in .cs) and source_code; overwrite defaults to false. Three gates fail with an actionable patch instead of a silent ok: the file stem must equal the declared class name, the class must be partial and derive from a ClassDB-known Godot base class, and the res:// root .csproj must reference the file (SDK-style globs count as referenced). Returns the CSharpFileRecord {path, class_name, csproj_ref_exists} plus base_class, project_file, is_dotnet_editor, verified/readback and a diagnostics object; the .cs diagnostics hint reports the .NET-editor state, the assembly build state and where in get_debugger_log to look. C# compiles out of band: run build_csharp_assembly afterwards and confirm via get_debugger_log, since nothing hot-reloads.",
      "Scripts", {"script", "csharp", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCreateCSharpScriptParams, text_ops::handle_create_csharp_script}));
  v.push_back(make_spec_tool(ToolSpec{
      "read_file",
      "Read a text file from disk (res:// or user:// below the namespace root; absolute paths only inside the project root) and return its full text content. This is the read-side counterpart of write_file: plain file I/O with no editor resource tracking. Requires 'path'. Returns result {'path', 'content', 'bytes', 'truncated', 'max_bytes'} on success — truncated is always false because an over-limit file is refused, never returned partly: a file larger than 2 MiB (GDA_SCAN_MAX_FILE_BYTES) fails with structured_error.code file_too_large carrying max_bytes and size_bytes, and a path outside the project namespaces fails with path rejected. Errors when the file cannot be opened.",
      "OS", {"file", "read"}, SideEffect::None, tool_flags::kNone,
      kReadFileParams, text_ops::handle_file_read}));
  v.push_back(make_spec_tool(ToolSpec{
      "find_in_files",
      "Recursively search text files under a directory (default res://) for a query string, returning each matching file with its occurrence count. Optional 'dir', 'extensions' (array, default gd/tscn/tres/cs/md/json/h/cpp), 'case_sensitive' (default false) and 'max_results' (default 500). Stops early once max_results files are collected and reports truncated: true; the result also carries the files array and total count.",
      "OS", {"file", "search"}, SideEffect::None, tool_flags::kNone,
      kFindInFilesParams, text_ops::handle_find_in_files}));
  v.push_back(make_spec_tool(ToolSpec{
      "list_directory",
      "List files and subdirectories of a project directory via plain filesystem I/O with no editor resource tracking. Accepts res:// and user:// paths (absolute paths inside the project root are normalized to res://). Requires 'path'; optional 'include_hidden' (default false, dotfile entries are skipped unless true) and 'max_entries' (default 500, minimum 1). Returns result with path, entries as {name, path, is_dir} objects, total and truncated (true when more entries exist than max_entries).",
      "OS", {"file", "directory", "list"}, SideEffect::None, tool_flags::kNone,
      kListDirectoryParams, file_dir_ops::handle_list_directory}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_file_info",
      "Report plain filesystem metadata for a project path with no editor resource tracking. Accepts res:// and user:// paths (absolute paths inside the project root are normalized to res://). Requires 'path'. Returns result with path, exists, is_dir, is_file, size (bytes, 0 for directories and missing paths) and modified_time (engine timestamp, 0 when unavailable).",
      "OS", {"file", "info"}, SideEffect::None, tool_flags::kNone,
      kGetFileInfoParams, file_dir_ops::handle_get_file_info}));
  v.push_back(make_spec_tool(ToolSpec{
      "copy_file",
      "Copy a single file to dest_path as plain byte-for-byte filesystem I/O with no editor resource tracking. Both 'path' and 'dest_path' are required res:// or user:// paths (absolute paths inside the project root are normalized to res://). Missing parent directories of dest_path are created recursively and an existing file at dest_path is overwritten. The .import and .uid sidecars are not copied or handled. Returns result (copied), from, to and overwritten. Use copy_resource_file when engine resource semantics are needed.",
      "OS", {"file", "copy"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCopyFileGenericParams, file_dir_ops::handle_copy_file_generic}));
  v.push_back(make_spec_tool(ToolSpec{
      "ensure_directory",
      "Create a directory and any missing parents as plain filesystem I/O with no editor resource tracking. Requires 'path' as a res:// or user:// directory below the namespace root (the root itself is rejected). Idempotent: an existing directory succeeds with already_existed true. Returns result (ok), path and already_existed.",
      "OS", {"file", "directory", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kEnsureDirectoryParams, file_dir_ops::handle_ensure_directory}));
  return v;
}

} // namespace os_tools
} // namespace godot_autopilot

#endif