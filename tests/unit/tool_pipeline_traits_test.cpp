#include "tools/tool_pipeline.hpp"
#include "tools/tool_spec.hpp"
#include "util/mcp_image_content.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace {

const mcp::ImageContent *image_at(
    const std::vector<mcp::ContentVariant> &content, size_t index) {
  if (index >= content.size())
    return nullptr;
  return std::get_if<mcp::ImageContent>(&content[index]);
}

godot_autopilot::ToolSpec make_plain_spec() {
  godot_autopilot::ToolSpec spec;
  spec.name = "dummy_no_flags";
  spec.description = "passthrough probe";
  spec.category = "Test";
  spec.side_effect = godot_autopilot::SideEffect::None;
  spec.flags = godot_autopilot::tool_flags::kNone;
  spec.handler = [](const mcp::JsonValue &) {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  };
  return spec;
}

} // namespace

TEST(ToolPipelineTraitsTest, AttachImageDataTopLevel) {
  mcp::JsonValue result = mcp::JsonValue::Parse(
      R"({"data":"QUJD","format":"png","width":640,"height":480})");
  std::vector<mcp::ContentVariant> content;
  ASSERT_TRUE(
      godot_autopilot::util::try_attach_image_data(result, content));
  ASSERT_EQ(content.size(), 1u);
  const mcp::ImageContent *image = image_at(content, 0);
  ASSERT_NE(image, nullptr);
  EXPECT_EQ(image->type, "image");
  EXPECT_EQ(image->data, "QUJD");
  EXPECT_EQ(image->mime_type, "image/png");
  EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
  ASSERT_TRUE(result["image_attached"].IsBool());
  EXPECT_TRUE(result["image_attached"].GetBool());
}

TEST(ToolPipelineTraitsTest, RejectNonPngFormat) {
  mcp::JsonValue result =
      mcp::JsonValue::Parse(R"({"data":"QUJD","format":"jpg"})");
  std::vector<mcp::ContentVariant> content;
  std::string reason;
  EXPECT_FALSE(
      godot_autopilot::util::try_attach_image_data(result, content, &reason));
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(result["data"].GetString(), "QUJD");
  EXPECT_FALSE(result.Contains("image_attached"));
  EXPECT_EQ(reason, "format is not png");
}

TEST(ToolPipelineTraitsTest, RejectMissingData) {
  mcp::JsonValue result =
      mcp::JsonValue::Parse(R"({"result":{"width":100,"height":50}})");
  std::vector<mcp::ContentVariant> content;
  EXPECT_FALSE(
      godot_autopilot::util::try_attach_image_data(result, content));
  EXPECT_TRUE(content.empty());
  EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(ToolPipelineTraitsTest, RunPostWithoutFlagsPassesThrough) {
  godot_autopilot::ToolSpec spec = make_plain_spec();
  mcp::JsonValue args =
      mcp::JsonValue::Parse(R"({"observe":true})");
  mcp::JsonValue result = mcp::JsonValue::Parse(
      R"({"result":{"ok":true,"width":100}})");
  const std::string before = result.Dump();
  mcp::JsonValue out =
      godot_autopilot::pipeline::run_post(spec, args, std::move(result));
  EXPECT_EQ(out.Dump(), before);
  EXPECT_FALSE(out["result"].Contains("observe_error"));
  EXPECT_FALSE(out["result"].Contains("scene_path"));
}
