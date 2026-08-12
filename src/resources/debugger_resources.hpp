#ifndef GODOT_AUTOPILOT_DEBUGGER_RESOURCES_HPP
#define GODOT_AUTOPILOT_DEBUGGER_RESOURCES_HPP

#include <mcp/server/McpServer.hpp>

namespace godot_autopilot {

void register_debugger_resources(mcp::McpServer &server);

}
#endif
