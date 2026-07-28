#ifndef GODOT_SELF_DRIVING_RESOURCE_HANDLERS_HPP
#define GODOT_SELF_DRIVING_RESOURCE_HANDLERS_HPP

#include <mcp/server/McpServer.hpp>
#include "core/command_queue.hpp"

namespace godot_self_driving {

void register_all_resources(mcp::McpServer& server, CommandQueue& queue);

} // namespace godot_self_driving

#endif
