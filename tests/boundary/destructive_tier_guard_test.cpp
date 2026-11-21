#include <gtest/gtest.h>

#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/command_queue.hpp"
#include "core/observability/log_system.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/side_effect_policy.hpp"
#include "tools/base/tool_base.hpp"
#include "tools/base/tool_catalog.hpp"
#include "util/bm25_index.hpp"

#include <algorithm>
#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using godot_autopilot::SideEffect;
namespace side_effect_policy = godot_autopilot::side_effect_policy;

std::unique_ptr<mcp::McpServer> &server_handle() {
  static std::unique_ptr<mcp::McpServer> server;
  return server;
}

godot_autopilot::CommandQueue &queue_handle() {
  static godot_autopilot::CommandQueue queue;
  return queue;
}

godot_autopilot::ToolCatalog &catalog_handle() {
  static godot_autopilot::ToolCatalog catalog;
  return catalog;
}

godot_autopilot::Bm25Index &index_handle() {
  static godot_autopilot::Bm25Index index;
  return index;
}

std::shared_ptr<godot_autopilot::ToolRegistry> ensure_registry() {
  auto registry = godot_autopilot::get_active_registry();
  if (server_handle() == nullptr) {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server_handle() = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server_handle(), queue_handle(),
                                       catalog_handle(), index_handle(), 9527);
    registry = godot_autopilot::get_active_registry();
  }
  return registry;
}

std::vector<side_effect_policy::ToolDeclaration> declarations() {
  auto registry = ensure_registry();
  std::vector<side_effect_policy::ToolDeclaration> result;
  for (const auto &tool : registry->all_any()) {
    side_effect_policy::ToolDeclaration entry;
    entry.name = tool->meta().name;
    entry.description = tool->meta().description;
    entry.destructive =
        godot_autopilot::side_effect_of(*tool) == SideEffect::Destructive;
    result.push_back(std::move(entry));
  }
  return result;
}

std::string side_effect_of_tool(const std::string &name) {
  auto registry = ensure_registry();
  auto tool = registry->find_any(name);
  if (tool == nullptr) {
    return "<missing>";
  }
  return godot_autopilot::side_effect_name(
      godot_autopilot::side_effect_of(*tool));
}

} // namespace

TEST(DestructiveTierGuardTest, EveryToolIsDeclared) {
  const auto all = declarations();
  EXPECT_GE(all.size(), 250u);
}

TEST(DestructiveTierGuardTest, DestructionShapedToolsAreDestructiveOrExempted) {
  const auto all = declarations();
  const std::vector<std::string> suspects =
      side_effect_policy::destructive_suspects(all);
  std::string report;
  for (const std::string &name : suspects) {
    if (!report.empty()) {
      report += ", ";
    }
    report += name;
  }
  EXPECT_TRUE(suspects.empty())
      << "tools whose name or description implies irreversible destruction but "
         "are not declared as the destructive side effect: "
      << report
      << ". Declare SideEffect::Destructive, or add an explicit exemption with "
         "a reason in src/tools/side_effect_policy.hpp";
}

TEST(DestructiveTierGuardTest, EveryExemptionNamesARegisteredTool) {
  auto registry = ensure_registry();
  for (const auto &entry : side_effect_policy::kDestructiveExemptions) {
    EXPECT_NE(registry->find_any(entry.tool), nullptr)
        << "exempted tool does not exist: " << entry.tool;
    EXPECT_FALSE(std::string(entry.reason).empty())
        << "exemption without a reason: " << entry.tool;
  }
}

TEST(DestructiveTierGuardTest, ExemptionLookupIsCaseSensitiveAndExact) {
  EXPECT_NE(side_effect_policy::find_exemption("erase_input_map"),
            nullptr);
  EXPECT_EQ(side_effect_policy::find_exemption("erase_input_map_action"),
            nullptr);
  EXPECT_EQ(side_effect_policy::find_exemption("delete_display_window"),
            nullptr);
}

