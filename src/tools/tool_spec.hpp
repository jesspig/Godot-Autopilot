#ifndef GODOT_AUTOPILOT_TOOL_SPEC_HPP
#define GODOT_AUTOPILOT_TOOL_SPEC_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mcp/JsonValue.hpp>
#include <core/command_queue.hpp>
#include <core/log_persist.hpp>
#include <core/log_system.hpp>
#include <core/trace_recorder.hpp>
#include "core/monitor.hpp"
#include "core/sanitize_policy.hpp"
#include <tools/runtime_ops.hpp>
#include <tools/schema_builder.hpp>
#include <tools/tool_base.hpp>
#include <tools/tool_invoke.hpp>

namespace godot_autopilot {

using ParamSpec = schema::ParamDef;

namespace tool_flags {
constexpr uint32_t kNone = 0;
constexpr uint32_t kMeta = 1u << 0;
constexpr uint32_t kDynamic = 1u << 1;
constexpr uint32_t kMutating = 1u << 2;
constexpr uint32_t kObserve = 1u << 3;
constexpr uint32_t kCaptureImage = 1u << 4;
constexpr uint32_t kSceneTarget = 1u << 5;
constexpr uint32_t kUndoable = 1u << 6;
} // namespace tool_flags

struct ToolSpec {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> tags;
  SideEffect side_effect = SideEffect::None;
  uint32_t flags = tool_flags::kNone;
  std::vector<ParamSpec> params;
  std::function<mcp::JsonValue(const mcp::JsonValue &)> handler;
  mcp::JsonValue raw_schema;
};

namespace spec_detail {

constexpr int64_t kSlowToolMs = 2000;
constexpr std::size_t kArgsDetailMax = 512;
constexpr std::size_t kErrDetailMax = 256;

inline std::string error_code_of(const mcp::JsonValue &result) {
  if (const mcp::JsonValue *structured = result.Find("structured_error")) {
    if (structured->IsObject()) {
      if (const mcp::JsonValue *code = structured->Find("code")) {
        if (code->IsString()) {
          return code->GetString();
        }
      }
    }
  }
  return "error";
}

inline std::string truncated_text(const std::string &text, std::size_t max) {
  if (text.size() <= max) {
    return text;
  }
  return text.substr(0, max) + " (truncated)";
}

struct ImageHit {
  std::string base64;
  std::string kind;
  int width = 0;
  int height = 0;
};

inline void append_image_hit(std::vector<ImageHit> &hits,
                             const mcp::JsonValue &scope, const char *key,
                             const char *kind) {
  const mcp::JsonValue *value = scope.Find(key);
  if (!value || !value->IsString()) {
    return;
  }
  ImageHit hit;
  hit.base64 = value->GetString();
  hit.kind = kind;
  if (const mcp::JsonValue *width = scope.Find("width");
      width && width->IsInt()) {
    hit.width = static_cast<int>(width->GetInt());
  }
  if (const mcp::JsonValue *height = scope.Find("height");
      height && height->IsInt()) {
    hit.height = static_cast<int>(height->GetInt());
  }
  hits.push_back(std::move(hit));
}

inline std::vector<ImageHit> collect_trace_images(const mcp::JsonValue &result) {
  std::vector<ImageHit> hits;
  const mcp::JsonValue *scopes[2] = {nullptr, nullptr};
  if (result.IsObject()) {
    scopes[0] = &result;
    if (const mcp::JsonValue *inner = result.Find("result");
        inner && inner->IsObject()) {
      scopes[1] = inner;
    }
  }
  for (const mcp::JsonValue *scope : scopes) {
    if (!scope) {
      continue;
    }
    if (const mcp::JsonValue *format = scope->Find("format");
        format && format->IsString() && format->GetString() == "png") {
      append_image_hit(hits, *scope, "data", "cap");
    }
    append_image_hit(hits, *scope, "diff_image_data", "diff");
  }
  return hits;
}

} // namespace spec_detail

namespace pipeline {

mcp::JsonValue run_post(const ToolSpec &spec, const mcp::JsonValue &args,
                        mcp::JsonValue result);

} // namespace pipeline

class SpecTool : public ToolBase, public ISideEffect {
public:
  explicit SpecTool(ToolSpec spec);

