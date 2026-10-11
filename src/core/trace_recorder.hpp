#ifndef GODOT_AUTOPILOT_TRACE_RECORDER_HPP
#define GODOT_AUTOPILOT_TRACE_RECORDER_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {

enum class TraceKind {
  ToolCall,
  ProtocolRequest,
  ProtocolResponse,
  ProtocolError,
  ProtocolNotification,
  Lifecycle,
  DataFlow,
  ExecutionState,
  Concurrency,
  Perf,
  Snapshot,
  Error,
  PersistHealth,
  UiAction,
  Security
};

const char *trace_kind_name(TraceKind kind);

struct TraceEvent {
  uint64_t seq = 0;
  std::string trace_id;
  std::string span_id;
  std::string parent_span;
  std::string session_id;
  std::string tool;
  std::string category;
  uint32_t flags = 0;
  int side_effect = 0;
  int depth = 0;
  std::string thread;
  int64_t queue_wait_ms = 0;
  int64_t duration_ms = 0;
  int64_t wall_start_ms = 0;
  int64_t wall_end_ms = 0;
  std::string auth;
  bool ok = false;
  std::string error_code;
  std::string args_digest;
  bool args_truncated = false;
  int64_t result_size = 0;
  std::string image_ref;
  int64_t image_bytes = 0;
  std::string image_hash;
  int image_width = 0;
  int image_height = 0;

  TraceKind kind = TraceKind::ToolCall;
  std::string name;
  std::string request_id;
  std::string correlation_id;
  std::string phase = "point";
  std::string state;
  int64_t monotonic_ns = 0;
  std::string thread_id;
  std::string attrs;
  std::string error_type;
  std::string stack;
  int64_t bytes = 0;
};

struct SanitizeResult {
  std::string text;
  bool truncated = false;
};

class TraceRecorder {
public:
  static constexpr std::size_t kCapacity = 20000;
  static constexpr std::size_t kDesensitizedMaxChars = 4000;
  static constexpr std::size_t kRawMaxChars = 64000;
  static constexpr std::size_t kAttrsMaxChars = 2000;

  static TraceRecorder &instance();

  TraceRecorder(const TraceRecorder &) = delete;
  TraceRecorder &operator=(const TraceRecorder &) = delete;

  uint64_t record(TraceEvent event);
  std::vector<TraceEvent> query_recent(std::size_t limit);
  std::vector<TraceEvent> query_by_trace(const std::string &trace_id);
  std::vector<TraceEvent> query_by_request(const std::string &request_id);
  std::vector<TraceEvent> query_since(uint64_t since_seq,
                                      uint64_t *next_seq) const;
  uint64_t next_seq() const;
  void clear_for_test();
  std::size_t size();

  static std::string new_trace_id();
  static std::string new_span_id();
  static SanitizeResult sanitize_args(const std::string &args_dump, bool desensitize);
  static std::string sanitize_text(const std::string &text, std::size_t max_chars);
  static std::string fnv1a_hex(const std::string &text);
  static std::string decode_base64(const std::string &input);
  static std::string to_json_line(const TraceEvent &event);
  static int64_t wall_now_ms();
  static int64_t monotonic_now_ns();
  static std::string current_thread_id();

private:
  TraceRecorder() = default;

  mutable std::mutex mutex_;
  std::deque<TraceEvent> events_;
  uint64_t next_seq_ = 1;
};

} // namespace godot_autopilot

#endif
