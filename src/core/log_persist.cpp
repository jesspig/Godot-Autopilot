#include "log_persist.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <version.hpp>

#include "core/project_store.hpp"
#include "core/storage_paths.hpp"
#include "log_system.hpp"
#include "monitor.hpp"
#include "sanitize_policy.hpp"
#include "trace_recorder.hpp"
#include "util/project_path.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kStorageRootEnvName = "GODOT_AUTOPILOT_STORAGE_ROOT";
constexpr const char *kProjectDirEnvName = "GODOT_AUTOPILOT_PROJECT_DIR";
constexpr const char *kLogDirEnvName = "GODOT_AUTOPILOT_LOG_DIR";
constexpr const char *kTraceDirEnvName = "GODOT_AUTOPILOT_TRACE_DIR";

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
    } else {
      LogPersist::instance().note_pruned(1);
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
  const std::string project_path = util::current_project_root_path();
  const std::string log_dir_path = StoragePaths::log_dir(project_path);
  const std::string trace_dir_path = StoragePaths::trace_dir(project_path);
  auto *os = godot::OS::get_singleton();
  if (os) {
    os->set_environment(kStorageRootEnvName,
                        godot::String::utf8(
                            ProjectStore::root_dir().c_str()));
    os->set_environment(kProjectDirEnvName,
                        godot::String::utf8(
                            StoragePaths::project_dir(project_path).c_str()));
    os->set_environment(kLogDirEnvName,
                        godot::String::utf8(log_dir_path.c_str()));
    os->set_environment(kTraceDirEnvName,
                        godot::String::utf8(trace_dir_path.c_str()));
  }
  if (log_dir_path.empty() || trace_dir_path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "GDA storage paths unavailable, log persistence disabled for this "
        "session");
  } else {
    godot::DirAccess::make_dir_recursive_absolute(
        godot::String::utf8(log_dir_path.c_str()));
    godot::DirAccess::make_dir_recursive_absolute(
        godot::String::utf8(trace_dir_path.c_str()));
  }
  const std::string trace_path =
      trace_dir_path.empty()
          ? std::string()
          : trace_dir_path + "/trace-" + stamp + ".jsonl";
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session_id_ = id;
    log_dir_ = log_dir_path;
    trace_dir_ = trace_dir_path;
    log_path_ = log_dir_path.empty()
                    ? std::string()
                    : log_dir_path + "/gda-" + stamp + ".log";
    trace_path_ = trace_path;
    trace_now_failure_reported_ = false;
    write_failed_ = false;
    last_log_serial_ = 0;
    last_trace_seq_ = 0;
  }
  if (trace_path.empty()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "trace path unavailable, trace persistence "
                              "disabled for this session");
  }
  reported_dropped_logs_.store(dropped_log_lines_.load());
  reported_dropped_traces_.store(dropped_trace_lines_.load());
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
    dropped_log_lines_.fetch_add(1, std::memory_order_relaxed);
  }
  log_buffer_.push_back(line);
}

void LogPersist::enqueue_trace(const std::string &line) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (trace_buffer_.size() >= kBufferCap) {
    trace_buffer_.erase(trace_buffer_.begin());
    dropped_trace_lines_.fetch_add(1, std::memory_order_relaxed);
  }
  trace_buffer_.push_back(line);
}

