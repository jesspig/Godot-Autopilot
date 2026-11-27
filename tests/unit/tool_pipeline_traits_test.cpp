#include "tools/base/tool_pipeline.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/tool_image_content.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace {

godot_autopilot::ToolSpec make_plain_spec() {
  godot_autopilot::ToolSpec spec;
  spec.name = "dummy_no_flags";
  spec.description = "passthrough probe";
  spec.category = "Test";
  spec.side_effect = godot_autopilot::SideEffect::None;
  spec.flags = godot_autopilot::tool_flags::kNone;
  spec.handler = [](const godot_autopilot::util::JsonValue &) {
    return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
  };
  return spec;
}

void expect_native_image(const godot_autopilot::util::JsonValue &image,
                         const std::string &data) {
  ASSERT_TRUE(image.IsObject());
  ASSERT_NE(image.Find("type"), nullptr);
  EXPECT_EQ(image.Find("type")->GetString(), "image");
  ASSERT_NE(image.Find("data"), nullptr);
  EXPECT_EQ(image.Find("data")->GetString(), data);
  ASSERT_NE(image.Find("mimeType"), nullptr);
  EXPECT_EQ(image.Find("mimeType")->GetString(), "image/png");
}

} // namespace

TEST(ToolPipelineTraitsTest, AttachImageDataTopLevel) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"data":"QUJD","format":"png","width":640,"height":480})");
  std::vector<godot_autopilot::util::JsonValue> content;
  ASSERT_TRUE(godot_autopilot::util::try_attach_image_data(result, content));
  ASSERT_EQ(content.size(), 1u);
  expect_native_image(content[0], "QUJD");
  EXPECT_EQ(result["data"].GetString(), "<attached-as-image-content>");
  ASSERT_TRUE(result["image_attached"].IsBool());
  EXPECT_TRUE(result["image_attached"].GetBool());
}

TEST(ToolPipelineTraitsTest, RejectNonPngFormat) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(R"({"data":"QUJD","format":"jpg"})");
  std::vector<godot_autopilot::util::JsonValue> content;
  std::string reason;
  EXPECT_FALSE(
      godot_autopilot::util::try_attach_image_data(result, content, &reason));
  EXPECT_TRUE(content.empty());
  EXPECT_EQ(result["data"].GetString(), "QUJD");
  EXPECT_FALSE(result.Contains("image_attached"));
  EXPECT_EQ(reason, "format is not png");
}

TEST(ToolPipelineTraitsTest, RejectMissingData) {
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"result":{"width":100,"height":50}})");
  std::vector<godot_autopilot::util::JsonValue> content;
  EXPECT_FALSE(godot_autopilot::util::try_attach_image_data(result, content));
  EXPECT_TRUE(content.empty());
  EXPECT_FALSE(result["result"].Contains("image_attached"));
}

TEST(ToolPipelineTraitsTest, RunPostWithoutFlagsPassesThrough) {
  godot_autopilot::ToolSpec spec = make_plain_spec();
  godot_autopilot::util::JsonValue args =
      godot_autopilot::util::JsonValue::Parse(R"({"observe":true})");
  godot_autopilot::util::JsonValue result =
      godot_autopilot::util::JsonValue::Parse(
          R"({"result":{"ok":true,"width":100}})");
  const std::string before = result.Dump();
  godot_autopilot::util::JsonValue out =
      godot_autopilot::pipeline::run_post(spec, args, std::move(result));
  EXPECT_EQ(out.Dump(), before);
  EXPECT_FALSE(out["result"].Contains("observe_error"));
  EXPECT_FALSE(out["result"].Contains("scene_path"));
}
