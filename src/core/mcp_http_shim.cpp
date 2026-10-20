#include "core/mcp_http_shim.hpp"

#include <chrono>
#include <string>
#include <utility>

#include "core/godot_transport.hpp"

namespace godot_autopilot {

namespace {

constexpr std::chrono::milliseconds kDispatchTimeout{30000};
constexpr const char *kJsonContentType = "application/json";
constexpr int kParseErrorCode = -32700;
constexpr int kServerErrorCode = -32000;
constexpr int kMethodNotFoundCode = -32601;

}

bool McpHttpShim::validate_protocol_version(const std::string &header) const {
  return mcp_wire::is_supported_protocol_version(header);
}

bool McpHttpShim::handle(const HttpRequest &req, HttpResponse &resp) const {
  if (req.method != "POST") {
    resp.status_code = 405;
    resp.status_text = "Method Not Allowed";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_wire::build_error_response_text(
        kMethodNotFoundCode, "method not allowed: " + req.method);
    return false;
  }

  const mcp_wire::ParsedRequest parsed = mcp_wire::parse_request(req.body);
  if (!parsed.ok) {
    resp.status_code = 400;
    resp.status_text = "Bad Request";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_wire::build_error_response_text(kParseErrorCode, parsed.error);
    return false;
  }

  const std::shared_ptr<GodotTransport> transport = godot_transport_instance();
  if (!transport) {
    resp.status_code = 503;
    resp.status_text = "Service Unavailable";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_wire::build_error_response_text(
        kServerErrorCode, "mcp transport is unavailable");
    return false;
  }

  std::string submit_error;
  if (!transport->submit(parsed, submit_error)) {
    resp.status_code = 503;
    resp.status_text = "Service Unavailable";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body =
        mcp_wire::build_error_response_text(kServerErrorCode, submit_error);
    return false;
  }

  if (!parsed.is_request) {
    resp.status_code = 202;
    resp.status_text = "Accepted";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = "{}";
    return true;
  }

  std::string response_text;
  if (!transport->await_response(parsed.request_id, response_text,
                                 kDispatchTimeout)) {
    resp.status_code = 504;
    resp.status_text = "Gateway Timeout";
    resp.headers["Content-Type"] = kJsonContentType;
    resp.body = mcp_wire::build_error_response_text(
        kServerErrorCode, "mcp request timed out: " + parsed.method);
    return false;
  }

  resp.status_code = 200;
  resp.status_text = "OK";
  resp.headers["Content-Type"] = kJsonContentType;
  resp.body = std::move(response_text);
  return true;
}

} // namespace godot_autopilot
