#include "monitor.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <initializer_list>
#include <variant>

#include "log_persist.hpp"
#include "sanitize_policy.hpp"
#include "trace_recorder.hpp"

namespace godot_autopilot {
namespace monitor {

namespace {

thread_local std::vector<std::string> g_request_stack;

struct CounterStore {
  std::atomic<uint64_t> requests{0};
  std::atomic<uint64_t> responses{0};
  std::atomic<uint64_t> errors{0};
  std::atomic<uint64_t> notifications{0};
  std::atomic<uint64_t> tool_calls{0};
  std::atomic<uint64_t> tool_errors{0};
  std::atomic<uint64_t> cancels{0};
  std::atomic<uint64_t> timeouts{0};
  std::atomic<int64_t> latency_ms_sum{0};
  std::atomic<uint64_t> latency_samples{0};
};

CounterStore &store() {
  static CounterStore counters;
  return counters;
}

std::string parse_traceparent_trace_id(const std::string &traceparent) {
  const std::size_t first = traceparent.find('-');
  if (first == std::string::npos) {
    return std::string();
  }
  const std::size_t second = traceparent.find('-', first + 1);
  if (second == std::string::npos) {
    return std::string();
  }
  const std::string candidate = traceparent.substr(first + 1, second - first - 1);
  if (candidate.size() != 32) {
    return std::string();
  }
  for (char c : candidate) {
    const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                     (c >= 'A' && c <= 'F');
    if (!hex) {
      return std::string();
    }
  }
  return candidate;
}

void fill_defaults(TraceEvent &event) {
  if (event.session_id.empty()) {
    event.session_id = LogPersist::instance().session_id();
  }
  if (event.monotonic_ns == 0) {
    event.monotonic_ns = TraceRecorder::monotonic_now_ns();
  }
  if (event.thread_id.empty()) {
    event.thread_id = TraceRecorder::current_thread_id();
  }
  if (event.wall_start_ms == 0) {
    event.wall_start_ms = TraceRecorder::wall_now_ms();
  }
  if (event.wall_end_ms == 0) {
    event.wall_end_ms = event.wall_start_ms;
  }
  if (event.attrs.size() > TraceRecorder::kAttrsMaxChars) {
    event.attrs = sanitize_field(event.attrs);
  }
}

} // namespace

void RequestRegistry::put(const RequestRecord &record) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (records_.find(record.request_id) == records_.end()) {
    order_.push_back(record.request_id);
  }
  records_[record.request_id] = record;
  while (order_.size() > kMaxEntries) {
    records_.erase(order_.front());
    order_.pop_front();
  }
}

bool RequestRegistry::get(const std::string &request_id, RequestRecord *out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = records_.find(request_id);
  if (it == records_.end()) {
    return false;
  }
  if (out) {
    *out = it->second;
  }
  return true;
}

bool RequestRegistry::take(const std::string &request_id, RequestRecord *out) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = records_.find(request_id);
  if (it == records_.end()) {
    return false;
  }
  if (out) {
    *out = it->second;
  }
  records_.erase(it);
  for (auto order_it = order_.begin(); order_it != order_.end(); ++order_it) {
    if (*order_it == request_id) {
      order_.erase(order_it);
      break;
    }
  }
  return true;
}

std::vector<RequestRecord> RequestRegistry::expire(int64_t now_monotonic_ns,
                                                   int64_t timeout_ns) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<RequestRecord> expired;
  for (auto it = records_.begin(); it != records_.end();) {
    if (now_monotonic_ns - it->second.begin_monotonic_ns > timeout_ns) {
      expired.push_back(it->second);
      for (auto order_it = order_.begin(); order_it != order_.end(); ++order_it) {
        if (*order_it == it->first) {
          order_.erase(order_it);
          break;
        }
      }
      it = records_.erase(it);
    } else {
      ++it;
    }
  }
  return expired;
}

std::size_t RequestRegistry::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return records_.size();
}

void RequestRegistry::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  records_.clear();
  order_.clear();
}

RequestRegistry &registry() {
  static RequestRegistry instance;
  return instance;
}

uint64_t emit(TraceEvent event) {
  fill_defaults(event);
  return TraceRecorder::instance().record(std::move(event));
}

