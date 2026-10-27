#ifndef GODOT_AUTOPILOT_RESOURCE_ERROR_MAPPING_HPP
#define GODOT_AUTOPILOT_RESOURCE_ERROR_MAPPING_HPP

#include <mcp/ErrorCodes.hpp>

#include <string>

namespace godot_autopilot {

mcp::McpErrorCode resource_error_code(const std::string &error_code);

} // namespace godot_autopilot

#endif
