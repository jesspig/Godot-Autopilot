#ifndef GODOT_SELF_DRIVING_REGISTER_ALL_HPP
#define GODOT_SELF_DRIVING_REGISTER_ALL_HPP

#include "core/command_queue.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include <chrono>
#include <functional>
#include <mcp/JsonValue.hpp>
#include <mcp/server/McpServer.hpp>

namespace godot_self_driving {

using ToolHandler = std::function<mcp::JsonValue(const mcp::JsonValue &)>;

void register_all_tools(mcp::McpServer &server, CommandQueue &queue,
                        ToolCatalog &catalog, Bm25Index &index, int port);

mcp::JsonValue call_handler(const std::string &name,
                            const mcp::JsonValue &args);

} // namespace godot_self_driving

#endif
