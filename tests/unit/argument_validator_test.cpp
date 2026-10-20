#include "tools/argument_validator.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::ParamSpec;
using godot_autopilot::argument_validator::ValidationOutcome;
using godot_autopilot::argument_validator::validate_params;

namespace {

std::vector<ParamSpec> full_params() {
  return {
      {"path", "string", "resource path", true},
      {"count", "integer", "", false},
      {"scale", "number", "", false},
      {"flag", "boolean", "", false},
      {"payload", "object", "", false},
      {"items", "array", "", false},
  };
}

std::string error_text(const ValidationOutcome &outcome) {
  const mcp::JsonValue *error = outcome.error.Find("error");
  return error != nullptr && error->IsString() ? error->GetString()
                                               : std::string();
}

std::string error_code(const ValidationOutcome &outcome) {
  const mcp::JsonValue *code = outcome.error.Find("code");
  return code != nullptr && code->IsString() ? code->GetString()
                                             : std::string();
}

} // namespace

TEST(ArgumentValidatorTest, MissingRequiredParameterIsRejected) {
  const std::vector<ParamSpec> params = full_params();

  const ValidationOutcome outcome =
      validate_params(params, mcp::JsonValue::Parse("{}"));

  EXPECT_FALSE(outcome.passed());
  EXPECT_EQ(error_text(outcome), "missing required parameter: path");
  EXPECT_EQ(error_code(outcome), "invalid_argument");
  const mcp::JsonValue *parameter = outcome.error.Find("parameter");
  ASSERT_NE(parameter, nullptr);
  ASSERT_TRUE(parameter->IsString());
  EXPECT_EQ(parameter->GetString(), "path");
}

TEST(ArgumentValidatorTest, TypeMismatchIsRejected) {
  const std::vector<ParamSpec> params = full_params();

  const ValidationOutcome wrong_string =
      validate_params(params, mcp::JsonValue::Parse(R"({"path": 3})"));
  EXPECT_FALSE(wrong_string.passed());
  EXPECT_EQ(error_text(wrong_string),
            "invalid parameter: path must be a string");
  EXPECT_EQ(error_code(wrong_string), "invalid_argument");

  const ValidationOutcome wrong_boolean = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "flag": 1})"));
  EXPECT_FALSE(wrong_boolean.passed());
  EXPECT_EQ(error_text(wrong_boolean),
            "invalid parameter: flag must be a boolean");

  const ValidationOutcome wrong_object = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "payload": 3})"));
  EXPECT_FALSE(wrong_object.passed());
  EXPECT_EQ(error_text(wrong_object),
            "invalid parameter: payload must be a object");

  const ValidationOutcome wrong_array = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "items": "y"})"));
  EXPECT_FALSE(wrong_array.passed());
  EXPECT_EQ(error_text(wrong_array),
            "invalid parameter: items must be a array");
}

TEST(ArgumentValidatorTest, IntegerOutOfRangeIsRejected) {
  const std::vector<ParamSpec> params = full_params();

  const ValidationOutcome fractional = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "count": 3.5})"));
  EXPECT_FALSE(fractional.passed());
  EXPECT_EQ(error_text(fractional),
            "invalid parameter: count must be a integer");
  EXPECT_EQ(error_code(fractional), "invalid_argument");

  const ValidationOutcome overflow = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "count": 1e300})"));
  EXPECT_FALSE(overflow.passed());
  EXPECT_EQ(error_text(overflow),
            "invalid parameter: count must be a integer");
}

TEST(ArgumentValidatorTest, UnknownKeysAreToleratedAndReported) {
  const std::vector<ParamSpec> params = full_params();

  const ValidationOutcome outcome = validate_params(
      params, mcp::JsonValue::Parse(R"({"path": "x", "extra": 1, "other": 2})"));

  EXPECT_TRUE(outcome.passed());
  EXPECT_TRUE(outcome.error.IsNull());
  ASSERT_EQ(outcome.unknown_keys.size(), 2u);
  EXPECT_EQ(outcome.unknown_keys[0], "extra");
  EXPECT_EQ(outcome.unknown_keys[1], "other");
}

TEST(ArgumentValidatorTest, ValidArgumentsPassWithoutWarnings) {
  const std::vector<ParamSpec> params = full_params();

  const ValidationOutcome outcome = validate_params(
      params,
      mcp::JsonValue::Parse(R"({"path": "res://scene.tscn", "count": 3, "scale": 1.5, "flag": true, "payload": {"a": 1}, "items": [1, 2]})"));

  EXPECT_TRUE(outcome.passed());
  EXPECT_TRUE(outcome.error.IsNull());
  EXPECT_TRUE(outcome.unknown_keys.empty());
}