void LogPersist::flush_on_main_thread() {
  monitor::LockProbe probe("log_persist_flush", 10.0);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (write_failed_) {
      std::vector<std::string> discarded_logs;
      std::vector<std::string> discarded_traces;
      std::vector<TraceImagePayload> discarded_images;
      discarded_logs.swap(log_buffer_);
      discarded_traces.swap(trace_buffer_);
      discarded_images.swap(image_buffer_);
      image_buffer_bytes_ = 0;
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
    flush_failures_.fetch_add(1, std::memory_order_relaxed);
    if (first_failure) {
      LogSystem::instance().log(
          LogLevel::Error, LogCategory::System,
          "log persist flush failed, persistence disabled for this session: " +
              path);
    }
    monitor::persist_health(
        "write_failed",
        monitor::build_attrs({{"path", path},
                              {"first_failure", first_failure ? "true" : "false"}}));
  };
  if (!logs.empty() && !log_path.empty()) {
    std::string payload;
    for (const std::string &line : logs) {
      append_persist_line(payload, line);
    }
    if (append_persist_file(log_path, payload)) {
      bytes_written_.fetch_add(payload.size(), std::memory_order_relaxed);
    } else {
      note_persist_failure(log_path);
    }
  }
  if (!traces.empty() && !trace_path.empty()) {
    std::string payload;
    for (const std::string &line : traces) {
      append_persist_line(payload, line);
    }
    if (append_trace_payload(payload)) {
      bytes_written_.fetch_add(payload.size(), std::memory_order_relaxed);
    } else {
      note_persist_failure(trace_path);
    }
  }
  flush_trace_images();
  const uint64_t dropped_logs = dropped_log_lines_.load(std::memory_order_relaxed);
  const uint64_t dropped_traces = dropped_trace_lines_.load(std::memory_order_relaxed);
  if (dropped_logs > reported_dropped_logs_.exchange(dropped_logs) ||
      dropped_traces > reported_dropped_traces_.exchange(dropped_traces)) {
    monitor::persist_health(
        "drop",
        monitor::build_attrs(
            {{"dropped_log_lines", std::to_string(dropped_logs)},
             {"dropped_trace_lines", std::to_string(dropped_traces)}}));
  }
}

bool LogPersist::append_trace_payload(const std::string &payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (trace_path_.empty()) {
    return false;
  }
  std::ofstream out(std::filesystem::u8path(trace_path_),
                    std::ios::app | std::ios::binary);
  if (!out.is_open()) {
    return false;
  }
  out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
  out.flush();
  out.close();
  return out.good();
}

void LogPersist::flush_trace_images() {
  std::vector<TraceImagePayload> images;
  std::string trace_dir_path;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (image_buffer_.empty()) {
      return;
    }
    images.swap(image_buffer_);
    image_buffer_bytes_ = 0;
    trace_dir_path = trace_dir_;
  }
  const std::string dir = trace_dir_path + "/images";
  godot::DirAccess::make_dir_recursive_absolute(godot::String::utf8(dir.c_str()));
  for (const TraceImagePayload &image : images) {
    godot::Ref<godot::FileAccess> file = godot::FileAccess::open(
        godot::String::utf8(image.absolute_path.c_str()), godot::FileAccess::WRITE);
    if (file.is_null()) {
      LogSystem::instance().log_detailed(LogLevel::Error, LogCategory::System,
                                         "trace image store failed",
                                         image.absolute_path);
      continue;
    }
    file->store_buffer(reinterpret_cast<const uint8_t *>(image.bytes.data()),
                       static_cast<int64_t>(image.bytes.size()));
    const godot::Error write_err = file->get_error();
    file->close();
    if (write_err != godot::OK) {
      LogSystem::instance().log_detailed(LogLevel::Error, LogCategory::System,
                                         "trace image store failed",
                                         image.absolute_path);
      continue;
    }
    bytes_written_.fetch_add(image.bytes.size(), std::memory_order_relaxed);
  }
}

void LogPersist::write_trace_now(const std::string &line) {
  try {
    std::string payload;
    append_persist_line(payload, line);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (trace_path_.empty()) {
        return;
      }
    }
    if (append_trace_payload(payload)) {
      bytes_written_.fetch_add(payload.size(), std::memory_order_relaxed);
      return;
    }
    flush_failures_.fetch_add(1, std::memory_order_relaxed);
    bool report = false;
    std::string path;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      path = trace_path_;
      if (!trace_now_failure_reported_) {
        trace_now_failure_reported_ = true;
        report = true;
      }
    }
    if (report) {
      LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                                "trace write failed: " + path);
    }
  } catch (...) {
  }
}

void LogPersist::enqueue_trace_image(const TraceImagePayload &payload) {
  if (payload.absolute_path.empty() || payload.bytes.empty()) {
    LogSystem::instance().log_detailed(LogLevel::Error, LogCategory::System,
                                       "trace image payload rejected",
                                       payload.absolute_path);
    return;
  }
  bool dropped = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!image_buffer_.empty() &&
           (image_buffer_.size() >= kTraceImageBufferMaxEntries ||
            image_buffer_bytes_ + payload.bytes.size() >
                kTraceImageBufferMaxBytes)) {
      image_buffer_bytes_ -= image_buffer_.front().bytes.size();
      image_buffer_.erase(image_buffer_.begin());
      dropped_trace_images_.fetch_add(1, std::memory_order_relaxed);
      dropped = true;
    }
    image_buffer_bytes_ += payload.bytes.size();
    image_buffer_.push_back(payload);
  }
  if (dropped) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                              "trace image buffer overflow, oldest pending "
                              "image dropped");
  }
}

