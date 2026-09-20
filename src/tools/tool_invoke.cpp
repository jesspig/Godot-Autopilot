#include "tools/tool_invoke.hpp"

#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "core/log_persist.hpp"
#include "core/log_system.hpp"
#include "core/sanitize_policy.hpp"
#include "core/trace_recorder.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"
#include "util/error_util.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace tools {

namespace {

thread_local int g_depth = 0;
thread_local std::vector<std::pair<std::string, std::string>> g_span_stack;
thread_local int64_t g_queued_wait_ms = 0;

struct DepthGuard {
  DepthGuard() { ++g_depth; }
  ~DepthGuard() { --g_depth; }
};

void record_invoke_trace(const std::string &name, const mcp::JsonValue &args,
                         const mcp::JsonValue &result,
                         const std::string &error_code) {
  std::string trace = current_trace_id();
  if (trace.empty()) {
    trace = TraceRecorder::new_trace_id();
  }
  const std::string span = TraceRecorder::new_span_id();
  const std::string parent = current_span_id();
  const int64_t wall = TraceRecorder::wall_now_ms();
  const SanitizeResult sanitized =
      TraceRecorder::sanitize_args(args.Dump(), sanitize_policy::enabled());
  std::string err_text = error_code;
  if (const mcp::JsonValue *err = result.Find("error")) {
    if (err->IsString()) {
      err_text = err->GetString();
      if (err_text.size() > 256) {
        err_text.resize(256);
      }
    }
  }
  TraceEvent event;
  event.trace_id = trace;
  event.span_id = span;
  event.parent_span = parent;
  event.session_id = LogPersist::instance().session_id();
  event.tool = name;
  event.depth = g_depth;
  event.thread = invoke_thread_label();
  event.wall_start_ms = wall;
  event.wall_end_ms = wall;
  event.auth = "ok";
  event.ok = false;
  event.error_code = error_code;
  event.args_digest = sanitized.text;
  event.args_truncated = sanitized.truncated;
  event.result_size = static_cast<int64_t>(result.Dump().size());
  TraceRecorder::instance().record(std::move(event));
  std::string args_text = sanitized.text;
  if (args_text.size() > 512) {
    args_text.resize(512);
  }
  LogSystem::instance().log_detailed(
      LogLevel::Warning, LogCategory::Tools,
      "call_tool " + name + " -> error " + error_code,
      "trace=" + trace + " span=" + span + " depth=" +
          std::to_string(g_depth) + " queue=0ms args=" + args_text +
          " err=" + err_text);
}

} // namespace

int invoke_depth() { return g_depth; }

const std::string &current_trace_id() {
  static const std::string kNoSpan;
  if (g_span_stack.empty()) {
    return kNoSpan;
  }
  return g_span_stack.back().first;
}

const std::string &current_span_id() {
  static const std::string kNoSpan;
  if (g_span_stack.empty()) {
    return kNoSpan;
  }
  return g_span_stack.back().second;
}

void push_span(const std::string &trace_id, const std::string &span_id) {
  g_span_stack.emplace_back(trace_id, span_id);
}

void pop_span() {
  if (!g_span_stack.empty()) {
    g_span_stack.pop_back();
  }
}

TraceContext capture_trace_context() {
  TraceContext ctx;
  if (!g_span_stack.empty()) {
    ctx.trace_id = g_span_stack.back().first;
    ctx.span_id = g_span_stack.back().second;
  }
  return ctx;
}

ScopedTraceContext::ScopedTraceContext(const TraceContext &ctx) {
  if (!ctx.valid()) {
    return;
  }
  saved_ = g_span_stack;
  active_ = true;
  g_span_stack.emplace_back(ctx.trace_id, ctx.span_id);
}

ScopedTraceContext::~ScopedTraceContext() {
  if (active_) {
    g_span_stack = std::move(saved_);
  }
}

void set_queued_wait_ms(int64_t ms) { g_queued_wait_ms = ms; }

int64_t take_queued_wait_ms() {
  const int64_t value = g_queued_wait_ms;
  g_queued_wait_ms = 0;
  return value;
}

std::string invoke_thread_label() {
  if (!runtime_ops::has_editor_queue()) {
    return "main";
  }
  return get_editor_queue().is_main_thread() ? "main" : "non-main";
}

SpanGuard::SpanGuard(const std::string &trace_id, const std::string &span_id) {
  push_span(trace_id, span_id);
}

SpanGuard::~SpanGuard() { pop_span(); }

mcp::JsonValue invoke_tool(const std::string &name,
                           const mcp::JsonValue &args) {
  if (ExportGuard::is_exporting()) {
    mcp::JsonValue blocked = util::error_json(
        "editor is exporting; retry after export completes");
    record_invoke_trace(name, args, blocked, "export_blocked");
    return blocked;
  }
  if (g_depth > kMaxInvokeDepth) {
    mcp::JsonValue overflow = util::error_json(
        "tool invoke depth exceeded (max 8): '" + name + "'");
    record_invoke_trace(name, args, overflow, "depth_exceeded");
    return overflow;
  }
  std::string trace = current_trace_id();
  if (trace.empty()) {
    trace = TraceRecorder::new_trace_id();
  }
  const std::string span = TraceRecorder::new_span_id();
  mcp::JsonValue result;
  {
    SpanGuard span_guard(trace, span);
    DepthGuard guard;
    result = dispatch::call_handler(name, args);
  }
  if (result.IsObject()) {
    const mcp::JsonValue *pending = result.Find("__gda_pending");
    if (pending != nullptr && pending->IsInt()) {
      mcp::JsonValue rejected = util::error_json(
          "cannot invoke async tool '" + name +
          "' from within another tool on the main thread");
      record_invoke_trace(name, args, rejected, "async_nested");
      return rejected;
    }
  }
  return result;
}

} // namespace tools
} // namespace godot_autopilot
