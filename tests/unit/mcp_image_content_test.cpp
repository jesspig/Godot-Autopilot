#include "util/mcp_image_content.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

using godot_autopilot::util::is_image_capture_tool;
using godot_autopilot::util::try_attach_image_content;

namespace {

const mcp::ImageContent *image_at(const std::vector<mcp::ContentVariant> &content,
                                  size_t index) {
    if (index >= content.size())
        return nullptr;
    return std::get_if<mcp::ImageContent>(&content[index]);
}

} // namespace

TEST(McpImageContentTest, WhitelistCoversThreeCaptureTools) {
    EXPECT_TRUE(is_image_capture_tool("capture_editor_viewport"));
    EXPECT_TRUE(is_image_capture_tool("capture_game_viewport"));
    EXPECT_TRUE(is_image_capture_tool("capture_display_screen"));
    EXPECT_FALSE(is_image_capture_tool("create_scene_node"));
    EXPECT_FALSE(is_image_capture_tool(""));
}

TEST(McpImageContentTest, AttachesTopLevelData) {
    mcp::JsonValue result = mcp::JsonValue::Parse(
        R"({"data":"QUJD","format":"png","width":640,"height":480,"path":"user://shot.png"})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(try_attach_image_content("capture_game_viewport", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->type, "image");
    EXPECT_EQ(image->data, "QUJD");
    EXPECT_EQ(image->mime_type, "image/png");
    EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
    ASSERT_TRUE(result["image_attached"].IsBool());
    EXPECT_TRUE(result["image_attached"].GetBool());
    EXPECT_EQ(result["format"].GetString(), "png");
    EXPECT_EQ(result["width"].GetInt(), 640);
    EXPECT_EQ(result["height"].GetInt(), 480);
    EXPECT_EQ(result["path"].GetString(), "user://shot.png");
}

TEST(McpImageContentTest, AttachesNestedEditorResultData) {
    mcp::JsonValue result = mcp::JsonValue::Parse(
        R"({"result":{"data":"QUJD","format":"png","width":100,"height":50}})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(
        try_attach_image_content("capture_editor_viewport", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->data, "QUJD");
    EXPECT_EQ(image->mime_type, "image/png");
    mcp::JsonValue &inner = result["result"];
    EXPECT_EQ(inner["data"].GetString(), "<attached-as-image-content>");
    EXPECT_TRUE(inner["image_attached"].GetBool());
    EXPECT_EQ(inner["width"].GetInt(), 100);
    EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(McpImageContentTest, AttachesNestedDisplayResultData) {
    mcp::JsonValue result = mcp::JsonValue::Parse(
        R"({"result":{"mime":"image/png","format":"png","data":"QUJD"}})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(
        try_attach_image_content("capture_display_screen", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->data, "QUJD");
    mcp::JsonValue &inner = result["result"];
    EXPECT_EQ(inner["data"].GetString(), "<attached-as-image-content>");
    EXPECT_EQ(inner["mime"].GetString(), "image/png");
    EXPECT_TRUE(inner["image_attached"].GetBool());
}

TEST(McpImageContentTest, PrefersTopLevelDataOverNested) {
    mcp::JsonValue result = mcp::JsonValue::Parse(
        R"({"data":"TOPDATA","result":{"data":"NESTED","format":"png"}})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(try_attach_image_content("capture_editor_viewport", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->data, "TOPDATA");
    EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
    EXPECT_TRUE(result["image_attached"].GetBool());
    EXPECT_EQ(result["result"]["data"].GetString(), "NESTED");
    EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(McpImageContentTest, FallsBackToNestedDataWhenTopLevelEmpty) {
    mcp::JsonValue result = mcp::JsonValue::Parse(
        R"({"data":"","result":{"data":"QUJD","format":"png"}})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(
        try_attach_image_content("capture_editor_viewport", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->data, "QUJD");
    EXPECT_EQ(result["data"].GetString(), "");
    EXPECT_FALSE(result.Contains("image_attached"));
    EXPECT_EQ(result["result"]["data"].GetString(),
              "<attached-as-image-content>");
    EXPECT_TRUE(result["result"]["image_attached"].GetBool());
}

TEST(McpImageContentTest, IgnoresNonWhitelistedTool) {
    mcp::JsonValue result =
        mcp::JsonValue::Parse(R"({"data":"QUJD","format":"png"})");
    std::vector<mcp::ContentVariant> content;
    EXPECT_FALSE(
        try_attach_image_content("create_scene_node", result, content));
    EXPECT_TRUE(content.empty());
    EXPECT_EQ(result["data"].GetString(), "QUJD");
    EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(McpImageContentTest, IgnoresNonObjectJson) {
    mcp::JsonValue result = mcp::JsonValue::Parse(R"(["QUJD"])");
    std::vector<mcp::ContentVariant> content;
    EXPECT_FALSE(
        try_attach_image_content("capture_game_viewport", result, content));
    EXPECT_TRUE(content.empty());
    ASSERT_TRUE(result.IsArray());
    EXPECT_EQ(result[0].GetString(), "QUJD");
}

TEST(McpImageContentTest, IgnoresMissingData) {
    mcp::JsonValue result =
        mcp::JsonValue::Parse(R"({"result":{"width":100,"height":50}})");
    std::vector<mcp::ContentVariant> content;
    EXPECT_FALSE(
        try_attach_image_content("capture_editor_viewport", result, content));
    EXPECT_TRUE(content.empty());
    EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(McpImageContentTest, RejectsNonPngFormat) {
    mcp::JsonValue result =
        mcp::JsonValue::Parse(R"({"data":"QUJD","format":"jpg"})");
    std::vector<mcp::ContentVariant> content;
    EXPECT_FALSE(
        try_attach_image_content("capture_game_viewport", result, content));
    EXPECT_TRUE(content.empty());
    EXPECT_EQ(result["data"].GetString(), "QUJD");
    EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(McpImageContentTest, TreatsMissingFormatAsPng) {
    mcp::JsonValue result = mcp::JsonValue::Parse(R"({"data":"QUJD"})");
    std::vector<mcp::ContentVariant> content;
    ASSERT_TRUE(try_attach_image_content("capture_game_viewport", result, content));
    ASSERT_EQ(content.size(), 1u);
    const mcp::ImageContent *image = image_at(content, 0);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->mime_type, "image/png");
    EXPECT_EQ(image->data, "QUJD");
}
