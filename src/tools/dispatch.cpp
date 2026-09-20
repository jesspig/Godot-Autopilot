#include "core/command_queue.hpp"
#include "core/log_persist.hpp"
#include "core/log_system.hpp"
#include "core/sanitize_policy.hpp"
#include "core/trace_recorder.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_invoke.hpp"
#include <mcp/Content.hpp>
#include <chrono>
#include <cstdint>
#include <exception>
#include <mutex>
#include <utility>

namespace godot_autopilot {
namespace dispatch {

namespace {

std::mutex g_handlers_mutex;
HandlerMap g_handlers;
HandlerMap g_meta_handlers;

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
  TraceRecorder::instance().record(std::move(event));
  std::string args_text = sanitized.text;
  if (args_text.size() > 512) {
    args_text.resize(512);
  }
  LogSystem::instance().log_detailed(
      LogLevel::Error, LogCategory::Tools,
      "call_tool " + name + " -> error internal",
      "trace=" + trace + " span=" + span + " depth=" +
          std::to_string(tools::invoke_depth()) +
          " queue=0ms args=" + args_text + " err=internal");
}

mcp::JsonValue call_handler_impl(const std::string &name,
                                 const mcp::JsonValue &args) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  HandlerFn handler;
  {
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
      std::string args_dump = args.Dump();
      if (args_dump.size() > 256) {
        args_dump.resize(256);
      }
      result = mcp::JsonValue(mcp::JsonValue::object_tag);
      result["error"] = mcp::JsonValue(
          "internal error in tool '" + name + "': " + std::string(ex.what()) +
          " (args: " + args_dump + ")");
      record_dispatch_exception(name, args, result);
    } catch (...) {
      std::string args_dump = args.Dump();
      if (args_dump.size() > 256) {
        args_dump.resize(256);
      }
      result = mcp::JsonValue(mcp::JsonValue::object_tag);
      result["error"] = mcp::JsonValue("internal error in tool '" + name +
                                       "': unexpected C++ exception (args: " +
                                       args_dump + ")");
      record_dispatch_exception(name, args, result);
    }
  } else {
    result = mcp::JsonValue(mcp::JsonValue::object_tag);
    result["error"] =
        mcp::JsonValue("domain tool '" + name +
                       "' not found — use search_tools to discover available "
                       "tools");
  }
  bool has_error = result.IsObject() && result.Find("error") != nullptr;
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
      "call_tool: " + name + " -> " + (has_error ? "error" : "ok"));
  tools::take_queued_wait_ms();
  return result;
}

} // namespace

void replace_handlers(HandlerMap handlers, HandlerMap meta_handlers) {
  std::lock_guard<std::mutex> lock(g_handlers_mutex);
  g_handlers = std::move(handlers);
  g_meta_handlers = std::move(meta_handlers);
}

void clear_handlers() { replace_handlers({}, {}); }

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args) {
  if (!godot_autopilot::runtime_ops::has_editor_queue() ||
      get_editor_queue().is_main_thread()) {
    return call_handler_impl(name, args);
  }
  const auto submit_time = std::chrono::steady_clock::now();
  const tools::TraceContext trace_context = tools::capture_trace_context();
  return get_editor_queue().execute_sync(
      [name, args, submit_time, trace_context] {
        tools::ScopedTraceContext restore(trace_context);
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
  err.content.push_back(mcp::TextContent{
      "text", R"({"error":"editor is exporting; retry after export completes"})"});
  return err;
}

} // namespace dispatch
} // namespace godot_autopilot
