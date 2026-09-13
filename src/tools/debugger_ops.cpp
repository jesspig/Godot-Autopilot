#include "debugger_ops.hpp"
#include "core/log_system.hpp"
#include "debugger_access.hpp"
#include "runtime/gda_protocol.hpp"
#include "runtime_ops.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace debugger_ops {

namespace {

struct LogEntry {
  std::string text;
  bool is_error;
  uint64_t timestamp_ms;
};

struct ErrorEntry {
  int hr, min, sec, msec;
  std::string source_file, source_func;
  int source_line;
  std::string error, error_descr;
  bool is_warning;
  std::vector<std::string> stack_files;
  std::vector<std::string> stack_funcs;
  std::vector<int> stack_lines;
};

struct GameOutputEntry {
  std::string text;
  int type;
  uint64_t timestamp_ms;
};

struct StackFrame {
  std::string file;
  int line;
  std::string func;
};

struct MonitorFrame {
  std::vector<double> values;
  uint64_t timestamp_ms;
};

class DebuggerCapture {
public:
  static DebuggerCapture &instance() {
    static DebuggerCapture inst;
    return inst;
  }

  void add_log_entry(const std::string &text, bool is_error) {
    std::lock_guard<std::mutex> lock(mtx_);
    log_buffer_.push_back({text, is_error, now_ms()});
    if (log_buffer_.size() > MAX_LOG)
      log_buffer_.erase(log_buffer_.begin());
  }

  void add_error(int hr, int min, int sec, int msec,
                 const std::string &source_file, const std::string &source_func,
                 int source_line, const std::string &error,
                 const std::string &error_descr, bool is_warning,
                 const std::vector<std::string> &stack_files,
                 const std::vector<std::string> &stack_funcs,
                 const std::vector<int> &stack_lines) {
    std::lock_guard<std::mutex> lock(mtx_);
    errors_.push_back({hr, min, sec, msec, source_file, source_func,
                       source_line, error, error_descr, is_warning, stack_files,
                       stack_funcs, stack_lines});
    if (errors_.size() > MAX_ERRORS)
      errors_.erase(errors_.begin());
  }

  void add_game_output(const std::string &text, int type) {
    std::lock_guard<std::mutex> lock(mtx_);
    game_output_.push_back({text, type, now_ms()});
    if (game_output_.size() > MAX_GAME_OUTPUT)
      game_output_.erase(game_output_.begin());
  }

  void set_stack_dump(const std::vector<StackFrame> &frames) {
    std::lock_guard<std::mutex> lock(mtx_);
    stack_ = frames;
  }

  void set_scene_tree_raw(const std::vector<std::string> &node_lines) {
    std::lock_guard<std::mutex> lock(mtx_);
    scene_tree_lines_ = node_lines;
  }

  void add_monitor_frame(const std::vector<double> &values) {
    std::lock_guard<std::mutex> lock(mtx_);
    monitors_.push_back({values, now_ms()});
    if (monitors_.size() > MAX_MONITORS)
      monitors_.erase(monitors_.begin());
  }

  size_t stack_size() {
    std::lock_guard<std::mutex> lock(mtx_);
    return stack_.size();
  }

  std::string get_log_text(size_t limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t start =
        (limit >= log_buffer_.size()) ? 0 : log_buffer_.size() - limit;
    for (size_t i = start; i < log_buffer_.size(); i++) {
      auto &e = log_buffer_[i];
      oss << "[" << ms_to_time(e.timestamp_ms) << "] "
          << (e.is_error ? "[ERROR] " : "[INFO] ") << e.text << "\n";
    }
    return oss.str();
  }

  size_t log_count() {
    std::lock_guard<std::mutex> lock(mtx_);
    return log_buffer_.size();
  }

  std::string get_log_text_since(size_t since_count, bool errors_only) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t start =
        (since_count >= log_buffer_.size()) ? log_buffer_.size() : since_count;
    for (size_t i = start; i < log_buffer_.size(); i++) {
      auto &e = log_buffer_[i];
      if (errors_only && !e.is_error)
        continue;
      oss << "[" << ms_to_time(e.timestamp_ms) << "] "
          << (e.is_error ? "[ERROR] " : "[INFO] ") << e.text << "\n";
    }
    return oss.str();
  }

