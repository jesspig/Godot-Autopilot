#include "tools/domains/debugger/debugger_ops.hpp"
#include "core/observability/log_system.hpp"
#include "tools/base/debugger_access.hpp"
#include "runtime/gda_protocol.hpp"
#include "tools/base/runtime_ops.hpp"
#include "util/error_util.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <godot_cpp/classes/editor_debugger_plugin.hpp>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/script_editor.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/logger.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/script_backtrace.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
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

  godot_autopilot::util::JsonValue get_grouped_errors(size_t group_limit) {
    std::lock_guard<std::mutex> lock(mtx_);
    struct Group {
      size_t count = 0;
      size_t first_index = 0;
      size_t last_index = 0;
      size_t sample = 0;
    };
    std::unordered_map<std::string, size_t> index_by_key;
    std::vector<Group> groups;
    auto time_text = [](const ErrorEntry &e) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", e.hr, e.min, e.sec,
               e.msec);
      return std::string(buf);
    };
    for (size_t i = 0; i < errors_.size(); i++) {
      const ErrorEntry &e = errors_[i];
      const std::string key = e.source_file + "\n" + e.source_func + "\n" +
                              std::to_string(e.source_line) + "\n" + e.error;
      auto it = index_by_key.find(key);
      if (it == index_by_key.end()) {
        Group group;
        group.count = 1;
        group.first_index = i;
        group.last_index = i;
        group.sample = i;
        index_by_key.emplace(key, groups.size());
        groups.push_back(group);
      } else {
        Group &group = groups[it->second];
        group.count++;
        group.last_index = i;
      }
    }
    std::vector<size_t> order(groups.size());
    for (size_t i = 0; i < order.size(); i++)
      order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return groups[a].count > groups[b].count;
    });
    if (group_limit < order.size())
      order.resize(group_limit);
    godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
    for (size_t gi : order) {
      const ErrorEntry &sample = errors_[groups[gi].sample];
      godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
      item["count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(groups[gi].count));
      item["first_time"] =
          godot_autopilot::util::JsonValue(time_text(errors_[groups[gi].first_index]));
      item["last_time"] =
          godot_autopilot::util::JsonValue(time_text(errors_[groups[gi].last_index]));
      item["file"] = godot_autopilot::util::JsonValue(sample.source_file);
      item["func"] = godot_autopilot::util::JsonValue(sample.source_func);
      item["line"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(sample.source_line));
      item["error"] = godot_autopilot::util::JsonValue(sample.error);
      item["descr"] = godot_autopilot::util::JsonValue(sample.error_descr);
      item["is_warning"] = godot_autopilot::util::JsonValue(sample.is_warning);
      godot_autopilot::util::JsonValue stack(godot_autopilot::util::JsonValue::array_tag);
      const size_t frames =
          std::min<size_t>(sample.stack_files.size(), 8);
      for (size_t j = 0; j < frames; j++) {
        stack.PushBack(godot_autopilot::util::JsonValue(sample.stack_files[j] + ":" +
                                      std::to_string(sample.stack_lines[j]) +
                                      " @ " + sample.stack_funcs[j] + "()"));
      }
      item["stack"] = std::move(stack);
      arr.PushBack(std::move(item));
    }
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["grouped"] = godot_autopilot::util::JsonValue(true);
    result["groups"] = std::move(arr);
    result["total_errors"] =
        godot_autopilot::util::JsonValue(static_cast<int64_t>(errors_.size()));
    result["total_groups"] =
        godot_autopilot::util::JsonValue(static_cast<int64_t>(groups.size()));
    return result;
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
  start_hello_retry(p_session_id);
}

void DebugCapturePlugin::_bind_methods() {
  godot::ClassDB::bind_method(
      godot::D_METHOD("_on_hello_timer", "session_id"),
      &DebugCapturePlugin::_on_hello_timer);
}

SessionReadiness DebugCapturePlugin::readiness(int32_t id) const {
  std::lock_guard<std::mutex> lock(session_mtx_);
  auto it = readiness_.find(id);
  return it == readiness_.end() ? SessionReadiness{} : it->second;
}

