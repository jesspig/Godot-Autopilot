#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarness;

TEST_F(IntegrationHarness, SearchToolsEmptyQueryReturnsNonEmptyResults) {
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue result = call("search_tools", args);
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr) << "search_tools returned no results key";
  ASSERT_TRUE(results->IsArray());
  EXPECT_FALSE(results->GetArray().empty());
}

} // namespace