uint64_t tool_call(TraceEvent event, LogLevel level, const std::string &summary,
                   const std::string &detail) {
  event.kind = TraceKind::ToolCall;
  if (event.name.empty()) {
    event.name = event.tool;
  }
  event.state = event.ok ? "succeeded" : "failed";
  if (event.attrs.empty()) {
    event.attrs = build_attrs({{"tool", event.tool}});
  }
  const std::string trace_id = event.trace_id;
  const std::string span_id = event.span_id;
  const bool ok = event.ok;
  const uint64_t seq = emit(std::move(event));
  store().tool_calls.fetch_add(1, std::memory_order_relaxed);
  if (!ok) {
    store().tool_errors.fetch_add(1, std::memory_order_relaxed);
  }
  LogSystem::instance().log_detailed(level, LogCategory::Tools, summary, detail,
                                     trace_id, span_id);
  return seq;
}

uint64_t lifecycle(const std::string &name, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::Lifecycle;
  event.name = name;
  event.ok = true;
  event.state = "succeeded";
  event.attrs = sanitize_field(attrs);
  return emit(std::move(event));
}

uint64_t data_flow(const std::string &name, int64_t bytes, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::DataFlow;
  event.name = name;
  event.bytes = bytes;
  event.ok = true;
  event.state = "succeeded";
  event.attrs = sanitize_field(attrs);
  return emit(std::move(event));
}

uint64_t ui_action(const std::string &name, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::UiAction;
  event.name = name;
  event.ok = true;
  event.state = "succeeded";
  event.attrs = sanitize_field(attrs);
  return emit(std::move(event));
}

uint64_t security(const std::string &name, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::Security;
  event.name = name;
  event.ok = true;
  event.state = "succeeded";
  event.attrs = sanitize_field(attrs);
  return emit(std::move(event));
}

uint64_t state(const std::string &name, const std::string &state_value, bool ok) {
  TraceEvent event;
  event.kind = TraceKind::ExecutionState;
  event.name = name;
  event.state = state_value;
  event.ok = ok;
  return emit(std::move(event));
}

uint64_t error_event(const std::string &code, const std::string &type,
                     const std::string &message, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::Error;
  event.error_code = code;
  event.error_type = type;
  event.name = type.empty() ? code : type;
  event.ok = false;
  event.state = "failed";
  event.args_digest = sanitize_field(message);
  event.attrs = sanitize_field(attrs);
  return emit(std::move(event));
}

uint64_t perf(const std::string &name, const std::string &attrs, int64_t bytes) {
  TraceEvent event;
  event.kind = TraceKind::Perf;
  event.name = name;
  event.attrs = sanitize_field(attrs);
  event.bytes = bytes;
  event.ok = true;
  event.state = "succeeded";
  return emit(std::move(event));
}

RequestRecord begin_request(const std::string &request_id,
                            const std::string &method,
                            const std::string &params_digest,
                            const std::string &traceparent) {
  RequestRecord record;
  record.request_id = request_id;
  record.method = method;
  record.correlation_id = traceparent;
  const std::string parent_trace = parse_traceparent_trace_id(traceparent);
  record.trace_id =
      parent_trace.empty() ? TraceRecorder::new_trace_id() : parent_trace;
  record.span_id = TraceRecorder::new_span_id();
  record.begin_wall_ms = TraceRecorder::wall_now_ms();
  record.begin_monotonic_ns = TraceRecorder::monotonic_now_ns();
  registry().put(record);

  TraceEvent event;
  event.kind = TraceKind::ProtocolRequest;
  event.name = method;
  event.request_id = request_id;
  event.correlation_id = traceparent;
  event.trace_id = record.trace_id;
  event.span_id = record.span_id;
  event.phase = "begin";
  event.state = "started";
  event.ok = true;
  event.args_digest = sanitize_field(params_digest);
  event.wall_start_ms = record.begin_wall_ms;
  event.monotonic_ns = record.begin_monotonic_ns;
  emit(std::move(event));

  store().requests.fetch_add(1, std::memory_order_relaxed);
  return record;
}

