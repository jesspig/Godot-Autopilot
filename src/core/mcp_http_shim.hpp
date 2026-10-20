#ifndef GODOT_AUTOPILOT_MCP_HTTP_SHIM_HPP
#define GODOT_AUTOPILOT_MCP_HTTP_SHIM_HPP

#include <string>

#include "core/http_transport.hpp"

namespace godot_autopilot {

class McpHttpShim {
public:
  bool handle(const HttpRequest &req, HttpResponse &resp) const;
  bool validate_protocol_version(const std::string &header) const;
};

} // namespace godot_autopilot

#endif
