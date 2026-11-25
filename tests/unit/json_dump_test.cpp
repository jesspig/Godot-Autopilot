#include <gtest/gtest.h>

#include <limits>
#include <string>

#include "util/json_value.hpp"

using godot_autopilot::util::JsonValue;

TEST(JsonDumpTest, CompactDumpOfMixedDocument) {
  JsonValue value = JsonValue::Parse(R"({"a":[1,2.5,true,null,"x"]})");
  EXPECT_EQ(value.Dump(), R"({"a":[1,2.5,true,null,"x"]})");
}

TEST(JsonDumpTest, ObjectKeysAreLexicographicallyOrdered) {
  JsonValue value = JsonValue::Parse(R"({"z":1,"a":2,"m":3})");
  EXPECT_EQ(value.Dump(), R"({"a":2,"m":3,"z":1})");
}

TEST(JsonDumpTest, IndentedDumpUsesTwoSpacesPerLevel) {
  JsonValue value = JsonValue::Parse(R"({"a":{"b":1},"c":[2,[3]]})");
  EXPECT_EQ(value.Dump(2), std::string(
      "{\n"
      "  \"a\": {\n"
      "    \"b\": 1\n"
      "  },\n"
      "  \"c\": [\n"
      "    2,\n"
      "    [\n"
      "      3\n"
      "    ]\n"
      "  ]\n"
      "}"));
}

TEST(JsonDumpTest, IndentZeroStillBreaksLines) {
  JsonValue value = JsonValue::Parse(R"({"k":{"n":"x"}})");
  EXPECT_EQ(value.Dump(0), std::string(
      "{\n"
      "\"k\": {\n"
      "\"n\": \"x\"\n"
      "}\n"
      "}"));
  EXPECT_EQ(value.Dump(4), std::string(
      "{\n"
      "    \"k\": {\n"
      "        \"n\": \"x\"\n"
      "    }\n"
      "}"));
}

TEST(JsonDumpTest, EmptyContainersAndControlEscapes) {
  EXPECT_EQ(JsonValue::Parse("[]").Dump(), "[]");
  EXPECT_EQ(JsonValue::Parse("{}").Dump(), "{}");
  EXPECT_EQ(JsonValue::Parse("\"\\u0001\"").Dump(), "\"\\u0001\"");
  EXPECT_EQ(JsonValue::Parse("\"a\\/b\"").Dump(), "\"a/b\"");
}

TEST(JsonDumpTest, NonFiniteDoublesBecomeNull) {
  JsonValue nan(std::numeric_limits<double>::quiet_NaN());
  JsonValue inf(std::numeric_limits<double>::infinity());
  JsonValue neg_inf(-std::numeric_limits<double>::infinity());
  EXPECT_EQ(nan.Dump(), "null");
  EXPECT_EQ(inf.Dump(), "null");
  EXPECT_EQ(neg_inf.Dump(), "null");
}

TEST(JsonDumpTest, RoundTripIsIdempotent) {
  JsonValue first = JsonValue::Parse(R"({"a":[1,2.5,true,null,"x"]})");
  std::string dump_first = first.Dump();
  JsonValue second = JsonValue::Parse(dump_first);
  EXPECT_EQ(second.Dump(), dump_first);
}