void end_request(const std::string &request_id, bool ok,
                 const std::string &error_code, int64_t result_size) {
  RequestRecord record;
  const bool found = registry().take(request_id, &record);
  const int64_t now_ns = TraceRecorder::monotonic_now_ns();

  TraceEvent event;
  event.kind = TraceKind::ProtocolResponse;
  event.name = found ? record.method : std::string();
  event.request_id = request_id;
  event.correlation_id = found ? record.correlation_id : std::string();
  event.trace_id = found ? record.trace_id : TraceRecorder::new_trace_id();
  event.span_id = found ? record.span_id : TraceRecorder::new_span_id();
  event.phase = "end";
  event.ok = ok;
  event.state = ok ? "succeeded" : "failed";
  event.error_code = error_code;
  event.result_size = result_size;
  event.wall_start_ms = found ? record.begin_wall_ms : 0;
  event.wall_end_ms = TraceRecorder::wall_now_ms();
  event.monotonic_ns = now_ns;
  if (found) {
    event.duration_ms = (now_ns - record.begin_monotonic_ns) / 1000000;
    store().latency_ms_sum.fetch_add(event.duration_ms, std::memory_order_relaxed);
    store().latency_samples.fetch_add(1, std::memory_order_relaxed);
  }
  emit(std::move(event));

  store().responses.fetch_add(1, std::memory_order_relaxed);
  if (!ok) {
    store().errors.fetch_add(1, std::memory_order_relaxed);
  }
}

void note_protocol_error(const std::string &request_id, int64_t code,
                         const std::string &message, int64_t size) {
  RequestRecord record;
  const bool found = registry().take(request_id, &record);
  TraceEvent event;
  event.kind = TraceKind::ProtocolError;
  event.request_id = request_id;
  event.trace_id = found ? record.trace_id : TraceRecorder::new_trace_id();
  event.span_id = found ? record.span_id : TraceRecorder::new_span_id();
  event.correlation_id = found ? record.correlation_id : std::string();
  event.name = found ? record.method : std::string();
  event.phase = "end";
  event.ok = false;
  event.state = "failed";
  event.error_code = std::to_string(code);
  event.args_digest = sanitize_field(message);
  event.result_size = size;
  event.wall_end_ms = TraceRecorder::wall_now_ms();
  emit(std::move(event));
  store().errors.fetch_add(1, std::memory_order_relaxed);
}

void note_notification(const std::string &method, const std::string &digest) {
  TraceEvent event;
  event.kind = TraceKind::ProtocolNotification;
  event.name = method;
  event.phase = "point";
  event.ok = true;
  event.state = "succeeded";
  event.args_digest = sanitize_field(digest);
  emit(std::move(event));
  store().notifications.fetch_add(1, std::memory_order_relaxed);
  if (method == "notifications/cancelled") {
    note_cancel();
  }
}

void note_client(const std::string &name, const std::string &version, bool connected) {
  TraceEvent event;
  event.kind = TraceKind::Lifecycle;
  event.name = connected ? "client_connected" : "client_disconnected";
  event.ok = connected;
  event.state = connected ? "succeeded" : "failed";
  event.attrs = sanitize_field(build_attrs({{"client", name}, {"version", version}}));
  emit(std::move(event));
}

void note_transport(const std::string &name, bool ok, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::Lifecycle;
  event.name = name;
  event.ok = ok;
  event.state = ok ? "succeeded" : "failed";
  event.attrs = sanitize_field(attrs);
  emit(std::move(event));
}

void push_request(const std::string &request_id) {
  g_request_stack.push_back(request_id);
}

void pop_request() {
  if (!g_request_stack.empty()) {
    g_request_stack.pop_back();
  }
}

std::string current_request_id() {
  if (g_request_stack.empty()) {
    return std::string();
  }
  return g_request_stack.back();
}

RequestScope::RequestScope(const std::string &request_id) {
  if (!request_id.empty()) {
    active_ = true;
    push_request(request_id);
  }
}

RequestScope::~RequestScope() {
  if (active_) {
    pop_request();
  }
}

