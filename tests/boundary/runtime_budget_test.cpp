#include "tools/domains/capture/capture_retention.hpp"
#include "tools/domains/capture/capture_tools.hpp"
#include "tools/domains/nav/nav_budget.hpp"
#include "tools/domains/nav/nav_tools.hpp"
#include "tools/domains/render/render_budget.hpp"
#include "tools/domains/render/render_tools.hpp"
#include "util/rid_registry.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using godot_autopilot::capture_retention::prune_allowed;
using godot_autopilot::nav_budget::GridExtentFailure;
using godot_autopilot::nav_budget::validate_grid_extent;
using godot_autopilot::render_budget::ViewportSizeFailure;
using godot_autopilot::render_budget::validate_viewport_size;

namespace {

struct RenderDomainTag {};

mcp::JsonValue find_tool_schema(
    const std::vector<std::unique_ptr<godot_autopilot::ToolBase>> &tools,
    const std::string &name) {
  for (const auto &tool : tools) {
    if (tool->meta().name == name) {
      return tool->input_schema();
    }
  }
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

bool schema_has_property(const mcp::JsonValue &schema, const std::string &name) {
  const mcp::JsonValue *properties = schema.Find("properties");
  return properties != nullptr && properties->IsObject() &&
         properties->Find(name) != nullptr;
}

} // namespace

TEST(RuntimeBudgetTest, ViewportSizeAcceptsNormalDimensions) {
  const auto verdict = validate_viewport_size(640, 480);
  EXPECT_EQ(verdict.failure, ViewportSizeFailure::kNone);
  EXPECT_EQ(verdict.width, 640);
  EXPECT_EQ(verdict.height, 480);
}

TEST(RuntimeBudgetTest, ViewportSizeRejectsInvalidAndOversized) {
  EXPECT_EQ(validate_viewport_size(0, 480).failure,
            ViewportSizeFailure::kInvalidDimension);
  EXPECT_EQ(validate_viewport_size(-1, 480).failure,
            ViewportSizeFailure::kInvalidDimension);
  EXPECT_EQ(validate_viewport_size(8193, 480).failure,
            ViewportSizeFailure::kDimensionLimit);
  EXPECT_EQ(validate_viewport_size(8192, 4096).failure,
            ViewportSizeFailure::kPixelLimit);
  EXPECT_EQ(validate_viewport_size(4096, 4096).failure,
            ViewportSizeFailure::kNone);
}

TEST(RuntimeBudgetTest, GridExtentRejectsOverflowAndOversizedAreas) {
  const double nan_value = std::numeric_limits<double>::quiet_NaN();
  const double inf_value = std::numeric_limits<double>::infinity();
  EXPECT_EQ(validate_grid_extent(nan_value, 16).failure,
            GridExtentFailure::kNonFinite);
  EXPECT_EQ(validate_grid_extent(inf_value, 16).failure,
            GridExtentFailure::kNonFinite);
  EXPECT_EQ(validate_grid_extent(0, 16).failure,
            GridExtentFailure::kOutOfRange);
  EXPECT_EQ(validate_grid_extent(2049, 16).failure,
            GridExtentFailure::kDimensionLimit);
  EXPECT_EQ(validate_grid_extent(16.5, 16).failure,
            GridExtentFailure::kNotIntegral);
  EXPECT_EQ(validate_grid_extent(2048, 2048).failure,
            GridExtentFailure::kAreaLimit);
  const auto ok = validate_grid_extent(1024, 1024);
  EXPECT_EQ(ok.failure, GridExtentFailure::kNone);
  EXPECT_EQ(ok.width, 1024);
  EXPECT_EQ(ok.height, 1024);
}

TEST(RuntimeBudgetTest, CapturePruneOnlyManagedDirectory) {
  const std::string root = "C:/project/.gda/captures";
  EXPECT_TRUE(prune_allowed(root, root));
  EXPECT_TRUE(prune_allowed(root + "/nested", root));
  EXPECT_FALSE(prune_allowed("C:/project/screenshots", root));
  EXPECT_FALSE(prune_allowed("C:/project/.gda/captures_backup", root));
  EXPECT_FALSE(prune_allowed("", root));
  EXPECT_FALSE(prune_allowed(root, ""));
  EXPECT_TRUE(prune_allowed("C:\\project\\.gda\\captures", root));
}

TEST(RuntimeBudgetTest, RidStoreOwnershipLookup) {
  auto &store = godot_autopilot::util::rid_store<RenderDomainTag>();
  EXPECT_FALSE(store.contains(123456789));
  EXPECT_FALSE(store.erase(123456789));
}

TEST(RuntimeBudgetTest, SchemasExposeBudgetAndOwnershipGuards) {
  const auto render_tools = godot_autopilot::render_tools::make_tools();
  const mcp::JsonValue instance =
      find_tool_schema(render_tools, "render_instance");
  EXPECT_TRUE(schema_has_property(instance, "confirm"));
  const mcp::JsonValue viewport =
      find_tool_schema(render_tools, "render_viewport");
  EXPECT_TRUE(schema_has_property(viewport, "width"));
  EXPECT_TRUE(schema_has_property(viewport, "height"));

  const auto nav_tools = godot_autopilot::nav_tools::make_tools();
  const mcp::JsonValue astar = find_tool_schema(nav_tools, "astar_path");
  EXPECT_TRUE(schema_has_property(astar, "region"));
  EXPECT_TRUE(schema_has_property(astar, "size"));

  const auto capture_tools = godot_autopilot::capture_tools::make_tools();
  const mcp::JsonValue capture =
      find_tool_schema(capture_tools, "capture_editor_viewport");
  EXPECT_TRUE(schema_has_property(capture, "save_dir"));
}
