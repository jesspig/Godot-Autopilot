#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "util/input_guards.hpp"
#include "core/observability/log_persist.hpp"
#include "core/observability/log_system.hpp"
#include "core/observability/monitor.hpp"
#include "core/observability/sanitize_policy.hpp"
#include "core/observability/trace_recorder.hpp"
#include "core/export_guard.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/domains/editor/editor_ops.hpp"
#include "tools/domains/os/os_ops.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_invoke.hpp"
#include "tools/base/tool_spec.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace dispatch {

namespace {

std::mutex g_handlers_mutex;
HandlerMap g_handlers;
HandlerMap g_meta_handlers;
thread_local int g_depth = 0;

constexpr std::size_t kErrorArgsPreviewBytes = 256;
constexpr std::size_t kTraceArgsTextBytes = 512;

std::string truncate_utf8_prefix(const std::string &text,
                                 std::size_t max_bytes) {
  if (text.size() <= max_bytes) {
    return text;
  }
  std::size_t cut = max_bytes;
  while (cut > 0 &&
         (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
    --cut;
  }
  return text.substr(0, cut);
}

std::string sanitized_args_preview(const godot_autopilot::util::JsonValue &args) {
  const SanitizeResult sanitized =
      TraceRecorder::sanitize_args(args.Dump(), sanitize_policy::enabled());
  return godot_autopilot::util::JsonValue::SanitizeUtf8(
      truncate_utf8_prefix(sanitized.text, kErrorArgsPreviewBytes));
}

void record_dispatch_exception(const std::string &name,
                               const godot_autopilot::util::JsonValue &args,
                               const godot_autopilot::util::JsonValue &result) {
  std::string trace = tools::current_trace_id();
  if (trace.empty()) {
    trace = TraceRecorder::new_trace_id();
  }
  const std::string span = TraceRecorder::new_span_id();
  const SanitizeResult sanitized =
      TraceRecorder::sanitize_args(args.Dump(), sanitize_policy::enabled());
  const int64_t wall = TraceRecorder::wall_now_ms();
  TraceEvent event;
  event.trace_id = trace;
  event.span_id = span;
  event.parent_span = tools::current_span_id();
  event.session_id = LogPersist::instance().session_id();
  event.tool = name;
  event.depth = tools::invoke_depth();
  event.thread = tools::invoke_thread_label();
  event.wall_start_ms = wall;
  event.wall_end_ms = wall;
  event.auth = "ok";
  event.ok = false;
  event.error_code = "internal";
  event.args_digest = sanitized.text;
  event.args_truncated = sanitized.truncated;
  event.result_size = static_cast<int64_t>(result.Dump().size());
  event.name = name;
  event.thread_id = TraceRecorder::current_thread_id();
  event.monotonic_ns = TraceRecorder::monotonic_now_ns();
  event.error_type = "internal";
  std::string args_text =
      truncate_utf8_prefix(sanitized.text, kTraceArgsTextBytes);
  monitor::tool_call(
      std::move(event), LogLevel::Error,
      "call_tool " + name + " -> error internal",
      "trace=" + trace + " span=" + span + " depth=" +
          std::to_string(tools::invoke_depth()) +
          " queue=0ms args=" + args_text + " err=internal");
}

void log_alias_hit(const ToolAlias &alias) {
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::Tools,
      "tool alias '" + alias.name + "' resolved to '" + alias.canonical + "'");
}

std::optional<ToolAlias> resolve_tool_alias(const std::string &name) {
  auto registry = get_active_registry();
  if (!registry) {
    return std::nullopt;
  }
  return registry->resolve_alias(name);
}

godot_autopilot::util::JsonValue call_handler_impl(const std::string &name,
                                 const godot_autopilot::util::JsonValue &args) {
  if (godot_autopilot::ExportGuard::is_exporting()) {
    return export_blocked_json_result();
  }
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  std::optional<ToolAlias> alias;
  if (auto registry = get_active_registry()) {
    alias = registry->resolve_alias(name);
  }
  godot_autopilot::util::JsonValue alias_error;
  if (alias && alias->validate_args) {
    alias_error = alias->validate_args(args);
  }
  const bool has_alias_error = alias && !alias_error.IsNull();
  const std::string lookup_name = alias ? alias->canonical : name;
  const godot_autopilot::util::JsonValue call_args =
      alias && !has_alias_error ? apply_alias_args(*alias, args) : args;
  HandlerFn handler;
  {
    monitor::LockProbe probe("dispatch_handlers_lock", 5.0);
    std::lock_guard<std::mutex> lock(g_handlers_mutex);
    auto it = g_handlers.find(lookup_name);
    if (it != g_handlers.end()) {
      handler = it->second;
    } else {
      auto meta_it = g_meta_handlers.find(lookup_name);
      if (meta_it != g_meta_handlers.end()) {
        handler = meta_it->second;
      }
    }
  }
  if (has_alias_error) {
    result = std::move(alias_error);
  } else if (handler) {
    try {
      result = handler(call_args);
    } catch (const std::exception &ex) {
      result = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
      result["error"] =
          godot_autopilot::util::JsonValue(dispatch_exception_message(name, ex.what(), call_args));
      record_dispatch_exception(name, call_args, result);
    } catch (...) {
      result = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
      result["error"] = godot_autopilot::util::JsonValue(
          dispatch_exception_message(name, "unexpected C++ exception", call_args));
      record_dispatch_exception(name, call_args, result);
    }
  } else {
    result = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
    result["error"] =
        godot_autopilot::util::JsonValue("domain tool '" + lookup_name +
                       "' not found" + util::received_keys_suffix(args) +
                       " — use search_tools to discover available "
                       "tools");
  }
  if (alias) {
    log_alias_hit(*alias);
    if (has_deferred_marker(result)) {
      result = annotate_alias_result(*alias, std::move(result));
    } else {
      result = apply_alias_result(*alias, std::move(result), args);
    }
  }
  const bool has_error = !spec_detail::verdict_of(result).ok;
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
      "call_tool: " + name + " -> " + (has_error ? "error" : "ok"));
  tools::take_queued_wait_ms();
  return result;
}