void DebugCapturePlugin::send_hello(int32_t id) {
  godot::Ref<godot::EditorDebuggerSession> session;
  {
    std::lock_guard<std::mutex> lock(session_mtx_);
    if (std::find(session_ids_.begin(), session_ids_.end(), id) ==
        session_ids_.end())
      return;
  }
  session = get_session(id);
  if (session.is_null())
    return;
  godot::Array payload;
  session->send_message(
      godot::StringName(godot_autopilot::GDA_MSG_HELLO.data()), payload);
}

void DebugCapturePlugin::_on_hello_timer(int32_t session_id) {
  const SessionReadiness state = readiness(session_id);
  if (state.ready || !has_session(session_id)) {
    stop_hello_retry(session_id);
    return;
  }
  if (state.hello_attempts >= 32) {
    stop_hello_retry(session_id);
    return;
  }
  send_hello(session_id);
  {
    std::lock_guard<std::mutex> lock(session_mtx_);
    SessionReadiness &entry = readiness_[session_id];
    entry.hello_attempts += 1;
    entry.last_hello_ms = godot::Time::get_singleton()
                              ? godot::Time::get_singleton()->get_ticks_msec()
                              : 0;
  }
  auto *tree = godot::Engine::get_singleton()
                   ? godot::Engine::get_singleton()->get_main_loop()
                   : nullptr;
  auto *scene_tree = godot::Object::cast_to<godot::SceneTree>(tree);
  if (scene_tree == nullptr)
    return;
  godot::Ref<godot::SceneTreeTimer> timer =
      scene_tree->create_timer(0.25, true, false, true);
  if (timer.is_null())
    return;
  timer->connect("timeout",
                 godot::Callable(this, godot::StringName("_on_hello_timer"))
                     .bind(session_id));
  std::lock_guard<std::mutex> lock(session_mtx_);
  hello_timers_[session_id] = timer;
}

void DebugCapturePlugin::start_hello_retry(int32_t id) {
  {
    std::lock_guard<std::mutex> lock(session_mtx_);
    SessionReadiness &entry = readiness_[id];
    entry = SessionReadiness{};
  }
  _on_hello_timer(id);
}

void DebugCapturePlugin::stop_hello_retry(int32_t id) {
  godot::Ref<godot::SceneTreeTimer> timer;
  {
    std::lock_guard<std::mutex> lock(session_mtx_);
    auto it = hello_timers_.find(id);
    if (it != hello_timers_.end()) {
      timer = it->second;
      hello_timers_.erase(it);
    }
  }
  timer.unref();
}

void DebugCapturePlugin::forget_ready_session(int32_t session_id) {
  std::lock_guard<std::mutex> lock(session_mtx_);
  ready_session_ids_.erase(std::remove(ready_session_ids_.begin(),
                                       ready_session_ids_.end(), session_id),
                           ready_session_ids_.end());
  scene_ready_session_ids_.erase(
      std::remove(scene_ready_session_ids_.begin(),
                  scene_ready_session_ids_.end(), session_id),
      scene_ready_session_ids_.end());
  readiness_.erase(session_id);
  hello_timers_.erase(session_id);
}