  const ToolMeta &meta() const override;
  mcp::JsonValue execute(const mcp::JsonValue &args) override;
  mcp::JsonValue input_schema() const override;
  SideEffect side_effects() const override;
  uint32_t tool_flags() const override;
  const ToolSpec &spec() const;

private:
  ToolSpec spec_;
  ToolMeta meta_;
  mcp::JsonValue schema_;
};

inline SpecTool::SpecTool(ToolSpec spec) : spec_(std::move(spec)) {
  schema_ = spec_.raw_schema.IsObject() ? spec_.raw_schema
                                        : schema::build_schema(spec_.params);
  meta_ = ToolMeta{spec_.name, spec_.description, spec_.category, spec_.tags};
}

inline const ToolMeta &SpecTool::meta() const { return meta_; }

inline mcp::JsonValue SpecTool::execute(const mcp::JsonValue &args) {
  const int64_t wall_start = TraceRecorder::wall_now_ms();
  const auto steady_start = std::chrono::steady_clock::now();
  const std::string parent_span = tools::current_span_id();
  std::string trace_id = tools::current_trace_id();
  if (trace_id.empty()) {
    trace_id = TraceRecorder::new_trace_id();
  }
  const std::string span_id = TraceRecorder::new_span_id();
  tools::SpanGuard span_guard(trace_id, span_id);
  const int64_t queue_wait = tools::take_queued_wait_ms();
  const std::string thread = tools::invoke_thread_label();
  const int depth = tools::invoke_depth();
  const SanitizeResult sanitized =
      TraceRecorder::sanitize_args(args.Dump(), sanitize_policy::enabled());
  mcp::JsonValue denied =
      authorization::deny_if_unauthorized(spec_.name, spec_.side_effect);
  if (!denied.IsNull()) {
    const char *capability =
        authorization::capability_for_tool(spec_.name, spec_.side_effect);
    const std::string denied_error =
        std::string("denied:") + (capability ? capability : "unknown");
    TraceEvent denied_event;
    denied_event.trace_id = trace_id;
    denied_event.span_id = span_id;
    denied_event.parent_span = parent_span;
    denied_event.session_id = LogPersist::instance().session_id();
    denied_event.tool = spec_.name;
    denied_event.category = spec_.category;
    denied_event.flags = spec_.flags;
    denied_event.side_effect = static_cast<int>(spec_.side_effect);
    denied_event.depth = depth;
    denied_event.thread = thread;
    denied_event.queue_wait_ms = queue_wait;
    denied_event.duration_ms = 0;
    denied_event.wall_start_ms = wall_start;
    denied_event.wall_end_ms = TraceRecorder::wall_now_ms();
    denied_event.auth = "denied";
    denied_event.ok = false;
    denied_event.error_code = "denied";
    denied_event.args_digest = sanitized.text;
    denied_event.args_truncated = sanitized.truncated;
    denied_event.result_size = static_cast<int64_t>(denied.Dump().size());
    denied_event.name = spec_.name;
    denied_event.request_id = monitor::current_request_id();
    denied_event.thread_id = TraceRecorder::current_thread_id();
    denied_event.monotonic_ns = TraceRecorder::monotonic_now_ns();
    const std::string denied_summary =
        "call_tool " + spec_.name + " -> error 0ms";
    const std::string denied_detail =
        "trace=" + trace_id + " span=" + span_id + " parent=" + parent_span +
        " depth=" + std::to_string(depth) + " queue=" +
        std::to_string(queue_wait) + "ms dur=0ms auth=denied err=" +
        denied_error + " args=" +
        spec_detail::truncated_text(sanitized.text,
                                    spec_detail::kArgsDetailMax);
    monitor::tool_call(std::move(denied_event), LogLevel::Warning,
                       denied_summary, denied_detail);
    return denied;
  }
  mcp::JsonValue result = spec_.handler(args);
  result = pipeline::run_post(spec_, args, std::move(result));
  const int64_t duration_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - steady_start)
          .count();
  const bool ok =
      !(result.IsObject() && result.Find("error") != nullptr);
  const std::string error_code =
      ok ? std::string() : spec_detail::error_code_of(result);
  std::string err_text;
  if (!ok) {
    if (const mcp::JsonValue *err = result.Find("error")) {
      if (err->IsString()) {
        err_text = spec_detail::truncated_text(err->GetString(),
                                               spec_detail::kErrDetailMax);
      }
    }
  }
  bool retryable = false;
  if (const mcp::JsonValue *retryable_flag = result.Find("retryable")) {
    retryable = retryable_flag->IsBool() && retryable_flag->GetBool();
  }
  const bool slow = duration_ms > spec_detail::kSlowToolMs;
  TraceEvent event;
  event.trace_id = trace_id;
  event.span_id = span_id;
  event.parent_span = parent_span;
  event.session_id = LogPersist::instance().session_id();
  event.tool = spec_.name;
  event.category = spec_.category;
  event.flags = spec_.flags;
  event.side_effect = static_cast<int>(spec_.side_effect);
  event.depth = depth;
  event.thread = thread;
  event.queue_wait_ms = queue_wait;
  event.duration_ms = duration_ms;
  event.wall_start_ms = wall_start;
  event.wall_end_ms = TraceRecorder::wall_now_ms();
  event.auth = "ok";
  event.ok = ok;
  event.error_code = error_code;
  event.args_digest = sanitized.text;
  event.args_truncated = sanitized.truncated;
  event.result_size = static_cast<int64_t>(result.Dump().size());
  const std::vector<spec_detail::ImageHit> images =
      spec_detail::collect_trace_images(result);
  std::string image_detail;
  for (const spec_detail::ImageHit &image : images) {
    const std::string image_hash = TraceRecorder::fnv1a_hex(image.base64);
    const int64_t image_bytes = static_cast<int64_t>(image.base64.size());
    const std::string image_ref = LogPersist::instance().store_trace_image(
        span_id, image.kind, image.base64);
    event.image_bytes = image_bytes;
    event.image_hash = image_hash;
    event.image_width = image.width;
    event.image_height = image.height;
    if (!image_ref.empty()) {
      event.image_ref = image_ref;
      image_detail += " image=" + image_ref;
    } else {
      image_detail += " image=" + image_hash;
    }
    image_detail += " img_bytes=" + std::to_string(image_bytes);
  }
  event.name = spec_.name;
  event.request_id = monitor::current_request_id();
  event.thread_id = TraceRecorder::current_thread_id();
  event.monotonic_ns = TraceRecorder::monotonic_now_ns();
  std::string detail = "trace=" + trace_id + " span=" + span_id + " parent=" +
                       parent_span + " depth=" + std::to_string(depth) +
                       " queue=" + std::to_string(queue_wait) + "ms dur=" +
                       std::to_string(duration_ms) + "ms auth=ok err=" +
                       err_text + " args=" +
                       spec_detail::truncated_text(
                           sanitized.text, spec_detail::kArgsDetailMax);
  if (slow) {
    detail += " slow_tool=true";
  }
  if (retryable) {
    detail += " retryable=true";
  }
  detail += image_detail;
  LogLevel level = LogLevel::Debug;
  if (!ok || slow) {
    level = LogLevel::Warning;
  }
  const std::string summary = "call_tool " + spec_.name + " -> " +
                              (ok ? "ok " : "error ") +
                              std::to_string(duration_ms) + "ms";
  monitor::tool_call(std::move(event), level, summary, detail);
  return result;
}

inline mcp::JsonValue SpecTool::input_schema() const { return schema_; }

inline SideEffect SpecTool::side_effects() const { return spec_.side_effect; }

inline uint32_t SpecTool::tool_flags() const { return spec_.flags; }

inline const ToolSpec &SpecTool::spec() const { return spec_; }

inline std::unique_ptr<ToolBase> make_spec_tool(ToolSpec spec) {
  return std::make_unique<SpecTool>(std::move(spec));
}

} // namespace godot_autopilot

#endif