godot_autopilot::util::JsonValue depth_exceeded_result(const std::string &name) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue(
      "tool invoke depth exceeded (max " +
      std::to_string(tools::kMaxInvokeDepth) + "): '" + name + "'");
  result["code"] = godot_autopilot::util::JsonValue("depth_exceeded");
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  result["tool"] = godot_autopilot::util::JsonValue(name);
  result["depth"] = godot_autopilot::util::JsonValue(tools::kMaxInvokeDepth);
  return result;
}

godot_autopilot::util::JsonValue call_handler_with_context(const std::string &name,
                                         const godot_autopilot::util::JsonValue &args,
                                         const DepthContext &depth_ctx) {
  ScopedDepthContext scope(depth_ctx);
  return call_handler_impl(name, args);
}

int64_t env_int_clamped(const char *name, int64_t fallback, int64_t min_value,
                        int64_t max_value) {
  int64_t value = fallback;
  if (const char *raw = std::getenv(name)) {
    char *end = nullptr;
    const long long parsed = std::strtoll(raw, &end, 10);
    if (end && *end == '\0' && parsed > 0) {
      value = static_cast<int64_t>(parsed);
    }
  }
  if (value < min_value) {
    value = min_value;
  }
  if (value > max_value) {
    value = max_value;
  }
  return value;
}

int64_t dispatch_wait_default_ms() {
  static const int64_t value = env_int_clamped(
      "GODOT_AUTOPILOT_DISPATCH_TIMEOUT_MS", GDA_DISPATCH_WAIT_DEFAULT_MS,
      GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_MAX_MS);
  return value;
}

int64_t dispatch_wait_health_ms() {
  static const int64_t value = env_int_clamped(
      "GODOT_AUTOPILOT_HEALTH_TIMEOUT_MS", GDA_DISPATCH_WAIT_HEALTH_MS,
      GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_MAX_MS);
  return value;
}

godot_autopilot::util::JsonValue internal_error_result(const std::string &name,
                                     const std::string &reason) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] =
      godot_autopilot::util::JsonValue("tool '" + name + "' failed: " + reason);
  result["code"] = godot_autopilot::util::JsonValue("internal_error");
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  result["tool"] = godot_autopilot::util::JsonValue(name);
  return result;
}

