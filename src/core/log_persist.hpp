#ifndef GODOT_AUTOPILOT_LOG_PERSIST_HPP
#define GODOT_AUTOPILOT_LOG_PERSIST_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "log_system.hpp"

namespace godot_autopilot {

struct TraceImagePayload {
  std::string relative_path;
  std::string absolute_path;
  std::string bytes;
};

class LogPersist {
public:
  static constexpr std::size_t kBufferCap = 20000;
  static constexpr std::size_t kMaxFiles = 20;
  static constexpr uint64_t kMaxTotalBytes = 50ULL * 1024ULL * 1024ULL;
  static constexpr std::size_t kTraceImageBufferMaxEntries = 64;
  static constexpr uint64_t kTraceImageBufferMaxBytes = 64ULL * 1024ULL * 1024ULL;

  static LogPersist &instance();

  LogPersist(const LogPersist &) = delete;
  LogPersist &operator=(const LogPersist &) = delete;

  void init_session();
  void enqueue_log(const std::string &line);
  void enqueue_trace(const std::string &line);
  void flush_on_main_thread();
  void write_trace_now(const std::string &line);
  void rotate_on_init();

  std::string store_trace_image(const std::string &span_id,
                                const std::string &kind,
                                const std::string &base64_png);
  void enqueue_trace_image(const TraceImagePayload &payload);

  std::string session_id();
  std::string log_path();
  std::string trace_path();
  std::string trace_dir();
  std::string health_summary();

  void note_pruned(std::size_t count);
  void note_rotation();

  uint64_t dropped_log_lines() const;
  uint64_t dropped_trace_lines() const;
  uint64_t bytes_written() const;
  uint64_t flush_failures() const;
  uint64_t rotations() const;
  uint64_t pruned_files() const;

  std::size_t pending_trace_image_count();
  uint64_t pending_trace_image_bytes();
  uint64_t dropped_trace_images() const;

  static std::string level_name(LogLevel level);
  static std::string category_name(LogCategory category);
  static std::string format_human_line(int64_t wall_ms, const std::string &level,
                                       const std::string &category,
                                       const std::string &summary,
                                       const std::string &detail, bool include_detail);
  static std::string session_stamp_from_ticks(uint64_t wall_ms);
  static bool should_prune(std::size_t file_count, uint64_t total_bytes);

private:
  LogPersist() = default;

  bool append_trace_payload(const std::string &payload);
  void flush_trace_images();

  std::mutex mutex_;
  std::vector<std::string> log_buffer_;
  std::vector<std::string> trace_buffer_;
  std::vector<TraceImagePayload> image_buffer_;
  uint64_t image_buffer_bytes_ = 0;
  std::string session_id_;
  std::string log_path_;
  std::string trace_path_;
  std::string trace_os_path_;
  std::size_t last_log_serial_ = 0;
  uint64_t last_trace_seq_ = 0;
  bool write_failed_ = false;
  bool trace_now_failure_reported_ = false;
  std::atomic<uint64_t> dropped_log_lines_{0};
  std::atomic<uint64_t> dropped_trace_lines_{0};
  std::atomic<uint64_t> dropped_trace_images_{0};
  std::atomic<uint64_t> bytes_written_{0};
  std::atomic<uint64_t> flush_failures_{0};
  std::atomic<uint64_t> rotations_{0};
  std::atomic<uint64_t> pruned_files_{0};
  std::atomic<uint64_t> reported_dropped_logs_{0};
  std::atomic<uint64_t> reported_dropped_traces_{0};
};

} // namespace godot_autopilot

#endif
