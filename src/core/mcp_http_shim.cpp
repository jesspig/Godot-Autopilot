#include "core/mcp_http_shim.hpp"

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <mcp/JsonRpc.hpp>
#include <mcp/JsonValue.hpp>
#include <mcp/ProtocolVersion.hpp>

#include "core/godot_transport.hpp"
#include "core/http_parse.hpp"
#include "core/http_stream_registry.hpp"
#include "core/http_transport_ext.hpp"

namespace godot_autopilot {

namespace mcp_http {

namespace {

constexpr const char *kJsonMediaType = "application/json";
constexpr const char *kEventStreamMediaType = "text/event-stream";
constexpr int kUnsupportedProtocolVersionCode = -32022;

std::string lower_copy(const std::string &text) {
  return http_parse::to_lower(text);
}

std::string trim_media_type(const std::string &text) {
  return http_parse::trim(text);
}

std::string media_type_base(const std::string &content_type) {
  std::string value = trim_media_type(content_type);
  const size_t separator = value.find(';');
  if (separator != std::string::npos) {
    value = trim_media_type(value.substr(0, separator));
  }
  return lower_copy(value);
}

HeaderDecision reject(int status_code, const std::string &status_text,
                      const std::string &message) {
  HeaderDecision decision;
  decision.accepted = false;
  decision.status_code = status_code;
  decision.status_text = status_text;
  decision.message = message;
  return decision;
}

} // namespace

bool media_type_accepted(const std::string &accept_header,
                          const std::string &media_type) {
  const std::string wanted = lower_copy(trim_media_type(media_type));
  if (wanted.empty()) {
    return false;
  }
  const std::string accept = trim_media_type(accept_header);
  if (accept.empty()) {
    return false;
  }
  size_t cursor = 0;
  while (cursor <= accept.size()) {
    size_t end = accept.find(',', cursor);
    if (end == std::string::npos) {
      end = accept.size();
    }
    std::string entry = trim_media_type(accept.substr(cursor, end - cursor));
    const size_t parameter = entry.find(';');
    if (parameter != std::string::npos) {
      entry = trim_media_type(entry.substr(0, parameter));
    }
    entry = lower_copy(entry);
    if (entry == "*/*" || entry == wanted) {
      return true;
    }
    const size_t slash = wanted.find('/');
    if (slash != std::string::npos && entry == wanted.substr(0, slash) + "/*") {
      return true;
    }
    if (end == accept.size()) {
      break;
    }
    cursor = end + 1;
  }
  return false;
}

HeaderDecision evaluate_accept_header(const std::string &accept_header) {
  if (!media_type_accepted(accept_header, kJsonMediaType)) {
    return reject(406, "Not Acceptable",
                  "the client must accept " + std::string(kJsonMediaType));
  }
  if (!media_type_accepted(accept_header, kEventStreamMediaType)) {
    return reject(406, "Not Acceptable",
                  "the client must accept " +
                      std::string(kEventStreamMediaType));
  }
  HeaderDecision decision;
  decision.accepted = true;
  decision.status_code = 200;
  decision.status_text = "OK";
  return decision;
}

HeaderDecision evaluate_accept_media_type(const std::string &accept_header,
                                           const std::string &media_type) {
  if (!media_type_accepted(accept_header, media_type)) {
    return reject(406, "Not Acceptable",
                  "the client must accept " + media_type);
  }
  HeaderDecision decision;
  decision.accepted = true;
  decision.status_code = 200;
  decision.status_text = "OK";
  return decision;
}

HeaderDecision evaluate_content_type(const std::string &content_type) {
  const std::string base = media_type_base(content_type);
  if (base != kJsonMediaType) {
    return reject(415, "Unsupported Media Type",
                  "the request body must be " + std::string(kJsonMediaType));
  }
  HeaderDecision decision;
  decision.accepted = true;
  decision.status_code = 200;
  decision.status_text = "OK";
  return decision;
}

HeaderDecision evaluate_protocol_version(const std::string &version) {
  const std::string trimmed = trim_media_type(version);
  if (trimmed.empty()) {
    HeaderDecision decision;
    decision.accepted = true;
    decision.status_code = 200;
    decision.status_text = "OK";
    return decision;
  }
  if (mcp_wire::is_supported_protocol_version(trimmed)) {
    HeaderDecision decision;
    decision.accepted = true;
    decision.status_code = 200;
    decision.status_text = "OK";
    return decision;
  }
  return reject(400, "Bad Request",
                "Unsupported protocol version '" + trimmed + "'");
}

SessionDisposition evaluate_session_header(const std::string &session_id) {
  (void)session_id;
  return SessionDisposition::kIgnore;
}

DispatchKind classify_dispatch(const mcp_wire::ParsedRequest &parsed) {
  if (parsed.is_request) {
    return DispatchKind::kRequest;
  }
  if (parsed.is_notification) {
    return DispatchKind::kNotification;
  }
  return DispatchKind::kInvalid;
}

void build_accepted_response(HttpResponse &resp) {
  resp.status_code = 202;
  resp.status_text = "Accepted";
  resp.headers.erase("Content-Type");
  resp.body.clear();
}

std::string supported_protocol_versions_json() {
  std::string out = "[";
  bool first = true;
  for (const std::string_view &known : mcp::kProtocolVersions) {
    if (!first) {
      out += ",";
    }
    first = false;
    out += "\"" + std::string(known) + "\"";
  }
  out += "]";
  return out;
}

std::string build_error_body(int code, const std::string &message) {
  mcp::JsonValue::Object error;
  error["code"] = mcp::JsonValue(static_cast<int64_t>(code));
  error["message"] = mcp::JsonValue(message);
  mcp::JsonValue::Object root;
  root["jsonrpc"] = mcp::JsonValue("2.0");
  root["id"] = mcp::JsonValue(nullptr);
  root["error"] = mcp::JsonValue(std::move(error));
  return mcp::JsonValue(std::move(root)).Dump();
}

std::string build_unsupported_version_body(const std::string &version) {
  mcp::JsonValue::Object data;
  mcp::JsonValue::Array supported;
  for (const std::string_view &known : mcp::kProtocolVersions) {
    supported.push_back(mcp::JsonValue(std::string(known)));
  }
  data["supported"] = mcp::JsonValue(std::move(supported));
  data["requested"] = mcp::JsonValue(version);
  mcp::JsonValue::Object error;
  error["code"] = mcp::JsonValue(static_cast<int64_t>(
      kUnsupportedProtocolVersionCode));
  error["message"] =
      mcp::JsonValue("Unsupported protocol version '" + version + "'");
  error["data"] = mcp::JsonValue(std::move(data));
  mcp::JsonValue::Object root;
  root["jsonrpc"] = mcp::JsonValue("2.0");
  root["id"] = mcp::JsonValue(nullptr);
  root["error"] = mcp::JsonValue(std::move(error));
  return mcp::JsonValue(std::move(root)).Dump();
}

int status_for_jsonrpc_error(int code) {
  switch (code) {
  case -32700:
  case -32600:
  case -32602:
  case -32020:
  case -32021:
  case -32022:
    return 400;
  case -32601:
    return 404;
  default:
    return 200;
  }
}

} // namespace mcp_http

namespace {

constexpr std::chrono::milliseconds kDispatchTimeout{30000};
constexpr const char *kJsonContentType = "application/json";
constexpr const char *kEventStreamContentType = "text/event-stream";
constexpr const char *kMcpEndpointPath = "/mcp";
constexpr int kServerErrorCode = -32000;
constexpr int kMethodNotFoundCode = -32601;
constexpr int kParseErrorCode = -32700;

void fill_json_response(HttpResponse &resp, int status_code,
                        const std::string &status_text, const std::string &body) {
  resp.status_code = status_code;
  resp.status_text = status_text;
  resp.headers["Content-Type"] = kJsonContentType;
  resp.body = body;
}

std::string negotiated_version_header(const std::string &requested) {
  if (!requested.empty()) {
    return requested;
  }
  return std::string(mcp::kDefaultNegotiatedProtocolVersion);
}

bool handle_post(const HttpRequest &req, HttpResponse &resp) {
  const std::string session_id =
      http_parse::header_value(req.headers, "Mcp-Session-Id");
  if (mcp_http::evaluate_session_header(session_id) ==
      mcp_http::SessionDisposition::kReject) {
    fill_json_response(resp, 404, "Not Found",
                       mcp_http::build_error_body(
                           kMethodNotFoundCode,
                           "the stateless endpoint has no session to adopt"));
    return false;
  }

  const std::string accept =
      http_parse::header_value(req.headers, "Accept");
  const mcp_http::HeaderDecision accept_decision =
      mcp_http::evaluate_accept_header(accept);
  if (!accept_decision.accepted) {
    fill_json_response(resp, accept_decision.status_code,
                       accept_decision.status_text,
                       mcp_http::build_error_body(-32600,
                                                  accept_decision.message));
    return false;
  }

  const std::string content_type =
      http_parse::header_value(req.headers, "Content-Type");
  const mcp_http::HeaderDecision content_decision =
      mcp_http::evaluate_content_type(content_type);
  if (!content_decision.accepted) {
    fill_json_response(resp, content_decision.status_code,
                       content_decision.status_text,
                       mcp_http::build_error_body(-32600,
                                                  content_decision.message));
    return false;
  }

  const std::string version =
      http_parse::header_value(req.headers, "MCP-Protocol-Version");
  const mcp_http::HeaderDecision version_decision =
      mcp_http::evaluate_protocol_version(version);
  if (!version_decision.accepted) {
    resp.status_code = version_decision.status_code;
    resp.status_text = version_decision.status_text;
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_http::build_unsupported_version_body(
        http_parse::trim(version));
    return false;
  }

  const mcp_wire::ParsedRequest parsed = mcp_wire::parse_request(req.body);
  const mcp_http::DispatchKind dispatch = mcp_http::classify_dispatch(parsed);
  if (dispatch == mcp_http::DispatchKind::kInvalid) {
    fill_json_response(resp, 400, "Bad Request",
                       mcp_http::build_error_body(kParseErrorCode,
                                                  parsed.error));
    return false;
  }

  const std::shared_ptr<GodotTransport> transport = godot_transport_instance();
  if (!transport) {
    fill_json_response(resp, 503, "Service Unavailable",
                       mcp_http::build_error_body(kServerErrorCode,
                                                  "mcp transport is unavailable"));
    return false;
  }

  std::string submit_error;
  if (!transport->submit(parsed, submit_error)) {
    fill_json_response(resp, 503, "Service Unavailable",
                       mcp_http::build_error_body(kServerErrorCode,
                                                  submit_error));
    return false;
  }

  resp.headers["mcp-protocol-version"] = negotiated_version_header(version);

  if (dispatch == mcp_http::DispatchKind::kNotification) {
    mcp_http::build_accepted_response(resp);
    return true;
  }

  mcp::JsonRpcMessage message;
  if (!transport->await_response(parsed.request_id, message, kDispatchTimeout)) {
    fill_json_response(resp, 504, "Gateway Timeout",
                       mcp_http::build_error_body(
                           kServerErrorCode,
                           "mcp request timed out: " + parsed.method));
    return false;
  }

  std::string body = mcp_wire::build_response_text(message);
  int status_code = 200;
  std::string status_text = "OK";
  if (const auto *failure = std::get_if<mcp::JsonRpcErrorResponse>(&message)) {
    const int mapped =
        mcp_http::status_for_jsonrpc_error(static_cast<int>(failure->error.code));
    if (mapped != 200) {
      status_code = mapped;
      status_text = mapped == 404 ? "Not Found" : "Bad Request";
    }
  }
  fill_json_response(resp, status_code, status_text, body);
  return status_code < 400;
}

bool handle_get(const HttpRequest &req, HttpResponse &resp) {
  const std::string accept = http_parse::header_value(req.headers, "Accept");
  const mcp_http::HeaderDecision accept_decision =
      mcp_http::evaluate_accept_media_type(accept, kEventStreamContentType);
  if (!accept_decision.accepted) {
    fill_json_response(resp, accept_decision.status_code,
                       accept_decision.status_text,
                       mcp_http::build_error_body(-32600,
                                                  accept_decision.message));
    return false;
  }

  const std::string version =
      http_parse::header_value(req.headers, "MCP-Protocol-Version");
  const mcp_http::HeaderDecision version_decision =
      mcp_http::evaluate_protocol_version(version);
  if (!version_decision.accepted) {
    resp.status_code = version_decision.status_code;
    resp.status_text = version_decision.status_text;
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_http::build_unsupported_version_body(
        http_parse::trim(version));
    return false;
  }

  const std::string connection_id =
      http_parse::header_value(req.headers, kConnectionIdHeader);
  if (connection_id.empty()) {
    fill_json_response(resp, 400, "Bad Request",
                       mcp_http::build_error_body(kParseErrorCode,
                                                  "the request carries no "
                                                  "connection identifier"));
    return false;
  }
  http_stream::acquire(connection_id);

  resp.status_code = 200;
  resp.status_text = "OK";
  resp.headers["Content-Type"] = kEventStreamContentType;
  resp.headers["Cache-Control"] = "no-cache";
  resp.headers["mcp-protocol-version"] = negotiated_version_header(version);
  resp.headers[kStreamIdHeader] = connection_id;
  resp.body = mcp_wire::build_event_frame("endpoint", kMcpEndpointPath);
  return true;
}

bool handle_delete(const HttpRequest &req, HttpResponse &resp) {
  (void)req;
  fill_json_response(resp, 405, "Method Not Allowed",
                     mcp_http::build_error_body(
                         kMethodNotFoundCode,
                         "the stateless endpoint has no session to terminate"));
  return false;
}

} // namespace

bool McpHttpShim::validate_protocol_version(const std::string &header) const {
  return mcp_http::evaluate_protocol_version(header).accepted;
}

bool McpHttpShim::handle(const HttpRequest &req, HttpResponse &resp) const {
  if (req.method == "POST") {
    return handle_post(req, resp);
  }
  if (req.method == "GET") {
    return handle_get(req, resp);
  }
  if (req.method == "DELETE") {
    return handle_delete(req, resp);
  }
  fill_json_response(resp, 405, "Method Not Allowed",
                     mcp_http::build_error_body(kMethodNotFoundCode,
                                                "method not allowed: " +
                                                    req.method));
  return false;
}

} // namespace godot_autopilot
