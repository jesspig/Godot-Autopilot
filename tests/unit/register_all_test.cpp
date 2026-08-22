
//

//

#include <gtest/gtest.h>

#include <mcp/client/McpClient.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/register_all.hpp"
#include "tools/tool_catalog.hpp"
#include "tools/tool_registry.hpp"
#include "util/bm25_index.hpp"

#include <string>
#include <thread>
#include <vector>

namespace {

// 协议面常量：meta 工具集合（7 个）由服务端直接注册，独立于领域工具注册管线，
// 不随插件工具数量变化，故保留精确值。
constexpr size_t kMetaToolCount = 7;

const char *const kMetaToolNames[kMetaToolCount] = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

const char *const kValidCategories[] = {
    "Audio",      "Capture",  "Config",      "Debug",   "Debugger",
    "Display",    "Docs",     "Editor",      "Game",    "Group",
    "Input",      "Navigation", "OS",        "Physics", "Properties",
    "Render",     "Resources", "Scene",      "Scripts", "SpriteFrames",
    "System",     "Text",     "TileMap",     "Meta",    "Auto"};

bool has_schema_params(const mcp::JsonValue &schema) {
  const auto *props = schema.Find("properties");
  return props != nullptr && props->IsObject() && !props->GetObject().empty();
}

bool is_valid_category(const std::string &category) {
  for (const char *c : kValidCategories) {
    if (category == c)
      return true;
  }
  return false;
}

struct RegisteredServerFixture : ::testing::Test {
  std::unique_ptr<mcp::McpServer> server;
  std::unique_ptr<mcp::McpClient> client;
  std::thread server_thread;
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server, queue, catalog, index,
                                           9527);
    server_thread = std::thread([this] { server->Run(); });

    mcp::ClientOptions cops;
    cops.client_info = mcp::Implementation{"GsdTestClient", "1.0.0"};
    cops.connect_mode = mcp::ConnectMode::Auto;
    client = mcp::McpClient::Create(pair.client, cops);
  }

  void TearDown() override {
    if (client)
      client->Close();
    if (server)
      server->Close();
    if (server_thread.joinable())
      server_thread.join();
  }
};

} // namespace

TEST_F(RegisteredServerFixture, ListToolsReturnsExactlySevenMetaTools) {
  auto result = client->ListTools();
  ASSERT_EQ(result.tools.size(), kMetaToolCount);
}

TEST_F(RegisteredServerFixture, AllMetaToolNamesPresent) {
  auto result = client->ListTools();
  for (const char *name : kMetaToolNames) {
    bool found = false;
    for (const auto &tool : result.tools) {
      if (tool.name == name)
        found = true;
    }
    EXPECT_TRUE(found) << "meta tool missing from ListTools: " << name;
  }
}

TEST_F(RegisteredServerFixture, MetaToolAttributesNonEmpty) {
  auto result = client->ListTools();
  for (const auto &tool : result.tools) {
    EXPECT_FALSE(tool.name.empty());
    EXPECT_TRUE(tool.description.has_value())
        << "tool has no description: " << tool.name;
    if (tool.description.has_value()) {
      EXPECT_FALSE(tool.description->empty());
    }
  }
}

TEST_F(RegisteredServerFixture, CatalogToolCountMatches) {
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
}

TEST_F(RegisteredServerFixture, CatalogEntriesWellFormed) {
  auto tools = catalog.get_all_tools();
  ASSERT_EQ(tools.size(), catalog.size());
  for (const auto *tool : tools) {
    EXPECT_FALSE(tool->name.empty());
    EXPECT_FALSE(tool->description.empty())
        << "empty description: " << tool->name;
    EXPECT_FALSE(tool->category.empty()) << "empty category: " << tool->name;
    EXPECT_TRUE(is_valid_category(tool->category))
        << "invalid category '" << tool->category << "' for tool "
        << tool->name;
  }
}

TEST_F(RegisteredServerFixture, SchemaStatisticsBaseline) {
  size_t non_empty = 0;
  size_t empty = 0;
  const auto tools = catalog.get_all_tools();
  for (const auto *tool : tools) {
    if (has_schema_params(tool->input_schema)) {
      ++non_empty;
    } else {
      ++empty;
    }
  }
  EXPECT_EQ(non_empty + empty, tools.size());
  EXPECT_GT(non_empty, empty);
  EXPECT_GT(empty, 0);
}

