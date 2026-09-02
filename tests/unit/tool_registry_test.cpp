#include "tools/fn_tool.hpp"
#include "tools/meta_tools.hpp"
#include "tools/tool_registry.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

using godot_autopilot::FnTool;
using godot_autopilot::SideEffect;
using godot_autopilot::ToolMeta;
using godot_autopilot::ToolRegistry;
using godot_autopilot::make_fn_tool;
using godot_autopilot::side_effect_of;

namespace {

ToolMeta make_meta(const std::string& name, const std::string& description,
                   const std::string& category, bool basic_schema) {
  return ToolMeta{name, description, category, {"tag"}, basic_schema};
}

mcp::JsonValue echo_handler(const mcp::JsonValue& args) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  for (const auto& entry : args.GetObject()) {
    result[entry.first] = entry.second;
  }
  return result;
}

} // namespace

TEST(ToolRegistryTest, AddFindCategories) {
  ToolRegistry registry;
  registry.add(std::make_unique<FnTool>(
      make_meta("tool_a", "desc a", "Scene", true), echo_handler,
      mcp::JsonValue(mcp::JsonValue::object_tag)));
  registry.add(std::make_unique<FnTool>(
      make_meta("tool_b", "desc b", "Resources", false), echo_handler,
      mcp::JsonValue(mcp::JsonValue::object_tag)));

  ASSERT_NE(registry.find("tool_a"), nullptr);
  EXPECT_EQ(registry.find("no"), nullptr);
  EXPECT_EQ(registry.size(), 2u);

  auto categories = registry.categories();
  ASSERT_EQ(categories.size(), 2u);
  EXPECT_NE(std::find(categories.begin(), categories.end(), "Scene"),
            categories.end());
  EXPECT_NE(std::find(categories.begin(), categories.end(), "Resources"),
            categories.end());
}

TEST(ToolRegistryTest, ExecuteEchoArgs) {
  ToolRegistry registry;
  registry.add(std::make_unique<FnTool>(
      make_meta("tool_a", "desc a", "Scene", true), echo_handler,
      mcp::JsonValue(mcp::JsonValue::object_tag)));

  auto tool = registry.find("tool_a");
  ASSERT_NE(tool, nullptr);
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["v"] = 5;
  mcp::JsonValue result = tool->execute(args);
  ASSERT_TRUE(result.IsObject());
  const mcp::JsonValue* v = result.Find("v");
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->GetInt(), 5);
}

TEST(ToolRegistryTest, RoleInterfaceHelpers) {
  auto plain = std::make_unique<FnTool>(
      make_meta("tool_a", "desc a", "Scene", true), echo_handler,
      mcp::JsonValue(mcp::JsonValue::object_tag));
  EXPECT_EQ(side_effect_of(*plain), SideEffect::None);

  auto factory = make_fn_tool(make_meta("tool_f", "desc f", "Scene", true),
                              echo_handler,
                              mcp::JsonValue(mcp::JsonValue::object_tag));
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(side_effect_of(*factory), SideEffect::None);
}

TEST(ToolRegistryTest, MetaRegistration) {
  ToolRegistry registry;
  registry.add(std::make_unique<::godot_autopilot::MetaTool>(
      make_meta("tool_m", "desc m", "Scene", true),
      [](const mcp::JsonValue&) -> mcp::JsonValue {
        return mcp::JsonValue(mcp::JsonValue::object_tag);
      },
      mcp::JsonValue(mcp::JsonValue::object_tag)));

  EXPECT_NE(registry.find_meta("tool_m"), nullptr);
  EXPECT_NE(registry.find_any("tool_m"), nullptr);
  EXPECT_EQ(registry.meta_size(), 1u);
}

TEST(ToolRegistryTest, AddRoutsMetaToolByInterface) {
  ToolRegistry registry;

  registry.add(std::make_unique<::godot_autopilot::MetaTool>(
      ::godot_autopilot::ToolMeta{"ping_clone", "desc", "Meta", {"health"}, true},
      [](const mcp::JsonValue&) -> mcp::JsonValue {
        return mcp::JsonValue(mcp::JsonValue::object_tag);
      },
      mcp::JsonValue(mcp::JsonValue::object_tag)));

  registry.add(std::make_unique<::godot_autopilot::FnTool>(
      ToolMeta{"plain_domain", "desc", "Scene", {"x"}, true}, echo_handler,
      mcp::JsonValue(mcp::JsonValue::object_tag)));

  EXPECT_NE(registry.find_meta("ping_clone"), nullptr);
  EXPECT_EQ(registry.find("ping_clone"), nullptr);
  EXPECT_EQ(registry.meta_size(), 1u);

  EXPECT_NE(registry.find("plain_domain"), nullptr);
  EXPECT_EQ(registry.find_meta("plain_domain"), nullptr);
}