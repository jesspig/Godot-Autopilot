#include "util/error_util.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_self_driving::util::error_detail;
using godot_self_driving::util::error_json;

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
