#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class EditorInputSuite : public IntegrationHarnessWithAllow {
 public:
  EditorInputSuite() : IntegrationHarnessWithAllow("write") {}
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

TEST_F(EditorInputSuite, ClickInputMouseMissingPosition) {
  godot_autopilot::util::JsonValue result = call("click_input_mouse", make_args());
  assert_exact_error(result, "missing required parameter: position");
}

TEST_F(EditorInputSuite, ScrollInputMouseInvalidDirection) {
  godot_autopilot::util::JsonValue args = make_args();
  args["direction"] = godot_autopilot::util::JsonValue("sideways");
  godot_autopilot::util::JsonValue position(godot_autopilot::util::JsonValue::object_tag);
  position["x"] = godot_autopilot::util::JsonValue(1);
  position["y"] = godot_autopilot::util::JsonValue(1);
  args["position"] = std::move(position);
  godot_autopilot::util::JsonValue result = call("scroll_input_mouse", args);
  assert_exact_error(result, "invalid scroll direction: sideways");
}

TEST_F(EditorInputSuite, DragInputMouseMissingTo) {
  godot_autopilot::util::JsonValue args = make_args();
  godot_autopilot::util::JsonValue from(godot_autopilot::util::JsonValue::object_tag);
  from["x"] = godot_autopilot::util::JsonValue(0);
  from["y"] = godot_autopilot::util::JsonValue(0);
  args["from"] = std::move(from);
  godot_autopilot::util::JsonValue result = call("drag_input_mouse", args);
  assert_exact_error(result, "missing required parameter: to");
}

TEST_F(EditorInputSuite, TypeInputTextMissingText) {
  godot_autopilot::util::JsonValue result = call("type_input_text", make_args());
  assert_exact_error(result, "missing required parameter: text");
}

TEST_F(EditorInputSuite, ClickEditorElementMissingPath) {
  godot_autopilot::util::JsonValue result = call("click_editor_element", make_args());
  assert_exact_error(result, "missing required parameter: path");
}

TEST_F(EditorInputSuite, RunEditorShortcutUnknownKey) {
  godot_autopilot::util::JsonValue args = make_args();
  args["shortcut"] = godot_autopilot::util::JsonValue("ctrl+notakey");
  godot_autopilot::util::JsonValue result = call("run_editor_shortcut", args);
  assert_exact_error(result, "invalid shortcut key: notakey");
}

} // namespace