std::string dispatch_timeout_message(const std::string &name,
                                     int64_t waited_ms, int64_t health_ms,
                                     bool health,
                                     bool cancelled_before_execution) {
  std::string message;
  if (health) {
    message = "editor main thread did not process health probe '" + name +
              "' within " + std::to_string(waited_ms) + " ms";
  } else {
    message = "editor main thread did not process tool '" + name + "' within " +
              std::to_string(waited_ms) + " ms";
  }
  message += " — the editor may be paused, blocked by a modal dialog, "
             "exporting, or busy; ";
  if (cancelled_before_execution) {
    message += "the task was cancelled before execution, so it did not run";
  } else {
    message +=
        "the main thread already picked the call up, so it may still execute";
  }
  message +=
      " (the call is cancelled — the side effect may still land in the "
      "editor, so verify the editor state before issuing it again";
  if (!health) {
    message += "; ping/system_status use a shorter " +
               std::to_string(health_ms) + " ms health budget";
  }
  message += ")";
  return message;
}

void emit_dispatch_timeout_observation(const std::string &name,
                                       int64_t waited_ms,
                                       const CommandQueue::Stats &stats,
                                       int64_t oldest_pending_ms,
                                       int64_t last_drain_age_ms,
                                       bool cancelled) {
  try {
    godot_autopilot::util::JsonValue line(godot_autopilot::util::JsonValue::object_tag);
    line["type"] = godot_autopilot::util::JsonValue("dispatch_timeout");
    line["tool"] = godot_autopilot::util::JsonValue(name);
    line["waited_ms"] = godot_autopilot::util::JsonValue(waited_ms);
    line["queue_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(stats.pending));
    line["oldest_pending_ms"] = godot_autopilot::util::JsonValue(oldest_pending_ms);
    line["last_drain_age_ms"] = godot_autopilot::util::JsonValue(last_drain_age_ms);
    line["cancelled"] = godot_autopilot::util::JsonValue(cancelled);
    LogPersist::instance().write_trace_now(line.Dump());
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::Transport,
        "dispatch timeout: tool='" + name + "' waited " +
            std::to_string(waited_ms) + "ms queue_depth=" +
            std::to_string(stats.pending) + " oldest_pending_ms=" +
            std::to_string(oldest_pending_ms) + " last_drain_age_ms=" +
            std::to_string(last_drain_age_ms) +
            " cancelled=" + (cancelled ? "true" : "false"));
  } catch (...) {
  }
}

} // namespace

DepthContext capture_depth_context() {
  DepthContext ctx;
  ctx.depth = g_depth;
  const tools::TraceContext trace = tools::capture_trace_context();
  ctx.trace_id = trace.trace_id;
  ctx.span_id = trace.span_id;
  ctx.request_id = trace.request_id;
  return ctx;
}

int current_depth() { return g_depth; }

ScopedDepthContext::ScopedDepthContext(const DepthContext &ctx)
    : saved_depth_(g_depth) {
  g_depth = ctx.depth < 0 ? 1 : ctx.depth + 1;
  tools::TraceContext trace;
  trace.trace_id = ctx.trace_id;
  trace.span_id = ctx.span_id;
  trace.request_id = ctx.request_id;
  trace_scope_.emplace(trace);
}

ScopedDepthContext::~ScopedDepthContext() { g_depth = saved_depth_; }

bool has_deferred_marker(const godot_autopilot::util::JsonValue &result) {
  if (!result.IsObject()) {
    return false;
  }
  return result.Find("__gda_pending") != nullptr ||
         result.Find("__gda_wait") != nullptr ||
         result.Find("__gda_job") != nullptr;
}

std::optional<std::string> resolve_alias_canonical(const std::string &name) {
  auto registry = get_active_registry();
  if (!registry) {
    return std::nullopt;
  }
  auto alias = registry->resolve_alias(name);
  if (!alias) {
    return std::nullopt;
  }
  return alias->canonical;
}

