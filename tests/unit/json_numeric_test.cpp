#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include "util/json_value.hpp"

using godot_autopilot::util::JsonErrorCode;
using godot_autopilot::util::JsonError;
using godot_autopilot::util::JsonValue;

namespace {

void ExpectParseError(std::string_view json, JsonErrorCode expected) {
  try {
    JsonValue::Parse(json);
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), expected);
  }
}

} // namespace

TEST(JsonNumericTest, IntegerBoundaries) {
  JsonValue min_value = JsonValue::Parse("-9223372036854775808");
  JsonValue max_value = JsonValue::Parse("9223372036854775807");
  EXPECT_TRUE(min_value.IsInt());
  EXPECT_EQ(min_value.GetInt(), std::numeric_limits<int64_t>::min());
  EXPECT_TRUE(max_value.IsInt());
  EXPECT_EQ(max_value.GetInt(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(JsonValue::Parse("-0").GetInt(), int64_t{0});
  EXPECT_EQ(JsonValue::Parse("0").GetInt(), int64_t{0});
  EXPECT_EQ(JsonValue::Parse("42").GetInt(), int64_t{42});
}

TEST(JsonNumericTest, Uint64RangeRejectedAsDeserializationFailed) {
  ExpectParseError("18446744073709551615", JsonErrorCode::deserialization_failed);
}

TEST(JsonNumericTest, BeyondUint64AndBelowInt64MinRejectedAsParseError) {
  ExpectParseError("18446744073709551616", JsonErrorCode::parse_error);
  ExpectParseError("-9223372036854775809", JsonErrorCode::parse_error);
}

TEST(JsonNumericTest, DoubleFormsAndTypeDiscrimination) {
  EXPECT_TRUE(JsonValue::Parse("0.0").IsDouble());
  EXPECT_TRUE(JsonValue::Parse("0.1").IsDouble());
  EXPECT_FALSE(JsonValue::Parse("0.1").IsInt());
  EXPECT_DOUBLE_EQ(JsonValue::Parse("-1.5e-3").GetDouble(), -0.0015);
  EXPECT_DOUBLE_EQ(JsonValue::Parse("1E+2").GetDouble(), 100.0);
  EXPECT_DOUBLE_EQ(JsonValue::Parse("2.5e3").GetDouble(), 2500.0);
  EXPECT_TRUE(JsonValue::Parse("42.0").IsDouble());
  EXPECT_FALSE(JsonValue::Parse("42").IsDouble());
}

TEST(JsonNumericTest, DoubleOverflowRejectedUnderflowAccepted) {
  ExpectParseError("1e400", JsonErrorCode::parse_error);
  JsonValue underflow = JsonValue::Parse("1e-400");
  EXPECT_TRUE(underflow.IsDouble());
  EXPECT_DOUBLE_EQ(underflow.GetDouble(), 0.0);
  EXPECT_TRUE(JsonValue::Parse("5e-324").IsDouble());
  EXPECT_TRUE(JsonValue::Parse("1.7976931348623157e308").IsDouble());
}

TEST(JsonNumericTest, InvalidNumberGrammarRejected) {
  ExpectParseError("1.", JsonErrorCode::parse_error);
  ExpectParseError(".5", JsonErrorCode::parse_error);
  ExpectParseError("1e", JsonErrorCode::parse_error);
  ExpectParseError("1e+", JsonErrorCode::parse_error);
  ExpectParseError("--1", JsonErrorCode::parse_error);
  ExpectParseError("1.2.3", JsonErrorCode::parse_error);
  ExpectParseError("1,", JsonErrorCode::parse_error);
}

TEST(JsonNumericTest, DumpUsesShortestRoundTripAndIntegerDoubleDotZero) {
  EXPECT_EQ(JsonValue::Parse("1.0").Dump(), "1.0");
  EXPECT_EQ(JsonValue::Parse("1e2").Dump(), "100.0");
  EXPECT_EQ(JsonValue::Parse("0.1").Dump(), "0.1");
  EXPECT_EQ(JsonValue::Parse("-17").Dump(), "-17");
  EXPECT_EQ(JsonValue::Parse("3.14").Dump(), "3.14");
}
