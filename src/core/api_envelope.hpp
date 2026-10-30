#ifndef GODOT_AUTOPILOT_API_ENVELOPE_HPP
#define GODOT_AUTOPILOT_API_ENVELOPE_HPP

#include <string>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

int jsonrpc_code_for(const std::string &code);

class ApiEnvelope {
public:
  mcp::JsonValue build(const mcp::JsonValue &result,
                       const std::string &tool_name,
                       const std::string &request_id_override = {}) const;
  int status_for(const std::string &code) const;
  std::string normalize_code(const mcp::JsonValue &result) const;
};

} // namespace godot_autopilot

#endif
