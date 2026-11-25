#include <gtest/gtest.h>

#include <mcp/Capabilities.hpp>
#include "util/json_value.hpp"
#include <mcp/client/McpClient.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/command_queue.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_registry.hpp"
#include "util/bm25_index.hpp"

namespace {

godot_autopilot::util::JsonValue load_json(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open " + path.generic_string());
  }
  const std::string text((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  try {
    return godot_autopilot::util::JsonValue::Parse(text);
  } catch (const std::exception &ex) {
    throw std::runtime_error("failed to parse " + path.generic_string() +
                             ": " + ex.what());
  }
}

std::optional<std::filesystem::path>
find_optional_file(const std::string &relative) {
  std::filesystem::path current = std::filesystem::current_path();
  while (true) {
    const std::filesystem::path candidate =
        current / std::filesystem::path(relative);
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
    if (!current.has_parent_path()) {
      break;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }
  return std::nullopt;
}

std::filesystem::path require_data_file(const std::string &relative) {
  const std::optional<std::filesystem::path> found =
      find_optional_file(relative);
  if (!found.has_value()) {
    throw std::runtime_error(relative + " not found from " +
                             std::filesystem::current_path().string());
  }
  return *found;
}

const godot_autopilot::util::JsonValue *require_member(const godot_autopilot::util::JsonValue &object,
                                     const char *key,
                                     const std::string &context) {
  const godot_autopilot::util::JsonValue *value = object.Find(key);
  if (value == nullptr) {
    throw std::runtime_error(context + " is missing member: " + key);
  }
  return value;
}

std::string require_string(const godot_autopilot::util::JsonValue &object, const char *key,
                           const std::string &context) {
  const godot_autopilot::util::JsonValue *value = require_member(object, key, context);
  if (!value->IsString()) {
    throw std::runtime_error(context + " member is not a string: " + key);
  }
  return value->GetString();
}

std::optional<std::string> row_status(const godot_autopilot::util::JsonValue &row) {
  const godot_autopilot::util::JsonValue *status = row.Find("status");
  if (status == nullptr || !status->IsString()) {
    return std::nullopt;
  }
  return status->GetString();
}

bool is_alias_row(const godot_autopilot::util::JsonValue &row) {
  const std::optional<std::string> status = row_status(row);
  if (status.has_value() && *status == "alias") {
    return true;
  }
  const godot_autopilot::util::JsonValue *flag = row.Find("alias");
  return flag != nullptr && flag->IsBool() && flag->GetBool();
}

std::optional<std::string>
resolve_declared_target(const godot_autopilot::ToolRegistry &registry,
                        const std::string &declared) {
  if (registry.find_any(declared) != nullptr) {
    return declared;
  }
  const std::optional<godot_autopilot::ToolAlias> alias =
      registry.resolve_alias(declared);
  if (alias.has_value()) {
    return alias->canonical;
  }
  return std::nullopt;
}

struct AliasCoverageTest : ::testing::Test {
  std::unique_ptr<mcp::McpServer> server;
  std::unique_ptr<mcp::McpClient> client;
  std::thread server_thread;
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server, queue, catalog, index, 9541);
    server_thread = std::thread([this] { server->Run(); });

    mcp::ClientOptions options;
    options.client_info =
        mcp::Implementation{"GdaAliasCoverageClient", "1.0.0"};
    options.connect_mode = mcp::ConnectMode::Auto;
    client = mcp::McpClient::Create(pair.client, options);

    queue.drain();
  }

  void TearDown() override {
    if (client) {
      client->Close();
    }
    if (server) {
      server->Close();
    }
    if (server_thread.joinable()) {
      server_thread.join();
    }
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::clear_active_registry();
  }
};

TEST_F(AliasCoverageTest, CapabilityMatrixAliasRowsResolveWithoutConflicts) {
  const std::filesystem::path matrix_path =
      require_data_file("tests/data/capability_matrix.json");
  const godot_autopilot::util::JsonValue matrix = load_json(matrix_path);
  const godot_autopilot::util::JsonValue *rows =
      require_member(matrix, "rows", matrix_path.generic_string());
  if (!rows->IsArray()) {
    throw std::runtime_error(matrix_path.generic_string() +
                             " member is not an array: rows");
  }

  std::vector<const godot_autopilot::util::JsonValue *> all_rows;
  all_rows.reserve(rows->GetArray().size());
  for (const godot_autopilot::util::JsonValue &row : rows->GetArray()) {
    all_rows.push_back(&row);
  }
  if (const godot_autopilot::util::JsonValue *meta_rows = matrix.Find("meta_rows");
      meta_rows != nullptr) {
    if (!meta_rows->IsArray()) {
      throw std::runtime_error(matrix_path.generic_string() +
                               " member is not an array: meta_rows");
    }
    for (const godot_autopilot::util::JsonValue &row : meta_rows->GetArray()) {
      all_rows.push_back(&row);
    }
  }

  const std::shared_ptr<godot_autopilot::ToolRegistry> registry =
      godot_autopilot::get_active_registry();
  ASSERT_NE(registry.get(), nullptr);

  std::size_t alias_rows = 0;
  std::unordered_set<std::string> seen_aliases;
  for (const godot_autopilot::util::JsonValue *row : all_rows) {
    if (!row->IsObject()) {
      ADD_FAILURE() << "capability matrix row is not an object";
      continue;
    }
    if (!is_alias_row(*row)) {
      continue;
    }
    ++alias_rows;
    const std::string old_tool =
        require_string(*row, "old_tool", "capability matrix alias row");
    const std::string declared =
        require_string(*row, "new_tool", "capability matrix row " + old_tool);

    EXPECT_TRUE(seen_aliases.insert(old_tool).second)
        << "duplicate alias row in capability matrix: " << old_tool;
    EXPECT_NE(old_tool, declared)
        << "alias row points at itself: " << old_tool;
    EXPECT_EQ(registry->find_any(old_tool).get(), nullptr)
        << "alias name shadows a real tool: " << old_tool;

    const std::optional<godot_autopilot::ToolAlias> alias =
        registry->resolve_alias(old_tool);
    EXPECT_TRUE(alias.has_value())
        << "capability matrix alias is not registered: " << old_tool;
    if (!alias.has_value()) {
      continue;
    }
    EXPECT_FALSE(alias->canonical.empty())
        << "registered alias has an empty canonical: " << old_tool;
    EXPECT_NE(alias->canonical, old_tool)
        << "registered alias resolves to its own name: " << old_tool;

    const std::shared_ptr<godot_autopilot::ToolBase> canonical_tool =
        registry->find_any(alias->canonical);
    EXPECT_NE(canonical_tool.get(), nullptr)
        << "registered alias canonical tool is missing: " << old_tool
        << " -> " << alias->canonical;

    const std::optional<godot_autopilot::ToolInfo> catalog_entry =
        catalog.get_tool(old_tool);
    EXPECT_TRUE(catalog_entry.has_value())
        << "catalog does not resolve alias: " << old_tool;
    if (catalog_entry.has_value()) {
      EXPECT_EQ(catalog_entry->name, alias->canonical)
          << "catalog alias target disagrees with registry: " << old_tool;
    }

    const std::optional<std::string> status = row_status(*row);
    if (status.has_value() && *status == "alias") {
      const std::optional<std::string> declared_target =
          resolve_declared_target(*registry, declared);
      EXPECT_TRUE(declared_target.has_value())
          << "capability matrix new_tool is neither a tool nor an alias: "
          << old_tool << " -> " << declared;
      if (declared_target.has_value()) {
        EXPECT_EQ(*declared_target, alias->canonical)
            << "capability matrix target disagrees with registry: " << old_tool
            << " declared=" << declared << " registry=" << alias->canonical;
      }
    }
  }

  std::cout << "[alias-coverage] alias_rows=" << alias_rows
            << " registry_aliases=" << registry->alias_count() << "\n";
  RecordProperty("alias_rows", std::to_string(alias_rows));
  EXPECT_GT(alias_rows, 0u)
      << "capability matrix exposes no alias rows to verify";
}

} // namespace