  std::string get_log_text_since_success(size_t since_count) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t start =
        (since_count >= log_buffer_.size()) ? log_buffer_.size() : since_count;
    for (size_t i = start; i < log_buffer_.size(); i++) {
      auto &e = log_buffer_[i];
      if (e.is_error)
        continue;
      oss << "[" << ms_to_time(e.timestamp_ms) << "] "
          << "[INFO] " << e.text << "\n";
    }
    return oss.str();
  }

  std::string get_errors_text(size_t limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t start = (limit >= errors_.size()) ? 0 : errors_.size() - limit;
    for (size_t i = start; i < errors_.size(); i++) {
      auto &e = errors_[i];
      oss << "[" << (e.is_warning ? "WARNING" : "ERROR") << "] "
          << std::setfill('0') << std::setw(2) << e.hr << ":"
          << std::setfill('0') << std::setw(2) << e.min << ":"
          << std::setfill('0') << std::setw(2) << e.sec << " - "
          << e.source_file << ":" << e.source_line << " @ " << e.source_func
          << "(): " << e.error << "\n";
      if (!e.error_descr.empty())
        oss << "  " << e.error_descr << "\n";
      if (!e.stack_files.empty()) {
        oss << "  Stack:\n";
        for (size_t j = 0; j < e.stack_files.size(); j++)
          oss << "    " << e.stack_files[j] << ":" << e.stack_lines[j] << " @ "
              << e.stack_funcs[j] << "()\n";
      }
      oss << "  ───\n\n";
    }
    return oss.str();
  }

  std::string get_game_output_text(size_t limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    size_t start =
        (limit >= game_output_.size()) ? 0 : game_output_.size() - limit;
    for (size_t i = start; i < game_output_.size(); i++) {
      auto &e = game_output_[i];
      std::string pfx =
          e.type == 0 ? "[STD] " : (e.type == 1 ? "[ERR] " : "[RICH] ");
      oss << pfx << e.text << "\n";
    }
    return oss.str();
  }

  std::string get_stack_dump_text() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    oss << "Stack Trace (" << stack_.size() << " frames):\n";
    for (size_t i = 0; i < stack_.size(); i++)
      oss << "  #" << i << " " << stack_[i].file << ":" << stack_[i].line
          << " @ " << stack_[i].func << "()\n";
    return oss.str();
  }

  std::string get_scene_tree_text() {
    std::lock_guard<std::mutex> lock(mtx_);
    std::ostringstream oss;
    for (auto &line : scene_tree_lines_)
      oss << line << "\n";
    return oss.str();
  }

  std::string get_monitors_text(size_t count) {
    std::lock_guard<std::mutex> lock(mtx_);
    static const char *names[] = {"FPS",
                                  "Time Process (ms)",
                                  "Time Physics (ms)",
                                  "Time Navigation (ms)",
                                  "Static Memory (MB)",
                                  "Static Memory Max (MB)",
                                  "Message Buffer Max (MB)",
                                  "Objects",
                                  "Resources",
                                  "Nodes",
                                  "Orphan Nodes",
                                  "Render Objects/Frame",
                                  "Render Primitives/Frame",
                                  "Render Draw Calls/Frame",
                                  "Video Mem Used (MB)",
                                  "Texture Mem Used (MB)",
                                  "Buffer Mem Used (MB)",
                                  "Physics 2D Active Objects",
                                  "Physics 2D Collision Pairs",
                                  "Physics 2D Islands",
                                  "Physics 3D Active Objects",
                                  "Physics 3D Collision Pairs",
                                  "Physics 3D Islands",
                                  "Audio Output Latency (ms)",
                                  "Nav Active Maps",
                                  "Nav Regions",
                                  "Nav Agents",
                                  "Nav Links",
                                  "Nav Polygons",
                                  "Nav Edges",
                                  "Nav Edge Merges",
                                  "Nav Edge Connections",
                                  "Nav Edge Frees",
                                  "Nav Obstacles"};
    std::ostringstream oss;
    size_t start = (count >= monitors_.size()) ? 0 : monitors_.size() - count;
    for (size_t i = start; i < monitors_.size(); i++) {
      oss << "Monitor Frame #" << (i + 1) << ":\n";
      size_t n = std::min(monitors_[i].values.size(),
                          sizeof(names) / sizeof(names[0]));
      for (size_t j = 0; j < n; j++)
        oss << "  " << names[j] << ": " << monitors_[i].values[j] << "\n";
    }
    return oss.str();
  }

  std::string get_session_info_text() {
    std::lock_guard<std::mutex> lock(mtx_);
    bool active = capture_session_active();
    bool breaked = capture_session_breaked();
    size_t session_count = 0;
    auto *plugin = DebugCapturePlugin::get_instance();
    if (plugin)
      session_count = plugin->get_session_ids().size();
    std::ostringstream oss;
    oss << "Debug Session:\n";
    oss << "  Active: " << (active ? "true" : "false") << "\n";
    oss << "  Breaked: " << (breaked ? "true" : "false") << "\n";
    oss << "  Running: " << (active && !breaked ? "true" : "false") << "\n";
    oss << "  Sessions: " << session_count << "\n";
    return oss.str();
  }

