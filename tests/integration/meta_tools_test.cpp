#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class MetaToolsSuite : public IntegrationHarnessWithAllow {
 public:
  MetaToolsSuite() : IntegrationHarnessWithAllow("code_execute") {}
};

mcp::JsonValue make_args() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

std::size_t results_count(const mcp::JsonValue &result) {
  const auto *results = result.Find("results");
  if (results == nullptr || !results->IsArray()) {
    return 0;
  }
  return results->GetArray().size();
}

TEST_F(MetaToolsSuite, PingReturnsPong) {
  mcp::JsonValue result = call("ping", make_args());
  const auto *v = result.Find("result");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_EQ(v->GetString(), "pong");
}

TEST_F(MetaToolsSuite, SearchToolsKeywordReturnsNonEmptyResults) {
  mcp::JsonValue args = make_args();
  args["query"] = mcp::JsonValue("create_scene_node");
  mcp::JsonValue result = call("search_tools", args);
  const auto *v = result.Find("results");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsArray()) << result.Dump();
  EXPECT_FALSE(v->GetArray().empty());
}

TEST_F(MetaToolsSuite, SearchToolsCategorySceneReturnsNonEmptyResults) {
  mcp::JsonValue args = make_args();
  args["query"] = mcp::JsonValue("create_scene_node");
  args["category"] = mcp::JsonValue("Scene");
  mcp::JsonValue result = call("search_tools", args);
  const auto *v = result.Find("results");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsArray()) << result.Dump();
  EXPECT_FALSE(v->GetArray().empty());
}

TEST_F(MetaToolsSuite, SearchToolsTagNodeReturnsNonEmptyResults) {
  mcp::JsonValue args = make_args();
  args["query"] = mcp::JsonValue("create_scene_node");
  mcp::JsonValue tags(mcp::JsonValue::array_tag);
  tags.PushBack(mcp::JsonValue("node"));
  args["tags"] = std::move(tags);
  mcp::JsonValue result = call("search_tools", args);
  const auto *v = result.Find("results");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsArray()) << result.Dump();
  EXPECT_FALSE(v->GetArray().empty());
}

TEST_F(MetaToolsSuite, SearchToolsDefaultsToTwentyResults) {
  mcp::JsonValue result = call("search_tools", make_args());
  EXPECT_EQ(results_count(result), static_cast<std::size_t>(20)) << result.Dump();
}

TEST_F(MetaToolsSuite, SearchToolsLimitTenReturnsTenResults) {
  mcp::JsonValue args = make_args();
  args["limit"] = mcp::JsonValue(static_cast<int64_t>(10));
  mcp::JsonValue result = call("search_tools", args);
  EXPECT_EQ(results_count(result), static_cast<std::size_t>(10)) << result.Dump();
}

TEST_F(MetaToolsSuite, SearchToolsLimitZeroReturnsAllTools) {
  mcp::JsonValue categories = call("list_categories", make_args());
  const auto *category_list = categories.Find("categories");
  ASSERT_NE(category_list, nullptr) << categories.Dump();
  ASSERT_TRUE(category_list->IsArray()) << categories.Dump();
  std::size_t total = 0;
  for (const auto &category : category_list->GetArray()) {
    const auto *count = category.Find("tool_count");
    ASSERT_NE(count, nullptr) << category.Dump();
    ASSERT_TRUE(count->IsInt()) << category.Dump();
    total += static_cast<std::size_t>(count->GetInt());
  }

  mcp::JsonValue args = make_args();
  args["limit"] = mcp::JsonValue(static_cast<int64_t>(0));
  mcp::JsonValue result = call("search_tools", args);
  EXPECT_EQ(results_count(result), total) << result.Dump();
  EXPECT_GT(total, static_cast<std::size_t>(20));
}

TEST_F(MetaToolsSuite, SearchToolsNegativeLimitReturnsInvalidParameter) {
  mcp::JsonValue args = make_args();
  args["limit"] = mcp::JsonValue(static_cast<int64_t>(-1));
  mcp::JsonValue result = call("search_tools", args);
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr) << result.Dump();
  ASSERT_TRUE(error->IsString()) << result.Dump();
  EXPECT_EQ(error->GetString(),
            "invalid parameter: limit must be a non-negative integer");
  EXPECT_EQ(result.Find("results"), nullptr) << result.Dump();
}

TEST_F(MetaToolsSuite, SearchToolsCategoryHonorsLimit) {
  mcp::JsonValue args = make_args();
  args["category"] = mcp::JsonValue("Scene");
  args["limit"] = mcp::JsonValue(static_cast<int64_t>(5));
  mcp::JsonValue result = call("search_tools", args);
  EXPECT_LE(results_count(result), static_cast<std::size_t>(5)) << result.Dump();
  EXPECT_GT(results_count(result), static_cast<std::size_t>(0)) << result.Dump();
}

TEST_F(MetaToolsSuite, ListCategoriesReturnsNonEmptyCategories) {
  mcp::JsonValue result = call("list_categories", make_args());
  const auto *v = result.Find("categories");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsArray()) << result.Dump();
  EXPECT_FALSE(v->GetArray().empty());
}

