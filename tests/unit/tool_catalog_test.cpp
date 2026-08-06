#include "tools/tool_catalog.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

using godot_self_driving::ToolCatalog;
using godot_self_driving::ToolInfo;

namespace {

ToolInfo make_tool(const std::string &name, const std::string &category) {
  ToolInfo info;
  info.name = name;
  info.description = "desc of " + name;
  info.category = category;
  info.tags = {"tag_a", "tag_b"};
  info.input_schema = mcp::JsonValue(mcp::JsonValue::object_tag);
  return info;
}

} // namespace

TEST(ToolCatalogTest, AddAndGetTool) {
  ToolCatalog catalog;
  catalog.add_tool(make_tool("alpha", "CatA"));
  const ToolInfo *info = catalog.get_tool("alpha");
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->description, "desc of alpha");
  EXPECT_EQ(info->category, "CatA");
  EXPECT_EQ(info->tags.size(), 2u);
}

TEST(ToolCatalogTest, GetMissingToolReturnsNullptr) {
  ToolCatalog catalog;
  EXPECT_EQ(catalog.get_tool("nope"), nullptr);
}

TEST(ToolCatalogTest, DuplicateAddOverwrites) {
  ToolCatalog catalog;
  ToolInfo first = make_tool("dup", "CatA");
  first.description = "first";
  catalog.add_tool(first);
  ToolInfo second = make_tool("dup", "CatB");
  second.description = "second";
  catalog.add_tool(second);
  EXPECT_EQ(catalog.size(), 1u);
  const ToolInfo *info = catalog.get_tool("dup");
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->description, "second");
  EXPECT_EQ(info->category, "CatB");
}

TEST(ToolCatalogTest, GetAllToolsReturnsAll) {
  ToolCatalog catalog;
  catalog.add_tool(make_tool("one", "CatA"));
  catalog.add_tool(make_tool("two", "CatB"));
  catalog.add_tool(make_tool("three", "CatC"));
  auto all = catalog.get_all_tools();
  ASSERT_EQ(all.size(), 3u);
  std::vector<std::string> names;
  for (const auto *info : all) {
    names.push_back(info->name);
  }
  EXPECT_NE(std::find(names.begin(), names.end(), "one"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "two"), names.end());
  EXPECT_NE(std::find(names.begin(), names.end(), "three"), names.end());
}

TEST(ToolCatalogTest, CategoriesAreUnique) {
  ToolCatalog catalog;
  catalog.add_tool(make_tool("a", "Same"));
  catalog.add_tool(make_tool("b", "Same"));
  catalog.add_tool(make_tool("c", "Other"));
  auto categories = catalog.get_categories();
  ASSERT_EQ(categories.size(), 2u);
  EXPECT_NE(std::find(categories.begin(), categories.end(), "Same"),
            categories.end());
  EXPECT_NE(std::find(categories.begin(), categories.end(), "Other"),
            categories.end());
}

TEST(ToolCatalogTest, ConcurrentAddIsSafe) {
  ToolCatalog catalog;
  const int kThreads = 8;
  const int kPerThread = 25;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&catalog, t] {
      for (int i = 0; i < kPerThread; ++i) {
        catalog.add_tool(make_tool(
            "ct_" + std::to_string(t) + "_" + std::to_string(i), "Cat"));
      }
    });
  }
  for (auto &th : threads) {
    th.join();
  }
  EXPECT_EQ(catalog.size(), static_cast<size_t>(kThreads * kPerThread));
  EXPECT_EQ(catalog.get_all_tools().size(),
            static_cast<size_t>(kThreads * kPerThread));
  EXPECT_NE(catalog.get_tool("ct_0_0"), nullptr);
  EXPECT_NE(catalog.get_tool("ct_7_24"), nullptr);
}

TEST(ToolCatalogTest, PopulateDefaultToolsOnce) {
  ToolCatalog catalog;
  catalog.populate_default_tools();
  EXPECT_EQ(catalog.size(), 5u);
  EXPECT_NE(catalog.get_tool("ping"), nullptr);
  EXPECT_NE(catalog.get_tool("search_tools"), nullptr);
  catalog.populate_default_tools();
  EXPECT_EQ(catalog.size(), 5u);
}