std::string LogPersist::store_trace_image(const std::string &span_id,
                                          const std::string &kind,
                                          const std::string &base64_png) {
  if (sanitize_policy::enabled()) {
    return std::string();
  }
  std::string session;
  std::string trace_dir_path;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    session = session_id_;
    trace_dir_path = trace_dir_;
  }
  const std::string decoded = TraceRecorder::decode_base64(base64_png);
  if (decoded.empty()) {
    LogSystem::instance().log_detailed(
        LogLevel::Error, LogCategory::System, "trace image decode failed",
        "span=" + span_id + " kind=" + kind);
    return std::string();
  }
  const std::string dir = trace_dir_path + "/images";
  const std::string name =
      "trace-" + session + "-" + span_id + "-" + kind + ".png";
  TraceImagePayload payload;
  payload.relative_path = "images/" + name;
  payload.absolute_path = dir + "/" + name;
  payload.bytes = decoded;
  enqueue_trace_image(payload);
  return payload.relative_path;
}

void LogPersist::rotate_on_init() {
  std::string log_dir_path;
  std::string trace_dir_path;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    log_dir_path = log_dir_;
    trace_dir_path = trace_dir_;
  }
  prune_persist_dir(godot::String::utf8(log_dir_path.c_str()), "gda-", ".log");
  prune_persist_dir(godot::String::utf8(trace_dir_path.c_str()), "trace-",
                    ".jsonl");
  prune_persist_dir(godot::String::utf8((trace_dir_path + "/images").c_str()),
                    "trace-", ".png");
  const uint64_t pruned = pruned_files_.load(std::memory_order_relaxed);
  note_rotation();
  monitor::persist_health(
      "rotate",
      monitor::build_attrs({{"pruned_files", std::to_string(pruned)},
                            {"rotations", std::to_string(rotations_.load())}}));
}

void LogPersist::note_pruned(std::size_t count) {
  pruned_files_.fetch_add(static_cast<uint64_t>(count), std::memory_order_relaxed);
}

void LogPersist::note_rotation() {
  rotations_.fetch_add(1, std::memory_order_relaxed);
}

uint64_t LogPersist::dropped_log_lines() const {
  return dropped_log_lines_.load(std::memory_order_relaxed);
}

uint64_t LogPersist::dropped_trace_lines() const {
  return dropped_trace_lines_.load(std::memory_order_relaxed);
}

uint64_t LogPersist::bytes_written() const {
  return bytes_written_.load(std::memory_order_relaxed);
}

uint64_t LogPersist::flush_failures() const {
  return flush_failures_.load(std::memory_order_relaxed);
}

uint64_t LogPersist::rotations() const {
  return rotations_.load(std::memory_order_relaxed);
}

uint64_t LogPersist::pruned_files() const {
  return pruned_files_.load(std::memory_order_relaxed);
}

std::size_t LogPersist::pending_trace_image_count() {
  std::lock_guard<std::mutex> lock(mutex_);
  return image_buffer_.size();
}

uint64_t LogPersist::pending_trace_image_bytes() {
  std::lock_guard<std::mutex> lock(mutex_);
  return image_buffer_bytes_;
}

uint64_t LogPersist::dropped_trace_images() const {
  return dropped_trace_images_.load(std::memory_order_relaxed);
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

std::string LogPersist::log_dir() {
  std::lock_guard<std::mutex> lock(mutex_);
  return log_dir_;
}

std::string LogPersist::trace_dir() {
  std::lock_guard<std::mutex> lock(mutex_);
  return trace_dir_;
}

std::string LogPersist::health_summary() {
  return monitor::build_attrs(
      {{"log_path", log_path()},
       {"trace_path", trace_path()},
       {"dropped_log_lines", std::to_string(dropped_log_lines())},
       {"dropped_trace_lines", std::to_string(dropped_trace_lines())},
       {"dropped_trace_images", std::to_string(dropped_trace_images())},
       {"bytes_written", std::to_string(bytes_written())},
       {"flush_failures", std::to_string(flush_failures())},
       {"rotations", std::to_string(rotations())},
       {"pruned_files", std::to_string(pruned_files())}});
}

} // namespace godot_autopilot