bool DebugCapturePlugin::_capture(const godot::String &p_message,
                                  const godot::Array &p_data,
                                  int32_t p_session_id) {
  std::string msg = p_message.utf8().ptr();

  if (msg == std::string(godot_autopilot::GDA_MSG_READY)) {
    bool scene_ready = true;
    if (p_data.size() >= 1) {
      const std::string ready_text =
          std::string(godot::String(p_data[0]).utf8().ptr());
      const godot_autopilot::util::JsonValue ready_body = godot_autopilot::util::JsonValue::Parse(ready_text);
      if (const godot_autopilot::util::JsonValue *flag =
              ready_body.Find(godot_autopilot::GDA_FIELD_SCENE_READY)) {
        if (flag->IsBool()) {
          scene_ready = flag->GetBool();
        }
      }
    }
    bool newly_ready = false;
    {
      std::lock_guard<std::mutex> lock(session_mtx_);
      if (std::find(ready_session_ids_.begin(), ready_session_ids_.end(),
                    p_session_id) == ready_session_ids_.end()) {
        ready_session_ids_.push_back(p_session_id);
        newly_ready = true;
      }
      SessionReadiness &entry = readiness_[p_session_id];
      entry.extension_responded = true;
      entry.ready = true;
      entry.scene_ready = scene_ready;
      if (p_data.size() >= 1) {
        const godot_autopilot::util::JsonValue ready_body =
            godot_autopilot::util::JsonValue::Parse(util::to_std(godot::String(p_data[0])));
        if (const godot_autopilot::util::JsonValue *bridge =
                ready_body.Find(std::string("csharp_bridge"))) {
          if (bridge->IsString())
            entry.csharp_bridge = bridge->GetString();
        }
      }
    }
    stop_hello_retry(p_session_id);
    if (newly_ready) {
      godot_autopilot::runtime_ops::run_channel_self_check(p_session_id);
    }
    if (scene_ready && mark_session_scene_ready(p_session_id)) {
      godot_autopilot::runtime_ops::on_session_ready(p_session_id);
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

godot_autopilot::util::JsonValue capture_note_for_empty_result() {
  return godot_autopilot::util::JsonValue(
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

godot_autopilot::util::JsonValue handle_output_get_log(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_log called");
  size_t limit = 50;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = godot_autopilot::util::JsonValue(DebuggerCapture::instance().get_log_text(limit));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_log completed");
  return r;
}

godot_autopilot::util::JsonValue handle_plugin_log_get(const godot_autopilot::util::JsonValue &args) {
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

  godot_autopilot::util::JsonValue arr(godot_autopilot::util::JsonValue::array_tag);
  for (const auto &entry : entries) {
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    item["serial"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(entry.serial));
    std::time_t tt = std::chrono::system_clock::to_time_t(entry.timestamp);
    char buf[32] = {0};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&tt));
    item["timestamp"] = godot_autopilot::util::JsonValue(buf);
    item["level"] = godot_autopilot::util::JsonValue(plugin_log_level_name(entry.level));
    item["category"] = godot_autopilot::util::JsonValue(plugin_log_category_name(entry.category));
    item["message"] = godot_autopilot::util::JsonValue(entry.message);
    arr.PushBack(std::move(item));
  }

  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["entries"] = std::move(arr);
  result["count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(entries.size()));
  result["next_index"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(next_index));
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] = std::move(result);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_plugin_log completed");
  return r;
}

godot_autopilot::util::JsonValue handle_debugger_get_errors(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_errors called");
  size_t limit = 20;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  bool grouped = false;
  if (auto *g = args.Find("group")) {
    if (!g->IsBool())
      return util::error_json("invalid parameter: group must be a boolean");
    grouped = g->GetBool();
  }
  int64_t since_seq = 0;
  bool has_since_seq = false;
  if (auto *s = args.Find("since_seq")) {
    if (!s->IsInt() || s->GetInt() < 0)
      return util::error_json(
          "invalid parameter: since_seq must be a non-negative integer");
    since_seq = s->GetInt();
    has_since_seq = true;
  }
  if (capture_session_active()) {
    godot_autopilot::util::JsonValue params(godot_autopilot::util::JsonValue::object_tag);
    params["limit"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(limit));
    if (grouped)
      params["group"] = godot_autopilot::util::JsonValue(true);
    if (has_since_seq)
      params["since_seq"] = godot_autopilot::util::JsonValue(since_seq);
    return runtime_ops::handle_gda_send("get_errors", params, 5000);
  }
  if (grouped) {
    godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
    r["result"] = DebuggerCapture::instance().get_grouped_errors(limit);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "get_debugger_errors completed (grouped)");
    return r;
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  std::string errors_text = DebuggerCapture::instance().get_errors_text(limit);
  r["result"] = godot_autopilot::util::JsonValue(errors_text);
  if (errors_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_errors completed");
  return r;
}

godot_autopilot::util::JsonValue handle_debugger_get_output(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_output called");
  size_t limit = 50;
  if (auto *l = args.Find("limit")) {
    if (l->IsInt())
      limit = static_cast<size_t>(l->GetInt());
  }
  if (capture_session_active()) {
    godot_autopilot::util::JsonValue params(godot_autopilot::util::JsonValue::object_tag);
    params["limit"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(limit));
    return runtime_ops::handle_gda_send("get_output", params, 5000);
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  std::string output_text =
      DebuggerCapture::instance().get_game_output_text(limit);
  r["result"] = godot_autopilot::util::JsonValue(output_text);
  if (output_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_output completed");
  return r;
}

godot_autopilot::util::JsonValue handle_debugger_get_scene_tree(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_scene_tree called");
  if (capture_session_active()) {
    godot_autopilot::util::JsonValue params(godot_autopilot::util::JsonValue::object_tag);
    return runtime_ops::handle_gda_send("get_tree", params, 5000);
  }
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  std::string scene_tree_text =
      DebuggerCapture::instance().get_scene_tree_text();
  r["result"] = godot_autopilot::util::JsonValue(scene_tree_text);
  if (scene_tree_text.empty())
    r["note"] = capture_note_for_empty_result();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_scene_tree completed");
  return r;
}

godot_autopilot::util::JsonValue handle_debugger_get_session_info(const godot_autopilot::util::JsonValue &) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_session_info called");
  godot_autopilot::util::JsonValue r(godot_autopilot::util::JsonValue::object_tag);
  r["result"] =
      godot_autopilot::util::JsonValue(DebuggerCapture::instance().get_session_info_text());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_session_info completed");
  return r;
}

godot_autopilot::util::JsonValue handle_debugger_stream(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "get_debugger_stream called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    return util::error_json("missing required parameter: action");
  }
  const std::string action = ap->GetString();
  std::string source;
  if (auto *sp = args.Find("source")) {
    if (!sp->IsString()) {
      return util::error_json("invalid parameter: source must be a string");
    }
    source = sp->GetString();
  }
  if (!source.empty() && source != "editor" && source != "game" &&
      source != "plugin") {
    return util::error_json(
        "invalid source '" + source +
        "': expected editor, game or plugin");
  }
  if (action == "log") {
    if (source == "plugin") {
      return handle_plugin_log_get(args);
    }
    if (source == "game") {
      return handle_debugger_get_output(args);
    }
    return handle_output_get_log(args);
  }
  if (action == "output") {
    if (source == "plugin" || source == "editor") {
      return util::error_json(
          "source '" + source +
          "' is not available for action 'output': use action 'log' with "
          "source 'editor' for the editor engine log, or source 'plugin' for "
          "the plugin LogSystem buffer");
    }
    return handle_debugger_get_output(args);
  }
  if (action == "errors") {
    if (source == "plugin" || source == "editor") {
      return util::error_json(
          "source '" + source +
          "' is not available for action 'errors': use action 'log' with "
          "source 'editor' for the editor engine log, or source 'plugin' for "
          "the plugin LogSystem buffer");
    }
    return handle_debugger_get_errors(args);
  }
  if (action == "session") {
    return handle_debugger_get_session_info(args);
  }
  return util::error_json("unknown action '" + action +
                          "': expected log, output, errors or session");
}

namespace {

godot_autopilot::util::JsonValue no_debugger_session_error() {
  godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
  error["error"] = godot_autopilot::util::JsonValue("no active debugger session");
  error["code"] = godot_autopilot::util::JsonValue("no_active_debugger_session");
  error["hint"] = godot_autopilot::util::JsonValue(
      "start the game from the editor (editor_session action=play_main or "
      "editor_scene_control action=play) and retry");
  return error;
}

godot::Ref<godot::EditorDebuggerSession> find_active_debugger_session(
    int32_t *out_session_id) {
  auto *plugin = DebugCapturePlugin::get_instance();
  if (!plugin)
    return {};
  for (int32_t id : plugin->get_session_ids()) {
    godot::Ref<godot::EditorDebuggerSession> session = plugin->get_session(id);
    if (session.is_valid() && session->is_active()) {
      if (out_session_id)
        *out_session_id = id;
      return session;
    }
  }
  return {};
}

godot_autopilot::util::JsonValue script_breakpoints_json(godot::ScriptEditor *script_editor) {
  godot::PackedStringArray raw = script_editor->get_breakpoints();
  godot_autopilot::util::JsonValue entries(godot_autopilot::util::JsonValue::array_tag);
  for (int i = 0; i < raw.size(); i++) {
    const godot::String entry = raw[i];
    godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
    item["raw"] = godot_autopilot::util::JsonValue(util::to_std(entry));
    const int colon = entry.rfind(":");
    if (colon > 0) {
      item["path"] = godot_autopilot::util::JsonValue(util::to_std(entry.substr(0, colon)));
      item["line"] = godot_autopilot::util::JsonValue(
          static_cast<int64_t>(entry.substr(colon + 1).to_int()));
      item["enabled"] = godot_autopilot::util::JsonValue(true);
    }
    entries.PushBack(std::move(item));
  }
  godot_autopilot::util::JsonValue payload(godot_autopilot::util::JsonValue::object_tag);
  payload["breakpoints"] = std::move(entries);
  payload["count"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(raw.size()));
  return payload;
}

} // namespace

godot_autopilot::util::JsonValue handle_debugger_breakpoints(const godot_autopilot::util::JsonValue &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "debugger_breakpoints called");
  auto *ap = args.Find("action");
  if (!ap || !ap->IsString()) {
    return util::error_json("missing required parameter: action");
  }
  const std::string action = ap->GetString();
  auto *editor = godot::EditorInterface::get_singleton();
  if (!editor) {
    return util::error_json("EditorInterface not available");
  }
  godot::ScriptEditor *script_editor = editor->get_script_editor();
  if (!script_editor) {
    return util::error_json("ScriptEditor not available");
  }
  if (action == "list") {
    godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
    result["result"] = script_breakpoints_json(script_editor);
    LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                              "debugger_breakpoints list completed");
    return result;
  }
  if (action != "set" && action != "clear") {
    return util::error_json(
        "unknown action '" + action + "': expected list, set or clear");
  }
  auto *pp = args.Find("path");
  if (!pp || !pp->IsString() || pp->GetString().empty()) {
    return util::error_json(
        "missing required parameter: path (script res:// path)");
  }
  auto *lp = args.Find("line");
  if (!lp || !lp->IsInt() || lp->GetInt() < 1) {
    return util::error_json(
        "missing required parameter: line (1-based integer)");
  }
  const std::string path = pp->GetString();
  const int64_t line = lp->GetInt();
  int32_t session_id = 0;
  godot::Ref<godot::EditorDebuggerSession> session =
      find_active_debugger_session(&session_id);
  if (session.is_null()) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::Tools,
                              "debugger_breakpoints " + action +
                                  " skipped: no active debugger session");
    return no_debugger_session_error();
  }
  const bool enabled = action == "set";
  session->set_breakpoint(godot::String(path.c_str()),
                          static_cast<int32_t>(line), enabled);
  godot_autopilot::util::JsonValue info = script_breakpoints_json(script_editor);
  info["path"] = godot_autopilot::util::JsonValue(path);
  info["line"] = godot_autopilot::util::JsonValue(line);
  info["enabled"] = godot_autopilot::util::JsonValue(enabled);
  info["session_id"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(session_id));
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["result"] = std::move(info);
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "debugger_breakpoints " + action + " completed: " +
                                path + ":" + std::to_string(line));
  return result;
}

SessionReadiness session_readiness(int32_t id) {
  auto *plugin = DebugCapturePlugin::get_instance();
  return plugin == nullptr ? SessionReadiness{} : plugin->readiness(id);
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
