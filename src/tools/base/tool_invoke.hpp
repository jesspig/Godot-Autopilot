#ifndef GODOT_AUTOPILOT_TOOL_INVOKE_HPP
#define GODOT_AUTOPILOT_TOOL_INVOKE_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace tools {

constexpr int kMaxInvokeDepth = 8;

mcp::JsonValue invoke_tool(const std::string &name, const mcp::JsonValue &args);

int invoke_depth();

const std::string &current_trace_id();
const std::string &current_span_id();
void push_span(const std::string &trace_id, const std::string &span_id);
void pop_span();
void set_queued_wait_ms(int64_t ms);
int64_t take_queued_wait_ms();
std::string invoke_thread_label();

struct SpanGuard {
  SpanGuard(const std::string &trace_id, const std::string &span_id);
  ~SpanGuard();
  SpanGuard(const SpanGuard &) = delete;
  SpanGuard &operator=(const SpanGuard &) = delete;
};
class RequestSpanGuard {
public:
  RequestSpanGuard(const std::string &trace_id, const std::string &span_id);
  ~RequestSpanGuard();
  RequestSpanGuard(const RequestSpanGuard &) = delete;
  RequestSpanGuard &operator=(const RequestSpanGuard &) = delete;

private:
  bool active_ = false;
};

struct TraceContext {
  std::string trace_id;
  std::string span_id;
  std::string request_id;
  bool valid() const { return !trace_id.empty(); }
};

TraceContext capture_trace_context();

struct ScopedTraceContext {
  explicit ScopedTraceContext(const TraceContext &ctx);
  ~ScopedTraceContext();
  ScopedTraceContext(const ScopedTraceContext &) = delete;
  ScopedTraceContext &operator=(const ScopedTraceContext &) = delete;

private:
  bool active_ = false;
  bool request_active_ = false;
  std::vector<std::pair<std::string, std::string>> saved_;
};

} // namespace tools
} // namespace godot_autopilot

#endif
