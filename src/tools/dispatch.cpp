#include "core/command_queue.hpp"
#include "core/config.hpp"
#include "core/log_persist.hpp"
#include "core/log_system.hpp"
#include "core/monitor.hpp"
#include "core/sanitize_policy.hpp"
#include "core/trace_recorder.hpp"
#include "tools/dispatch.hpp"
#include "tools/editor_ops.hpp"
#include "tools/os_ops.hpp"
#include "tools/register_all.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_invoke.hpp"
#include "tools/tool_spec.hpp"
#include <mcp/Content.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <mutex>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace dispatch {

namespace {

std::mutex g_handlers_mutex;
HandlerMap g_handlers;
HandlerMap g_meta_handlers;

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

std::string sanitized_args_preview(const mcp::JsonValue &args) {
  const SanitizeResult sanitized =
      TraceRecorder::sanitize_args(args.Dump(), sanitize_policy::enabled());
  return mcp::JsonValue::SanitizeUtf8(
      truncate_utf8_prefix(sanitized.text, kErrorArgsPreviewBytes));
}

void record_dispatch_exception(const std::string &name,
                               const mcp::JsonValue &args,
                               const mcp::JsonValue &result) {
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

std::string received_keys_suffix(const mcp::JsonValue &args) {
  if (!args.IsObject() || args.GetObject().empty()) {
    return " (received no parameters)";
  }
  std::string text = " (received keys: ";
  bool first = true;
  for (const auto &entry : args.GetObject()) {
    if (!first) {
      text += ", ";
    }
    first = false;
    text += entry.first;
  }
  text += ")";
  return text;
}

mcp::JsonValue call_handler_impl(const std::string &name,
                                 const mcp::JsonValue &args) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  HandlerFn handler;
  {
    monitor::LockProbe probe("dispatch_handlers_lock", 5.0);
    std::lock_guard<std::mutex> lock(g_handlers_mutex);
    auto it = g_handlers.find(name);
    if (it != g_handlers.end()) {
      handler = it->second;
    } else {
      auto meta_it = g_meta_handlers.find(name);
      if (meta_it != g_meta_handlers.end()) {
        handler = meta_it->second;
      }
    }
  }
  if (handler) {
    try {
      result = handler(args);
    } catch (const std::exception &ex) {
      result = mcp::JsonValue(mcp::JsonValue::object_tag);
      result["error"] =
          mcp::JsonValue(dispatch_exception_message(name, ex.what(), args));
      record_dispatch_exception(name, args, result);
    } catch (...) {
      result = mcp::JsonValue(mcp::JsonValue::object_tag);
      result["error"] = mcp::JsonValue(
          dispatch_exception_message(name, "unexpected C++ exception", args));
      record_dispatch_exception(name, args, result);
    }
  } else {
    result = mcp::JsonValue(mcp::JsonValue::object_tag);
    result["error"] =
        mcp::JsonValue("domain tool '" + name +
                       "' not found" + received_keys_suffix(args) +
                       " — use search_tools to discover available "
                       "tools");
  }
  bool has_error = result.IsObject() && result.Find("error") != nullptr;
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
      "call_tool: " + name + " -> " + (has_error ? "error" : "ok"));
  tools::take_queued_wait_ms();
  return result;
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

mcp::JsonValue internal_error_result(const std::string &name,
                                     const std::string &reason) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] =
      mcp::JsonValue("tool '" + name + "' failed: " + reason);
  result["code"] = mcp::JsonValue("internal_error");
  result["retryable"] = mcp::JsonValue(false);
  result["tool"] = mcp::JsonValue(name);
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
    mcp::JsonValue line(mcp::JsonValue::object_tag);
    line["type"] = mcp::JsonValue("dispatch_timeout");
    line["tool"] = mcp::JsonValue(name);
    line["waited_ms"] = mcp::JsonValue(waited_ms);
    line["queue_depth"] = mcp::JsonValue(static_cast<int64_t>(stats.pending));
    line["oldest_pending_ms"] = mcp::JsonValue(oldest_pending_ms);
    line["last_drain_age_ms"] = mcp::JsonValue(last_drain_age_ms);
    line["cancelled"] = mcp::JsonValue(cancelled);
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

std::string dispatch_exception_message(const std::string &name,
                                       const std::string &reason,
                                       const mcp::JsonValue &args) {
  return "internal error in tool '" + name + "': " + reason + " (args: " +
         sanitized_args_preview(args) + ")";
}

