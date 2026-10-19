#ifndef GODOT_AUTOPILOT_DISPATCH_HPP
#define GODOT_AUTOPILOT_DISPATCH_HPP

#include "core/command_queue.hpp"
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <mcp/JsonValue.hpp>
#include <mcp/McpTypes.hpp>

namespace godot_autopilot {
namespace dispatch {

using HandlerFn = std::function<mcp::JsonValue(const mcp::JsonValue &)>;

using HandlerMap = std::unordered_map<std::string, HandlerFn>;

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers);
void clear_handlers();

int64_t main_thread_wait_budget_ms(uint32_t tool_flags, int64_t default_ms,
                                   int64_t health_ms, int64_t long_blocking_ms);

std::string main_thread_timeout_message(const std::string &label,
                                        int64_t waited_ms, bool cancelled);

mcp::JsonValue main_thread_timeout_result(const std::string &label,
                                          int64_t waited_ms, bool cancelled);

mcp::JsonValue exec_timeout_result(int64_t timeout_ms,
                                   int64_t execution_time_ms);

struct BoundedWaitOutcome {
  bool ok = false;
  bool timed_out = false;
  bool cancelled = false;
  int64_t waited_ms = 0;
  std::string error;
};

template <typename State, typename Fn>
BoundedWaitOutcome run_bounded_on_main_thread(
    CommandQueue &queue, const std::string &label,
    const std::shared_ptr<State> &state, Fn fn, int64_t budget_ms) {
  BoundedWaitOutcome out;
  auto submission = queue.submit_tracked_bounded(
      [state, fn]() { fn(*state); }, budget_ms);
  out.waited_ms = submission.waited_ms;
  out.timed_out = submission.timed_out;
  out.cancelled = submission.cancelled;
  if (submission.timed_out) {
    out.error = main_thread_timeout_message(label, out.waited_ms,
                                            out.cancelled);
    return out;
  }
  try {
    submission.future.get();
  } catch (const std::exception &ex) {
    out.error = std::string("editor main thread task failed: ") + ex.what();
    return out;
  }
  out.ok = true;
  return out;
}

mcp::JsonValue run_on_main_thread_with_budget(
    CommandQueue &queue, const std::string &tool_name, uint32_t tool_flags,
    std::function<mcp::JsonValue()> fn);

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args);

mcp::CallToolResult export_blocked_result();

std::string export_blocked_error_text();

} // namespace dispatch
} // namespace godot_autopilot

#endif