private:
  DebuggerCapture() = default;
  static uint64_t now_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
  }
  static std::string ms_to_time(uint64_t ms) {
    time_t sec = static_cast<time_t>(ms / 1000);
    struct tm *ti = localtime(&sec);
    if (!ti)
      return "??:??:??";
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min,
             ti->tm_sec);
    return std::string(buf);
  }

  std::vector<LogEntry> log_buffer_;
  std::vector<ErrorEntry> errors_;
  std::vector<GameOutputEntry> game_output_;
  std::vector<StackFrame> stack_;
  std::vector<std::string> scene_tree_lines_;
  std::vector<MonitorFrame> monitors_;
  std::mutex mtx_;
  static constexpr size_t MAX_LOG = 2000;
  static constexpr size_t MAX_ERRORS = 500;
  static constexpr size_t MAX_GAME_OUTPUT = 2000;
  static constexpr size_t MAX_MONITORS = 500;
};

struct RenameHint {
  const char *old_name;
  const char *new_name;
};

const RenameHint RENAME_HINTS[] = {
    {"frames", "sprite_frames"},
    {"cast_to", "target_position"},
    {"rect_position", "position"},
    {"rect_global_position", "global_position"},
    {"rect_size", "size"},
    {"rect_min_size", "custom_minimum_size"},
    {"rect_rotation", "rotation"},
    {"rect_scale", "scale"},
    {"rect_pivot_offset", "pivot_offset"},
    {"translation", "position"},
};

constexpr const char INVALID_ACCESS_MARKER[] =
    "Invalid access to property or key '";

std::string append_rename_hint(const std::string &error_text) {
  size_t marker_pos = error_text.find(INVALID_ACCESS_MARKER);
  if (marker_pos == std::string::npos) {
    return error_text;
  }
  size_t key_start = marker_pos + sizeof(INVALID_ACCESS_MARKER) - 1;
  size_t key_end = error_text.find('\'', key_start);
  if (key_end == std::string::npos) {
    return error_text;
  }
  std::string key = error_text.substr(key_start, key_end - key_start);
  constexpr const char *BASE_TYPE_SUFFIX = " on a base object of type '";
  size_t suffix_pos = error_text.find(BASE_TYPE_SUFFIX, key_end);
  if (suffix_pos != std::string::npos) {
    size_t type_start = suffix_pos + sizeof(BASE_TYPE_SUFFIX) - 1;
    size_t type_end = error_text.find('\'', type_start);
    if (type_end != std::string::npos) {
      std::string base_type =
          error_text.substr(type_start, type_end - type_start);
      bool is_packed_array =
          base_type.compare(0, 6, "Packed") == 0 && base_type.size() >= 11 &&
          base_type.compare(base_type.size() - 5, 5, "Array") == 0;
      if (base_type == "Dictionary" || base_type == "Array" ||
          is_packed_array) {
        return error_text;
      }
    }
  }
  for (const RenameHint &hint : RENAME_HINTS) {
    if (key != hint.old_name) {
      continue;
    }
    std::string hinted = error_text;
    if (!hinted.empty() && hinted.back() != '\n') {
      hinted += '\n';
    }
    hinted += "Hint: did you mean '";
    hinted += hint.new_name;
    hinted += "'? The old Godot 3 name '";
    hinted += hint.old_name;
    hinted += "' was renamed in Godot 4.";
    return hinted;
  }
  return error_text;
}

} // namespace