mcp::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(
      "editor main thread queue unavailable for tool '" + name + "': " +
      reason +
      " — the editor may be shutting down or overloaded; retry after the "
      "editor is responsive");
  result["code"] = mcp::JsonValue("queue_unavailable");
  result["retryable"] = mcp::JsonValue(true);
  return result;
}

mcp::JsonValue queue_unavailable_result(const std::string &name,
                                        const std::string &reason,
                                        const CommandQueue::Stats &stats,
                                        int64_t oldest_pending_ms,
                                        int64_t last_drain_age_ms,
                                        int64_t waited_ms) {
  mcp::JsonValue result = queue_unavailable_result(name, reason);
  result["tool"] = mcp::JsonValue(name);
  result["waited_ms"] = mcp::JsonValue(waited_ms);
  result["queue_depth"] = mcp::JsonValue(static_cast<int64_t>(stats.pending));
  result["queue_capacity"] =
      mcp::JsonValue(static_cast<int64_t>(stats.capacity));
  result["oldest_pending_ms"] = mcp::JsonValue(oldest_pending_ms);
  result["last_drain_age_ms"] = mcp::JsonValue(last_drain_age_ms);
  return result;
}

mcp::JsonValue dispatch_timeout_result(const std::string &name,
                                       int64_t waited_ms,
                                       const CommandQueue::Stats &stats,
                                       int64_t oldest_pending_ms,
                                       int64_t last_drain_age_ms,
                                       bool cancelled_before_execution,
                                       bool health) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(dispatch_timeout_message(
      name, waited_ms, dispatch_wait_health_ms(), health,
      cancelled_before_execution));
  result["code"] = mcp::JsonValue("main_thread_timeout");
  result["retryable"] = mcp::JsonValue(false);
  result["tool"] = mcp::JsonValue(name);
  result["waited_ms"] = mcp::JsonValue(waited_ms);
  result["queue_depth"] = mcp::JsonValue(static_cast<int64_t>(stats.pending));
  result["oldest_pending_ms"] = mcp::JsonValue(oldest_pending_ms);
  result["last_drain_age_ms"] = mcp::JsonValue(last_drain_age_ms);
  result["cancelled"] = mcp::JsonValue(true);
  result["cancelled_before_execution"] =
      mcp::JsonValue(cancelled_before_execution);
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

mcp::JsonValue main_thread_timeout_result(const std::string &label,
                                          int64_t waited_ms,
                                          bool cancelled_before_execution) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(
      main_thread_timeout_message(label, waited_ms, cancelled_before_execution));
  result["code"] = mcp::JsonValue("main_thread_timeout");
  result["retryable"] = mcp::JsonValue(false);
  result["target"] = mcp::JsonValue(label);
  result["waited_ms"] = mcp::JsonValue(waited_ms);
  result["cancelled"] = mcp::JsonValue(true);
  result["cancelled_before_execution"] =
      mcp::JsonValue(cancelled_before_execution);
  return result;
}

mcp::JsonValue exec_timeout_result(int64_t timeout_ms,
                                   int64_t execution_time_ms) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue("execution timed out after " +
                                   std::to_string(timeout_ms) + " ms");
  result["code"] = mcp::JsonValue("exec_timeout");
  result["retryable"] = mcp::JsonValue(false);
  result["execution_time_ms"] = mcp::JsonValue(execution_time_ms);
  result["timeout_ms"] = mcp::JsonValue(timeout_ms);
  return result;
}