TEST_F(MetaToolsSuite, GetToolDetailExistingReturnsToolShape) {
  mcp::JsonValue args = make_args();
  args["name"] = mcp::JsonValue("create_scene_node");
  mcp::JsonValue result = call("get_tool_detail", args);
  const auto *tool = result.Find("tool");
  ASSERT_NE(tool, nullptr) << result.Dump();
  const auto *name = tool->Find("name");
  ASSERT_NE(name, nullptr) << result.Dump();
  ASSERT_TRUE(name->IsString());
  EXPECT_EQ(name->GetString(), "create_scene_node");
  const auto *description = tool->Find("description");
  ASSERT_NE(description, nullptr);
  ASSERT_TRUE(description->IsString());
  EXPECT_FALSE(description->GetString().empty());
  const auto *category = tool->Find("category");
  ASSERT_NE(category, nullptr);
  ASSERT_TRUE(category->IsString());
  EXPECT_FALSE(category->GetString().empty());
  const auto *tags = tool->Find("tags");
  ASSERT_NE(tags, nullptr);
  ASSERT_TRUE(tags->IsArray());
  EXPECT_FALSE(tags->GetArray().empty());
  const auto *schema = tool->Find("input_schema");
  ASSERT_NE(schema, nullptr);
  ASSERT_TRUE(schema->IsObject());
  const auto *schema_type = schema->Find("type");
  ASSERT_NE(schema_type, nullptr) << result.Dump();
  ASSERT_TRUE(schema_type->IsString());
  EXPECT_EQ(schema_type->GetString(), "object");
}

TEST_F(MetaToolsSuite, GetToolDetailMissingToolReportsNotFound) {
  mcp::JsonValue args = make_args();
  args["name"] = mcp::JsonValue("no_such_tool_xyz");
  mcp::JsonValue result = call("get_tool_detail", args);
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString());
  EXPECT_EQ(v->GetString(), "tool not found: no_such_tool_xyz");
}

TEST_F(MetaToolsSuite, GetToolDetailMissingNameReportsMissingParameter) {
  mcp::JsonValue result = call("get_tool_detail", make_args());
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString());
  EXPECT_EQ(v->GetString(), "missing required parameter: name");
}

TEST_F(MetaToolsSuite, CallToolUnknownToolReportsDomainToolNotFound) {
  mcp::JsonValue args = make_args();
  args["name"] = mcp::JsonValue("no_such_tool_xyz");
  args["arguments"] = make_args();
  mcp::JsonValue result = call("call_tool", args);
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString());
  EXPECT_EQ(v->GetString(),
            "domain tool 'no_such_tool_xyz' not found (received no "
            "parameters) — use search_tools to discover available tools");
}

TEST_F(MetaToolsSuite, CallToolMissingNameReportsMissingParameter) {
  mcp::JsonValue result = call("call_tool", make_args());
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString());
  EXPECT_EQ(v->GetString(),
            "missing required parameter: name (received no parameters)");
}

TEST_F(MetaToolsSuite, BatchExecuteAllSuccessCountsTwo) {
  mcp::JsonValue args = make_args();
  args["confirm"] = mcp::JsonValue(true);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  mcp::JsonValue op_ping = make_args();
  op_ping["tool"] = mcp::JsonValue("ping");
  op_ping["args"] = make_args();
  operations.PushBack(std::move(op_ping));
  mcp::JsonValue op_categories = make_args();
  op_categories["tool"] = mcp::JsonValue("list_categories");
  op_categories["args"] = make_args();
  operations.PushBack(std::move(op_categories));
  args["operations"] = std::move(operations);
  mcp::JsonValue result = call("batch_execute", args);
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr) << result.Dump();
  ASSERT_TRUE(results->IsArray());
  EXPECT_FALSE(results->GetArray().empty());
  const auto *succeeded = result.Find("succeeded");
  ASSERT_NE(succeeded, nullptr) << result.Dump();
  ASSERT_TRUE(succeeded->IsInt());
  EXPECT_EQ(succeeded->GetInt(), 2);
  const auto *failed = result.Find("failed");
  ASSERT_NE(failed, nullptr) << result.Dump();
  ASSERT_TRUE(failed->IsInt());
  EXPECT_EQ(failed->GetInt(), 0);
}

TEST_F(MetaToolsSuite, BatchExecuteUnknownToolFailsOne) {
  mcp::JsonValue args = make_args();
  args["confirm"] = mcp::JsonValue(true);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  mcp::JsonValue op_unknown = make_args();
  op_unknown["tool"] = mcp::JsonValue("no_such_tool_xyz");
  op_unknown["args"] = make_args();
  operations.PushBack(std::move(op_unknown));
  args["operations"] = std::move(operations);
  mcp::JsonValue result = call("batch_execute", args);
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr) << result.Dump();
  ASSERT_TRUE(results->IsArray());
  EXPECT_FALSE(results->GetArray().empty());
  const auto *succeeded = result.Find("succeeded");
  ASSERT_NE(succeeded, nullptr) << result.Dump();
  ASSERT_TRUE(succeeded->IsInt());
  EXPECT_EQ(succeeded->GetInt(), 0);
  const auto *failed = result.Find("failed");
  ASSERT_NE(failed, nullptr) << result.Dump();
  ASSERT_TRUE(failed->IsInt());
  EXPECT_EQ(failed->GetInt(), 1);
}



} // namespace
