#ifndef GODOT_AUTOPILOT_REGISTER_ALL_HPP
#define GODOT_AUTOPILOT_REGISTER_ALL_HPP

#include "core/command_queue.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include <chrono>
#include <mcp/JsonValue.hpp>
#include <mcp/server/McpServer.hpp>

namespace godot_autopilot {

void register_all_tools(mcp::McpServer &server, CommandQueue &queue,
                        ToolCatalog &catalog, Bm25Index &index, int port);

} // namespace godot_autopilot

#endif
