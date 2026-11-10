#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <stdexcept>
#include <string>

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

TEST(VariantJsonStrictTest, Rect2MissingSizeMessageSurvivesHeapChurn) {
  try {
    strict_parse(R"({"position":{"x":1,"y":2}})", "Rect2");
    FAIL() << "expected std::runtime_error";
  } catch (const std::runtime_error &e) {
    for (int i = 0; i < 32; ++i) {
      std::string scratch(96, static_cast<char>('a' + (i % 26)));
      scratch.assign(scratch.size(), 'z');
    }
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

TEST(VariantJsonStrictTest, Vector2RejectsArray) {
  EXPECT_THROW(strict_parse("[1,2]", "Vector2"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector2RejectsMissingY) {
  EXPECT_THROW(strict_parse(R"({"x":1})", "Vector2"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector2RejectsNonNumericX) {
  EXPECT_THROW(strict_parse(R"({"x":"a","y":2})", "Vector2"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector2iRejectsArray) {
  EXPECT_THROW(strict_parse("[1,2]", "Vector2i"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector2ArrayMessage) {
  try {
    strict_parse("[1,2]", "Vector2");
    FAIL() << "expected std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ(e.what(),
                 "invalid Vector2: expected a JSON object, e.g. "
                 "{\"x\":0,\"y\":0}");
  }
}

TEST(VariantJsonStrictTest, PlaneRejectsMissingDistance) {
  EXPECT_THROW(strict_parse(R"({"normal":{"x":0,"y":1,"z":0}})", "Plane"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneRejectsMissingZComponent) {
  EXPECT_THROW(strict_parse(R"({"x":0,"y":1,"d":0})", "Plane"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneRejectsMixedShapes) {
  EXPECT_THROW(
      strict_parse(R"({"normal":{"x":0,"y":1,"z":0},"x":0,"d":0})", "Plane"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneRejectsNeitherShape) {
  EXPECT_THROW(strict_parse(R"({"d":0})", "Plane"), std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneRejectsNonNumericNormalComponent) {
  EXPECT_THROW(
      strict_parse(R"({"normal":{"x":0,"y":"a","z":0},"d":0})", "Plane"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneRejectsZeroNormalWithDistance) {
  EXPECT_THROW(
      strict_parse(R"({"normal":{"x":0,"y":0,"z":0},"d":-15})", "Plane"),
      std::runtime_error);
  EXPECT_THROW(strict_parse(R"({"x":0,"y":0,"z":0,"d":-15})", "Plane"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, PlaneZeroNormalMessage) {
  try {
    strict_parse(R"({"x":0,"y":0,"z":0,"d":-15})", "Plane");
    FAIL() << "expected std::runtime_error";
  } catch (const std::runtime_error &e) {
    const std::string message = e.what();
    EXPECT_NE(message.find("normal must not be zero"), std::string::npos)
        << message;
    EXPECT_NE(message.find("WorldBoundaryShape3D"), std::string::npos)
        << message;
  }
}

TEST(VariantJsonStrictTest, Vector3RejectsMissingZ) {
  EXPECT_THROW(strict_parse(R"({"x":1,"y":2})", "Vector3"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector3iRejectsMissingZ) {
  EXPECT_THROW(strict_parse(R"({"x":1,"y":2})", "Vector3i"), std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector4RejectsMissingW) {
  EXPECT_THROW(strict_parse(R"({"x":1,"y":2,"z":3})", "Vector4"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, Vector4iRejectsMissingW) {
  EXPECT_THROW(strict_parse(R"({"x":1,"y":2,"z":3})", "Vector4i"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, QuaternionRejectsMissingW) {
  EXPECT_THROW(strict_parse(R"({"x":0,"y":0,"z":0})", "Quaternion"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, BasisRejectsMissingRows) {
  EXPECT_THROW(strict_parse(R"({"origin":{"x":0,"y":0,"z":0}})", "Basis"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, BasisRejectsTwoRows) {
  EXPECT_THROW(strict_parse(R"({"rows":[[1,0,0],[0,1,0]]})", "Basis"),
               std::runtime_error);
}

TEST(VariantJsonStrictTest, ProjectionRejectsThreeColumns) {
  EXPECT_THROW(
      strict_parse(R"({"columns":[[1,0,0,0],[0,1,0,0],[0,0,1,0]]})",
                   "Projection"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, ProjectionRejectsShortColumn) {
  EXPECT_THROW(
      strict_parse(R"({"columns":[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0]]})",
                   "Projection"),
      std::runtime_error);
}

TEST(VariantJsonStrictTest, ColorRejectsMissingG) {
  EXPECT_THROW(strict_parse(R"({"r":1,"b":1})", "Color"), std::runtime_error);
}

TEST(VariantJsonStrictTest, ColorRejectsNonNumericAlpha) {
  EXPECT_THROW(strict_parse(R"({"r":1,"g":1,"b":1,"a":"x"})", "Color"),
               std::runtime_error);
}

