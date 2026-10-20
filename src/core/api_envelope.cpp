#include "core/api_envelope.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "core/monitor.hpp"
#include "tools/tool_invoke.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kCodeUnauthorized = "unauthorized";
constexpr const char *kCodeInvalidArgument = "invalid_argument";
constexpr const char *kCodeToolNotFound = "tool_not_found";
constexpr const char *kCodeAuthorizationRequired = "authorization_required";
constexpr const char *kCodeExportInProgress = "export_in_progress";
constexpr const char *kCodeQueueUnavailable = "queue_unavailable";
constexpr const char *kCodeMainThreadTimeout = "main_thread_timeout";
constexpr const char *kCodeInternalError = "internal_error";
constexpr const char *kCodeToolError = "tool_error";

std::string lower_case(const std::string &text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

bool contains_text(const std::string &haystack, const char *needle) {
  return haystack.find(needle) != std::string::npos;
}

std::string error_text_of(const mcp::JsonValue &result) {
  if (!result.IsObject()) {
    return std::string();
  }
  const mcp::JsonValue *error = result.Find("error");
  if (error && error->IsString()) {
    return error->GetString();
  }
  const mcp::JsonValue *details = result.Find("error_details");
  if (details && details->IsString()) {
    return details->GetString();
  }
  return std::string();
}

std::string feature_code(const std::string &text) {
  const std::string lowered = lower_case(text);
  if (lowered.empty()) {
    return kCodeToolError;
  }
  if (contains_text(lowered, "authorization required") ||
      contains_text(lowered, "authorization_required")) {
    return kCodeAuthorizationRequired;
  }
  if (contains_text(lowered, "unauthorized") ||
      contains_text(lowered, "not authorized")) {
    return kCodeUnauthorized;
  }
  if (contains_text(lowered, "editor is exporting") ||
      contains_text(lowered, "export_in_progress")) {
    return kCodeExportInProgress;
  }
  if (contains_text(lowered, "main_thread_timeout") ||
      contains_text(lowered, "main thread timeout") ||
      contains_text(lowered, "main thread task failed")) {
    return kCodeMainThreadTimeout;
  }
  if (contains_text(lowered, "queue_unavailable") ||
      (contains_text(lowered, "queue") && contains_text(lowered, "unavailable"))) {
    return kCodeQueueUnavailable;
  }
  if (contains_text(lowered, "invalid parameter") ||
      contains_text(lowered, "invalid argument") ||
      contains_text(lowered, "missing required parameter") ||
      contains_text(lowered, "missing required field") ||
      contains_text(lowered, "unknown parameter")) {
    return kCodeInvalidArgument;
  }
  if (contains_text(lowered, "not found") && contains_text(lowered, "tool")) {
    return kCodeToolNotFound;
  }
  if (contains_text(lowered, "internal error") ||
      contains_text(lowered, "unexpected c++ exception")) {
    return kCodeInternalError;
  }
  return kCodeToolError;
}

} // namespace

std::string ApiEnvelope::normalize_code(const mcp::JsonValue &result) const {
  if (result.IsObject()) {
    const mcp::JsonValue *code = result.Find("code");
    if (code && code->IsString() && !code->GetString().empty()) {
      return code->GetString();
    }
    const mcp::JsonValue *required = result.Find("authorization_required");
    if (required && required->IsString() && !required->GetString().empty()) {
      return kCodeAuthorizationRequired;
    }
    const mcp::JsonValue *structured = result.Find("structured_error");
    if (structured && structured->IsObject()) {
      const mcp::JsonValue *inner = structured->Find("code");
      if (inner && inner->IsString() && !inner->GetString().empty()) {
        return inner->GetString();
      }
    }
  }
  return feature_code(error_text_of(result));
}

int ApiEnvelope::status_for(const std::string &code) const {
  if (code == kCodeUnauthorized) {
    return 401;
  }
  if (code == kCodeInvalidArgument) {
    return 400;
  }
  if (code == kCodeToolNotFound) {
    return 404;
  }
  if (code == kCodeAuthorizationRequired) {
    return 403;
  }
  if (code == kCodeExportInProgress) {
    return 503;
  }
  if (code == kCodeQueueUnavailable) {
    return 503;
  }
  if (code == kCodeMainThreadTimeout) {
    return 504;
  }
  if (code == kCodeInternalError) {
    return 500;
  }
  return 422;
}

mcp::JsonValue ApiEnvelope::build(const mcp::JsonValue &result,
                                  const std::string &tool_name) const {
  if (!result.IsObject() || result.Find("error") == nullptr) {
    return result;
  }
  mcp::JsonValue envelope = result;
  const mcp::JsonValue *error = result.Find("error");
  if (error->IsString()) {
    envelope["error"] = mcp::JsonValue(error->GetString());
  } else {
    envelope["error"] = mcp::JsonValue(error->Dump());
  }
  envelope["code"] = mcp::JsonValue(normalize_code(result));
  const mcp::JsonValue *warnings = envelope.Find("warnings");
  if (!warnings || !warnings->IsArray()) {
    envelope["warnings"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  }
  if (!tool_name.empty() && envelope.Find("tool") == nullptr) {
    envelope["tool"] = mcp::JsonValue(tool_name);
  }
  const std::string trace_id = tools::current_trace_id();
  if (!trace_id.empty()) {
    envelope["trace_id"] = mcp::JsonValue(trace_id);
  }
  const std::string request_id = monitor::current_request_id();
  if (!request_id.empty()) {
    envelope["request_id"] = mcp::JsonValue(request_id);
  }
  return envelope;
}

} // namespace godot_autopilot
