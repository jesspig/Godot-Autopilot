#ifndef GODOT_AUTOPILOT_RESOURCE_HANDLERS_HPP
#define GODOT_AUTOPILOT_RESOURCE_HANDLERS_HPP

#include "core/command_queue.hpp"
#include <mcp/server/McpServer.hpp>


namespace godot_autopilot {

void register_all_resources(mcp::McpServer &server, CommandQueue &queue);

}

#endif
