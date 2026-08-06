#include "log_ops.hpp"
#include "../util/error_util.hpp"
#include "core/log_system.hpp"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace godot_self_driving {
namespace log_ops {

using JV = mcp::JsonValue;

namespace {

constexpr char RUN_HINT[] = "game logs are written by the running game process "
                            "(start it with editor_play_current_scene)";

std::string to_std(const godot::String &s) {
  godot::CharString utf8 = s.utf8();
  return std::string(utf8.ptr());
}

JV error_json(const std::string &msg) {
  JV e(JV::object_tag);
  e["error"] = JV(msg);
  return e;
}

constexpr int64_t DEFAULT_LIMIT = 50;
constexpr int64_t MAX_LIMIT = 500;

std::string logs_dir_diagnostic(const godot::String &logs_dir) {
  auto dir = godot::DirAccess::open(logs_dir);
  if (!dir.is_valid()) {
    return "logs directory does not exist: \"" + to_std(logs_dir) + "\"";
  }
  godot::PackedStringArray names;
  dir->list_dir_begin();
  godot::String name = dir->get_next();
  while (!name.is_empty()) {
    if (name != "." && name != "..") {
      names.push_back(name);
    }
    name = dir->get_next();
  }
  dir->list_dir_end();
  if (names.size() == 0) {
    return "logs directory is empty: \"" + to_std(logs_dir) + "\"";
  }
  std::string joined = "directory contains: ";
  for (int i = 0; i < names.size(); i++) {
    if (i > 0)
      joined += ", ";
    joined += to_std(names[i]);
  }
  return joined;
}

struct TailResult {
  bool opened = false;
  std::vector<std::string> lines;
  int64_t total_lines = 0;
};

#ifdef _WIN32
static TailResult shared_read_tail(const godot::String &path, int64_t limit) {

  TailResult result;
  HANDLE handle =
      CreateFileW((LPCWSTR)path.utf16().get_data(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    return result;
  LARGE_INTEGER size;
  if (!GetFileSizeEx(handle, &size)) {
    CloseHandle(handle);
    return result;
  }
  constexpr int64_t TAIL_WINDOW = 256 * 1024;
  LARGE_INTEGER offset;
  offset.QuadPart =
      (size.QuadPart > TAIL_WINDOW) ? size.QuadPart - TAIL_WINDOW : 0;
  if (!SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN)) {
    CloseHandle(handle);
    return result;
  }
  std::string raw;
  std::vector<char> buffer(64 * 1024);
  DWORD read = 0;
  while (ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                  &read, nullptr) &&
         read > 0) {
    raw.append(buffer.data(), read);
  }
  CloseHandle(handle);
  result.opened = true;
  size_t start = 0;
  while (start < raw.size()) {
    size_t end = raw.find('\n', start);
    if (end == std::string::npos) {
      result.total_lines++;
      if (limit != 0 && static_cast<int64_t>(result.lines.size()) < limit) {
        result.lines.push_back(raw.substr(start));
      }
      break;
    }
    std::string line = raw.substr(start, end - start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    result.total_lines++;
    if (limit != 0) {
      if (static_cast<int64_t>(result.lines.size()) < limit) {
        result.lines.push_back(std::move(line));
      } else {
        result.lines.erase(result.lines.begin());
        result.lines.push_back(std::move(line));
      }
    }
    start = end + 1;
  }
  return result;
}
#endif

TailResult read_tail(const godot::String &path, int64_t limit) {
#ifdef _WIN32
  TailResult shared = shared_read_tail(path, limit);
  if (shared.opened)
    return shared;
#endif
  TailResult result;
  auto file = godot::FileAccess::open(path, godot::FileAccess::READ);
  if (file.is_null())
    return result;
  result.opened = true;
  while (!file->eof_reached()) {
    std::string line = to_std(file->get_line());
    result.total_lines++;
    if (limit == 0)
      continue;
    if (static_cast<int64_t>(result.lines.size()) < limit) {
      result.lines.push_back(std::move(line));
    } else {
      result.lines.erase(result.lines.begin());
      result.lines.push_back(std::move(line));
    }
  }
  file->close();
  return result;
}

constexpr char ARCHIVE_WARNING[] =
    "primary log locked by game process; returned archive godot.log.1";

JV build_result(const godot::String &path, const TailResult &tail,
                bool from_archive) {
  JV entries(JV::array_tag);
  for (const std::string &line : tail.lines) {
    entries.PushBack(JV(line));
  }
  JV result(JV::object_tag);
  result["path"] = JV(to_std(path));
  result["entries"] = std::move(entries);
  result["total_lines"] = JV(tail.total_lines);
  if (from_archive) {
    result["from_archive"] = JV(true);
    result["warning"] = JV(ARCHIVE_WARNING);
  }
  JV r(JV::object_tag);
  r["result"] = std::move(result);
  return r;
}

} // namespace

JV handle_log_get_game_entries(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "log_get_game_entries called");
  int64_t limit = DEFAULT_LIMIT;
  auto *lp = args.Find("limit");
  if (lp) {
    if (!lp->IsInt())
      return error_json("invalid parameter: limit must be an integer");
    limit = lp->GetInt();
    if (limit > MAX_LIMIT)
      limit = MAX_LIMIT;
    if (limit < 0)
      limit = 0;
  }
  auto *os = godot::OS::get_singleton();
  if (!os)
    return error_json("OS singleton not available");
  godot::String logs_dir = os->get_user_data_dir() + "/logs";
  godot::String path = logs_dir + "/godot.log";
  if (!godot::FileAccess::file_exists(path)) {
    return error_json("game log file not found: \"" + to_std(path) + "\" — " +
                      logs_dir_diagnostic(logs_dir) + " — " + RUN_HINT);
  }
  TailResult tail = read_tail(path, limit);
  if (!tail.opened) {
    os->delay_usec(100000);
    tail = read_tail(path, limit);
  }
  if (tail.opened) {
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "log_get_game_entries completed");
    return build_result(path, tail, false);
  }
  godot::String archive = logs_dir + "/godot.log.1";
  if (godot::FileAccess::file_exists(archive)) {
    TailResult archived = read_tail(archive, limit);
    if (archived.opened) {
      LogSystem::instance().log(
          LogLevel::Info, LogCategory::Tools,
          "log_get_game_entries completed (from archive)");
      return build_result(archive, archived, true);
    }
  }
  return util::error_detail(
      "open error code " +
          std::to_string(
              static_cast<int>(godot::FileAccess::get_open_error())) +
          " (game process holds the log file)",
      to_std(path) + " — " + logs_dir_diagnostic(logs_dir),
      "read the game process log",
      "stop the game first (editor_stop_playing), or use debugger_get_output / "
      "debugger_get_errors for in-memory capture — " +
          std::string(RUN_HINT));
}

} // namespace log_ops
} // namespace godot_self_driving