TEST_F(RegisteredServerFixture, SchemaSampledTools) {
  const char *const kNonEmpty[] = {
      "property_get",          "intersect_physics_2d_ray",
      "execute_script",        "call_scene_tree_group",
      "set_tilemap_cell",      "queue_game_input",
      "get_scene_tree",        "batch_execute",
      "call_tool",             "code_execute",
      "search_tools",          "get_tool_detail"};
  const char *const kEmpty[] = {"get_engine_version",   "create_physics_2d_body",
                                "ping",                 "system_status",
                                "list_categories"};
  for (const char *name : kNonEmpty) {
    const auto *info = catalog.get_tool(name);
    ASSERT_NE(info, nullptr) << name;
    EXPECT_TRUE(has_schema_params(info->input_schema)) << name;
  }
  for (const char *name : kEmpty) {
    const auto *info = catalog.get_tool(name);
    ASSERT_NE(info, nullptr) << name;
    EXPECT_FALSE(has_schema_params(info->input_schema)) << name;
  }
}

TEST_F(RegisteredServerFixture, CatalogCoversServerTools) {
  auto result = client->ListTools();
  for (const auto &tool : result.tools) {
    EXPECT_NE(catalog.get_tool(tool.name), nullptr)
        << "tool registered on server but missing from catalog: " << tool.name;
  }
}

TEST_F(RegisteredServerFixture, CallHandlerUnknownToolReturnsError) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  auto result = godot_autopilot::dispatch::call_handler("no_such_tool_xyz", args);
  const auto *err = result.Find("error");
  ASSERT_NE(err, nullptr);
  ASSERT_TRUE(err->IsString());
  const std::string &msg = err->GetString();
  EXPECT_NE(msg.find("domain tool 'no_such_tool_xyz' not found"),
            std::string::npos);
}

TEST_F(RegisteredServerFixture, CallHandlerOversizedArgsKeepsFixedError) {
  std::string big(600, 'x');
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["payload"] = mcp::JsonValue(big);

  auto result = godot_autopilot::dispatch::call_handler("no_such_tool_xyz", args);
  const auto *err = result.Find("error");
  ASSERT_NE(err, nullptr);
  ASSERT_TRUE(err->IsString());
  const std::string &msg = err->GetString();
  EXPECT_NE(msg.find("domain tool 'no_such_tool_xyz' not found"),
            std::string::npos);
  EXPECT_EQ(msg.find(big), std::string::npos)
      << "error message must not echo back oversized arguments";
}

TEST_F(RegisteredServerFixture, ReRegisterIsIdempotentForServerAndCatalog) {
  godot_autopilot::register_all_tools(*server, queue, catalog, index, 9527);

  auto result = client->ListTools();
  EXPECT_EQ(result.tools.size(), kMetaToolCount);
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
  EXPECT_EQ(index.size(), catalog.size());
}

TEST_F(RegisteredServerFixture, TwoIndependentServersRegisterIdentically) {
  auto pair2 = mcp::InMemoryTransport::CreatePair();
  auto server2 = mcp::McpServer::Create(pair2.server);

  godot_autopilot::CommandQueue queue2;
  godot_autopilot::ToolCatalog catalog2;
  godot_autopilot::Bm25Index index2;
  godot_autopilot::register_all_tools(*server2, queue2, catalog2, index2,
                                         9528);

  std::thread thread2([&server2] { server2->Run(); });

  mcp::ClientOptions cops2;
  cops2.client_info = mcp::Implementation{"GsdTestClient2", "1.0.0"};
  cops2.connect_mode = mcp::ConnectMode::Auto;
  auto client2 = mcp::McpClient::Create(pair2.client, cops2);

  auto result1 = client->ListTools();
  auto result2 = client2->ListTools();
  EXPECT_EQ(result1.tools.size(), kMetaToolCount);
  EXPECT_EQ(result2.tools.size(), kMetaToolCount);
  EXPECT_EQ(catalog2.size(), catalog.size());

  client2->Close();
  server2->Close();
  if (thread2.joinable())
    thread2.join();
}

TEST_F(RegisteredServerFixture, Bm25IndexPopulatedAfterRegistration) {
  EXPECT_EQ(index.size(), catalog.size());
}

TEST_F(RegisteredServerFixture, RegistryIsSingleSourceOfTools) {
  auto& reg = godot_autopilot::get_active_registry();

  EXPECT_EQ(reg.meta_size(), kMetaToolCount);

  EXPECT_EQ(reg.all_any().size(), catalog.size());

  for (const char* name : kMetaToolNames) {
    EXPECT_NE(reg.find_meta(name), nullptr) << "missing meta tool: " << name;
  }

  EXPECT_NE(reg.find("system_status"), nullptr);

  for (auto* t : reg.all_any()) {
    EXPECT_NE(catalog.get_tool(t->meta().name), nullptr)
        << "registry tool missing from catalog: " << t->meta().name;
  }

  for (const auto* tool : catalog.get_all_tools()) {
    EXPECT_NE(reg.find_any(tool->name), nullptr)
        << "catalog entry missing from registry: " << tool->name;
  }
}
