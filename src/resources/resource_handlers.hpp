#ifndef GODOT_SELF_DRIVING_RESOURCE_HANDLERS_HPP
#define GODOT_SELF_DRIVING_RESOURCE_HANDLERS_HPP

#include "core/command_queue.hpp"
#include <mcp/server/McpServer.hpp>


namespace godot_self_driving {

void register_all_resources(mcp::McpServer &server, CommandQueue &queue);

}

#endif