std::string dispatch_exception_message(const std::string &name,
                                       const std::string &reason,
                                       const godot_autopilot::util::JsonValue &args) {
  return "internal error in tool '" + name + "': " + reason + " (args: " +
         sanitized_args_preview(args) + ")";
}

godot_autopilot::util::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue(
      "editor main thread queue unavailable for tool '" + name + "': " +
      reason +
      " — the editor may be shutting down or overloaded; retry after the "
      "editor is responsive");
  result["code"] = godot_autopilot::util::JsonValue("queue_unavailable");
  result["retryable"] = godot_autopilot::util::JsonValue(true);
  return result;
}

godot_autopilot::util::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason,
                                        const CommandQueue::Stats &stats,
                                        int64_t oldest_pending_ms,
                                        int64_t last_drain_age_ms,
                                        int64_t waited_ms) {
  godot_autopilot::util::JsonValue result = queue_unavailable_result(name, reason);
  result["tool"] = godot_autopilot::util::JsonValue(name);
  result["waited_ms"] = godot_autopilot::util::JsonValue(waited_ms);
  result["queue_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(stats.pending));
  result["queue_capacity"] =
      godot_autopilot::util::JsonValue(static_cast<int64_t>(stats.capacity));
  result["oldest_pending_ms"] = godot_autopilot::util::JsonValue(oldest_pending_ms);
  result["last_drain_age_ms"] = godot_autopilot::util::JsonValue(last_drain_age_ms);
  return result;
}

godot_autopilot::util::JsonValue dispatch_timeout_result(const std::string &name,
                                       int64_t waited_ms,
                                       const CommandQueue::Stats &stats,
                                       int64_t oldest_pending_ms,
                                       int64_t last_drain_age_ms,
                                       bool cancelled_before_execution,
                                       bool health) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue(dispatch_timeout_message(
      name, waited_ms, dispatch_wait_health_ms(), health,
      cancelled_before_execution));
  result["code"] = godot_autopilot::util::JsonValue("main_thread_timeout");
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  result["tool"] = godot_autopilot::util::JsonValue(name);
  result["waited_ms"] = godot_autopilot::util::JsonValue(waited_ms);
  result["queue_depth"] = godot_autopilot::util::JsonValue(static_cast<int64_t>(stats.pending));
  result["oldest_pending_ms"] = godot_autopilot::util::JsonValue(oldest_pending_ms);
  result["last_drain_age_ms"] = godot_autopilot::util::JsonValue(last_drain_age_ms);
  result["cancelled"] = godot_autopilot::util::JsonValue(true);
  result["cancelled_before_execution"] =
      godot_autopilot::util::JsonValue(cancelled_before_execution);
  return result;
}

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers) {
  std::lock_guard<std::mutex> lock(g_handlers_mutex);
  g_handlers = std::move(handlers);
  g_meta_handlers = std::move(meta_handlers);
}

void clear_handlers() { replace_handlers({}, {}); }

int64_t dispatch_wait_long_blocking_ms() {
  static const int64_t value = env_int_clamped(
      "GODOT_AUTOPILOT_LONG_BLOCKING_TIMEOUT_MS",
      GDA_DISPATCH_WAIT_LONG_BLOCKING_MS, GDA_DISPATCH_WAIT_MIN_MS,
      GDA_DISPATCH_WAIT_MAX_MS);
  return value;
}

int64_t main_thread_wait_budget_ms(uint32_t flags, int64_t default_ms,
                                   int64_t health_ms,
                                   int64_t long_blocking_ms) {
  if ((flags & tool_flags::kLongBlocking) != 0) {
    return long_blocking_ms > 0 ? long_blocking_ms : default_ms;
  }
  if ((flags & tool_flags::kHealthProbe) != 0) {
    return health_ms;
  }
  return default_ms;
}

std::string main_thread_timeout_message(const std::string &label,
                                        int64_t waited_ms,
                                        bool cancelled_before_execution) {
  std::string message = "editor main thread did not process '" + label +
                        "' within " + std::to_string(waited_ms) + " ms";
  message += " — the editor may be paused, blocked by a modal dialog, "
             "exporting, or busy; ";
  if (cancelled_before_execution) {
    message += "the task was cancelled before execution, so it did not run";
  } else {
    message += "the main thread already picked the call up, so it may still "
               "execute";
  }
  message +=
      " (the call is cancelled — the side effect may still land in the "
      "editor, so verify the editor state before issuing it again)";
  return message;
}

