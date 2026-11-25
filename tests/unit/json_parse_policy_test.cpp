#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "util/json_value.hpp"

using godot_autopilot::util::DuplicateKeyPolicy;
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

TEST(JsonParsePolicyTest, WhitespacePaddingAndInterTokenWhitespace) {
  JsonValue padded = JsonValue::Parse(" \t\r\n42 \t\r\n");
  EXPECT_TRUE(padded.IsInt());
  EXPECT_EQ(padded.GetInt(), int64_t{42});
  JsonValue array = JsonValue::Parse("[ 1 , 2 , 3 ]");
  EXPECT_EQ(array.Size(), size_t{3});
  JsonValue object = JsonValue::Parse("{ \"a\" : 1 }");
  EXPECT_EQ(object.At("a").GetInt(), int64_t{1});
}

TEST(JsonParsePolicyTest, RejectsTrailingCharactersAndEmptyInput) {
  ExpectParseError("", JsonErrorCode::parse_error);
  ExpectParseError("   ", JsonErrorCode::parse_error);
  ExpectParseError("1 2", JsonErrorCode::parse_error);
  ExpectParseError("{}x", JsonErrorCode::parse_error);
  ExpectParseError(std::string("1\0 ", 3), JsonErrorCode::parse_error);
}

TEST(JsonParsePolicyTest, RejectsBomCommentsAndSingleQuotes) {
  ExpectParseError(std::string("\xEF\xBB\xBF{}"), JsonErrorCode::parse_error);
  ExpectParseError("{}//comment", JsonErrorCode::parse_error);
  ExpectParseError("/*comment*/{}", JsonErrorCode::parse_error);
  ExpectParseError("'a'", JsonErrorCode::parse_error);
}

TEST(JsonParsePolicyTest, RejectsTrailingCommasAndLeadingCommas) {
  ExpectParseError("[1,]", JsonErrorCode::parse_error);
  ExpectParseError(R"({"a":1,})", JsonErrorCode::parse_error);
  ExpectParseError("[,1]", JsonErrorCode::parse_error);
  ExpectParseError("{:}", JsonErrorCode::parse_error);
}

TEST(JsonParsePolicyTest, RejectsPlusSignLeadingZeroAndNonJsonLiterals) {
  ExpectParseError("+1", JsonErrorCode::parse_error);
  ExpectParseError("01", JsonErrorCode::parse_error);
  ExpectParseError("NaN", JsonErrorCode::parse_error);
  ExpectParseError("Infinity", JsonErrorCode::parse_error);
  ExpectParseError("-Infinity", JsonErrorCode::parse_error);
  ExpectParseError("0x10", JsonErrorCode::parse_error);
  ExpectParseError("abc", JsonErrorCode::parse_error);
}

TEST(JsonParsePolicyTest, RejectsTruncatedDocuments) {
  ExpectParseError("{", JsonErrorCode::parse_error);
  ExpectParseError(R"({"a":)", JsonErrorCode::parse_error);
  ExpectParseError("[1,", JsonErrorCode::parse_error);
  ExpectParseError(R"("abc)", JsonErrorCode::parse_error);
  ExpectParseError("tru", JsonErrorCode::parse_error);
  ExpectParseError("nu", JsonErrorCode::parse_error);
  ExpectParseError("fal", JsonErrorCode::parse_error);
  ExpectParseError("]", JsonErrorCode::parse_error);
  ExpectParseError("}", JsonErrorCode::parse_error);
}

TEST(JsonParsePolicyTest, CompatDuplicateKeysKeepFirstValue) {
  JsonValue value = JsonValue::Parse(R"({"a":1,"a":2})");
  EXPECT_EQ(value.Size(), size_t{1});
  EXPECT_EQ(value.At("a").GetInt(), int64_t{1});
}

TEST(JsonParsePolicyTest, StrictDuplicateKeysRejectedWithPath) {
  try {
    JsonValue::Parse(R"({"a":{"b":1,"b":2}})", DuplicateKeyPolicy::strict);
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), JsonErrorCode::parse_error);
    EXPECT_NE(std::string(error.what()).find("$.a.b"), std::string::npos);
  }
}

TEST(JsonParsePolicyTest, StrictDuplicateKeyInArrayReportsIndexedPath) {
  try {
    JsonValue::Parse(R"({"a":[{"x":1,"x":2}]})", DuplicateKeyPolicy::strict);
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_NE(std::string(error.what()).find("$.a[0].x"), std::string::npos);
  }
}

TEST(JsonParsePolicyTest, StrictPolicyAcceptsUniqueKeys) {
  JsonValue value = JsonValue::Parse(R"({"a":1,"b":{"c":2}})", DuplicateKeyPolicy::strict);
  EXPECT_EQ(value.At("b").At("c").GetInt(), int64_t{2});
}

TEST(JsonParsePolicyTest, MissingCommaAndUnquotedKeyRejected) {
  ExpectParseError("[1 2]", JsonErrorCode::parse_error);
  ExpectParseError("{a:1}", JsonErrorCode::parse_error);
}
