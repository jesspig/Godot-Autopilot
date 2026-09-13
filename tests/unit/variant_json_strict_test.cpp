#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <stdexcept>

#include "util/variant_json.hpp"

namespace {

using godot_autopilot::VariantJson;

godot::Variant strict_parse(const char *json, const char *type_hint) {
  return VariantJson::deserialize_strict(mcp::JsonValue::Parse(json), type_hint);
}

} // namespace

TEST(VariantJsonStrictTest, Rect2RejectsConflictingSizeAliases) {
  EXPECT_THROW(
      strict_parse(R"({"size":{"w":10,"h":4,"x":99}})", "Rect2"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, Rect2RejectsMissingSize) {
  EXPECT_THROW(strict_parse(R"({"position":{"x":1,"y":2}})", "Rect2"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Rect2MissingSizeMessage) {
  try {
    strict_parse(R"({"position":{"x":1,"y":2}})", "Rect2");
    FAIL() << "expected std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ(e.what(),
                 "invalid Rect2: 'size' must be an object, e.g. "
                 "{\"position\":{\"x\":0,\"y\":0},\"size\":{\"w\":64,\"h\":32}}");
  }
}

TEST(VariantJsonStrictTest, Rect2RejectsNonObject) {
  EXPECT_THROW(strict_parse("[1,2,3]", "Rect2"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Rect2RejectsNonNumericSizeComponent) {
  EXPECT_THROW(strict_parse(R"({"size":{"w":"a","h":4}})", "Rect2"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Rect2iRejectsMissingSizeComponent) {
  EXPECT_THROW(strict_parse(R"({"size":{"w":10}})", "Rect2i"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, AABBRejectsMissingDepth) {
  EXPECT_THROW(strict_parse(R"({"size":{"w":1,"h":2}})", "AABB"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Transform2DRejectsMissingColumns) {
  EXPECT_THROW(strict_parse(R"({"position":{"x":1,"y":2}})", "Transform2D"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Transform2DRejectsTwoColumns) {
  EXPECT_THROW(strict_parse(R"({"columns":[[1,0],[0,1]]})", "Transform2D"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Transform2DRejectsNonNumericColumnElement) {
  EXPECT_THROW(
      strict_parse(R"({"columns":[[1,0],[0,1],["a",6]]})", "Transform2D"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, Transform3DRejectsMissingBasis) {
  EXPECT_THROW(strict_parse(R"({"origin":{"x":1,"y":2,"z":3}})", "Transform3D"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Transform3DRejectsTwoBasisRows) {
  EXPECT_THROW(
      strict_parse(R"({"basis":{"rows":[[1,0,0],[0,1,0]]}})", "Transform3D"),
      std::runtime_error);
}
