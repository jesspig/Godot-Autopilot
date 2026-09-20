#include "log_persist.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <version.hpp>

#include "log_system.hpp"
#include "sanitize_policy.hpp"
#include "trace_recorder.hpp"

namespace godot_autopilot {

namespace {

const char *kLogDir = "user://godot_autopilot/logs";
const char *kTraceDir = "user://godot_autopilot/traces";

void append_persist_line(std::string &payload, const std::string &line) {
  payload += line;
  if (line.empty() || line.back() != '\n') {
    payload += '\n';
  }
}

bool append_persist_file(const std::string &path, const std::string &payload) {
  godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
      godot::String::utf8(path.c_str()), godot::FileAccess::READ_WRITE);
  if (file.is_null()) {
    file = godot::FileAccess::open(godot::String::utf8(path.c_str()),
                                   godot::FileAccess::WRITE);
  }
  if (file.is_null()) {
    return false;
  }
  file->seek_end();
  const bool stored = file->store_string(godot::String::utf8(payload.c_str()));
  file->close();
  return stored;
}

struct PruneEntry {
  uint64_t mtime = 0;
  uint64_t size = 0;
  godot::String name;
};

void prune_persist_dir(const godot::String &dir_path, const char *prefix, const char *suffix) {
  godot::Ref<godot::DirAccess> dir = godot::DirAccess::open(dir_path);
  if (dir.is_null()) {
    return;
  }
  if (dir->list_dir_begin() != godot::OK) {
    return;
  }
  std::vector<PruneEntry> files;
  uint64_t total = 0;
  godot::String entry = dir->get_next();
  while (entry != godot::String()) {
    if (entry != "." && entry != ".." && !dir->current_is_dir() &&
        entry.begins_with(prefix) && entry.ends_with(suffix)) {
      godot::String full = dir_path.path_join(entry);
      PruneEntry item;
      item.mtime = godot::FileAccess::get_modified_time(full);
      item.name = entry;
      godot::Ref<godot::FileAccess> probe =
          godot::FileAccess::open(full, godot::FileAccess::READ);
      if (!probe.is_null()) {
        int64_t len = probe->get_length();
        probe->close();
        if (len > 0) {
          item.size = static_cast<uint64_t>(len);
        }
      }
      total += item.size;
      files.push_back(item);
    }
    entry = dir->get_next();
  }
  dir->list_dir_end();
  std::sort(files.begin(), files.end(), [](const PruneEntry &a, const PruneEntry &b) {
    if (a.mtime != b.mtime) {
      return a.mtime < b.mtime;
    }
    return a.name < b.name;
  });
  while (!files.empty() && LogPersist::should_prune(files.size(), total)) {
    godot::String victim = dir_path.path_join(files.front().name);
    total -= files.front().size;
    files.erase(files.begin());
    if (godot::DirAccess::remove_absolute(victim) != godot::OK) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::Tools,
          "prune persist files: failed to remove " + std::string(victim.utf8().get_data()));
    }
  }
}

} // namespace

LogPersist &LogPersist::instance() {
  static LogPersist persist;
  return persist;
}

std::string LogPersist::level_name(LogLevel level) {
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

std::string LogPersist::category_name(LogCategory category) {
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

std::string LogPersist::format_human_line(int64_t wall_ms, const std::string &level,
                                          const std::string &category,
                                          const std::string &summary,
                                          const std::string &detail, bool include_detail) {
  std::string out = "[";
  out += std::to_string(wall_ms);
  out += "] [";
  out += level;
  out += "] [";
  out += category;
  out += "] ";
  out += summary;
  if (include_detail && !detail.empty()) {
    out += " | ";
    out += detail;
  }
  return out;
}

std::string LogPersist::session_stamp_from_ticks(uint64_t wall_ms) {
  std::time_t sec = static_cast<std::time_t>(wall_ms / 1000ULL);
  std::tm fields{};
#ifdef _WIN32
  gmtime_s(&fields, &sec);
#else
  gmtime_r(&sec, &fields);
#endif
  char buf[32];
  std::size_t len = std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &fields);
  if (len == 0) {
    return std::string("00000000_000000");
  }
  return std::string(buf, len);
}

bool LogPersist::should_prune(std::size_t file_count, uint64_t total_bytes) {
  return file_count > kMaxFiles || total_bytes > kMaxTotalBytes;
}

void LogPersist::init_session() {
  std::string id = TraceRecorder::new_trace_id();
  std::string stamp =
      session_stamp_from_ticks(static_cast<uint64_t>(TraceRecorder::wall_now_ms()));
  godot::DirAccess::make_dir_recursive_absolute(kLogDir);
  godot::DirAccess::make_dir_recursive_absolute(kTraceDir);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session_id_ = id;
    log_path_ = std::string(kLogDir) + "/gda-" + stamp + ".log";
    trace_path_ = std::string(kTraceDir) + "/trace-" + stamp + ".jsonl";
    write_failed_ = false;
  }
  rotate_on_init();
  std::string header = "{\"type\":\"session_start\",\"session_id\":\"";
  header += id;
  header += "\",\"gda_version\":\"";
  header += GDA_VERSION;
  header += "\",\"stamp\":\"";
  header += stamp;
  header += "\"}";
  enqueue_trace(header);
}

