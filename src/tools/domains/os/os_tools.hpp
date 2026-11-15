#ifndef GODOT_AUTOPILOT_OS_TOOLS_HPP
#define GODOT_AUTOPILOT_OS_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/os/os_file_dir_ops.hpp"
#include "tools/domains/os/os_ops.hpp"
#include "tools/domains/text/text_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace os_tools {

namespace {

const std::vector<ParamSpec> kShowOsAlertParams = {
    {"text", "string", "Message to show in the alert body; it is only displayed when modal=true, otherwise it is returned as part of the confirmation notice", true},
    {"title", "string", "Dialog window title (default: Alert!)", false},
    {"modal", "boolean", "Actually open the modal dialog (default: false). With false the call returns a structured notice (shown=false, requires_confirmation=true) instead of a dialog, because a modal alert blocks the editor main thread until a human dismisses it and freezes unattended runs. Set true only when a human is watching the editor", false},
};

const std::vector<ParamSpec> kOsProcessParams = {
    {"path", "string", "Executable to start or run (full path, or name resolved via PATH); required by action='create' and action='execute'", false},
    {"arguments", "array", "Command line arguments for action='create' and action='execute'; non-string entries are ignored", false},
    {"output", "boolean", "Capture stdout and stderr into the result for action='execute' (default: false); captured output is capped at 2 MiB per stream and stdout_truncated / stderr_truncated report the cap", false},
    {"timeout_ms", "integer", "Deadline for action='execute' in milliseconds (default: 5000, min: 100, max: 25000); when the deadline passes the process is terminated and the call fails with code main_thread_timeout", false},
    {"pid", "integer", "Process ID to kill for action='kill', as returned by action='create'; killing is immediate and unsaved data in the target process is lost", false},
};

const std::vector<ParamSpec> kOsEnvironmentParams = {
    {"variable", "string", "Environment variable name to read, test or set in the editor process environment", false},
    {"value", "string", "Environment variable value for action='set'; the change is session-only and not persisted", false},
};

const std::vector<ParamSpec> kOsTimeParams = {
    {"utc", "boolean", "Return UTC date/time for action='datetime' instead of local time (default: false)", false},
};

const std::vector<ParamSpec> kOsSystemInfoParams = {};

const std::vector<ParamSpec> kOsFileTextParams = {
    {"path", "string", "File path to read for action='read' (res:// or user:// below the namespace root; an absolute path is accepted only inside the project root and is normalized to res://); plain file I/O with no editor resource tracking", false},
    {"query", "string", "Text to search for within each file for action='find'; files that contain no occurrence are skipped", false},
    {"dir", "string", "Directory to search recursively for action='find' (default: res://)", false},
    {"extensions", "array", "File extensions to include for action='find', without leading dot (default: ['gd','tscn','tres','cs','md','json','h','cpp'])", false},
    {"case_sensitive", "boolean", "Match the query with case sensitivity for action='find' (default: false)", false},
    {"max_results", "integer", "Maximum number of matching files for action='find' (default: 500); stops early and sets truncated: true when exceeded", false},
};

const std::vector<ParamSpec> kOsFileWriteParams = {
    {"path", "string", "File path for action='write' (res:// paths are engine-managed with automatic reimport/update_file sync and script diagnostics; user:// or absolute paths are plain I/O), source file path for action='copy', directory for action='ensure', or file/folder path for action='trash'", false},
    {"content", "string", "Text content to store in the file for action='write'", false},
    {"mode", "string", "Write mode for action='write': 'WRITE' overwrites existing content (default), 'APPEND' appends to the end of the file", false},
    {"dest_path", "string", "Destination file path for action='copy' (res:// or user://); missing parent directories are created recursively and an existing file is overwritten", false},
};

const std::vector<ParamSpec> kOsFileManagerParams = {
    {"path", "string", "Directory to list for action='list' or file/directory path to inspect for action='info' (res:// or user://; absolute paths inside the project root are normalized to res://)", true},
    {"include_hidden", "boolean", "Include dotfile entries starting with '.' for action='list' (default: false)", false},
    {"max_entries", "integer", "Maximum number of entries for action='list' (default: 500, minimum: 1); stops collecting early and sets truncated: true when exceeded", false},
};

const std::vector<ParamSpec> kCreateCSharpScriptParams = {
    {"path", "string", "C# file path to create under res:// (e.g. res://scripts/Player.cs); the file stem must equal the declared class name", true},
    {"source_code", "string", "C# source code; must declare one partial class deriving from a Godot base class, covered by the project .csproj", true},
    {"overwrite", "boolean", "Overwrite an existing file (default: false); false errors when the file already exists", false},
};

const std::vector<ParamSpec> kGetOsUniqueIdParams = {};

const std::vector<ParamSpec> kGetOsUserDataDirParams = {};

const std::vector<ParamSpec> kOpenOsPathParams = {
    {"uri", "string", "URL or file path to open with the default application (browser, file manager, etc.)", true},
};

inline std::vector<std::string> os_actions(
    std::initializer_list<const char *> actions) {
  std::vector<std::string> out;
  out.reserve(actions.size());
  for (const char *action : actions) {
    out.emplace_back(action);
  }
  return out;
}

inline std::vector<std::string> os_examples(
    std::initializer_list<const char *> examples) {
  std::vector<std::string> out;
  out.reserve(examples.size());
  for (const char *example : examples) {
    out.emplace_back(example);
  }
  return out;
}

inline ToolSpec os_action_spec(ToolSpec spec,
                               std::initializer_list<const char *> actions,
                               std::initializer_list<const char *> examples) {
  spec.raw_schema = schema::build_action_schema(
      os_actions(actions), spec.params,
      "Action to perform; per-action parameters are documented in the tool "
      "description");
  spec.examples = os_examples(examples);
  return spec;
}

inline mcp::JsonValue os_args_with_action(const mcp::JsonValue &args,
                                          const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline ToolAlias os_alias(const char *name, const char *canonical,
                          const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return os_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(12);
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_process",
              "Manage external processes, actions: create, execute, kill. create starts an executable as a detached background process and returns its PID immediately for long-running or fire-and-forget programs. execute runs a command and returns exit_code plus optional stdout/stderr while the editor main thread stays responsive; timeout_ms defaults to 5000 and bounds the wait, and the process is ended with code main_thread_timeout when the deadline passes. The third action (pid) stops the process identified by pid immediately, so unsaved data in the target is lost. Requires the process capability; create and execute capture output capped at 2 MiB per stream. Deprecated aliases: create_os_process, execute_os_process, kill_os_process.",
              "OS",
              {"os", "process", "execute", "kill", "create_os_process",
               "execute_os_process", "kill_os_process"},
              SideEffect::Process,
              tool_flags::kMutating,
              kOsProcessParams,
              os_ops::handle_os_process},
          {"create", "execute", "kill"},
          {"{\"action\":\"execute\",\"path\":\"git\",\"arguments\":[\"status\"],\"output\":true}",
           "{\"action\":\"create\",\"path\":\"notepad.exe\",\"arguments\":[]}",
           "{\"action\":\"kill\",\"pid\":1234}"}),
      {
          os_alias("create_os_process", "os_process", "create"),
          os_alias("execute_os_process", "os_process", "execute"),
          os_alias("kill_os_process", "os_process", "kill"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_environment",
              "Read and write environment variables of the editor process, actions: get (value string, empty when unset), set (session-only change, not persisted) and has (boolean existence check). variable is the variable name; value is only used by set. The editor process environment inherits the launch shell and can carry cloud credentials or tokens, so the tool is gated by the process capability. Deprecated aliases: get_os_environment, set_os_environment.",
              "OS",
              {"os", "environment", "variable", "get_os_environment",
               "set_os_environment"},
              SideEffect::Process,
              tool_flags::kMutating,
              kOsEnvironmentParams,
              os_ops::handle_os_environment},
          {"get", "set", "has"},
          {"{\"action\":\"get\",\"variable\":\"PATH\"}",
           "{\"action\":\"has\",\"variable\":\"PATH\"}",
           "{\"action\":\"set\",\"variable\":\"MY_FLAG\",\"value\":\"1\"}"}),
      {
          os_alias("get_os_environment", "os_environment", "get"),
          os_alias("set_os_environment", "os_environment", "set"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_time",
              "Read system time, actions: datetime (calendar dictionary with year, month, day, weekday, hour, minute, second, dst and related fields; utc selects UTC over local time) and unix_time (floating-point seconds since the epoch, preserving sub-second precision). Deprecated aliases: get_os_datetime, get_os_unix_time.",
              "OS",
              {"os", "time", "datetime", "unix_time", "get_os_datetime",
               "get_os_unix_time"},
              SideEffect::None,
              tool_flags::kNone,
              kOsTimeParams,
              os_ops::handle_os_time},
          {"datetime", "unix_time"},
          {"{\"action\":\"datetime\",\"utc\":true}",
           "{\"action\":\"unix_time\"}"}),
      {
          os_alias("get_os_datetime", "os_time", "datetime"),
          os_alias("get_os_unix_time", "os_time", "unix_time"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_system_info",
              "Read host system information that needs no process capability, actions: info (name, version, processor_count and processor_name of the host), locale (system locale string such as 'en' or 'zh_CN') and fonts (names of installed system fonts). Machine identity and user data paths remain separate process-gated tools. Deprecated aliases: get_os_system_info, get_os_locale, get_os_system_fonts.",
              "OS",
              {"os", "system", "info", "locale", "fonts",
               "get_os_system_info", "get_os_locale", "get_os_system_fonts"},
              SideEffect::None,
              tool_flags::kNone,
              kOsSystemInfoParams,
              os_ops::handle_os_system_info},
          {"info", "locale", "fonts"},
          {"{\"action\":\"info\"}",
           "{\"action\":\"locale\"}",
           "{\"action\":\"fonts\"}"}),
      {
          os_alias("get_os_system_info", "os_system_info", "info"),
          os_alias("get_os_locale", "os_system_info", "locale"),
          os_alias("get_os_system_fonts", "os_system_info", "fonts"),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_unique_id",
      "Read the machine unique identifier string. Use it to distinguish machines or persist device-bound state across sessions. Returns the unique ID string, which is stable for the machine; takes no parameters. It is derived from hardware characteristics and identifies the machine uniquely, so the call is gated by the 'process' capability: without 'process' in the allow list (GODOT_AUTOPILOT_ALLOW=process) it is rejected with code authorization_required.",
      "OS", {"os", "unique_id"}, SideEffect::Process, tool_flags::kNone,
      kGetOsUniqueIdParams, os_ops::handle_os_get_unique_id}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_os_user_data_dir",
      "Read the user data directory path where the project stores persistent files. Use it to locate game logs (user://logs) or write user files from scripts. Returns an absolute path string; takes no parameters. The directory is platform-dependent and shared across project instances. The path exposes the host layout of the editor process, so the call is gated by the 'process' capability: without 'process' in the allow list (GODOT_AUTOPILOT_ALLOW=process) it is rejected with code authorization_required.",
      "OS", {"os", "data_dir"}, SideEffect::Process, tool_flags::kNone,
      kGetOsUserDataDirParams, os_ops::handle_os_get_user_data_dir}));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_file_text",
              "Read and search text files with plain file I/O and no editor resource tracking, actions: read (full text content of path; files larger than 2 MiB fail with structured_error.code file_too_large carrying max_bytes and size_bytes) and find (recursive text search under dir for query; optional extensions, case_sensitive and max_results; stops early with truncated: true). Paths accept res:// and user:// below the namespace root, and absolute paths only inside the project root. Writing text lives in os_file_write action='write'. Deprecated aliases: read_file, find_in_files.",
              "OS",
              {"os", "file", "text", "read", "find", "read_file",
               "find_in_files"},
              SideEffect::None,
              tool_flags::kNone,
              kOsFileTextParams,
              file_dir_ops::handle_os_file_text},
          {"read", "find"},
          {"{\"action\":\"read\",\"path\":\"res://project.godot\"}",
           "{\"action\":\"find\",\"query\":\"autoload\",\"dir\":\"res://\",\"max_results\":50}"}),
      {
          os_alias("read_file", "os_file_text", "read"),
          os_alias("find_in_files", "os_file_text", "find"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_file_write",
              "Write-side file operations with plain byte-for-byte filesystem I/O, actions: write (write or append text content; res:// paths are engine-managed with automatic reimport/update_file sync and script diagnostics, user:// and absolute paths are plain I/O), copy (copy path to dest_path, creating missing parents and overwriting an existing destination), ensure (create a directory and missing parents; idempotent with already_existed) and trash (move a file or folder to the system trash; path is validated against the project namespaces first). Paths accept res:// and user:// below the namespace root. The .import and .uid sidecars are not copied by copy. Deprecated aliases: write_file, copy_file, ensure_directory, move_os_file_to_trash.",
              "OS",
              {"os", "file", "write", "copy", "directory", "trash",
               "write_file", "copy_file", "ensure_directory",
               "move_os_file_to_trash"},
              SideEffect::WritesFile,
              tool_flags::kMutating,
              kOsFileWriteParams,
              file_dir_ops::handle_os_file_write},
          {"write", "copy", "ensure", "trash"},
          {"{\"action\":\"write\",\"path\":\"user://notes.txt\",\"content\":\"hello\"}",
           "{\"action\":\"copy\",\"path\":\"res://a.txt\",\"dest_path\":\"res://b.txt\"}",
           "{\"action\":\"ensure\",\"path\":\"res://assets/audio\"}",
           "{\"action\":\"trash\",\"path\":\"user://old.txt\"}"}),
      {
          os_alias("write_file", "os_file_write", "write"),
          os_alias("copy_file", "os_file_write", "copy"),
          os_alias("ensure_directory", "os_file_write", "ensure"),
          os_alias("move_os_file_to_trash", "os_file_write", "trash"),
      })));
  v.push_back(make_spec_tool(with_aliases(
      os_action_spec(
          ToolSpec{
              "os_file_manager",
              "Read-only file and directory inspection with plain filesystem I/O and no editor resource tracking, actions: list (entries of a directory as {name, path, is_dir} objects, with include_hidden and max_entries; sets truncated when more entries exist) and info (path, exists, is_dir, is_file, size in bytes and modified_time for one path). Paths accept res:// and user:// below the namespace root. Mutating file operations live in os_file_write. Deprecated aliases: list_directory, get_file_info.",
              "OS",
              {"os", "file", "directory", "list", "info", "list_directory",
               "get_file_info"},
              SideEffect::None,
              tool_flags::kNone,
              kOsFileManagerParams,
              file_dir_ops::handle_os_file_manager},
          {"list", "info"},
          {"{\"action\":\"list\",\"path\":\"res://\",\"max_entries\":100}",
           "{\"action\":\"info\",\"path\":\"res://project.godot\"}"}),
      {
          os_alias("list_directory", "os_file_manager", "list"),
          os_alias("get_file_info", "os_file_manager", "info"),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "create_csharp_script",
      "Create a new C# script file under res:// with pre-write validation. Requires path (must end in .cs) and source_code; overwrite defaults to false. Three gates fail with an actionable patch instead of a silent ok: the file stem must equal the declared class name, the class must be partial and derive from a ClassDB-known Godot base class, and the res:// root .csproj must reference the file (SDK-style globs count as referenced). Returns the CSharpFileRecord {path, class_name, csproj_ref_exists} plus base_class, project_file, is_dotnet_editor, verified/readback and a diagnostics object; the .cs diagnostics hint reports the .NET-editor state, the assembly build state and where in get_debugger_log to look. C# compiles out of band: run build_csharp_assembly afterwards and confirm via get_debugger_log, since nothing hot-reloads.",
      "Scripts", {"script", "csharp", "create"}, SideEffect::WritesFile, tool_flags::kMutating,
      kCreateCSharpScriptParams, text_ops::handle_create_csharp_script}));
  v.push_back(make_spec_tool(ToolSpec{
      "open_os_path",
      "Open a URL or file path with the default application (for example a browser or file manager). Use it to show documentation, folders or web resources to the user. Returns a Godot error code (0 means OK); requires 'uri' (string).",
      "OS", {"os", "shell"}, SideEffect::Process, tool_flags::kMutating,
      kOpenOsPathParams, os_ops::handle_os_shell_open}));
  v.push_back(make_spec_tool(ToolSpec{
      "show_os_alert",
      "Ask the editor to show an alert to the user. By default the call does NOT open a dialog: it returns a structured notice (shown=false, requires_confirmation=true) carrying the text and title and explaining that the alert needs human confirmation. A modal alert blocks the editor main thread until the user dismisses it, so unattended or automated runs would hang — only pass modal=true when a human is watching the editor. Requires 'text' (string); optional 'title' (default: Alert!) and 'modal' (boolean, default false). Returns result 'ok' plus shown.",
      "OS", {"os", "alert"}, SideEffect::ShowsAlert, tool_flags::kMutating,
      kShowOsAlertParams, os_ops::handle_os_alert}));
  return v;
}

} // namespace os_tools
} // namespace godot_autopilot

#endif
