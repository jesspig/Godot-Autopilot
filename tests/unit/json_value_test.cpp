#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "util/json_value.hpp"

using godot_autopilot::util::JsonErrorCode;
using godot_autopilot::util::JsonError;
using godot_autopilot::util::JsonValue;

namespace {

void ExpectFailure(const JsonValue& value, JsonErrorCode expected) {
  try {
    if (value.IsNull() || value.IsString()) {
      value.GetBool();
    } else if (value.IsBool()) {
      value.GetInt();
    } else if (value.IsInt()) {
      value.GetDouble();
    } else if (value.IsDouble()) {
      value.GetString();
    } else if (value.IsArray()) {
      value.GetObject();
    } else {
      value.GetArray();
    }
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), expected);
  }
}

} // namespace

TEST(JsonValueTest, DefaultConstructedIsNull) {
  JsonValue value;
  EXPECT_TRUE(value.IsNull());
  EXPECT_FALSE(value.IsBool());
  EXPECT_FALSE(value.IsNumber());
  EXPECT_TRUE(value.Empty());
  EXPECT_EQ(value.Dump(), "null");
}

TEST(JsonValueTest, ScalarConstructorsAndPredicates) {
  JsonValue null_value(nullptr);
  JsonValue bool_value(true);
  JsonValue int_value(int64_t{-17});
  JsonValue double_value(3.25);
  JsonValue string_value(std::string("hello"));
  EXPECT_TRUE(null_value.IsNull());
  EXPECT_TRUE(bool_value.IsBool());
  EXPECT_TRUE(bool_value.GetBool());
  EXPECT_TRUE(int_value.IsInt());
  EXPECT_TRUE(int_value.IsNumber());
  EXPECT_EQ(int_value.GetInt(), int64_t{-17});
  EXPECT_TRUE(double_value.IsDouble());
  EXPECT_TRUE(double_value.IsNumber());
  EXPECT_DOUBLE_EQ(double_value.GetDouble(), 3.25);
  EXPECT_TRUE(string_value.IsString());
  EXPECT_EQ(string_value.GetString(), "hello");
}

TEST(JsonValueTest, NarrowIntAndCharPointerConstructors) {
  JsonValue narrow(42);
  EXPECT_TRUE(narrow.IsInt());
  EXPECT_EQ(narrow.GetInt(), int64_t{42});
  JsonValue literal("text");
  EXPECT_EQ(literal.GetString(), "text");
  JsonValue sized("abcd", 2);
  EXPECT_EQ(sized.GetString(), "ab");
}

TEST(JsonValueTest, ArrayAndObjectConstructors) {
  JsonValue array(JsonValue::Array{JsonValue(1), JsonValue("x")});
  EXPECT_TRUE(array.IsArray());
  EXPECT_EQ(array.Size(), size_t{2});
  EXPECT_EQ(array[0].GetInt(), int64_t{1});
  JsonValue object(JsonValue::Object{{"b", JsonValue(2)}, {"a", JsonValue(1)}});
  EXPECT_TRUE(object.IsObject());
  EXPECT_EQ(object.Size(), size_t{2});
  EXPECT_EQ(object.At("a").GetInt(), int64_t{1});
  EXPECT_EQ(object.Dump(), "{\"a\":1,\"b\":2}");
}

TEST(JsonValueTest, TagConstructorsCreateEmptyContainers) {
  JsonValue object(JsonValue::object_tag);
  JsonValue array(JsonValue::array_tag);
  EXPECT_TRUE(object.IsObject());
  EXPECT_TRUE(object.Empty());
  EXPECT_TRUE(array.IsArray());
  EXPECT_TRUE(array.Empty());
}

TEST(JsonValueTest, GetterTypeMismatchThrowsDeserializationFailed) {
  ExpectFailure(JsonValue(nullptr), JsonErrorCode::deserialization_failed);
  ExpectFailure(JsonValue(true), JsonErrorCode::deserialization_failed);
  ExpectFailure(JsonValue(int64_t{1}), JsonErrorCode::deserialization_failed);
  ExpectFailure(JsonValue(std::string("x")), JsonErrorCode::deserialization_failed);
  EXPECT_THROW(JsonValue(int64_t{1}).GetBool(), JsonError);
  EXPECT_THROW(JsonValue(true).GetInt(), JsonError);
  EXPECT_THROW(JsonValue(int64_t{1}).GetDouble(), JsonError);
  EXPECT_THROW(JsonValue(std::string("x")).GetArray(), JsonError);
  EXPECT_THROW(JsonValue(std::string("x")).GetObject(), JsonError);
}