godot_autopilot::util::JsonValue main_thread_timeout_result(const std::string &label,
                                          int64_t waited_ms,
                                          bool cancelled_before_execution) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue(
      main_thread_timeout_message(label, waited_ms, cancelled_before_execution));
  result["code"] = godot_autopilot::util::JsonValue("main_thread_timeout");
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  result["target"] = godot_autopilot::util::JsonValue(label);
  result["waited_ms"] = godot_autopilot::util::JsonValue(waited_ms);
  result["cancelled"] = godot_autopilot::util::JsonValue(true);
  result["cancelled_before_execution"] =
      godot_autopilot::util::JsonValue(cancelled_before_execution);
  return result;
}

godot_autopilot::util::JsonValue exec_timeout_result(int64_t timeout_ms,
                                   int64_t execution_time_ms) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue("execution timed out after " +
                                   std::to_string(timeout_ms) + " ms");
  result["code"] = godot_autopilot::util::JsonValue("exec_timeout");
  result["retryable"] = godot_autopilot::util::JsonValue(false);
  result["execution_time_ms"] = godot_autopilot::util::JsonValue(execution_time_ms);
  result["timeout_ms"] = godot_autopilot::util::JsonValue(timeout_ms);
  return result;
}

godot_autopilot::util::JsonValue run_on_main_thread_with_budget(
    CommandQueue &queue, const std::string &tool_name, uint32_t flags,
    std::function<godot_autopilot::util::JsonValue()> fn) {
  if (queue.is_main_thread()) {
    return fn();
  }
  int64_t budget_ms = main_thread_wait_budget_ms(
      flags, dispatch_wait_default_ms(), dispatch_wait_health_ms(),
      dispatch_wait_long_blocking_ms());
  if (budget_ms <= 0) {
    budget_ms = dispatch_wait_default_ms();
  }
  const auto submit_time = std::chrono::steady_clock::now();
  CommandQueue::TrackedSubmission<godot_autopilot::util::JsonValue> submission;
  try {
    const CommandQueue::Stats stats_before = queue.stats();
    if (stats_before.capacity > 0 &&
        stats_before.pending >= stats_before.capacity) {
      return queue_unavailable_result(tool_name,
                                      "the command queue is full",
                                      stats_before,
                                      queue.oldest_pending_age_ms(),
                                      queue.last_drain_age_ms(), 0);
    }
    if (queue.is_closed()) {
      return queue_unavailable_result(tool_name,
                                      "the command queue is closed",
                                      stats_before,
                                      queue.oldest_pending_age_ms(),
                                      queue.last_drain_age_ms(), 0);
    }
    submission = queue.submit_tracked(std::move(fn));
  } catch (const std::exception &ex) {
    return queue_unavailable_result(tool_name, std::string(ex.what()));
  } catch (...) {
    return queue_unavailable_result(tool_name, "unknown queue failure");
  }
  std::future_status status = std::future_status::timeout;
  try {
    status = submission.future.wait_for(std::chrono::milliseconds(budget_ms));
  } catch (const std::exception &ex) {
    return queue_unavailable_result(tool_name, std::string(ex.what()));
  } catch (...) {
    return queue_unavailable_result(tool_name, "unknown queue failure");
  }
  if (status == std::future_status::ready) {
    try {
      return submission.future.get();
    } catch (const CommandQueue::Rejected &ex) {
      return queue_unavailable_result(
          tool_name, ex.what(), queue.stats(),
          queue.oldest_pending_age_ms(), queue.last_drain_age_ms(),
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - submit_time)
              .count());
    } catch (const std::exception &ex) {
      return internal_error_result(tool_name, ex.what());
    } catch (...) {
      return internal_error_result(tool_name, "unexpected C++ exception");
    }
  }
  bool cancelled = false;
  int64_t waited_ms = budget_ms;
  CommandQueue::Stats stats;
  int64_t oldest_pending_ms = -1;
  int64_t last_drain_age_ms = -1;
  try {
    cancelled = queue.cancel(submission.id);
    waited_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - submit_time)
                    .count();
    stats = queue.stats();
    oldest_pending_ms = queue.oldest_pending_age_ms();
    last_drain_age_ms = queue.last_drain_age_ms();
  } catch (...) {
    cancelled = false;
  }
  emit_dispatch_timeout_observation(tool_name, waited_ms, stats,
                                    oldest_pending_ms, last_drain_age_ms,
                                    cancelled);
  return dispatch_timeout_result(
      tool_name, waited_ms, stats, oldest_pending_ms, last_drain_age_ms,
      cancelled, (flags & tool_flags::kHealthProbe) != 0);
}

