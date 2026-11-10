#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class ClickUiCoordsSuite : public IntegrationHarnessWithAllow {
 public:
  ClickUiCoordsSuite() : IntegrationHarnessWithAllow("game_runtime") {}
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

TEST_F(ClickUiCoordsSuite, ClickMissingPathAndText) {
  mcp::JsonValue result = call("click_game_ui_element", make_args());
  assert_exact_error(
      result,
      "missing required parameter: path or text (non-empty node path "
      "from get_game_ui_elements, or visible element text; path takes "
      "priority when both are given)");
}

TEST_F(ClickUiCoordsSuite, ClickPathWrongTypeReportsMissingPathOrText) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue(7);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "missing required parameter: path or text (non-empty node path "
      "from get_game_ui_elements, or visible element text; path takes "
      "priority when both are given)");
}

TEST_F(ClickUiCoordsSuite, ClickButtonIndexOutOfRange) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  args["button_index"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "button_index must be an integer between 1 and 3 (1=left, 2=right, "
      "3=middle)");
}

TEST_F(ClickUiCoordsSuite, ClickDoubleClickWrongType) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  args["double_click"] = mcp::JsonValue("yes");
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result, "double_click must be a boolean");
}

TEST_F(ClickUiCoordsSuite, ClickMaxElementsNonPositive) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  args["max_elements"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result, "max_elements must be a positive integer");
}

TEST_F(ClickUiCoordsSuite, ClickTimeoutOutOfRange) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  args["timeout_ms"] = mcp::JsonValue(0);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result,
                     "timeout_ms must be an integer between 1 and 25000");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterWindowPosition) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  mcp::JsonValue window_position(mcp::JsonValue::object_tag);
  window_position["x"] = mcp::JsonValue(640);
  window_position["y"] = mcp::JsonValue(360);
  args["window_position"] = std::move(window_position);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: window_position — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterCoordSpace) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  args["coord_space"] = mcp::JsonValue("viewport");
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: coord_space — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterWindowRect) {
  mcp::JsonValue args = make_args();
  args["path"] = mcp::JsonValue("Hud/Start");
  mcp::JsonValue window_rect(mcp::JsonValue::object_tag);
  mcp::JsonValue position(mcp::JsonValue::object_tag);
  position["x"] = mcp::JsonValue(640);
  position["y"] = mcp::JsonValue(360);
  window_rect["position"] = std::move(position);
  mcp::JsonValue size(mcp::JsonValue::object_tag);
  size["x"] = mcp::JsonValue(10);
  size["y"] = mcp::JsonValue(10);
  window_rect["size"] = std::move(size);
  args["window_rect"] = std::move(window_rect);
  mcp::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: window_rect — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, QueueInputCoordSpaceIllegal) {
  mcp::JsonValue args = make_args();
  args["type"] = mcp::JsonValue("key");
  args["keycode"] = mcp::JsonValue("A");
  args["coord_space"] = mcp::JsonValue("screen");
  mcp::JsonValue result = call("queue_game_input", args);
  assert_exact_error(result, "coord_space must be window|viewport");
}

TEST_F(ClickUiCoordsSuite, SequenceInputsCoordSpaceIllegal) {
  mcp::JsonValue args = make_args();
  mcp::JsonValue inputs(mcp::JsonValue::array_tag);
  mcp::JsonValue item(mcp::JsonValue::object_tag);
  item["kind"] = mcp::JsonValue("key");
  item["keycode"] = mcp::JsonValue("A");
  item["at_frame"] = mcp::JsonValue(0);
  inputs.PushBack(std::move(item));
  args["inputs"] = std::move(inputs);
  args["coord_space"] = mcp::JsonValue("screen");
  mcp::JsonValue result = call("sequence_game_inputs", args);
  assert_exact_error(result, "coord_space must be window|viewport");
}

} // namespace
