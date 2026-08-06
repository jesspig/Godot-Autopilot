#ifndef GODOT_SELF_DRIVING_DEBUGGER_PROMPTS_HPP
#define GODOT_SELF_DRIVING_DEBUGGER_PROMPTS_HPP

#include <mcp/server/McpServer.hpp>

namespace godot_self_driving {

void register_debugger_prompts(mcp::McpServer &server);

}
#endif