godot_autopilot::util::JsonValue run_main_thread_slice(CommandQueue &queue, const std::string &label,
                                     std::function<godot_autopilot::util::JsonValue()> slice,
                                     int64_t budget_ms) {
  if (queue.is_main_thread()) {
    return slice();
  }
  const auto submit_time = std::chrono::steady_clock::now();
  CommandQueue::TrackedSubmission<godot_autopilot::util::JsonValue> submission;
  try {
    submission = queue.submit_tracked(std::move(slice));
  } catch (const std::exception &ex) {
    return queue_unavailable_result(label, ex.what());
  } catch (...) {
    return queue_unavailable_result(label, "unknown queue failure");
  }
  std::future_status status = std::future_status::timeout;
  try {
    status = submission.future.wait_for(std::chrono::milliseconds(budget_ms));
  } catch (const std::exception &ex) {
    return queue_unavailable_result(label, ex.what());
  } catch (...) {
    return queue_unavailable_result(label, "unknown queue failure");
  }
  if (status != std::future_status::ready) {
    bool cancelled = false;
    int64_t waited_ms = budget_ms;
    try {
      cancelled = queue.cancel(submission.id);
      const int64_t elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - submit_time)
              .count();
      waited_ms = elapsed_ms < budget_ms ? elapsed_ms : budget_ms;
    } catch (...) {
      cancelled = false;
    }
    return main_thread_timeout_result(label, waited_ms, cancelled);
  }
  try {
    return submission.future.get();
  } catch (const CommandQueue::Rejected &ex) {
    return queue_unavailable_result(label, ex.what());
  } catch (const std::exception &ex) {
    return internal_error_result(label, ex.what());
  } catch (...) {
    return internal_error_result(label, "unexpected C++ exception");
  }
}

