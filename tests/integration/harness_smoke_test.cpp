#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarness;

TEST_F(IntegrationHarness, SearchToolsEmptyQueryReturnsNonEmptyResults) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  mcp::JsonValue result = call("search_tools", args);
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr) << "search_tools returned no results key";
  ASSERT_TRUE(results->IsArray());
  EXPECT_FALSE(results->GetArray().empty());
}

} // namespace
