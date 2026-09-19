#include "tools/tool_registry.hpp"
#include "tools/tool_spec.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

using godot_autopilot::SideEffect;
using godot_autopilot::ToolRegistry;
using godot_autopilot::ToolSpec;
using godot_autopilot::make_spec_tool;
using godot_autopilot::make_tool_info;
using godot_autopilot::side_effect_of;

namespace {

mcp::JsonValue echo_handler(const mcp::JsonValue& args) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  for (const auto& entry : args.GetObject()) {
    result[entry.first] = entry.second;
  }
  return result;
}

ToolSpec make_spec(const std::string& name, const std::string& description,
                   const std::string& category,
                   uint32_t flags = godot_autopilot::tool_flags::kNone) {
  ToolSpec spec;
  spec.name = name;
  spec.description = description;
  spec.category = category;
  spec.tags = {"tag"};
  spec.flags = flags;
  spec.handler = echo_handler;
  return spec;
}

} // namespace

TEST(ToolRegistryTest, AddFindCategories) {
  ToolRegistry registry;
  registry.add(make_spec_tool(make_spec("tool_a", "desc a", "Scene")));
  registry.add(make_spec_tool(make_spec("tool_b", "desc b", "Resources")));

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
  registry.add(make_spec_tool(make_spec("tool_a", "desc a", "Scene")));

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
  auto plain = make_spec_tool(make_spec("tool_a", "desc a", "Scene"));
  EXPECT_EQ(side_effect_of(*plain), SideEffect::None);

  auto factory = make_spec_tool(make_spec("tool_f", "desc f", "Scene"));
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(side_effect_of(*factory), SideEffect::None);
}

TEST(ToolRegistryTest, MetaRegistration) {
  ToolRegistry registry;
  registry.add(make_spec_tool(
      make_spec("tool_m", "desc m", "Scene", godot_autopilot::tool_flags::kMeta)));

  EXPECT_NE(registry.find_meta("tool_m"), nullptr);
  EXPECT_NE(registry.find_any("tool_m"), nullptr);
  EXPECT_EQ(registry.meta_size(), 1u);
}

TEST(ToolRegistryTest, AddRoutesMetaToolByFlags) {
  ToolRegistry registry;

  registry.add(make_spec_tool(
      make_spec("ping_clone", "desc", "Meta", godot_autopilot::tool_flags::kMeta)));
  registry.add(make_spec_tool(make_spec("plain_domain", "desc", "Scene")));

  EXPECT_NE(registry.find_meta("ping_clone"), nullptr);
  EXPECT_EQ(registry.find("ping_clone"), nullptr);
  EXPECT_EQ(registry.meta_size(), 1u);

  EXPECT_NE(registry.find("plain_domain"), nullptr);
  EXPECT_EQ(registry.find_meta("plain_domain"), nullptr);
}

TEST(ToolRegistryTest, SpecToolFlagsDriveMetaRoutingAndToolInfo) {
  ToolRegistry registry;
  ToolSpec spec;
  spec.name = "spec_meta_probe";
  spec.description = "d";
  spec.category = "Meta";
  spec.flags = godot_autopilot::tool_flags::kMeta |
               godot_autopilot::tool_flags::kDynamic;
  spec.handler = [](const mcp::JsonValue&) {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  };

  auto tool = make_spec_tool(std::move(spec));
  auto* raw = tool.get();
  registry.add(std::move(tool));

  EXPECT_NE(registry.find_meta("spec_meta_probe"), nullptr);
  EXPECT_EQ(registry.find("spec_meta_probe"), nullptr);

  const auto info = make_tool_info(*raw);
  EXPECT_TRUE(info.dynamic);
  EXPECT_FALSE(info.mutating);
}
