#ifndef GODOT_AUTOPILOT_DEBUGGER_PROMPTS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_PROMPTS_HPP

#include <mcp/server/McpServer.hpp>

namespace godot_autopilot {

void register_debugger_prompts(mcp::McpServer &server);

}
#endif
