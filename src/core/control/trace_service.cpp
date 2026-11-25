#include "core/control/trace_service.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/control/control_types.hpp"
#include "core/protocol/gcp_envelope.hpp"

namespace gda {
namespace control {
namespace {

std::uint64_t next_trace_counter() {
  static std::uint64_t counter = 0;
  ++counter;
  return counter;
}

std::uint64_t next_span_counter() {
  static std::uint64_t counter = 0;
  ++counter;
  return counter;
}

std::string trace_hex_id(std::uint64_t seed, std::size_t width) {
  std::string out;
  out.reserve(width);
  while (out.size() < width) {
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    out += fnv1a_hex(std::to_string(seed));
  }
  return out.substr(0, width);
}

} // namespace

std::string TraceService::new_trace_id() {
  return trace_hex_id(next_trace_counter(), 32);
}

std::string TraceService::new_span_id() {
  return trace_hex_id(next_span_counter() + 0x1000000ULL, 16);
}

SpanContext TraceService::parse(const std::string &traceparent) {
  SpanContext context;
  if (!gcp::valid_traceparent(traceparent)) {
    return context;
  }
  context.traceparent = traceparent;
  context.trace_id = traceparent.substr(3, 32);
  context.span_id = traceparent.substr(36, 16);
  context.parent_span = context.span_id;
  context.flags = traceparent.substr(53, 2);
  return context;
}

std::string TraceService::make_traceparent(const std::string &trace_id,
                                           const std::string &span_id,
                                           const std::string &flags) {
  return "00-" + trace_id + "-" + span_id + "-" + flags;
}

SpanContext TraceService::begin(const std::string &incoming_traceparent,
                                const std::string &local_span_id) {
  SpanContext context = parse(incoming_traceparent);
  const std::string trace_id =
      context.trace_id.empty() ? new_trace_id() : context.trace_id;
  const std::string parent_span = context.span_id;
  const std::string span_id =
      local_span_id.empty() ? new_span_id() : local_span_id;
  const std::string flags = context.flags.empty() ? "01" : context.flags;
  context.trace_id = trace_id;
  context.parent_span = parent_span;
  context.span_id = span_id;
  context.flags = flags;
  context.traceparent = make_traceparent(trace_id, span_id, flags);
  stack_.push_back(context);
  return context;
}

bool TraceService::end(const std::string &span_id) {
  if (stack_.empty() || stack_.back().span_id != span_id) {
    return false;
  }
  stack_.pop_back();
  return true;
}

std::size_t TraceService::depth() const { return stack_.size(); }

std::vector<SpanContext> TraceService::stack() const { return stack_; }

SpanContext TraceService::current() const {
  return stack_.empty() ? SpanContext() : stack_.back();
}

void TraceService::clear() { stack_.clear(); }

} // namespace control
} // namespace gda
