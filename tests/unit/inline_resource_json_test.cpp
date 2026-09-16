#include "util/inline_resource_json.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using godot_autopilot::util::inline_resource::Description;
using godot_autopilot::util::inline_resource::Shape;
using godot_autopilot::util::inline_resource::inspect;
using godot_autopilot::util::inline_resource::kMaxDepth;
using godot_autopilot::util::inline_resource::nested_depth;

Description inspect_json(const std::string &json) {
  return inspect(mcp::JsonValue::Parse(json));
}

int depth_of(const std::string &json) {
  return nested_depth(mcp::JsonValue::Parse(json));
}

const char kChainOfTwo[] = R"({
  "type": "AtlasTexture",
  "properties": {"atlas": {"type": "AtlasTexture", "properties": {}}}
})";

const char kChainOfThree[] = R"({
  "type": "AtlasTexture",
  "properties": {"atlas": {
    "type": "AtlasTexture",
    "properties": {"atlas": {"type": "AtlasTexture", "properties": {}}}
  }}
})";

const char kChainOfFive[] = R"({
  "type": "AtlasTexture",
  "properties": {"atlas": {
    "type": "AtlasTexture",
    "properties": {"atlas": {
      "type": "AtlasTexture",
      "properties": {"atlas": {
        "type": "AtlasTexture",
        "properties": {"atlas": {
          "type": "AtlasTexture",
          "properties": {}
        }}
      }}
    }}
  }}
})";

} // namespace

TEST(InlineResourceJsonTest, DepthLimitIsFour) {
  EXPECT_EQ(kMaxDepth, 4);
}

TEST(InlineResourceJsonTest, NonObjectValuesAreNotInline) {
  const char *const values[] = {"42", "-1.5", R"("RectangleShape2D")", "null",
                                "true", "[]", R"(["type", "properties"])"};
  for (const char *value : values) {
    const Description description = inspect_json(value);
    EXPECT_EQ(description.shape, Shape::not_inline) << value;
    EXPECT_TRUE(description.type.empty()) << value;
    EXPECT_FALSE(description.has_properties) << value;
  }
}

TEST(InlineResourceJsonTest, RequiresNonEmptyStringType) {
  const char *const values[] = {
      R"({"properties": {"size": {"x": 1}}})",
      R"({"type": 7, "properties": {}})",
      R"({"type": null})",
      R"({"type": ""})",
      R"({"resource": "memory://shape"})",
      R"({"path": "res://shape.tres"})",
  };
  for (const char *value : values) {
    const Description description = inspect_json(value);
    EXPECT_NE(description.shape, Shape::inline_description) << value;
  }
}

TEST(InlineResourceJsonTest, AcceptsTypeOnlyDescription) {
  const Description description =
      inspect_json(R"({"type": "RectangleShape2D"})");
  EXPECT_EQ(description.shape, Shape::inline_description);
  EXPECT_EQ(description.type, "RectangleShape2D");
  EXPECT_FALSE(description.has_properties);
}

TEST(InlineResourceJsonTest, AcceptsExtraKeysAlongsideType) {
  const Description description =
      inspect_json(R"({"type": "RectangleShape2D", "name": "probe"})");
  EXPECT_EQ(description.shape, Shape::inline_description);
  EXPECT_EQ(description.type, "RectangleShape2D");
  EXPECT_FALSE(description.has_properties);
}

TEST(InlineResourceJsonTest, AcceptsTypeWithObjectProperties) {
  const Description description = inspect_json(
      R"({"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}})");
  EXPECT_EQ(description.shape, Shape::inline_description);
  EXPECT_EQ(description.type, "RectangleShape2D");
  EXPECT_TRUE(description.has_properties);
}

TEST(InlineResourceJsonTest, FlagsNonObjectProperties) {
  const char *const values[] = {
      R"({"type": "RectangleShape2D", "properties": 5})",
      R"({"type": "RectangleShape2D", "properties": []})",
      R"({"type": "RectangleShape2D", "properties": "size"})",
      R"({"type": "RectangleShape2D", "properties": null})",
  };
  for (const char *value : values) {
    const Description description = inspect_json(value);
    EXPECT_EQ(description.shape, Shape::invalid_properties) << value;
    EXPECT_EQ(description.type, "RectangleShape2D") << value;
    EXPECT_FALSE(description.has_properties) << value;
  }
}

TEST(InlineResourceJsonTest, NestedDepthZeroForPlainValues) {
  const char *const values[] = {"42", R"("shape")", "null", "[]",
                                R"({"x": 1, "y": 2})",
                                R"({"path": "res://shape.tres"})",
                                R"({"resource": "memory://shape"})",
                                R"({"type": 7, "properties": {}})"};
  for (const char *value : values) {
    EXPECT_EQ(depth_of(value), 0) << value;
  }
}

TEST(InlineResourceJsonTest, NestedDepthOneForSingleDescription) {
  EXPECT_EQ(depth_of(R"({"type": "RectangleShape2D"})"), 1);
  EXPECT_EQ(
      depth_of(R"({"type": "RectangleShape2D", "properties": {"size": {"x": 20, "y": 28}}})"),
      1);
  EXPECT_EQ(depth_of(R"({"wrapper": {"type": "RectangleShape2D"}})"), 1);
}

TEST(InlineResourceJsonTest, NestedDepthCountsInlineChain) {
  EXPECT_EQ(depth_of(kChainOfTwo), 2);
  EXPECT_EQ(depth_of(kChainOfThree), 3);
  EXPECT_EQ(depth_of(kChainOfFive), 5);
  EXPECT_GT(depth_of(kChainOfFive), kMaxDepth);
}

TEST(InlineResourceJsonTest, NestedDepthCountsDescriptionsInsideArrays) {
  EXPECT_EQ(
      depth_of(R"({"type": "AtlasTexture", "properties": {"list": [{"type": "AtlasTexture", "properties": {}}]}})"),
      2);
  EXPECT_EQ(
      depth_of(R"({"items": [{"type": "AtlasTexture", "properties": {}}]})"),
      1);
}

TEST(InlineResourceJsonTest, NestedDepthIgnoresNonInlineContainers) {
  EXPECT_EQ(depth_of(R"({"type": "RectangleShape2D", "properties": {}})"), 1);
  EXPECT_EQ(
      depth_of(R"({"type": "RectangleShape2D", "properties": {"size": {"x": 20}}})"),
      1);
  EXPECT_EQ(depth_of(R"({"meta": {"nested": {"deep": {"deeper": {}}}}})"), 0);
}
