#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gda {
namespace control {

struct SpanContext {
  std::string traceparent;
  std::string trace_id;
  std::string span_id;
  std::string parent_span;
  std::string flags;
};

class TraceService {
public:
  static std::string new_trace_id();
  static std::string new_span_id();
  static SpanContext parse(const std::string &traceparent);
  static std::string make_traceparent(const std::string &trace_id,
                                      const std::string &span_id,
                                      const std::string &flags = "01");

  SpanContext begin(const std::string &incoming_traceparent,
                    const std::string &local_span_id = std::string());
  bool end(const std::string &span_id);
  std::size_t depth() const;
  std::vector<SpanContext> stack() const;
  SpanContext current() const;
  void clear();

private:
  std::vector<SpanContext> stack_;
};

} // namespace control
} // namespace gda
