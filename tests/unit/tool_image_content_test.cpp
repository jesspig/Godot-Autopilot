#include "util/tool_image_content.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

using godot_autopilot::util::is_image_capture_tool;
using godot_autopilot::util::try_attach_image_content;

namespace {

const godot_autopilot::util::JsonValue *image_at(
    const std::vector<godot_autopilot::util::JsonValue> &content, size_t index) {
  if (index >= content.size())
    return nullptr;
  return &content[index];
}

void expect_image(const godot_autopilot::util::JsonValue *image,
                  const std::string &data) {
  ASSERT_NE(image, nullptr);
  ASSERT_TRUE(image->IsObject());
  const godot_autopilot::util::JsonValue *type = image->Find("type");
  ASSERT_NE(type, nullptr);
  EXPECT_EQ(type->GetString(), "image");
  const godot_autopilot::util::JsonValue *payload = image->Find("data");
  ASSERT_NE(payload, nullptr);
  EXPECT_EQ(payload->GetString(), data);
  const godot_autopilot::util::JsonValue *mime = image->Find("mimeType");
  ASSERT_NE(mime, nullptr);
  EXPECT_EQ(mime->GetString(), "image/png");
}

} // namespace

TEST(ToolImageContentTest, WhitelistCoversCaptureAndObserveTools) {
  EXPECT_TRUE(is_image_capture_tool("capture_editor_viewport"));
  EXPECT_TRUE(is_image_capture_tool("capture_game_viewport"));
  EXPECT_TRUE(is_image_capture_tool("capture_display_screen"));
  EXPECT_TRUE(is_image_capture_tool("click_input_mouse"));
  EXPECT_TRUE(is_image_capture_tool("scroll_input_mouse"));
  EXPECT_TRUE(is_image_capture_tool("drag_input_mouse"));
  EXPECT_TRUE(is_image_capture_tool("type_input_text"));
  EXPECT_TRUE(is_image_capture_tool("click_editor_element"));
  EXPECT_TRUE(is_image_capture_tool("type_editor_element_text"));
  EXPECT_FALSE(is_image_capture_tool("create_scene_node"));
  EXPECT_FALSE(is_image_capture_tool(""));
}

TEST(ToolImageContentTest, AttachesTopLevelData) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"data":"QUJD","format":"png","width":640,"height":480,"path":"user://shot.png"})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(try_attach_image_content("capture_game_viewport", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "QUJD");
  EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
  ASSERT_TRUE(result["image_attached"].IsBool());
  EXPECT_TRUE(result["image_attached"].GetBool());
  EXPECT_EQ(result["format"].GetString(), "png");
  EXPECT_EQ(result["width"].GetInt(), 640);
  EXPECT_EQ(result["height"].GetInt(), 480);
  EXPECT_EQ(result["path"].GetString(), "user://shot.png");
}

TEST(ToolImageContentTest, AttachesNestedEditorResultData) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"result":{"data":"QUJD","format":"png","width":100,"height":50}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(
      try_attach_image_content("capture_editor_viewport", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "QUJD");
  godot_autopilot::util::JsonValue &inner = result["result"];
  EXPECT_EQ(inner["data"].GetString(), "<attached-as-image-content>");
  EXPECT_TRUE(inner["image_attached"].GetBool());
  EXPECT_EQ(inner["width"].GetInt(), 100);
  EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(ToolImageContentTest, AttachesNestedDisplayResultData) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"result":{"mime":"image/png","format":"png","data":"QUJD"}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(
      try_attach_image_content("capture_display_screen", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "QUJD");
  godot_autopilot::util::JsonValue &inner = result["result"];
  EXPECT_EQ(inner["data"].GetString(), "<attached-as-image-content>");
  EXPECT_EQ(inner["mime"].GetString(), "image/png");
  EXPECT_TRUE(inner["image_attached"].GetBool());
}

TEST(ToolImageContentTest, PrefersTopLevelDataOverNested) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"data":"TOPDATA","result":{"data":"NESTED","format":"png"}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(
      try_attach_image_content("capture_editor_viewport", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "TOPDATA");
  EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
  EXPECT_TRUE(result["image_attached"].GetBool());
  EXPECT_EQ(result["result"]["data"].GetString(), "NESTED");
  EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(ToolImageContentTest, FallsBackToNestedDataWhenTopLevelEmpty) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"data":"","result":{"data":"QUJD","format":"png"}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(
      try_attach_image_content("capture_editor_viewport", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "QUJD");
  EXPECT_EQ(result["data"].GetString(), "");
  EXPECT_FALSE(result.Contains("image_attached"));
  EXPECT_EQ(result["result"]["data"].GetString(),
            "<attached-as-image-content>");
  EXPECT_TRUE(result["result"]["image_attached"].GetBool());
}

TEST(ToolImageContentTest, IgnoresNonWhitelistedTool) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(R"({"data":"QUJD","format":"png"})");
  std::vector<godot_autopilot::util::JsonValue> content;
  EXPECT_FALSE(
      try_attach_image_content("create_scene_node", result, content));
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(result["data"].GetString(), "QUJD");
  EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(ToolImageContentTest, IgnoresNonObjectJson) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(R"(["QUJD"])");
  std::vector<godot_autopilot::util::JsonValue> content;
  EXPECT_FALSE(
      try_attach_image_content("capture_game_viewport", result, content));
  EXPECT_TRUE(content.empty());
  ASSERT_TRUE(result.IsArray());
  EXPECT_EQ(result[0].GetString(), "QUJD");
}

TEST(ToolImageContentTest, IgnoresMissingData) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"result":{"width":100,"height":50}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  EXPECT_FALSE(
      try_attach_image_content("capture_editor_viewport", result, content));
  EXPECT_TRUE(content.empty());
  EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(ToolImageContentTest, RejectsNonPngFormat) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(R"({"data":"QUJD","format":"jpg"})");
  std::vector<godot_autopilot::util::JsonValue> content;
  EXPECT_FALSE(
      try_attach_image_content("capture_game_viewport", result, content));
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(result["data"].GetString(), "QUJD");
  EXPECT_FALSE(result.Contains("image_attached"));
}

TEST(ToolImageContentTest, TreatsMissingFormatAsPng) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(R"({"data":"QUJD"})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(try_attach_image_content("capture_game_viewport", result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_image(image_at(content, 0), "QUJD");
}
