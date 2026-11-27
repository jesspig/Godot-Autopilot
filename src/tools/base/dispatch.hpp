#ifndef GODOT_AUTOPILOT_DISPATCH_HPP
#define GODOT_AUTOPILOT_DISPATCH_HPP

#include "core/command_queue.hpp"
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include "util/json_value.hpp"
#include <tools/base/tool_invoke.hpp>

namespace godot_autopilot {
namespace dispatch {

using HandlerFn = std::function<godot_autopilot::util::JsonValue(const godot_autopilot::util::JsonValue &)>;

using HandlerMap = std::unordered_map<std::string, HandlerFn>;

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers);
void clear_handlers();

int64_t main_thread_wait_budget_ms(uint32_t tool_flags, int64_t default_ms,
                                   int64_t health_ms, int64_t long_blocking_ms);

int64_t dispatch_wait_long_blocking_ms();

std::string main_thread_timeout_message(const std::string &label,
                                        int64_t waited_ms,
                                        bool cancelled_before_execution);

godot_autopilot::util::JsonValue main_thread_timeout_result(const std::string &label,
                                          int64_t waited_ms,
                                          bool cancelled_before_execution);

std::string dispatch_exception_message(const std::string &name,
                                       const std::string &reason,
                                       const godot_autopilot::util::JsonValue &args);

godot_autopilot::util::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason);

godot_autopilot::util::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason,
                                        const CommandQueue::Stats &stats,
                                        int64_t oldest_pending_ms,
                                        int64_t last_drain_age_ms,
                                        int64_t waited_ms);

godot_autopilot::util::JsonValue dispatch_timeout_result(const std::string &name,
                                       int64_t waited_ms,
                                       const CommandQueue::Stats &stats,
                                       int64_t oldest_pending_ms,
                                       int64_t last_drain_age_ms,
                                       bool cancelled_before_execution,
                                       bool health);

godot_autopilot::util::JsonValue exec_timeout_result(int64_t timeout_ms,
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
  if (queue.is_main_thread()) {
    const auto start = std::chrono::steady_clock::now();
    try {
      fn(*state);
    } catch (const std::exception &ex) {
      out.waited_ms = static_cast<int64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start)
              .count());
      out.error = std::string("editor main thread task failed: ") + ex.what();
      return out;
    } catch (...) {
      out.waited_ms = static_cast<int64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start)
              .count());
      out.error = "editor main thread task failed: unexpected C++ exception";
      return out;
    }
    out.waited_ms = static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
    if (budget_ms <= 0 || out.waited_ms > budget_ms) {
      out.timed_out = true;
      out.error =
          "editor main thread inline task '" + label + "' exceeded its " +
          std::to_string(budget_ms) + " ms budget after " +
          std::to_string(out.waited_ms) +
          " ms; the task already ran to completion and could not be preempted";
      return out;
    }
    out.ok = true;
    return out;
  }
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

godot_autopilot::util::JsonValue run_on_main_thread_with_budget(
    CommandQueue &queue, const std::string &tool_name, uint32_t tool_flags,
    std::function<godot_autopilot::util::JsonValue()> fn);

godot_autopilot::util::JsonValue run_main_thread_slice(CommandQueue &queue, const std::string &label,
                                     std::function<godot_autopilot::util::JsonValue()> slice,
                                     int64_t budget_ms);

godot_autopilot::util::JsonValue resolve_deferred_wait(const std::string &name,
                                      godot_autopilot::util::JsonValue result);

bool has_deferred_marker(const godot_autopilot::util::JsonValue &result);

struct DepthContext {
  int depth = 0;
  std::string trace_id;
  std::string span_id;
  std::string request_id;
  bool run_on_calling_thread = false;
};

DepthContext capture_depth_context();
int current_depth();

class ScopedDepthContext {
public:
  explicit ScopedDepthContext(const DepthContext &ctx);
  ~ScopedDepthContext();
  ScopedDepthContext(const ScopedDepthContext &) = delete;
  ScopedDepthContext &operator=(const ScopedDepthContext &) = delete;

private:
  int saved_depth_ = 0;
  std::optional<tools::ScopedTraceContext> trace_scope_;
};

godot_autopilot::util::JsonValue call_handler(const std::string &name,
                            const godot_autopilot::util::JsonValue &args,
                            const DepthContext &depth_ctx = {});

godot_autopilot::util::JsonValue export_blocked_json_result();

std::string export_blocked_error_text();

std::optional<std::string> resolve_alias_canonical(const std::string &name);

} // namespace dispatch
} // namespace godot_autopilot

#endif
