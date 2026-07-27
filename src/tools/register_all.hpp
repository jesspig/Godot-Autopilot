#ifndef GODOT_SELF_DRIVING_REGISTER_ALL_HPP
#define GODOT_SELF_DRIVING_REGISTER_ALL_HPP

#include <chrono>
#include <mcp/JsonValue.hpp>
#include <mcp/server/McpServer.hpp>
#include "core/command_queue.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"

namespace godot_self_driving {

void register_all_tools(mcp::McpServer& server, CommandQueue& queue, ToolCatalog& catalog, Bm25Index& index, int port);

} // namespace godot_self_driving

#endif
