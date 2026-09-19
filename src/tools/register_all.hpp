#ifndef GODOT_AUTOPILOT_REGISTER_ALL_HPP
#define GODOT_AUTOPILOT_REGISTER_ALL_HPP

#include "core/command_queue.hpp"
#include "tools/tool_catalog.hpp"
#include "tools/tool_registry.hpp"
#include "util/bm25_index.hpp"
#include <chrono>
#include <memory>
#include <mcp/JsonValue.hpp>
#include <mcp/server/McpServer.hpp>
#include <string>

namespace godot_autopilot {

void register_all_tools(mcp::McpServer &server, CommandQueue &queue,
                        ToolCatalog &catalog, Bm25Index &index, int port);

void refresh_derived(const std::shared_ptr<ToolRegistry>& registry,
                     ToolCatalog& catalog, Bm25Index& index);

void refresh_dynamic_tools();

std::shared_ptr<ToolRegistry> get_active_registry();
void clear_active_registry();

} // namespace godot_autopilot

#endif
