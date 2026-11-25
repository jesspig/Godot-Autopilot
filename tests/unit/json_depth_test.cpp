#include <gtest/gtest.h>

#include <string>

#include "util/json_value.hpp"

using godot_autopilot::util::JsonErrorCode;
using godot_autopilot::util::JsonError;
using godot_autopilot::util::JsonValue;

namespace {

std::string MakeNestedArray(int depth) {
  return std::string(depth, '[') + std::string(depth, ']');
}

void WalkToInnermost(const JsonValue& value, int depth) {
  const JsonValue* current = &value;
  for (int i = 0; i < depth; ++i) {
    ASSERT_TRUE(current->IsArray());
    if (i < depth - 1) {
      ASSERT_EQ(current->Size(), size_t{1});
      current = &current->GetArray()[0];
    } else {
      EXPECT_EQ(current->Size(), size_t{0});
    }
  }
}

} // namespace

TEST(JsonDepthTest, ThousandLevelsParseSuccess) {
  JsonValue value = JsonValue::Parse(MakeNestedArray(1000));
  EXPECT_NO_THROW(JsonValue::Parse(MakeNestedArray(1000)));
  WalkToInnermost(value, 1000);
}

TEST(JsonDepthTest, ThousandAndOneLevelsRejected) {
  try {
    JsonValue::Parse(MakeNestedArray(1001));
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), JsonErrorCode::parse_error);
    EXPECT_NE(std::string(error.what()).find("nesting depth exceeds 1000"), std::string::npos);
  }
}

TEST(JsonDepthTest, DeepCopyPreservesStructure) {
  JsonValue parsed = JsonValue::Parse(MakeNestedArray(1000));
  JsonValue copy(parsed);
  WalkToInnermost(copy, 1000);
}

TEST(JsonDepthTest, DeepCopyAssignmentPreservesStructure) {
  JsonValue parsed = JsonValue::Parse(MakeNestedArray(1000));
  JsonValue copy;
  copy = parsed;
  WalkToInnermost(copy, 1000);
}

TEST(JsonDepthTest, DeepMovePreservesStructure) {
  JsonValue parsed = JsonValue::Parse(MakeNestedArray(1000));
  JsonValue moved(std::move(parsed));
  WalkToInnermost(moved, 1000);
}

TEST(JsonDepthTest, DeepObjectNestingAtLimit) {
  std::string json;
  for (int i = 0; i < 1000; ++i) json += "{\"a\":";
  json += "1";
  json.append(1000, '}');
  JsonValue value = JsonValue::Parse(json);
  const JsonValue* current = &value;
  for (int i = 0; i < 999; ++i) {
    ASSERT_TRUE(current->IsObject());
    current = &current->At("a");
  }
  EXPECT_EQ(current->At("a").GetInt(), int64_t{1});
}