void capture_add_log_entry(const std::string &text, bool is_error) {
  DebuggerCapture::instance().add_log_entry(text, is_error);
}
std::string capture_get_log_text(size_t limit) {
  return DebuggerCapture::instance().get_log_text(limit);
}
std::string capture_get_errors_text(size_t limit) {
  return DebuggerCapture::instance().get_errors_text(limit);
}
std::string capture_get_game_output_text(size_t limit) {
  return DebuggerCapture::instance().get_game_output_text(limit);
}
std::string capture_get_stack_dump_text() {
  return DebuggerCapture::instance().get_stack_dump_text();
}
size_t capture_stack_size() { return DebuggerCapture::instance().stack_size(); }
std::string capture_get_scene_tree_text() {
  return DebuggerCapture::instance().get_scene_tree_text();
}
std::string capture_get_monitors_text(size_t count) {
  return DebuggerCapture::instance().get_monitors_text(count);
}
std::string capture_get_session_info_text() {
  return DebuggerCapture::instance().get_session_info_text();
}
bool capture_session_active() {
  auto *plugin = DebugCapturePlugin::get_instance();
  if (!plugin)
    return false;
  for (int32_t id : plugin->get_session_ids()) {
    auto s = plugin->get_session(id);
    if (s.is_valid() && s->is_active())
      return true;
  }
  return false;
}
bool capture_session_breaked() {
  auto *plugin = DebugCapturePlugin::get_instance();
  if (!plugin)
    return false;
  for (int32_t id : plugin->get_session_ids()) {
    auto s = plugin->get_session(id);
    if (s.is_valid() && s->is_breaked())
      return true;
  }
  return false;
}
size_t capture_log_count() { return DebuggerCapture::instance().log_count(); }
std::string capture_new_error_text(size_t since_count) {
  return append_rename_hint(
      DebuggerCapture::instance().get_log_text_since(since_count, true));
}
std::string capture_new_output_text(size_t since_count) {
  return DebuggerCapture::instance().get_log_text_since_success(since_count);
}

std::atomic<DebugCapturePlugin *> DebugCapturePlugin::s_instance{nullptr};

void OutputCaptureLogger::_log_error(const godot::String &p_function,
                                     const godot::String &p_file,
                                     int32_t p_line,
                                     const godot::String &p_code,
                                     const godot::String &p_rationale, bool,
                                     int32_t,
                                     const godot::TypedArray<
                                         godot::Ref<godot::ScriptBacktrace>> &) {
  std::string text(p_file.utf8().ptr());
  text += ":";
  text += std::to_string(p_line);
  text += " - ";
  text += p_rationale.utf8().ptr();
  if (p_code.length() > 0) {
    text += " (";
    text += p_code.utf8().ptr();
    text += ")";
  }
  godot_autopilot::debugger_ops::capture_add_log_entry(text, true);
}

void OutputCaptureLogger::_log_message(const godot::String &p_message,
                                       bool p_error) {
  godot_autopilot::debugger_ops::capture_add_log_entry(
      p_message.utf8().ptr(), p_error);
}

bool DebugCapturePlugin::_has_capture(const godot::String &p_name) const {
  std::string n = p_name.utf8().ptr();
  return n == "gda";
}

void DebugCapturePlugin::_setup_session(int32_t p_session_id) {
  {
    std::lock_guard<std::mutex> lock(session_mtx_);
    session_ids_.push_back(p_session_id);
    ready_session_ids_.erase(std::remove(ready_session_ids_.begin(),
                                         ready_session_ids_.end(),
                                         p_session_id),
                             ready_session_ids_.end());
  }
  session_ = get_session(p_session_id);
  if (session_.is_valid()) {
    godot_autopilot::debugger_ops::capture_add_log_entry(
        "Debug session started", false);
  }
}

bool DebugCapturePlugin::_capture(const godot::String &p_message,
                                  const godot::Array &p_data,
                                  int32_t p_session_id) {
  std::string msg = p_message.utf8().ptr();

  if (msg == std::string(godot_autopilot::GDA_MSG_READY)) {
    bool newly_ready = false;
    {
      std::lock_guard<std::mutex> lock(session_mtx_);
      if (std::find(ready_session_ids_.begin(), ready_session_ids_.end(),
                    p_session_id) == ready_session_ids_.end()) {
        ready_session_ids_.push_back(p_session_id);
        newly_ready = true;
      }
    }
    if (newly_ready) {
      godot_autopilot::runtime_ops::run_channel_self_check(p_session_id);
    }
    return true;
  }

  if (msg == std::string(godot_autopilot::GDA_MSG_RESPONSE)) {
    if (p_data.size() >= 1) {
      godot::String payload = p_data[0];
      godot_autopilot::runtime_ops::handle_game_response(
          std::string(payload.utf8().ptr()));
    }
    return true;
  }

  return false;
}

