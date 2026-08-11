#ifndef GODOT_AUTOPILOT_PROMPT_HANDLERS_HPP
#define GODOT_AUTOPILOT_PROMPT_HANDLERS_HPP

#include "core/command_queue.hpp"
#include <mcp/server/McpServer.hpp>

namespace godot_autopilot {

std::string prompt_tool_usage();
std::string prompt_keycode_reference();

void register_all_prompts(mcp::McpServer &server, CommandQueue &queue);

} // namespace godot_autopilot

#endif