void LogPersist::enqueue_log(const std::string &line) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (log_buffer_.size() >= kBufferCap) {
    log_buffer_.erase(log_buffer_.begin());
  }
  log_buffer_.push_back(line);
}

void LogPersist::enqueue_trace(const std::string &line) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (trace_buffer_.size() >= kBufferCap) {
    trace_buffer_.erase(trace_buffer_.begin());
  }
  trace_buffer_.push_back(line);
}

void LogPersist::flush_on_main_thread() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (write_failed_) {
      std::vector<std::string> discarded_logs;
      std::vector<std::string> discarded_traces;
      discarded_logs.swap(log_buffer_);
      discarded_traces.swap(trace_buffer_);
      return;
    }
  }
  std::size_t log_cursor = 0;
  uint64_t trace_cursor = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    log_cursor = last_log_serial_;
    trace_cursor = last_trace_seq_;
  }
  if (LogSystem::instance().next_index() > log_cursor) {
    std::size_t next_index = log_cursor;
    std::vector<LogEntry> entries =
        LogSystem::instance().query_from(log_cursor, &next_index);
    for (const LogEntry &entry : entries) {
      const int64_t wall_ms = static_cast<int64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              entry.timestamp.time_since_epoch())
              .count());
      enqueue_log(format_human_line(wall_ms, level_name(entry.level),
                                    category_name(entry.category),
                                    entry.message, entry.detail, true));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    last_log_serial_ = next_index;
  }
  if (TraceRecorder::instance().next_seq() > trace_cursor) {
    uint64_t next_seq = trace_cursor;
    std::vector<TraceEvent> events =
        TraceRecorder::instance().query_since(trace_cursor, &next_seq);
    for (const TraceEvent &event : events) {
      enqueue_trace(TraceRecorder::to_json_line(event));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    last_trace_seq_ = next_seq;
  }
  std::vector<std::string> logs;
  std::vector<std::string> traces;
  std::string log_path;
  std::string trace_path;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    logs.swap(log_buffer_);
    traces.swap(trace_buffer_);
    log_path = log_path_;
    trace_path = trace_path_;
  }
  auto note_persist_failure = [this](const std::string &path) {
    bool first_failure = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      first_failure = !write_failed_;
      write_failed_ = true;
    }
    if (first_failure) {
      LogSystem::instance().log(
          LogLevel::Error, LogCategory::System,
          "log persist flush failed, persistence disabled for this session: " +
              path);
    }
  };
  if (!logs.empty() && !log_path.empty()) {
    std::string payload;
    for (const std::string &line : logs) {
      append_persist_line(payload, line);
    }
    if (!append_persist_file(log_path, payload)) {
      note_persist_failure(log_path);
    }
  }
  if (!traces.empty() && !trace_path.empty()) {
    std::string payload;
    for (const std::string &line : traces) {
      append_persist_line(payload, line);
    }
    if (!append_persist_file(trace_path, payload)) {
      note_persist_failure(trace_path);
    }
  }
}

std::string LogPersist::store_trace_image(const std::string &span_id,
                                          const std::string &kind,
                                          const std::string &base64_png) {
  if (sanitize_policy::enabled()) {
    return std::string();
  }
  std::string session;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = session_id_;
  }
  const std::string decoded = TraceRecorder::decode_base64(base64_png);
  if (decoded.empty()) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System, "trace image decode failed",
        "span=" + span_id + " kind=" + kind);
    return std::string();
  }
  const std::string dir = std::string(kTraceDir) + "/images";
  const std::string name =
      "trace-" + session + "-" + span_id + "-" + kind + ".png";
  const std::string path = dir + "/" + name;
  godot::DirAccess::make_dir_recursive_absolute(godot::String::utf8(dir.c_str()));
  godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
      godot::String::utf8(path.c_str()), godot::FileAccess::WRITE);
  if (file.is_null()) {
    LogSystem::instance().log_detailed(LogLevel::Error, LogCategory::System,
                                       "trace image store failed", path);
    return std::string();
  }
  file->store_buffer(reinterpret_cast<const uint8_t *>(decoded.data()),
                     static_cast<int64_t>(decoded.size()));
  const godot::Error write_err = file->get_error();
  file->close();
  if (write_err != godot::OK) {
    LogSystem::instance().log_detailed(LogLevel::Error, LogCategory::System,
                                       "trace image store failed", path);
    return std::string();
  }
  return "images/" + name;
}

void LogPersist::rotate_on_init() {
  prune_persist_dir(kLogDir, "gda-", ".log");
  prune_persist_dir(kTraceDir, "trace-", ".jsonl");
  prune_persist_dir(godot::String(kTraceDir) + "/images", "trace-", ".png");
}

std::string LogPersist::session_id() {
  std::lock_guard<std::mutex> lock(mutex_);
  return session_id_;
}

std::string LogPersist::log_path() {
  std::lock_guard<std::mutex> lock(mutex_);
  return log_path_;
}

std::string LogPersist::trace_path() {
  std::lock_guard<std::mutex> lock(mutex_);
  return trace_path_;
}

} // namespace godot_autopilot