godot_autopilot::util::JsonValue resolve_deferred_wait(const std::string &name,
                                      godot_autopilot::util::JsonValue result) {
  std::string deferred_trace_key;
  const bool has_deferred_trace =
      spec_detail::deferred_key_of(result, &deferred_trace_key);
  int64_t job_id = 0;
  const bool is_game_job =
      runtime_ops::payload_is_game_job(result, &job_id);
  const godot_autopilot::util::JsonValue *wait_p = result.Find("__gda_wait");
  const bool is_deferred_wait = wait_p && wait_p->IsObject();
  auto *pending_p = result.Find("__gda_pending");
  const bool is_pending_op = !is_game_job && pending_p && pending_p->IsInt();
  if (!is_game_job && !is_deferred_wait && !is_pending_op) {
    if (!result.IsObject()) {
      godot_autopilot::util::JsonValue wrapped(godot_autopilot::util::JsonValue::object_tag);
      wrapped["result"] = std::move(result);
      result = std::move(wrapped);
    }
    return result;
  }
  int64_t timeout = GDA_DEFAULT_TIMEOUT_MS;
  const godot_autopilot::util::JsonValue *timeout_source =
      is_deferred_wait ? wait_p->Find("timeout_ms") : result.Find("timeout_ms");
  if (timeout_source && timeout_source->IsInt() &&
      timeout_source->GetInt() > 0) {
    timeout = timeout_source->GetInt();
  }
  if (is_game_job) {
    result = runtime_ops::wait_game_job(job_id, timeout);
  } else if (is_deferred_wait) {
    const godot_autopilot::util::JsonValue *kind_p = wait_p->Find("kind");
    const godot_autopilot::util::JsonValue *id_p = wait_p->Find("id");
    const std::string kind =
        (kind_p && kind_p->IsString()) ? kind_p->GetString() : std::string();
    const int64_t id = (id_p && id_p->IsInt()) ? id_p->GetInt() : 0;
    if (kind == "os_process") {
      result = os_ops::wait_os_process(id, timeout);
    } else if (kind == "editor_scene_switch") {
      result = editor_ops::wait_scene_switch(id, timeout);
    } else if (kind == "csharp_build") {
      result = editor_ops::wait_csharp_build(*wait_p, timeout);
    } else {
      godot_autopilot::util::JsonValue error(godot_autopilot::util::JsonValue::object_tag);
      error["error"] = godot_autopilot::util::JsonValue("unknown deferred wait kind: " + kind);
      error["code"] = godot_autopilot::util::JsonValue("internal_error");
      error["retryable"] = godot_autopilot::util::JsonValue(false);
      result = std::move(error);
    }
  } else {
    const int64_t rid = pending_p->GetInt();
    godot_autopilot::util::JsonValue pending_options = result;
    godot_autopilot::util::JsonValue awaited = runtime_ops::wait_pending_response(rid, timeout);
    const bool is_capture_tool =
        name == "capture_game_viewport" || name == "capture_editor_viewport";
    if (is_capture_tool) {
      awaited =
          runtime_ops::finalize_capture_response(awaited, pending_options);
    }
    result = std::move(awaited);
  }
  if (!result.IsObject()) {
    godot_autopilot::util::JsonValue wrapped(godot_autopilot::util::JsonValue::object_tag);
    wrapped["result"] = std::move(result);
    result = std::move(wrapped);
  }
  if (has_deferred_trace) {
    spec_detail::finish_deferred_trace(deferred_trace_key, result);
  }
  if (auto alias = resolve_tool_alias(name)) {
    result = apply_alias_result(*alias, std::move(result));
  }
  return result;
}

godot_autopilot::util::JsonValue call_handler(const std::string &name,
                            const godot_autopilot::util::JsonValue &args,
                            const DepthContext &depth_ctx) {
  if (depth_ctx.depth >= tools::kMaxInvokeDepth) {
    return depth_exceeded_result(name);
  }
  if (godot_autopilot::ExportGuard::is_exporting()) {
    return export_blocked_json_result();
  }
  if (!godot_autopilot::runtime_ops::has_editor_queue()) {
    return queue_unavailable_result(
        name, "the plugin has not attached the editor main thread queue yet "
              "(startup) or has already detached it (shutdown)");
  }
  if (get_editor_queue().is_main_thread() || depth_ctx.run_on_calling_thread) {
    return call_handler_with_context(name, args, depth_ctx);
  }
  CommandQueue &queue = get_editor_queue();
  uint32_t flags = tool_flags::kNone;
  if (auto registry = get_active_registry()) {
    if (auto tool = registry->find_any(name)) {
      flags = tool->tool_flags();
    } else if (auto alias = registry->resolve_alias(name)) {
      if (auto tool = registry->find_any(alias->canonical)) {
        flags = tool->tool_flags();
      }
    }
  }
  const auto submit_time = std::chrono::steady_clock::now();
  return run_on_main_thread_with_budget(
      queue, name, flags,
      [name, args, submit_time, depth_ctx] {
        const int64_t waited_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - submit_time)
                .count();
        tools::set_queued_wait_ms(waited_ms);
        return call_handler_with_context(name, args, depth_ctx);
      });
}

godot_autopilot::util::JsonValue export_blocked_json_result() {
  godot_autopilot::util::JsonValue body(godot_autopilot::util::JsonValue::object_tag);
  body["error"] = godot_autopilot::util::JsonValue(export_blocked_error_text());
  body["code"] = godot_autopilot::util::JsonValue("export_in_progress");
  body["retryable"] = godot_autopilot::util::JsonValue(true);
  return body;
}

std::string export_blocked_error_text() {
  return "editor is exporting; retry after export completes";
}

} // namespace dispatch
} // namespace godot_autopilot