TEST(JsonValueTest, SizeAndEmptySemantics) {
  JsonValue empty_array(JsonValue::array_tag);
  JsonValue empty_object(JsonValue::object_tag);
  JsonValue empty_string("");
  EXPECT_EQ(empty_array.Size(), size_t{0});
  EXPECT_TRUE(empty_array.Empty());
  EXPECT_EQ(empty_object.Size(), size_t{0});
  EXPECT_TRUE(empty_object.Empty());
  EXPECT_EQ(empty_string.Size(), size_t{0});
  EXPECT_FALSE(empty_string.Empty());
  EXPECT_TRUE(JsonValue(nullptr).Empty());
  EXPECT_FALSE(JsonValue(0).Empty());
  EXPECT_EQ(JsonValue(std::string("abc")).Size(), size_t{3});
  EXPECT_EQ(JsonValue(42).Size(), size_t{0});
}

TEST(JsonValueTest, FindAtAndContains) {
  JsonValue object = JsonValue::Parse(R"({"a":1,"b":{"c":2}})");
  const JsonValue* found = object.Find("a");
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->GetInt(), int64_t{1});
  EXPECT_EQ(object.Find("missing"), nullptr);
  EXPECT_EQ(object.Find("b")->Find("c")->GetInt(), int64_t{2});
  EXPECT_EQ(object.At("b").At("c").GetInt(), int64_t{2});
  EXPECT_TRUE(object.Contains("a"));
  EXPECT_FALSE(object.Contains("missing"));
  EXPECT_EQ(object[std::string_view("a")].GetInt(), int64_t{1});
  EXPECT_EQ(JsonValue(JsonValue::array_tag).Find("a"), nullptr);
  EXPECT_FALSE(JsonValue(JsonValue::array_tag).Contains("a"));
}

TEST(JsonValueTest, MutableSubscriptInsertsNullForMissingKey) {
  JsonValue object(JsonValue::object_tag);
  object["new_key"] = JsonValue(7);
  EXPECT_TRUE(object.Contains("new_key"));
  EXPECT_EQ(object.At("new_key").GetInt(), int64_t{7});
  object["another"];
  EXPECT_TRUE(object.Find("another")->IsNull());
}

TEST(JsonValueTest, MissingKeyAccessThrowsDeserializationFailed) {
  JsonValue object(JsonValue::object_tag);
  EXPECT_THROW(object.At("missing"), JsonError);
  const JsonValue& const_object = object;
  EXPECT_THROW(const_object[std::string_view("missing")], JsonError);
  try {
    object.At("missing");
    EXPECT_TRUE(false);
  } catch (const JsonError& error) {
    EXPECT_EQ(error.Code(), JsonErrorCode::deserialization_failed);
    EXPECT_NE(std::string(error.what()).find("JsonValue::At: key not found: 'missing'"),
              std::string::npos);
  }
}

TEST(JsonValueTest, PushBackAndArraySubscript) {
  JsonValue array(JsonValue::array_tag);
  array.PushBack(JsonValue(1));
  array.PushBack(JsonValue("two"));
  EXPECT_EQ(array.Size(), size_t{2});
  EXPECT_EQ(array[0].GetInt(), int64_t{1});
  const JsonValue& const_array = array;
  EXPECT_EQ(const_array[1].GetString(), "two");
  array[1] = JsonValue(3);
  EXPECT_EQ(array[1].GetInt(), int64_t{3});
  EXPECT_THROW(array.At("unused"), JsonError);
}

TEST(JsonValueTest, EqualityUsesVariantValueComparison) {
  EXPECT_TRUE(JsonValue::Parse("1") == JsonValue::Parse("1"));
  EXPECT_TRUE(JsonValue::Parse("1") != JsonValue::Parse("1.0"));
  EXPECT_TRUE(JsonValue::Parse(R"({"a":1})") == JsonValue::Parse(R"({"a":1})"));
  JsonValue copy = JsonValue::Parse(R"({"a":[1,2]})");
  JsonValue assigned;
  assigned = copy;
  EXPECT_TRUE(copy == assigned);
  JsonValue moved(std::move(copy));
  EXPECT_TRUE(moved == assigned);
}

TEST(JsonValueTest, ObjectIterationIsKeyOrdered) {
  JsonValue object = JsonValue::Parse(R"({"c":3,"a":1,"b":2})");
  std::string keys;
  for (const auto& entry : object) {
    keys += entry.first;
  }
  EXPECT_EQ(keys, "abc");
}
