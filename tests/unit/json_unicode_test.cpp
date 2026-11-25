#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "util/json_value.hpp"

using godot_autopilot::util::JsonErrorCode;
using godot_autopilot::util::JsonError;
using godot_autopilot::util::JsonValue;

namespace {

void ExpectParseError(std::string_view json) {
  try {
    JsonValue::Parse(json);
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), JsonErrorCode::parse_error);
  }
}

} // namespace

TEST(JsonUnicodeTest, ParsesSurrogatePairAndRawMultibyte) {
  JsonValue emoji = JsonValue::Parse(R"("\uD83D\uDE00")");
  EXPECT_EQ(emoji.GetString(), "\xF0\x9F\x98\x80");
  JsonValue han = JsonValue::Parse(std::string("\"\xE4\xB8\x96\""));
  EXPECT_EQ(han.GetString(), "\xE4\xB8\x96");
}

TEST(JsonUnicodeTest, ParsesCommonEscapeSequences) {
  JsonValue value = JsonValue::Parse(R"("test \" \\ \/ \b \f \n \r \t")");
  EXPECT_EQ(value.GetString(), "test \" \\ / \b \f \n \r \t");
}

TEST(JsonUnicodeTest, RejectsInvalidRawUtf8) {
  ExpectParseError(std::string("\"\xFF\""));
  ExpectParseError(std::string("\"\xC0\x80\""));
  ExpectParseError(std::string("\"\xE2\x82\""));
  ExpectParseError(std::string("\"\xED\xA0\x80\""));
}

TEST(JsonUnicodeTest, RejectsInvalidEscapeAndSurrogate) {
  ExpectParseError(std::string("\"a\\x\""));
  ExpectParseError(std::string("\"\\uD800\""));
  ExpectParseError(std::string("\"\\uDC00\""));
  ExpectParseError(std::string("\"\\u12\""));
  ExpectParseError(std::string("\"\\uZZZZ\""));
  ExpectParseError(std::string("\"a\nb\""));
}

TEST(JsonUnicodeTest, SanitizeUtf8KeepsValidText) {
  EXPECT_EQ(JsonValue::SanitizeUtf8(""), "");
  EXPECT_EQ(JsonValue::SanitizeUtf8("hello"), "hello");
  std::string text = "hello \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x98\x80";
  EXPECT_EQ(JsonValue::SanitizeUtf8(text), text);
}

TEST(JsonUnicodeTest, SanitizeUtf8ReplacesInvalidSequences) {
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xFF"), "\xEF\xBF\xBD");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\x80"), "\xEF\xBF\xBD");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xC0"), "\xEF\xBF\xBD");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xC0\x80"), "\xEF\xBF\xBD\xEF\xBF\xBD");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xE2\x82"), "\xEF\xBF\xBD\xEF\xBF\xBD");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xE2\x82\xAC"), "\xE2\x82\xAC");
  EXPECT_EQ(JsonValue::SanitizeUtf8("\xED\xA0\x80"),
            "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
}

TEST(JsonUnicodeTest, SanitizedContextParsesWithReplacementCharacters) {
  std::string raw = "{\"arguments\":{\"q\":\"\xCA\xC0\"}}";
  JsonValue parsed = JsonValue::Parse(JsonValue::SanitizeUtf8(raw));
  EXPECT_EQ(parsed.At("arguments").At("q").GetString(), "\xEF\xBF\xBD\xEF\xBF\xBD");
}
