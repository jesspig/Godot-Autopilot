#include "resources/resource_error_mapping.hpp"

namespace godot_autopilot {

mcp::McpErrorCode resource_error_code(const std::string &error_code) {
  if (error_code == "main_thread_timeout") {
    return mcp::McpErrorCode::RequestTimeout;
  }
  if (error_code == "resource_not_found") {
    return mcp::McpErrorCode::InvalidParams;
  }
  if (error_code == "resource_read_failed") {
    return mcp::McpErrorCode::InternalError;
  }
  return mcp::McpErrorCode::InternalError;
}

} // namespace godot_autopilot