namespace {

mcp::JsonValue capture_note_for_empty_result() {
  return mcp::JsonValue(
      capture_session_active()
          ? "session active but the game reported no data — verify the game "
            "project loads the godot-autopilot extension; fall back to "
            "get_game_log_entries for the game process log"
          : "no active debug session — start the game with "
            "play_editor_current_scene; game errors/output/scene-tree are now "
            "fetched over the runtime channel (game must load the "
            "godot-autopilot extension)");
}

const char *plugin_log_level_name(LogLevel level) {
  switch (level) {
  case LogLevel::Debug:
    return "debug";
  case LogLevel::Info:
    return "info";
  case LogLevel::Warning:
    return "warning";
  case LogLevel::Error:
    return "error";
  default:
    return "unknown";
  }
}

const char *plugin_log_category_name(LogCategory category) {
  switch (category) {
  case LogCategory::System:
    return "system";
  case LogCategory::Transport:
    return "transport";
  case LogCategory::Tools:
    return "tools";
  case LogCategory::Resources:
    return "resources";
  case LogCategory::Prompts:
    return "prompts";
  default:
    return "unknown";
  }
}

bool plugin_log_entry_matches(const godot_autopilot::LogEntry &entry,
                              const LogSystem::Query &query) {
  if (entry.level < query.min_level)
    return false;
  if (query.category.has_value() && entry.category != *query.category)
    return false;
  if (!query.filter_text.empty()) {
    auto it =
        std::search(entry.message.begin(), entry.message.end(),
                    query.filter_text.begin(), query.filter_text.end(),
                    [](char a, char b) {
                      return std::tolower(static_cast<unsigned char>(a)) ==
                             std::tolower(static_cast<unsigned char>(b));
                    });
    if (it == entry.message.end())
      return false;
  }
  return true;
}

} // namespace

mcp::JsonValue handle_output_get_log(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_log called");
  size_t limit = 50;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = mcp::JsonValue(DebuggerCapture::instance().get_log_text(limit));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_log completed");
  return r;
}

mcp::JsonValue handle_plugin_log_get(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_plugin_log called");
  constexpr int64_t DEFAULT_LIMIT = 100;
  constexpr int64_t MAX_LIMIT = 1000;
  int64_t limit = DEFAULT_LIMIT;
  if (auto *l = args.Find("limit")) {
    if (!l->IsInt())
      return util::error_json("invalid parameter: limit must be an integer");
    limit = l->GetInt();
    if (limit > MAX_LIMIT)
      limit = MAX_LIMIT;
    if (limit < 0)
      limit = 0;
  }

  LogSystem::Query query;
  if (auto *l = args.Find("level")) {
    if (!l->IsString())
      return util::error_json("invalid parameter: level must be a string");
    const std::string level = l->GetString();
    if (level == "debug")
      query.min_level = LogLevel::Debug;
    else if (level == "info")
      query.min_level = LogLevel::Info;
    else if (level == "warning")
      query.min_level = LogLevel::Warning;
    else if (level == "error")
      query.min_level = LogLevel::Error;
    else
      return util::error_json(
          "invalid level '" + level +
          "': expected one of debug, info, warning, error");
  }
  if (auto *c = args.Find("category")) {
    if (!c->IsString())
      return util::error_json("invalid parameter: category must be a string");
    const std::string category = c->GetString();
    if (category == "system")
      query.category = LogCategory::System;
    else if (category == "transport")
      query.category = LogCategory::Transport;
    else if (category == "tools")
      query.category = LogCategory::Tools;
    else if (category == "resources")
      query.category = LogCategory::Resources;
    else if (category == "prompts")
      query.category = LogCategory::Prompts;
    else
      return util::error_json(
          "invalid category '" + category +
          "': expected one of system, transport, tools, resources, prompts");
  }
  if (auto *f = args.Find("filter")) {
    if (!f->IsString())
      return util::error_json("invalid parameter: filter must be a string");
    query.filter_text = f->GetString();
  }

  LogSystem &log = LogSystem::instance();
  std::vector<godot_autopilot::LogEntry> entries;
  size_t next_index = log.next_index();
  if (auto *s = args.Find("since_index")) {
    if (!s->IsInt() || s->GetInt() < 0)
      return util::error_json(
          "invalid parameter: since_index must be a non-negative integer");
    entries = log.query_from(static_cast<size_t>(s->GetInt()), &next_index);
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&query](const godot_autopilot::LogEntry &e) {
                                   return !plugin_log_entry_matches(e, query);
                                 }),
                  entries.end());
    if (static_cast<int64_t>(entries.size()) > limit) {
      entries.resize(static_cast<size_t>(limit));
      if (!entries.empty())
        next_index = entries.back().serial + 1;
    }
  } else {
    entries = log.query(query);
    if (static_cast<int64_t>(entries.size()) > limit) {
      entries.erase(entries.begin(),
                    entries.end() - static_cast<size_t>(limit));
    }
    if (!entries.empty())
      next_index = entries.back().serial + 1;
  }

  mcp::JsonValue arr(mcp::JsonValue::array_tag);
  for (const auto &entry : entries) {
    mcp::JsonValue item(mcp::JsonValue::object_tag);
    item["serial"] = mcp::JsonValue(static_cast<int64_t>(entry.serial));
    std::time_t tt = std::chrono::system_clock::to_time_t(entry.timestamp);
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&tt));
    item["timestamp"] = mcp::JsonValue(buf);
    item["level"] = mcp::JsonValue(plugin_log_level_name(entry.level));
    item["category"] = mcp::JsonValue(plugin_log_category_name(entry.category));
    item["message"] = mcp::JsonValue(entry.message);
    arr.PushBack(std::move(item));
  }

  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["entries"] = std::move(arr);
  result["count"] = mcp::JsonValue(static_cast<int64_t>(entries.size()));
  result["next_index"] = mcp::JsonValue(static_cast<int64_t>(next_index));
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] = std::move(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_plugin_log completed");
  return r;
}

