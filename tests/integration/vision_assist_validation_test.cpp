#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class VisionAssistSuite : public IntegrationHarnessWithAllow {
 public:
  VisionAssistSuite() : IntegrationHarnessWithAllow("write") {}
};

mcp::JsonValue make_args() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

void assert_exact_error(const mcp::JsonValue &result,
                        const std::string &expected) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_EQ(v->GetString(), expected);
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesNotArray) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["annotate_nodes"] = mcp::JsonValue("Player");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must be an array of node path strings");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesEmpty) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["annotate_nodes"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must contain between 1 and 50 entries");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesEmptyString) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  nodes.PushBack(mcp::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must contain non-empty path strings");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesMaxNotInt) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  nodes.PushBack(mcp::JsonValue("Player"));
  args["annotate_nodes"] = std::move(nodes);
  args["annotate_nodes_max"] = mcp::JsonValue("10");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "invalid parameter: annotate_nodes_max must be an integer");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesMaxOutOfRange) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["annotate_nodes_max"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result, "invalid parameter: annotate_nodes_max out of range (1-50): 0");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesExceedsSelfMax) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  nodes.PushBack(mcp::JsonValue("A"));
  nodes.PushBack(mcp::JsonValue("B"));
  args["annotate_nodes"] = std::move(nodes);
  args["annotate_nodes_max"] = mcp::JsonValue(1);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes length exceeds annotate_nodes_max (1)");
}

TEST_F(VisionAssistSuite, EditorDiffImageNotBool) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["diff_image"] = mcp::JsonValue("yes");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: diff_image must be a boolean");
}

TEST_F(VisionAssistSuite, GameTargetDiffImageRejected) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("game");
  args["diff_image"] = mcp::JsonValue(true);
  args["diff_against_last"] = mcp::JsonValue(true);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "diff_image is only supported for target='editor'");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesEmpty) {
  mcp::JsonValue args = make_args();
  args["annotate_nodes"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of 1-50 strings");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesEmptyString) {
  mcp::JsonValue args = make_args();
  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  nodes.PushBack(mcp::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of non-empty strings");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesMaxZero) {
  mcp::JsonValue args = make_args();
  args["annotate_nodes_max"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "annotate_nodes_max must be an integer between 1 and 50");
}

TEST_F(VisionAssistSuite, ReviewUnknownParameter) {
  mcp::JsonValue args = make_args();
  args["bogus_param"] = mcp::JsonValue(1);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "unknown parameter for review_scene_visually: bogus_param");
}

TEST_F(VisionAssistSuite, ReviewBothTargetsSkipped) {
  mcp::JsonValue args = make_args();
  args["include_editor"] = mcp::JsonValue(false);
  args["include_game"] = mcp::JsonValue(false);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "at least one of include_editor/include_game must be true");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesEmpty) {
  mcp::JsonValue args = make_args();
  args["annotate_nodes"] = mcp::JsonValue(mcp::JsonValue::array_tag);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of 1-50 strings");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesEmptyString) {
  mcp::JsonValue args = make_args();
  mcp::JsonValue nodes(mcp::JsonValue::array_tag);
  nodes.PushBack(mcp::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of non-empty strings");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesMaxZero) {
  mcp::JsonValue args = make_args();
  args["annotate_nodes_max"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "annotate_nodes_max must be an integer between 1 and 50");
}

TEST_F(VisionAssistSuite, ReviewNodeViewportInvalid) {
  mcp::JsonValue args = make_args();
  args["node_viewport"] = mcp::JsonValue("side");
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "node_viewport must be one of: auto, 2d, 3d");
}

TEST_F(VisionAssistSuite, ReviewMaxDimensionOutOfRange) {
  mcp::JsonValue args = make_args();
  args["max_dimension"] = mcp::JsonValue(32);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "max_dimension must be an integer between 64 and 4096");
}

TEST_F(VisionAssistSuite, ReviewScaleOutOfRange) {
  mcp::JsonValue args = make_args();
  args["scale"] = mcp::JsonValue(9);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(VisionAssistSuite, ReviewRegionMissingDimensions) {
  mcp::JsonValue args = make_args();
  mcp::JsonValue region(mcp::JsonValue::object_tag);
  region["x"] = mcp::JsonValue(0);
  region["y"] = mcp::JsonValue(0);
  args["region"] = std::move(region);
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "region must be an object with numeric x, y, width and height");
}

TEST_F(VisionAssistSuite, ReviewIncludeEditorNotBool) {
  mcp::JsonValue args = make_args();
  args["include_editor"] = mcp::JsonValue("yes");
  mcp::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "include_editor must be a boolean");
}

} // namespace
