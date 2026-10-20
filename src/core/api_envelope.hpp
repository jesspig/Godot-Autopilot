#ifndef GODOT_AUTOPILOT_API_ENVELOPE_HPP
#define GODOT_AUTOPILOT_API_ENVELOPE_HPP

#include <string>

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

class ApiEnvelope {
public:
  mcp::JsonValue build(const mcp::JsonValue &result,
                       const std::string &tool_name) const;
  int status_for(const std::string &code) const;
  std::string normalize_code(const mcp::JsonValue &result) const;
};

} // namespace godot_autopilot

#endif
