#ifndef GODOT_AUTOPILOT_MCP_HTTP_SHIM_HPP
#define GODOT_AUTOPILOT_MCP_HTTP_SHIM_HPP

#include <string>

#include "core/http_transport.hpp"

namespace godot_autopilot {

namespace mcp_http {

struct HeaderDecision {
  bool accepted{true};
  int status_code{200};
  std::string status_text;
  std::string message;
};

enum class SessionDisposition { kIgnore, kReject };

bool media_type_accepted(const std::string &accept_header,
                          const std::string &media_type);

HeaderDecision evaluate_accept_header(const std::string &accept_header);

HeaderDecision evaluate_accept_media_type(const std::string &accept_header,
                                           const std::string &media_type);

HeaderDecision evaluate_content_type(const std::string &content_type);

HeaderDecision evaluate_protocol_version(const std::string &version);

SessionDisposition evaluate_session_header(const std::string &session_id);

std::string supported_protocol_versions_json();

std::string build_error_body(int code, const std::string &message);

std::string build_unsupported_version_body(const std::string &version);

int status_for_jsonrpc_error(int code);

} // namespace mcp_http

class McpHttpShim {
public:
  bool handle(const HttpRequest &req, HttpResponse &resp) const;
  bool validate_protocol_version(const std::string &header) const;
};

} // namespace godot_autopilot

#endif