mcp::JsonValue handle_debugger_get_errors(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_errors called");
  size_t limit = 20;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  if (capture_session_active()) {
    mcp::JsonValue params(mcp::JsonValue::object_tag);
    params["limit"] = mcp::JsonValue(static_cast<int64_t>(limit));
    return runtime_ops::handle_gda_send("get_errors", params, 5000);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  std::string errors_text = DebuggerCapture::instance().get_errors_text(limit);
  r["result"] = mcp::JsonValue(errors_text);
  if (errors_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_errors completed");
  return r;
}

mcp::JsonValue handle_debugger_get_output(const mcp::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_output called");
  size_t limit = 50;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  if (capture_session_active()) {
    mcp::JsonValue params(mcp::JsonValue::object_tag);
    params["limit"] = mcp::JsonValue(static_cast<int64_t>(limit));
    return runtime_ops::handle_gda_send("get_output", params, 5000);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  std::string output_text =
      DebuggerCapture::instance().get_game_output_text(limit);
  r["result"] = mcp::JsonValue(output_text);
  if (output_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_output completed");
  return r;
}

mcp::JsonValue handle_debugger_get_scene_tree(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_scene_tree called");
  if (capture_session_active()) {
    mcp::JsonValue params(mcp::JsonValue::object_tag);
    return runtime_ops::handle_gda_send("get_tree", params, 5000);
  }
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  std::string scene_tree_text =
      DebuggerCapture::instance().get_scene_tree_text();
  r["result"] = mcp::JsonValue(scene_tree_text);
  if (scene_tree_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_scene_tree completed");
  return r;
}

mcp::JsonValue handle_debugger_get_session_info(const mcp::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_session_info called");
  mcp::JsonValue r(mcp::JsonValue::object_tag);
  r["result"] =
      mcp::JsonValue(DebuggerCapture::instance().get_session_info_text());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_session_info completed");
  return r;
}

void register_classes() {
  godot::ClassDB::register_class<OutputCaptureLogger>();
  godot::ClassDB::register_class<DebugCapturePlugin>();
}

::godot::Ref<OutputCaptureLogger> create_output_logger() {
  ::godot::Ref<OutputCaptureLogger> logger;
  logger.instantiate();
  return logger;
}

::godot::Ref<DebugCapturePlugin> create_debug_plugin() {
  ::godot::Ref<DebugCapturePlugin> plugin;
  plugin.instantiate();
  DebugCapturePlugin::s_instance.store(plugin.ptr(), std::memory_order_relaxed);
  return plugin;
}

} // namespace debugger_ops

} // namespace godot_autopilot
