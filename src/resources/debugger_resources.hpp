#ifndef GODOT_SELF_DRIVING_DEBUGGER_RESOURCES_HPP
#define GODOT_SELF_DRIVING_DEBUGGER_RESOURCES_HPP

#include <mcp/server/McpServer.hpp>

namespace godot_self_driving {

void register_debugger_resources(mcp::McpServer &server);

}
#endif