mcp::JsonValue run_on_main_thread_with_budget(
    CommandQueue &queue, const std::string &tool_name, uint32_t flags,
    std::function<mcp::JsonValue()> fn) {
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
  CommandQueue::TrackedSubmission<mcp::JsonValue> submission;
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

mcp::JsonValue run_main_thread_slice(CommandQueue &queue, const std::string &label,
                                     std::function<mcp::JsonValue()> slice,
                                     int64_t budget_ms) {
  if (queue.is_main_thread()) {
    return slice();
  }
  const auto submit_time = std::chrono::steady_clock::now();
  CommandQueue::TrackedSubmission<mcp::JsonValue> submission;
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

mcp::JsonValue resolve_deferred_wait(const std::string &name,
                                      mcp::JsonValue result) {
  int64_t job_id = 0;
  const bool is_game_job =
      runtime_ops::payload_is_game_job(result, &job_id);
  const mcp::JsonValue *wait_p = result.Find("__gda_wait");
  const bool is_deferred_wait = wait_p && wait_p->IsObject();
  auto *pending_p = result.Find("__gda_pending");
  const bool is_pending_op = !is_game_job && pending_p && pending_p->IsInt();
  if (!is_game_job && !is_deferred_wait && !is_pending_op) {
    if (!result.IsObject()) {
      mcp::JsonValue wrapped(mcp::JsonValue::object_tag);
      wrapped["result"] = std::move(result);
      result = std::move(wrapped);
    }
    return result;
  }
  int64_t timeout = GDA_DEFAULT_TIMEOUT_MS;
  const mcp::JsonValue *timeout_source =
      is_deferred_wait ? wait_p->Find("timeout_ms") : result.Find("timeout_ms");
  if (timeout_source && timeout_source->IsInt() &&
      timeout_source->GetInt() > 0) {
    timeout = timeout_source->GetInt();
  }
  if (is_game_job) {
    result = runtime_ops::wait_game_job(job_id, timeout);
  } else if (is_deferred_wait) {
    const mcp::JsonValue *kind_p = wait_p->Find("kind");
    const mcp::JsonValue *id_p = wait_p->Find("id");
    const std::string kind =
        (kind_p && kind_p->IsString()) ? kind_p->GetString() : std::string();
    const int64_t id = (id_p && id_p->IsInt()) ? id_p->GetInt() : 0;
    if (kind == "os_process") {
      result = os_ops::wait_os_process(id, timeout);
    } else if (kind == "editor_scene_switch") {
      result = editor_ops::wait_scene_switch(id, timeout);
    } else {
      mcp::JsonValue error(mcp::JsonValue::object_tag);
      error["error"] = mcp::JsonValue("unknown deferred wait kind: " + kind);
      error["code"] = mcp::JsonValue("internal_error");
      error["retryable"] = mcp::JsonValue(false);
      result = std::move(error);
    }
  } else {
    const int64_t rid = pending_p->GetInt();
    mcp::JsonValue pending_options = result;
    mcp::JsonValue awaited = runtime_ops::wait_pending_response(rid, timeout);
    if (name == "capture_game_viewport" ||
        (awaited.IsObject() && awaited.Contains("path"))) {
      awaited =
          runtime_ops::finalize_capture_response(awaited, pending_options);
    }
    result = std::move(awaited);
  }
  if (!result.IsObject()) {
    mcp::JsonValue wrapped(mcp::JsonValue::object_tag);
    wrapped["result"] = std::move(result);
    result = std::move(wrapped);
  }
  return result;
}

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args) {
  if (!godot_autopilot::runtime_ops::has_editor_queue()) {
    return queue_unavailable_result(
        name, "the plugin has not attached the editor main thread queue yet "
              "(startup) or has already detached it (shutdown)");
  }
  if (get_editor_queue().is_main_thread()) {
    return call_handler_impl(name, args);
  }
  CommandQueue &queue = get_editor_queue();
  uint32_t flags = tool_flags::kNone;
  if (auto registry = get_active_registry()) {
    if (auto tool = registry->find_any(name)) {
      flags = tool->tool_flags();
    }
  }
  const auto submit_time = std::chrono::steady_clock::now();
  const tools::TraceContext trace_context = tools::capture_trace_context();
  const std::string request_id = monitor::current_request_id();
  return run_on_main_thread_with_budget(
      queue, name, flags,
      [name, args, submit_time, trace_context, request_id] {
        tools::ScopedTraceContext restore(trace_context);
        monitor::RequestScope request_scope(request_id);
        const int64_t waited_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - submit_time)
                .count();
        tools::set_queued_wait_ms(waited_ms);
        return call_handler_impl(name, args);
      });
}

mcp::CallToolResult export_blocked_result() {
  mcp::CallToolResult err;
  err.is_error = true;
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["error"] = mcp::JsonValue(export_blocked_error_text());
  err.content.push_back(mcp::TextContent{"text", body.Dump()});
  return err;
}

std::string export_blocked_error_text() {
  return "editor is exporting; retry after export completes";
}

} // namespace dispatch
} // namespace godot_autopilot
