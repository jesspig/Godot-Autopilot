#include "util/error_util.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::util::error_detail;
using godot_autopilot::util::error_json;
using godot_autopilot::util::find_result_or_data;
using godot_autopilot::util::ok_result;

TEST(ErrorUtilTest, ErrorJsonShape) {
    auto value = error_json("something failed");
    ASSERT_TRUE(value.IsObject());
    EXPECT_EQ(value.Size(), 1u);
    const mcp::JsonValue* err = value.Find("error");
    ASSERT_NE(err, nullptr);
    EXPECT_TRUE(err->IsString());
    EXPECT_EQ(err->GetString(), "something failed");
}

TEST(ErrorUtilTest, ErrorDetailHasAllFourFields) {
    auto value = error_detail("fact here", "position here", "expected here", "action here");
    ASSERT_TRUE(value.IsObject());
    const mcp::JsonValue* err = value.Find("error");
    ASSERT_NE(err, nullptr);
    EXPECT_TRUE(err->IsString());
    std::string expected = "fact here \xe2\x80\x94 position: position here"
                           " \xe2\x80\x94 expected: expected here"
                           " \xe2\x80\x94 action: action here";
    EXPECT_EQ(err->GetString(), expected);
}

TEST(ErrorUtilTest, OkResultShape) {
    auto value = ok_result(mcp::JsonValue("v"));
    ASSERT_TRUE(value.IsObject());
    EXPECT_EQ(value.Size(), 1u);
    const mcp::JsonValue* result = value.Find("result");
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->IsString());
    EXPECT_EQ(result->GetString(), "v");
}

TEST(ErrorUtilTest, FindResultOrDataPrefersResult) {
    auto both = mcp::JsonValue::Parse(R"({"result":1,"data":2})");
    const mcp::JsonValue* found = find_result_or_data(both);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetInt(), 1);
    auto legacy = mcp::JsonValue::Parse(R"({"data":2})");
    found = find_result_or_data(legacy);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->GetInt(), 2);
    auto none = mcp::JsonValue::Parse(R"({"error":"x"})");
    EXPECT_EQ(find_result_or_data(none), nullptr);
    mcp::JsonValue mutable_body =
        mcp::JsonValue::Parse(R"({"data":{"v":3}})");
    mcp::JsonValue* slot = find_result_or_data(mutable_body);
    ASSERT_NE(slot, nullptr);
    ASSERT_TRUE(slot->IsObject());
    EXPECT_NE(slot->Find("v"), nullptr);
}

