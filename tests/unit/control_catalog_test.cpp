#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/control/catalog_service.hpp"
#include "support/control_fixture.hpp"

namespace {

using namespace gda::control;
using namespace gda::control::fixture;
namespace gcp = gda::gcp;

TEST(ControlCatalogTest, AddDescribeAndAliasResolution) {
  CatalogService catalog = fixture::sample_catalog();
  EXPECT_EQ(catalog.size(), 3u);
  EXPECT_EQ(catalog.resolve_alias("inspect_scene"), "scene.inspect");
  EXPECT_EQ(catalog.resolve_alias("scene.inspect"), "scene.inspect");
  EXPECT_EQ(catalog.resolve_alias("missing"), "");
  const std::optional<CatalogEntry> described =
      catalog.describe("inspect_scene");
  ASSERT_TRUE(described.has_value());
  EXPECT_EQ(described->name, "scene.inspect");
  EXPECT_FALSE(described->high_risk);
  const std::optional<CatalogEntry> risky = catalog.describe("project.open");
  ASSERT_TRUE(risky.has_value());
  EXPECT_TRUE(risky->high_risk);
  EXPECT_TRUE(risky->mutating);
}

TEST(ControlCatalogTest, ListFiltersCategoryAndPaginates) {
  const CatalogService catalog = fixture::sample_catalog();
  const std::vector<CatalogEntry> all = catalog.list("", 0, 0);
  ASSERT_EQ(all.size(), 3u);
  EXPECT_EQ(all[0].name, "project.open");
  const std::vector<CatalogEntry> scene = catalog.list("scene", 10, 0);
  ASSERT_EQ(scene.size(), 1u);
  EXPECT_EQ(scene[0].name, "scene.inspect");
  const std::vector<CatalogEntry> page = catalog.list("", 1, 1);
  ASSERT_EQ(page.size(), 1u);
  EXPECT_EQ(page[0].name, "scene.inspect");
}

TEST(ControlCatalogTest, SearchScoresAndRespectsFilters) {
  const CatalogService catalog = fixture::sample_catalog();
  CatalogQuery query;
  query.text = "scene";
  query.limit = 10;
  const std::vector<CatalogScore> results = catalog.search(query);
  ASSERT_FALSE(results.empty());
  EXPECT_EQ(results[0].entry.name, "scene.inspect");
  EXPECT_GT(results[0].score, 0.0);

  CatalogQuery filtered;
  filtered.text = "scene";
  filtered.category = "project";
  EXPECT_TRUE(catalog.search(filtered).empty());

  CatalogQuery tagged;
  tagged.tags = {"async"};
  const std::vector<CatalogScore> tagged_results = catalog.search(tagged);
  ASSERT_EQ(tagged_results.size(), 1u);
  EXPECT_EQ(tagged_results[0].entry.name, "task.run");

  CatalogQuery capped;
  capped.limit = 2;
  EXPECT_EQ(catalog.search(capped).size(), 2u);
  CatalogQuery skipped;
  skipped.offset = 2;
  skipped.limit = 1;
  const std::vector<CatalogScore> tail = catalog.search(skipped);
  ASSERT_EQ(tail.size(), 1u);
  EXPECT_EQ(tail[0].entry.name, "task.run");
}

TEST(ControlCatalogTest, ReplaceRemoveAndCategories) {
  CatalogService catalog;
  CatalogEntry entry;
  entry.name = "a";
  entry.category = "x";
  entry.aliases = {"alias-a"};
  catalog.replace({entry});
  EXPECT_EQ(catalog.size(), 1u);
  EXPECT_EQ(catalog.resolve_alias("alias-a"), "a");
  EXPECT_TRUE(catalog.remove("alias-a"));
  EXPECT_EQ(catalog.size(), 0u);
  EXPECT_EQ(catalog.resolve_alias("alias-a"), "");
  EXPECT_FALSE(catalog.remove("alias-a"));
  const std::vector<std::string> categories =
      fixture::sample_catalog().categories();
  ASSERT_EQ(categories.size(), 3u);
  EXPECT_EQ(categories[0], "project");
  EXPECT_EQ(categories[1], "scene");
  EXPECT_EQ(categories[2], "task");
}

} // namespace
