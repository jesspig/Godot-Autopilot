#ifndef GODOT_AUTOPILOT_OS_TOOLS_HPP
#define GODOT_AUTOPILOT_OS_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/os_ops.hpp"
#include "tools/text_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace os_tools {

GDA_TOOL_CLASS_SIDE(ShowOsAlertTool, "show_os_alert",
               "Show a modal alert dialog with the given text and optional title. The call blocks the editor UI until the user dismisses the dialog, so avoid it in unattended or automated runs where it would hang. Requires 'text' (string); returns result 'ok' once dismissed.",
               "OS", std::vector<std::string>({"os", "alert"}), os_ops::handle_os_alert, false, ::godot_autopilot::SideEffect::ShowsAlert)

GDA_TOOL_CLASS_SIDE(CreateOsProcessTool, "create_os_process",
               "Start an executable as a detached background process and return immediately. Use it for long-running or fire-and-forget programs; keep the returned PID to kill it later with kill_os_process. Unlike execute_os_process it does not wait for the process or capture output. Requires 'path' (string) and 'arguments' (array of strings); returns the process PID.",
               "OS", std::vector<std::string>({"os", "process"}), os_ops::handle_os_create_process, false, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS_SIDE(ExecuteOsProcessTool, "execute_os_process",
               "Run a command synchronously and wait until it exits, so the call may block for the full runtime of the command. Use it for short commands whose result is needed immediately. Returns exit_code, plus stdout and stderr (empty unless 'output' is true). Requires 'path' and 'arguments'; 'output' (bool, default false) enables capturing output instead of discarding it.",
               "OS", std::vector<std::string>({"os", "execute"}), os_ops::handle_os_execute, false, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS(GetOsDatetimeTool, "get_os_datetime",
               "Read the current system date and time as a dictionary with year, month, day, weekday, hour, minute, second, dst and other fields. Use it when calendar fields are needed; for a raw second-based timestamp use get_os_unix_time instead. Optional 'utc' (bool, default false) selects UTC over local time.",
               "OS", std::vector<std::string>({"os", "datetime"}), os_ops::handle_os_get_datetime, false)

GDA_TOOL_CLASS(GetOsEnvironmentTool, "get_os_environment",
               "Read an environment variable from the editor process environment. Use it to adapt behavior to configuration set outside the editor or to inspect configuration injected by the launch script. Returns the variable value as a string, or an empty string if it is not set. Requires 'variable' (string).",
               "OS", std::vector<std::string>({"os", "environment"}), os_ops::handle_os_get_environment, false)

GDA_TOOL_CLASS(GetOsLocaleTool, "get_os_locale",
               "Read the system locale string (for example 'en' or 'zh_CN'). Use it to localize tool output or UI decisions and to reproduce locale-dependent formatting issues. Returns a language code string; takes no parameters. The format follows the BCP 47 convention used by Godot.",
               "OS", std::vector<std::string>({"os", "locale"}), os_ops::handle_os_get_locale, false)

GDA_TOOL_CLASS(GetOsSystemFontsTool, "get_os_system_fonts",
               "List the names of fonts installed on the system. Use it to pick a font for UI or rendering experiments before loading actual font resources. Returns an array of font name strings; takes no parameters. Font names can be loaded with FontFile or used by UI themes.",
               "OS", std::vector<std::string>({"os", "fonts"}), os_ops::handle_os_get_system_fonts, false)

GDA_TOOL_CLASS(GetOsSystemInfoTool, "get_os_system_info",
               "Read basic information about the host machine and operating system. Use it to report the environment in diagnostics or adapt behavior to the platform. Returns name (OS name), version, processor_count and processor_name; takes no parameters. The OS name matches the platform Godot was built for, such as Windows or Linux.",
               "OS", std::vector<std::string>({"os", "system"}), os_ops::handle_os_get_system_info, false)

GDA_TOOL_CLASS(GetOsUniqueIdTool, "get_os_unique_id",
               "Read the machine unique identifier string. Use it to distinguish machines or persist device-bound state across sessions. Returns the unique ID string, which is stable for the machine; takes no parameters. It is derived from hardware characteristics and should not be treated as a secret.",
               "OS", std::vector<std::string>({"os", "unique_id"}), os_ops::handle_os_get_unique_id, false)

GDA_TOOL_CLASS(GetOsUnixTimeTool, "get_os_unix_time",
               "Read the current Unix time as a floating-point seconds timestamp since the epoch. Use it for elapsed-time measurement and timing comparisons; for calendar fields such as year or month use get_os_datetime instead. Returns a number; takes no parameters. Fractional seconds preserve sub-second precision.",
               "OS", std::vector<std::string>({"os", "time"}), os_ops::handle_os_get_unix_time, false)

GDA_TOOL_CLASS(GetOsUserDataDirTool, "get_os_user_data_dir",
               "Read the user data directory path where the project stores persistent files. Use it to locate game logs (user://logs) or write user files from scripts. Returns an absolute path string; takes no parameters. The directory is platform-dependent and shared across project instances.",
               "OS", std::vector<std::string>({"os", "data_dir"}), os_ops::handle_os_get_user_data_dir, false)

GDA_TOOL_CLASS_SIDE(KillOsProcessTool, "kill_os_process",
               "Forcefully kill a running process by its PID. Use it to stop processes started with create_os_process; killing is immediate and cannot be undone, and unsaved data in the target process is lost. Returns a Godot error code (0 means OK); requires 'pid' (integer).",
               "OS", std::vector<std::string>({"os", "kill"}), os_ops::handle_os_kill, false, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS_SIDE(MoveOsFileToTrashTool, "move_os_file_to_trash",
               "Move a file or folder to the system trash (recycle bin). Use it to delete files safely without permanent removal, leaving a recovery option. Returns a Godot error code (0 means OK); requires 'path' (string). On systems without a trash, deletion may fail.",
               "OS", std::vector<std::string>({"os", "trash"}), os_ops::handle_os_move_to_trash, false, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS_SIDE(SetOsEnvironmentTool, "set_os_environment",
               "Set an environment variable in the editor process environment. Use it to influence the editor or child processes started afterwards, for example to override tooling paths; the change is not persisted and only affects the current session. Requires 'variable' and 'value' (strings); returns result 'ok'.",
               "OS", std::vector<std::string>({"os", "environment"}), os_ops::handle_os_set_environment, false, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS_SIDE(OpenOsPathTool, "open_os_path",
               "Open a URL or file path with the default application (for example a browser or file manager). Use it to show documentation, folders or web resources to the user. Returns a Godot error code (0 means OK); requires 'uri' (string).",
               "OS", std::vector<std::string>({"os", "shell"}), os_ops::handle_os_shell_open, false, ::godot_autopilot::SideEffect::Process)

GDA_TOOL_CLASS_SIDE(WriteFileTool, "write_file",
               "Write or append text content to a file using plain file I/O, bypassing editor resource tracking. Use it for temporary or external files that are not part of the Godot project; project resources should be created through scene or resource tools instead. Requires 'path' and 'content' (strings); optional 'mode' is 'WRITE' (overwrite, default) or 'APPEND'. Returns result 'ok' on success.",
               "OS", std::vector<std::string>({"file", "write"}), text_ops::handle_file_write, true, ::godot_autopilot::SideEffect::WritesFile)

GDA_TOOL_CLASS(ReadFileTool, "read_file",
               "Read a text file from disk (absolute path, or res:// / user://-relative) and return its full text content. This is the read-side counterpart of write_file: plain file I/O with no editor resource tracking. Requires 'path'. Returns result {'path', 'content'} on success; errors when the file cannot be opened.",
               "OS", std::vector<std::string>({"file", "read"}), text_ops::handle_file_read, true)

GDA_TOOL_CLASS(FindInFilesTool, "find_in_files",
               "Recursively search text files under a directory (default res://) for a query string, returning each matching file with its occurrence count. Optional 'dir', 'extensions' (array, default gd/tscn/tres/cs/md/json/h/cpp), 'case_sensitive' (default false) and 'max_results' (default 500). Stops early once max_results files are collected and reports truncated: true; the result also carries the files array and total count.",
               "OS", std::vector<std::string>({"file", "search"}), text_ops::handle_find_in_files, true)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(18);
  v.push_back(std::make_unique<ShowOsAlertTool>());
  v.push_back(std::make_unique<CreateOsProcessTool>());
  v.push_back(std::make_unique<ExecuteOsProcessTool>());
  v.push_back(std::make_unique<GetOsDatetimeTool>());
  v.push_back(std::make_unique<GetOsEnvironmentTool>());
  v.push_back(std::make_unique<GetOsLocaleTool>());
  v.push_back(std::make_unique<GetOsSystemFontsTool>());
  v.push_back(std::make_unique<GetOsSystemInfoTool>());
  v.push_back(std::make_unique<GetOsUniqueIdTool>());
  v.push_back(std::make_unique<GetOsUnixTimeTool>());
  v.push_back(std::make_unique<GetOsUserDataDirTool>());
  v.push_back(std::make_unique<KillOsProcessTool>());
  v.push_back(std::make_unique<MoveOsFileToTrashTool>());
  v.push_back(std::make_unique<SetOsEnvironmentTool>());
  v.push_back(std::make_unique<OpenOsPathTool>());
  v.push_back(std::make_unique<WriteFileTool>());
  v.push_back(std::make_unique<ReadFileTool>());
  v.push_back(std::make_unique<FindInFilesTool>());
  return v;
}

} // namespace os_tools
} // namespace godot_autopilot

#endif