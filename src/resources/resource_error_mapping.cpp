#include "resources/resource_error_mapping.hpp"

namespace godot_autopilot {

mcp::McpErrorCode resource_error_code(const std::string &error_code) {
  if (error_code == "resource_not_found") {
    return mcp::McpErrorCode::InvalidParams;
  }
  return mcp::McpErrorCode::InternalError;
}

} // namespace godot_autopilot