Counters counters() {
  Counters out;
  out.requests = store().requests.load(std::memory_order_relaxed);
  out.responses = store().responses.load(std::memory_order_relaxed);
  out.errors = store().errors.load(std::memory_order_relaxed);
  out.notifications = store().notifications.load(std::memory_order_relaxed);
  out.tool_calls = store().tool_calls.load(std::memory_order_relaxed);
  out.tool_errors = store().tool_errors.load(std::memory_order_relaxed);
  out.cancels = store().cancels.load(std::memory_order_relaxed);
  out.timeouts = store().timeouts.load(std::memory_order_relaxed);
  out.latency_ms_sum = store().latency_ms_sum.load(std::memory_order_relaxed);
  out.latency_samples = store().latency_samples.load(std::memory_order_relaxed);
  out.inflight = static_cast<int64_t>(registry().size());
  return out;
}

void note_cancel() {
  store().cancels.fetch_add(1, std::memory_order_relaxed);
}

void note_timeout() {
  store().timeouts.fetch_add(1, std::memory_order_relaxed);
}

std::vector<RequestRecord> expire_requests(int64_t timeout_ns) {
  const int64_t now = TraceRecorder::monotonic_now_ns();
  std::vector<RequestRecord> expired = registry().expire(now, timeout_ns);
  for (const RequestRecord &record : expired) {
    TraceEvent event;
    event.kind = TraceKind::ExecutionState;
    event.name = record.method;
    event.request_id = record.request_id;
    event.correlation_id = record.correlation_id;
    event.trace_id = record.trace_id;
    event.span_id = record.span_id;
    event.phase = "end";
    event.ok = false;
    event.state = "timed_out";
    event.error_code = "timeout";
    event.duration_ms = (now - record.begin_monotonic_ns) / 1000000;
    event.wall_start_ms = record.begin_wall_ms;
    event.wall_end_ms = TraceRecorder::wall_now_ms();
    emit(std::move(event));
    note_timeout();
  }
  return expired;
}

std::string sanitize_field(const std::string &text) {
  if (sanitize_policy::enabled()) {
    return TraceRecorder::sanitize_text(text, TraceRecorder::kAttrsMaxChars);
  }
  if (text.size() > TraceRecorder::kAttrsMaxChars) {
    std::string out = text.substr(0, TraceRecorder::kAttrsMaxChars);
    out += "...[truncated]";
    return out;
  }
  return text;
}

std::string json_escape(const std::string &text) {
  std::string out;
  out.reserve(text.size() + 2);
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
        break;
    }
  }
  return out;
}

std::string build_attrs(
    std::initializer_list<std::pair<const char *, std::string>> fields) {
  std::string out = "{";
  bool first = true;
  for (const auto &field : fields) {
    if (!first) {
      out += ',';
    }
    first = false;
    out += '"';
    out += json_escape(field.first);
    out += "\":\"";
    out += json_escape(field.second);
    out += '"';
  }
  out += '}';
  return out;
}

void persist_health(const std::string &name, const std::string &attrs) {
  TraceEvent event;
  event.kind = TraceKind::PersistHealth;
  event.name = name;
  event.ok = name != "write_failed";
  event.state = event.ok ? "succeeded" : "degraded";
  event.attrs = sanitize_field(attrs);
  emit(std::move(event));
}

LockProbe::LockProbe(const char *name, double threshold_ms)
    : name_(name),
      start_ns_(TraceRecorder::monotonic_now_ns()),
      threshold_ns_(static_cast<int64_t>(threshold_ms * 1000000.0)),
      active_(true) {}

LockProbe::~LockProbe() {
  if (!active_) {
    return;
  }
  const int64_t elapsed = TraceRecorder::monotonic_now_ns() - start_ns_;
  if (elapsed <= threshold_ns_) {
    return;
  }
  TraceEvent event;
  event.kind = TraceKind::Concurrency;
  event.name = name_ ? name_ : "lock";
  event.duration_ms = elapsed / 1000000;
  event.ok = true;
  event.state = "succeeded";
  event.attrs = sanitize_field(build_attrs({{"wait_ns", std::to_string(elapsed)}}));
  emit(std::move(event));
}

std::string request_id_text(const mcp::RequestId &id) {
  if (std::holds_alternative<int64_t>(id)) {
    return std::to_string(std::get<int64_t>(id));
  }
  return std::get<std::string>(id);
}

} // namespace monitor
} // namespace godot_autopilot
