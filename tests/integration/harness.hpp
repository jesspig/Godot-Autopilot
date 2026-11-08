#ifndef GDA_INTEGRATION_HARNESS_HPP
#define GDA_INTEGRATION_HARNESS_HPP

#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include <memory>
#include <string>

#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/register_all.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_catalog.hpp"
#include "tools/tool_registry.hpp"
#include "util/bm25_index.hpp"

namespace gda_integration {

class IntegrationHarness : public ::testing::Test {
 public:
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;
  std::unique_ptr<mcp::McpServer> server;

  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server, queue, catalog, index, 9527);
    queue.drain();
    godot_autopilot::runtime_ops::set_editor_queue(&queue);
  }

  void TearDown() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    godot_autopilot::dispatch::clear_handlers();
    if (server) {
      server->Close();
      server.reset();
    }
  }

  mcp::JsonValue call(const std::string &name, const mcp::JsonValue &args) {
    return godot_autopilot::dispatch::call_handler(name, args);
  }
};

} // namespace gda_integration

#endif