TEST(DestructiveTierGuardTest, ExemptedToolsPassTheGuard) {
  auto registry = ensure_registry();
  for (const auto &entry : side_effect_policy::kDestructiveExemptions) {
    auto tool = registry->find_any(entry.tool);
    ASSERT_NE(tool, nullptr) << entry.tool;
    side_effect_policy::ToolDeclaration declaration;
    declaration.name = tool->meta().name;
    declaration.description = tool->meta().description;
    declaration.destructive =
        godot_autopilot::side_effect_of(*tool) == SideEffect::Destructive;
    EXPECT_TRUE(side_effect_policy::looks_destructive(declaration))
        << "exemption is stale, the tool no longer looks destructive: "
        << entry.tool;
  }
}

TEST(DestructiveTierGuardTest, DestructiveToolsKeepDestructiveTier) {
  auto registry = ensure_registry();
  int destructive_count = 0;
  for (const auto &tool : registry->all_any()) {
    if (godot_autopilot::side_effect_of(*tool) != SideEffect::Destructive) {
      continue;
    }
    ++destructive_count;
    EXPECT_EQ(side_effect_of_tool(tool->meta().name), "destructive")
        << tool->meta().name;
  }
  EXPECT_GT(destructive_count, 0);
}

TEST(DestructiveTierGuardTest, SceneWriteToolsKeepWriteTier) {
  auto registry = ensure_registry();
  int scene_write_count = 0;
  for (const auto &tool : registry->all_any()) {
    if (godot_autopilot::side_effect_of(*tool) != SideEffect::WritesScene) {
      continue;
    }
    ++scene_write_count;
    EXPECT_EQ(side_effect_of_tool(tool->meta().name), "writes_scene")
        << tool->meta().name;
  }
  EXPECT_GT(scene_write_count, 0);
}

TEST(DestructiveTierGuardTest, ProcessEnvironmentReadsKeepProcessTier) {
  const std::vector<std::string> names = {
      "os_environment", "get_os_user_data_dir"};
  for (const std::string &name : names) {
    EXPECT_EQ(side_effect_of_tool(name), "process") << name;
  }
}
TEST(DestructiveTierGuardTest, ReviewedDestructiveToolsKeepTheirTier) {
  EXPECT_EQ(side_effect_of_tool("delete_display_window"), "destructive");
  EXPECT_EQ(side_effect_of_tool("delete_scene_node"), "destructive");
  EXPECT_EQ(side_effect_of_tool("remove_animation"), "destructive");
  EXPECT_EQ(side_effect_of_tool("remove_animation_track"), "destructive");
  EXPECT_EQ(side_effect_of_tool("remove_audio_bus_effect"), "destructive");
  EXPECT_EQ(side_effect_of_tool("remove_debug_custom_monitor"), "destructive");
}

