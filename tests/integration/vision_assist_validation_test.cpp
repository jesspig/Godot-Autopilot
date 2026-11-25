#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class VisionAssistSuite : public IntegrationHarnessWithAllow {
 public:
  VisionAssistSuite() : IntegrationHarnessWithAllow("write") {}
};

godot_autopilot::util::JsonValue make_args() {
  return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
}

void assert_exact_error(const godot_autopilot::util::JsonValue &result,
                        const std::string &expected) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_EQ(v->GetString(), expected);
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesNotArray) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["annotate_nodes"] = godot_autopilot::util::JsonValue("Player");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must be an array of node path strings");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesEmpty) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["annotate_nodes"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must contain between 1 and 50 entries");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesEmptyString) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  nodes.PushBack(godot_autopilot::util::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes must contain non-empty path strings");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesMaxNotInt) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  nodes.PushBack(godot_autopilot::util::JsonValue("Player"));
  args["annotate_nodes"] = std::move(nodes);
  args["annotate_nodes_max"] = godot_autopilot::util::JsonValue("10");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "invalid parameter: annotate_nodes_max must be an integer");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesMaxOutOfRange) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["annotate_nodes_max"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result, "invalid parameter: annotate_nodes_max out of range (1-50): 0");
}

TEST_F(VisionAssistSuite, EditorAnnotateNodesExceedsSelfMax) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  nodes.PushBack(godot_autopilot::util::JsonValue("A"));
  nodes.PushBack(godot_autopilot::util::JsonValue("B"));
  args["annotate_nodes"] = std::move(nodes);
  args["annotate_nodes_max"] = godot_autopilot::util::JsonValue(1);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result,
      "invalid parameter: annotate_nodes length exceeds annotate_nodes_max (1)");
}

TEST_F(VisionAssistSuite, EditorDiffImageNotBool) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["diff_image"] = godot_autopilot::util::JsonValue("yes");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: diff_image must be a boolean");
}

TEST_F(VisionAssistSuite, GameTargetDiffImageRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("game");
  args["diff_image"] = godot_autopilot::util::JsonValue(true);
  args["diff_against_last"] = godot_autopilot::util::JsonValue(true);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "diff_image is only supported for target='editor'");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesEmpty) {
  godot_autopilot::util::JsonValue args = make_args();
  args["annotate_nodes"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of 1-50 strings");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesEmptyString) {
  godot_autopilot::util::JsonValue args = make_args();
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  nodes.PushBack(godot_autopilot::util::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of non-empty strings");
}

TEST_F(VisionAssistSuite, GameAnnotateNodesMaxZero) {
  godot_autopilot::util::JsonValue args = make_args();
  args["annotate_nodes_max"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "annotate_nodes_max must be an integer between 1 and 50");
}

TEST_F(VisionAssistSuite, ReviewUnknownParameter) {
  godot_autopilot::util::JsonValue args = make_args();
  args["bogus_param"] = godot_autopilot::util::JsonValue(1);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "unknown parameter for review_scene_visually: bogus_param");
}

TEST_F(VisionAssistSuite, ReviewBothTargetsSkipped) {
  godot_autopilot::util::JsonValue args = make_args();
  args["include_editor"] = godot_autopilot::util::JsonValue(false);
  args["include_game"] = godot_autopilot::util::JsonValue(false);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "at least one of include_editor/include_game must be true");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesEmpty) {
  godot_autopilot::util::JsonValue args = make_args();
  args["annotate_nodes"] = godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::array_tag);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of 1-50 strings");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesEmptyString) {
  godot_autopilot::util::JsonValue args = make_args();
  godot_autopilot::util::JsonValue nodes(godot_autopilot::util::JsonValue::array_tag);
  nodes.PushBack(godot_autopilot::util::JsonValue(""));
  args["annotate_nodes"] = std::move(nodes);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "annotate_nodes must be an array of non-empty strings");
}

TEST_F(VisionAssistSuite, ReviewAnnotateNodesMaxZero) {
  godot_autopilot::util::JsonValue args = make_args();
  args["annotate_nodes_max"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "annotate_nodes_max must be an integer between 1 and 50");
}

TEST_F(VisionAssistSuite, ReviewNodeViewportInvalid) {
  godot_autopilot::util::JsonValue args = make_args();
  args["node_viewport"] = godot_autopilot::util::JsonValue("side");
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "node_viewport must be one of: auto, 2d, 3d");
}

TEST_F(VisionAssistSuite, ReviewMaxDimensionOutOfRange) {
  godot_autopilot::util::JsonValue args = make_args();
  args["max_dimension"] = godot_autopilot::util::JsonValue(32);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result,
                     "max_dimension must be an integer between 64 and 4096");
}

TEST_F(VisionAssistSuite, ReviewScaleOutOfRange) {
  godot_autopilot::util::JsonValue args = make_args();
  args["scale"] = godot_autopilot::util::JsonValue(9);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(VisionAssistSuite, ReviewRegionMissingDimensions) {
  godot_autopilot::util::JsonValue args = make_args();
  godot_autopilot::util::JsonValue region(godot_autopilot::util::JsonValue::object_tag);
  region["x"] = godot_autopilot::util::JsonValue(0);
  region["y"] = godot_autopilot::util::JsonValue(0);
  args["region"] = std::move(region);
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(
      result, "region must be an object with numeric x, y, width and height");
}

TEST_F(VisionAssistSuite, ReviewIncludeEditorNotBool) {
  godot_autopilot::util::JsonValue args = make_args();
  args["include_editor"] = godot_autopilot::util::JsonValue("yes");
  godot_autopilot::util::JsonValue result = call("review_scene_visually", args);
  assert_exact_error(result, "include_editor must be a boolean");
}

} // namespace
