#ifndef GODOT_AUTOPILOT_NATIVE_CALL_HPP
#define GODOT_AUTOPILOT_NATIVE_CALL_HPP

#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "core/control/trace_service.hpp"
#include "core/error_watermark.hpp"
#include "core/export_guard.hpp"
#include "core/observability/monitor.hpp"
#include "tools/base/confirmation.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/register_all.hpp"
#include "util/tool_image_content.hpp"
#include "tools/base/tool_registry.hpp"
#include "util/json_value.hpp"

namespace gda {
namespace control {
namespace native {

using JsonValue = godot_autopilot::util::JsonValue;

struct NativeCallResult {
  JsonValue result;
  bool pending = false;
  std::string traceparent;
  JsonValue payload;
  std::string tool;
  std::string deferred_tool;
};

inline JsonValue error_result(const std::string &message, const std::string &code) {
  JsonValue out(JsonValue::object_tag);
  out["error"] = JsonValue(message);
  out["code"] = JsonValue(code);
  out["retryable"] = JsonValue(false);
  return out;
}

inline JsonValue wrap_result(const JsonValue &payload, bool is_error,
                             const std::vector<JsonValue> &images) {
  JsonValue content(JsonValue::array_tag);
  JsonValue text(JsonValue::object_tag);
  text["type"] = JsonValue("text");
  text["text"] = JsonValue(payload.Dump());
  content.PushBack(std::move(text));
  for (const JsonValue &image : images) {
    content.PushBack(image);
  }
  JsonValue out(JsonValue::object_tag);
  out["content"] = std::move(content);
  out["isError"] = JsonValue(is_error);
  if (const JsonValue *code = payload.Find("code");
      code != nullptr && code->IsString() &&
      code->GetString() == "confirmation_required") {
    out["needs_confirmation"] = JsonValue(true);
  }
  return out;
}

inline std::string jsonrpc_result(const JsonValue &id, const JsonValue &result) {
  JsonValue out(JsonValue::object_tag);
  out["jsonrpc"] = JsonValue("2.0");
  out["id"] = id;
  out["result"] = result;
  return out.Dump();
}

inline std::string jsonrpc_error(const JsonValue &id, int code,
                                 const std::string &message) {
  JsonValue out(JsonValue::object_tag);
  out["jsonrpc"] = JsonValue("2.0");
  out["id"] = id;
  JsonValue failure(JsonValue::object_tag);
  failure["code"] = JsonValue(static_cast<int64_t>(code));
  failure["message"] = JsonValue(message);
  out["error"] = std::move(failure);
  return out.Dump();
}

inline NativeCallResult call_tool(const std::string &tool, const JsonValue &args,
                                  const std::string &traceparent = std::string(),
                                  const JsonValue *input_responses = nullptr) {
  NativeCallResult out;
  out.tool = tool;
  out.traceparent = traceparent;
  if (out.traceparent.empty()) {
    out.traceparent = TraceService::make_traceparent(
        TraceService::new_trace_id(), TraceService::new_span_id());
  }
  if (godot_autopilot::ExportGuard::is_exporting()) {
    out.payload = godot_autopilot::dispatch::export_blocked_json_result();
    out.result = wrap_result(out.payload, true, {});
    return out;
  }
  JsonValue arguments = args;
  if (!arguments.IsObject()) {
    out.payload = error_result("tool arguments must be an object", "invalid_arguments");
    out.result = wrap_result(out.payload, true, {});
    return out;
  }
  const bool is_call_tool = tool == "call_tool";
  std::string target_name = tool;
  if (is_call_tool) {
    if (const JsonValue *name = arguments.Find("name");
        name != nullptr && name->IsString()) {
      target_name = name->GetString();
    }
  }
  out.deferred_tool = target_name;
  godot_autopilot::SideEffect effect = godot_autopilot::SideEffect::None;
  auto registry = godot_autopilot::get_active_registry();
  if (registry != nullptr) {
    auto target = registry->find_any(target_name);
    if (target != nullptr) {
      effect = godot_autopilot::side_effect_of(*target);
    }
  }
  std::string confirm_error;
  const godot_autopilot::confirmation::ConfirmationResolution resolution =
      godot_autopilot::confirmation::resolve_confirmation(
          arguments, is_call_tool, input_responses, &confirm_error);
  if (resolution.decision ==
      godot_autopilot::confirmation::ConfirmationDecision::kMalformed) {
    out.payload = error_result("invalid_input_response: " + confirm_error,
                               "invalid_input_response");
    out.result = wrap_result(out.payload, true, {});
    return out;
  }
  if (resolution.decision ==
      godot_autopilot::confirmation::ConfirmationDecision::kDeclined) {
    out.payload = error_result("confirmation declined: the tool was not executed",
                               "confirmation_declined");
    out.result = wrap_result(out.payload, true, {});
    return out;
  }
  const bool accepted =
      resolution.decision ==
      godot_autopilot::confirmation::ConfirmationDecision::kAccepted;
  if (godot_autopilot::confirmation::required_for(effect) && !accepted) {
    out.payload = godot_autopilot::confirmation::error(target_name, effect);
    out.result = wrap_result(out.payload, true, {});
    return out;
  }
  if (accepted) {
    arguments["confirm"] = JsonValue(true);
    if (is_call_tool) {
      if (JsonValue *nested = arguments.Find("arguments");
          nested != nullptr && nested->IsObject()) {
        (*nested)["confirm"] = JsonValue(true);
      }
    }
  }
  const SpanContext span = TraceService::parse(out.traceparent);
  godot_autopilot::tools::RequestSpanGuard span_guard(span.trace_id,
                                                      span.span_id);
  godot_autopilot::monitor::RequestScope request_scope(out.traceparent);
  godot_autopilot::dispatch::DepthContext depth_ctx =
      godot_autopilot::dispatch::capture_depth_context();
  try {
    out.payload =
        godot_autopilot::dispatch::call_handler(tool, arguments, depth_ctx);
  } catch (const std::exception &error) {
    out.payload = error_result(
        "internal error in tool '" + tool + "': " + error.what(),
        "internal_error");
  } catch (...) {
    out.payload = error_result(
        "internal error in tool '" + tool + "': unexpected C++ exception",
        "internal_error");
  }
  if (out.payload.IsObject()) {
    const int64_t new_errors =
        godot_autopilot::error_watermark::count_response_errors(out.payload);
    if (new_errors > 0) {
      godot_autopilot::error_watermark::record_error(new_errors,
                                                     out.traceparent);
    }
    out.payload["new_errors_since_last_call"] = JsonValue(
        godot_autopilot::error_watermark::consume_new_errors(out.traceparent));
  }
  out.pending = godot_autopilot::dispatch::has_deferred_marker(out.payload);
  std::vector<JsonValue> images;
  if (is_call_tool) {
    if (const JsonValue *name = arguments.Find("name");
        name != nullptr && name->IsString()) {
      if (registry != nullptr) {
        auto target = registry->find_any(name->GetString());
        if (target != nullptr &&
            (target->tool_flags() & godot_autopilot::tool_flags::kCaptureImage) != 0) {
          godot_autopilot::util::try_attach_image_data(out.payload, images);
        } else {
          godot_autopilot::util::try_attach_image_content(name->GetString(),
                                                          out.payload, images);
        }
      } else {
        godot_autopilot::util::try_attach_image_content(name->GetString(),
                                                        out.payload, images);
      }
    }
  } else {
    godot_autopilot::util::try_attach_image_content(tool, out.payload, images);
  }
  out.result = wrap_result(out.payload,
                           godot_autopilot::tool_result_is_error(out.payload),
                           images);
  return out;
}

inline NativeCallResult await_pending(NativeCallResult call,
                                      int64_t deadline_ms) {
  if (!call.pending) {
    return call;
  }
  if (deadline_ms > 0 && call.payload.IsObject() &&
      call.payload.Find("timeout_ms") == nullptr) {
    call.payload["timeout_ms"] = JsonValue(deadline_ms);
  }
  const std::string &wait_tool =
      call.deferred_tool.empty() ? call.tool : call.deferred_tool;
  call.payload = godot_autopilot::dispatch::resolve_deferred_wait(
      wait_tool, std::move(call.payload));
  call.pending = false;
  std::vector<JsonValue> images;
  auto registry = godot_autopilot::get_active_registry();
  if (registry != nullptr) {
    auto target = registry->find_any(wait_tool);
    if (target != nullptr &&
        (target->tool_flags() &
         godot_autopilot::tool_flags::kCaptureImage) != 0) {
      godot_autopilot::util::try_attach_image_data(call.payload, images);
    } else {
      godot_autopilot::util::try_attach_image_content(wait_tool,
                                                      call.payload, images);
    }
  } else {
    godot_autopilot::util::try_attach_image_content(wait_tool,
                                                    call.payload, images);
  }
  call.result = wrap_result(
      call.payload, godot_autopilot::tool_result_is_error(call.payload), images);
  return call;
}

} // namespace native
} // namespace control
} // namespace gda

#endif