TEST(DestructiveTierGuardTest, ReviewedWriteAndDangerToolsKeepTheirTier) {
  EXPECT_EQ(side_effect_of_tool("resource_file"), "writes_file");
  EXPECT_EQ(side_effect_of_tool("os_file_write"), "writes_file");
  EXPECT_EQ(side_effect_of_tool("erase_input_map"), "writes_config");
  EXPECT_EQ(side_effect_of_tool("update_autoloads"), "writes_config");
  EXPECT_EQ(side_effect_of_tool("os_process"), "process");
  EXPECT_EQ(side_effect_of_tool("execute_script"), "code_execute");
  EXPECT_EQ(side_effect_of_tool("run_gdscript_tests"), "code_execute");
  EXPECT_EQ(side_effect_of_tool("game_script"), "game_runtime");
  EXPECT_EQ(side_effect_of_tool("property_set"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("scene_camera"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("create_scene_node"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("signal_connect"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("set_tilemap_cells"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("attach_script_to_node"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("node_group"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("os_environment"), "process");
  EXPECT_EQ(side_effect_of_tool("get_os_unique_id"), "process");
  EXPECT_EQ(side_effect_of_tool("get_os_user_data_dir"), "process");
}

TEST(DestructiveTierGuardTest, GuardFlagsUndeclaredDestructionTools) {
  std::vector<side_effect_policy::ToolDeclaration> tools;
  side_effect_policy::ToolDeclaration suspect;
  suspect.name = "delete_everything";
  suspect.description = "Delete every object that already exists.";
  tools.push_back(suspect);
  const std::vector<std::string> flagged =
      side_effect_policy::destructive_suspects(tools);
  ASSERT_EQ(flagged.size(), 1u);
  EXPECT_EQ(flagged[0], "delete_everything");
}

TEST(DestructiveTierGuardTest, GuardAcceptsDeclaredDestructiveTools) {
  std::vector<side_effect_policy::ToolDeclaration> tools;
  side_effect_policy::ToolDeclaration declared;
  declared.name = "delete_everything";
  declared.description = "Delete every object that already exists.";
  declared.destructive = true;
  tools.push_back(declared);
  EXPECT_TRUE(side_effect_policy::destructive_suspects(tools).empty());
}

TEST(DestructiveTierGuardTest, ExemptionMechanismSuppressesTheFinding) {
  std::vector<side_effect_policy::ToolDeclaration> tools;
  side_effect_policy::ToolDeclaration suspect;
  suspect.name = "erase_input_map";
  suspect.description = "Erase an action from the input map.";
  tools.push_back(suspect);
  ASSERT_NE(side_effect_policy::find_exemption(suspect.name), nullptr);
  EXPECT_TRUE(side_effect_policy::destructive_suspects(tools).empty());
}

TEST(DestructiveTierGuardTest, TierCriteriaCoverEverySideEffect) {
  EXPECT_GE(sizeof(side_effect_policy::kTierCriteria) /
                sizeof(side_effect_policy::kTierCriteria[0]),
            4u);
  EXPECT_FALSE(std::string(side_effect_policy::kBoundaryDiskDeletion).empty());
  EXPECT_FALSE(std::string(side_effect_policy::kBoundaryHostObject).empty());
  EXPECT_FALSE(std::string(side_effect_policy::kBoundarySceneWrite).empty());
}

TEST(DestructiveTierGuardTest, RegistrationAuditsUndeclaredDestructiveTools) {
  ensure_registry();
  const size_t marker = godot_autopilot::LogSystem::instance().next_index();
  {
    godot_autopilot::ToolSpec suspect;
    suspect.name = "wipe_all_records";
    suspect.description = "Wipe all records permanently.";
    suspect.category = "Test";
    suspect.handler = [](const mcp::JsonValue &) {
      return mcp::JsonValue(mcp::JsonValue::object_tag);
    };
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::dynamic_specs::store().push_back(std::move(suspect));
  }

  auto pair = mcp::InMemoryTransport::CreatePair();
  auto audit_server = mcp::McpServer::Create(pair.server);
  godot_autopilot::register_all_tools(
      *audit_server, queue_handle(), catalog_handle(), index_handle(), 9527);

  {
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    auto &specs = godot_autopilot::dynamic_specs::store();
    specs.erase(std::remove_if(specs.begin(), specs.end(),
                               [](const godot_autopilot::ToolSpec &spec) {
                                 return spec.name == "wipe_all_records";
                               }),
                specs.end());
  }
  auto restore_pair = mcp::InMemoryTransport::CreatePair();
  auto restore_server = mcp::McpServer::Create(restore_pair.server);
  godot_autopilot::register_all_tools(
      *restore_server, queue_handle(), catalog_handle(), index_handle(), 9527);

  godot_autopilot::LogSystem::Query query;
  query.min_level = godot_autopilot::LogLevel::Warning;
  query.filter_text = "not declared destructive";
  const auto entries = godot_autopilot::LogSystem::instance().query(query);
  bool found = false;
  for (const auto &entry : entries) {
    if (entry.serial >= marker &&
        entry.message.find("wipe_all_records") != std::string::npos) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "registration-period side_effect_policy audit did not "
                        "flag the undeclared destruction-shaped tool";
}
