#include <gtest/gtest.h>

#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/command_queue.hpp"
#include "tools/authorization.hpp"
#include "tools/register_all.hpp"
#include "tools/side_effect_policy.hpp"
#include "tools/tool_base.hpp"
#include "tools/tool_catalog.hpp"
#include "util/bm25_index.hpp"

#include <memory>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using godot_autopilot::SideEffect;
namespace authorization = godot_autopilot::authorization;
namespace side_effect_policy = godot_autopilot::side_effect_policy;

class AllowEnvScope {
public:
  AllowEnvScope() {
    const char *saved = std::getenv("GODOT_AUTOPILOT_ALLOW");
    if (saved != nullptr)
      saved_ = saved;
#ifdef _WIN32
    _putenv_s("GODOT_AUTOPILOT_ALLOW", "__no_capabilities__");
#else
    setenv("GODOT_AUTOPILOT_ALLOW", "__no_capabilities__", 1);
#endif
  }

  ~AllowEnvScope() {
#ifdef _WIN32
    _putenv_s("GODOT_AUTOPILOT_ALLOW", saved_.c_str());
#else
    if (saved_.empty())
      unsetenv("GODOT_AUTOPILOT_ALLOW");
    else
      setenv("GODOT_AUTOPILOT_ALLOW", saved_.c_str(), 1);
#endif
  }

private:
  std::string saved_;
};

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
  EXPECT_GT(all.size(), 400u);
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
  EXPECT_NE(side_effect_policy::find_exemption("remove_resource_file"),
            nullptr);
  EXPECT_EQ(side_effect_policy::find_exemption("remove_resource"), nullptr);
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

TEST(DestructiveTierGuardTest, DestructiveToolsAreDeniedByDefault) {
  AllowEnvScope allow_scope;
  auto registry = ensure_registry();
  int destructive_count = 0;
  for (const auto &tool : registry->all_any()) {
    if (godot_autopilot::side_effect_of(*tool) != SideEffect::Destructive) {
      continue;
    }
    ++destructive_count;
    EXPECT_FALSE(authorization::tier_permits(authorization::kCapabilityDestructive,
                                             ""))
        << tool->meta().name;
    const mcp::JsonValue denied = authorization::deny_if_unauthorized(
        tool->meta().name, SideEffect::Destructive);
    ASSERT_FALSE(denied.IsNull()) << tool->meta().name;
    const mcp::JsonValue *required = denied.Find("authorization_required");
    ASSERT_NE(required, nullptr) << tool->meta().name;
    ASSERT_TRUE(required->IsString()) << tool->meta().name;
    EXPECT_EQ(required->GetString(), "destructive") << tool->meta().name;
  }
  EXPECT_GT(destructive_count, 0);
}

TEST(DestructiveTierGuardTest, SceneWriteToolsAreDeniedByDefault) {
  AllowEnvScope allow_scope;
  auto registry = ensure_registry();
  int scene_write_count = 0;
  for (const auto &tool : registry->all_any()) {
    if (godot_autopilot::side_effect_of(*tool) != SideEffect::WritesScene) {
      continue;
    }
    ++scene_write_count;
    const mcp::JsonValue denied = authorization::deny_if_unauthorized(
        tool->meta().name, SideEffect::WritesScene);
    ASSERT_FALSE(denied.IsNull()) << tool->meta().name;
    const mcp::JsonValue *required = denied.Find("authorization_required");
    ASSERT_NE(required, nullptr) << tool->meta().name;
    ASSERT_TRUE(required->IsString()) << tool->meta().name;
    EXPECT_EQ(required->GetString(), "write") << tool->meta().name;
  }
  EXPECT_GT(scene_write_count, 0);
}

TEST(DestructiveTierGuardTest, ProcessEnvironmentReadsAreDeniedByDefault) {
  AllowEnvScope allow_scope;
  const std::vector<std::string> names = {
      "get_os_environment", "get_os_unique_id", "get_os_user_data_dir"};
  for (const std::string &name : names) {
    EXPECT_EQ(side_effect_of_tool(name), "process") << name;
    const mcp::JsonValue denied =
        authorization::deny_if_unauthorized(name, SideEffect::Process);
    ASSERT_FALSE(denied.IsNull()) << name;
    const mcp::JsonValue *code = denied.Find("code");
    ASSERT_NE(code, nullptr) << name;
    ASSERT_TRUE(code->IsString()) << name;
    EXPECT_EQ(code->GetString(), "authorization_required") << name;
    const mcp::JsonValue *required = denied.Find("authorization_required");
    ASSERT_NE(required, nullptr) << name;
    ASSERT_TRUE(required->IsString()) << name;
    EXPECT_EQ(required->GetString(), "process") << name;
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
  EXPECT_EQ(side_effect_of_tool("remove_resource_file"), "writes_file");
  EXPECT_EQ(side_effect_of_tool("move_os_file_to_trash"), "writes_file");
  EXPECT_EQ(side_effect_of_tool("erase_input_map_action"), "writes_config");
  EXPECT_EQ(side_effect_of_tool("remove_autoload"), "writes_config");
  EXPECT_EQ(side_effect_of_tool("kill_os_process"), "process");
  EXPECT_EQ(side_effect_of_tool("execute_os_process"), "process");
  EXPECT_EQ(side_effect_of_tool("execute_script"), "code_execute");
  EXPECT_EQ(side_effect_of_tool("execute_game_script"), "game_runtime");
  EXPECT_EQ(side_effect_of_tool("property_set"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("set_properties_batch"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("create_scene_node"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("signal_connect"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("set_tilemap_cell"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("attach_script_to_node"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("add_group_node"), "writes_scene");
  EXPECT_EQ(side_effect_of_tool("get_os_environment"), "process");
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
  suspect.name = "remove_resource_file";
  suspect.description = "Delete a resource file from disk.";
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
