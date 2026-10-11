#ifndef GODOT_AUTOPILOT_MONITOR_HPP
#define GODOT_AUTOPILOT_MONITOR_HPP

#include <cstddef>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <mcp/JsonRpc.hpp>

#include "log_system.hpp"
#include "trace_recorder.hpp"

namespace godot_autopilot {
namespace monitor {

std::string request_id_text(const mcp::RequestId &id);

struct RequestRecord {
  std::string request_id;
  std::string trace_id;
  std::string span_id;
  std::string correlation_id;
  std::string method;
  int64_t begin_wall_ms = 0;
  int64_t begin_monotonic_ns = 0;
};

class RequestRegistry {
public:
  static constexpr std::size_t kMaxEntries = 4096;

  void put(const RequestRecord &record);
  bool get(const std::string &request_id, RequestRecord *out) const;
  bool take(const std::string &request_id, RequestRecord *out);
  std::vector<RequestRecord> expire(int64_t now_monotonic_ns, int64_t timeout_ns);
  std::size_t size() const;
  void clear();

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, RequestRecord> records_;
  std::deque<std::string> order_;
};

RequestRegistry &registry();

uint64_t emit(TraceEvent event);
uint64_t tool_call(TraceEvent event, LogLevel level, const std::string &summary,
                   const std::string &detail);

uint64_t lifecycle(const std::string &name, const std::string &attrs = {});
uint64_t data_flow(const std::string &name, int64_t bytes, const std::string &attrs = {});
uint64_t ui_action(const std::string &name, const std::string &attrs = {});
uint64_t security(const std::string &name, const std::string &attrs = {});
uint64_t state(const std::string &name, const std::string &state, bool ok);
uint64_t error_event(const std::string &code, const std::string &type,
                     const std::string &message, const std::string &attrs = {});
uint64_t perf(const std::string &name, const std::string &attrs, int64_t bytes = 0);

RequestRecord begin_request(const std::string &request_id, const std::string &method,
                            const std::string &params_digest,
                            const std::string &traceparent);
void end_request(const std::string &request_id, bool ok,
                 const std::string &error_code, int64_t result_size);
void note_protocol_error(const std::string &request_id, int64_t code,
                         const std::string &message, int64_t size);
void note_notification(const std::string &method, const std::string &digest);
void note_client(const std::string &name, const std::string &version, bool connected);
void note_transport(const std::string &name, bool ok, const std::string &attrs = {});

void push_request(const std::string &request_id);
void pop_request();
std::string current_request_id();

class RequestScope {
public:
  explicit RequestScope(const std::string &request_id);
  ~RequestScope();
  RequestScope(const RequestScope &) = delete;
  RequestScope &operator=(const RequestScope &) = delete;

private:
  bool active_ = false;
};

struct Counters {
  uint64_t requests = 0;
  uint64_t responses = 0;
  uint64_t errors = 0;
  uint64_t notifications = 0;
  uint64_t tool_calls = 0;
  uint64_t tool_errors = 0;
  uint64_t cancels = 0;
  uint64_t timeouts = 0;
  int64_t latency_ms_sum = 0;
  uint64_t latency_samples = 0;
  int64_t inflight = 0;
};

Counters counters();
void note_cancel();
void note_timeout();

std::vector<RequestRecord> expire_requests(int64_t timeout_ns);

std::string sanitize_field(const std::string &text);
std::string json_escape(const std::string &text);
std::string build_attrs(
    std::initializer_list<std::pair<const char *, std::string>> fields);

void environment_snapshot();
void persist_health(const std::string &name, const std::string &attrs);

class LockProbe {
public:
  explicit LockProbe(const char *name, double threshold_ms = 1.0);
  ~LockProbe();
  LockProbe(const LockProbe &) = delete;
  LockProbe &operator=(const LockProbe &) = delete;

private:
  const char *name_;
  int64_t start_ns_;
  int64_t threshold_ns_;
  bool active_;
};

} // namespace monitor
} // namespace godot_autopilot

#endif
