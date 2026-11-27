#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class ClickUiCoordsSuite : public IntegrationHarnessWithAllow {
 public:
  ClickUiCoordsSuite() : IntegrationHarnessWithAllow("game_runtime") {}
};

godot_autopilot::util::JsonValue make_args() {
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["confirm"] = godot_autopilot::util::JsonValue(true);
  return args;
}

void assert_exact_error(const godot_autopilot::util::JsonValue &result,
                        const std::string &expected) {
  const auto *v = result.Find("error");
  ASSERT_NE(v, nullptr) << result.Dump();
  ASSERT_TRUE(v->IsString()) << result.Dump();
  EXPECT_EQ(v->GetString(), expected);
}

TEST_F(ClickUiCoordsSuite, ClickMissingPathAndText) {
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", make_args());
  assert_exact_error(
      result,
      "missing required parameter: path or text (non-empty node path "
      "from get_game_ui_elements, or visible element text; path takes "
      "priority when both are given)");
}

TEST_F(ClickUiCoordsSuite, ClickPathWrongTypeReportsMissingPathOrText) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue(7);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "missing required parameter: path or text (non-empty node path "
      "from get_game_ui_elements, or visible element text; path takes "
      "priority when both are given)");
}

TEST_F(ClickUiCoordsSuite, ClickButtonIndexOutOfRange) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  args["button_index"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "button_index must be an integer between 1 and 3 (1=left, 2=right, "
      "3=middle)");
}

TEST_F(ClickUiCoordsSuite, ClickDoubleClickWrongType) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  args["double_click"] = godot_autopilot::util::JsonValue("yes");
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result, "double_click must be a boolean");
}

TEST_F(ClickUiCoordsSuite, ClickMaxElementsNonPositive) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  args["max_elements"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result, "max_elements must be a positive integer");
}

TEST_F(ClickUiCoordsSuite, ClickTimeoutOutOfRange) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  args["timeout_ms"] = godot_autopilot::util::JsonValue(0);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(result,
                     "timeout_ms must be an integer between 1 and 25000");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterWindowPosition) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  godot_autopilot::util::JsonValue window_position(godot_autopilot::util::JsonValue::object_tag);
  window_position["x"] = godot_autopilot::util::JsonValue(640);
  window_position["y"] = godot_autopilot::util::JsonValue(360);
  args["window_position"] = std::move(window_position);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: window_position — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterCoordSpace) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  args["coord_space"] = godot_autopilot::util::JsonValue("viewport");
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: coord_space — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, ClickUnknownParameterWindowRect) {
  godot_autopilot::util::JsonValue args = make_args();
  args["path"] = godot_autopilot::util::JsonValue("Hud/Start");
  godot_autopilot::util::JsonValue window_rect(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue position(godot_autopilot::util::JsonValue::object_tag);
  position["x"] = godot_autopilot::util::JsonValue(640);
  position["y"] = godot_autopilot::util::JsonValue(360);
  window_rect["position"] = std::move(position);
  godot_autopilot::util::JsonValue size(godot_autopilot::util::JsonValue::object_tag);
  size["x"] = godot_autopilot::util::JsonValue(10);
  size["y"] = godot_autopilot::util::JsonValue(10);
  window_rect["size"] = std::move(size);
  args["window_rect"] = std::move(window_rect);
  godot_autopilot::util::JsonValue result = call("click_game_ui_element", args);
  assert_exact_error(
      result,
      "unknown parameter for click_game_ui_element: window_rect — "
      "annotate id is a per-capture sequence number and is not clickable; "
      "use path from the same get_game_ui_elements row (path takes priority) "
      "or text/text_index");
}

TEST_F(ClickUiCoordsSuite, QueueInputCoordSpaceIllegal) {
  godot_autopilot::util::JsonValue args = make_args();
  args["type"] = godot_autopilot::util::JsonValue("key");
  args["keycode"] = godot_autopilot::util::JsonValue("A");
  args["coord_space"] = godot_autopilot::util::JsonValue("screen");
  godot_autopilot::util::JsonValue result = call("queue_game_input", args);
  assert_exact_error(result, "coord_space must be window|viewport");
}

TEST_F(ClickUiCoordsSuite, SequenceInputsCoordSpaceIllegal) {
  godot_autopilot::util::JsonValue args = make_args();
  godot_autopilot::util::JsonValue inputs(godot_autopilot::util::JsonValue::array_tag);
  godot_autopilot::util::JsonValue item(godot_autopilot::util::JsonValue::object_tag);
  item["kind"] = godot_autopilot::util::JsonValue("key");
  item["keycode"] = godot_autopilot::util::JsonValue("A");
  item["at_frame"] = godot_autopilot::util::JsonValue(0);
  inputs.PushBack(std::move(item));
  args["inputs"] = std::move(inputs);
  args["coord_space"] = godot_autopilot::util::JsonValue("screen");
  godot_autopilot::util::JsonValue result = call("sequence_game_inputs", args);
  assert_exact_error(result, "coord_space must be window|viewport");
}

} // namespace
