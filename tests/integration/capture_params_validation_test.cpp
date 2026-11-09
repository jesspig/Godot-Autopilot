#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class CaptureParamsSuite : public IntegrationHarnessWithAllow {
 public:
  CaptureParamsSuite() : IntegrationHarnessWithAllow("write") {}
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

void assert_runtime_channel_error_with_hint(const mcp::JsonValue &result) {
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
  mcp::JsonValue args = make_args();
  args["after_frames"] = mcp::JsonValue(-1);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "after_frames must be an integer greater than or equal to 0");
}

TEST_F(CaptureParamsSuite, GameCaptureAfterFramesWrongType) {
  mcp::JsonValue args = make_args();
  args["after_frames"] = mcp::JsonValue("2");
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(
      result, "after_frames must be an integer greater than or equal to 0");
}

TEST_F(CaptureParamsSuite, EditorCaptureAfterFramesRejected) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["after_frames"] = mcp::JsonValue(3);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "after_frames/when are only supported for target='game'");
}

TEST_F(CaptureParamsSuite, EditorCaptureWhenRejected) {
  mcp::JsonValue args = make_args();
  args["target"] = mcp::JsonValue("editor");
  args["when"] =
      mcp::JsonValue("get_node(\"/root\").get_child_count() > 0");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result,
                     "after_frames/when are only supported for target='game'");
}

TEST_F(CaptureParamsSuite, EditorCaptureScaleBelowMin) {
  mcp::JsonValue args = make_args();
  args["scale"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(CaptureParamsSuite, GameCaptureScaleAboveMax) {
  mcp::JsonValue args = make_args();
  args["scale"] = mcp::JsonValue(9);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "scale must be an integer between 1 and 8");
}

TEST_F(CaptureParamsSuite, GameCaptureValidDelayNoGameReturnsRuntimeError) {
  mcp::JsonValue args = make_args();
  args["after_frames"] = mcp::JsonValue(5);
  args["when"] = mcp::JsonValue("true");
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_runtime_channel_error_with_hint(result);
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveWrongType) {
  mcp::JsonValue args = make_args();
  args["save"] = mcp::JsonValue("yes");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: save must be a boolean");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirWrongType) {
  mcp::JsonValue args = make_args();
  args["save_dir"] = mcp::JsonValue(5);
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "invalid parameter: save_dir must be a string");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirEscapeRejected) {
  mcp::JsonValue args = make_args();
  args["save"] = mcp::JsonValue(true);
  args["save_dir"] = mcp::JsonValue("res://../gda_escape");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(result, "path rejected: path escapes the namespace root");
}

TEST_F(CaptureParamsSuite, EditorCaptureSaveDirUserSchemeRejected) {
  mcp::JsonValue args = make_args();
  args["save_dir"] = mcp::JsonValue("user://captures");
  mcp::JsonValue result = call("capture_editor_viewport", args);
  assert_exact_error(
      result, "path rejected: user:// paths are not allowed for this operation");
}

TEST_F(CaptureParamsSuite, GameCaptureSaveWrongType) {
  mcp::JsonValue args = make_args();
  args["save"] = mcp::JsonValue(1);
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "invalid parameter: save must be a boolean");
}

TEST_F(CaptureParamsSuite, GameCaptureSaveDirEscapeRejected) {
  mcp::JsonValue args = make_args();
  args["save_dir"] = mcp::JsonValue("res://../gda_escape");
  mcp::JsonValue result = call("capture_game_viewport", args);
  assert_exact_error(result, "path rejected: path escapes the namespace root");
}

} // namespace
