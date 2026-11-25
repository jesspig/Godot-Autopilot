#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class CaptureParamsSuite : public IntegrationHarnessWithAllow {
 public:
  CaptureParamsSuite() : IntegrationHarnessWithAllow("write") {}
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

void assert_runtime_channel_error_with_hint(const godot_autopilot::util::JsonValue &result) {
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr) << result.Dump();
  ASSERT_TRUE(error->IsString()) << result.Dump();
  EXPECT_FALSE(error->GetString().empty());
  const auto *hint = result.Find("hint");
  ASSERT_NE(hint, nullptr) << result.Dump();
  ASSERT_TRUE(hint->IsString()) << result.Dump();
  EXPECT_FALSE(hint->GetString().empty());
}

TEST_F(CaptureParamsSuite, GameCaptureAfterFramesNegative) {
  godot_autopilot::util::JsonValue args = make_args();
  args["after_frames"] = godot_autopilot::util::JsonValue(-1);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "after_frames must be an integer greater than or equal to 0");
}

TEST_F(CaptureParamsSuite, GameCaptureAfterFramesWrongType) {
  godot_autopilot::util::JsonValue args = make_args();
  args["after_frames"] = godot_autopilot::util::JsonValue("2");
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "after_frames must be an integer greater than or equal to 0");
}

TEST_F(CaptureParamsSuite, EditorCaptureAfterFramesRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["after_frames"] = godot_autopilot::util::JsonValue(3);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "after_frames/when are only supported for target='game'");
}

TEST_F(CaptureParamsSuite, EditorCaptureWhenRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["target"] = godot_autopilot::util::JsonValue("editor");
  args["when"] =
      godot_autopilot::util::JsonValue("get_node(\"/root\").get_child_count() > 0");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "after_frames/when are only supported for target='game'");
}

TEST_F(CaptureParamsSuite, EditorCaptureScaleBelowMin) {
  godot_autopilot::util::JsonValue args = make_args();
  args["scale"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(CaptureParamsSuite, GameCaptureScaleAboveMax) {
  godot_autopilot::util::JsonValue args = make_args();
  args["scale"] = godot_autopilot::util::JsonValue(9);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(CaptureParamsSuite, GameCaptureValidDelayNoGameReturnsRuntimeError) {
  godot_autopilot::util::JsonValue args = make_args();
  args["after_frames"] = godot_autopilot::util::JsonValue(5);
  args["when"] = godot_autopilot::util::JsonValue("true");
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_runtime_channel_error_with_hint(result);
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveWrongType) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save"] = godot_autopilot::util::JsonValue("yes");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: save must be a boolean");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirWrongType) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save_dir"] = godot_autopilot::util::JsonValue(5);
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: save_dir must be a string");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirEscapeRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save"] = godot_autopilot::util::JsonValue(true);
  args["save_dir"] = godot_autopilot::util::JsonValue("res://../gda_escape");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "path rejected: path escapes the namespace root");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirUserSchemeRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save_dir"] = godot_autopilot::util::JsonValue("user://captures");
  godot_autopilot::util::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result, "path rejected: user:// paths are not allowed for this operation");
}

TEST_F(CaptureParamsSuite, GameCaptureSaveWrongType) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save"] = godot_autopilot::util::JsonValue(1);
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "invalid parameter: save must be a boolean");
}

TEST_F(CaptureParamsSuite, GameCaptureSaveDirEscapeRejected) {
  godot_autopilot::util::JsonValue args = make_args();
  args["save_dir"] = godot_autopilot::util::JsonValue("res://../gda_escape");
  godot_autopilot::util::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "path rejected: path escapes the namespace root");
}

} // namespace
