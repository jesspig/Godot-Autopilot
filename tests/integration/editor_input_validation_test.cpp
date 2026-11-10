#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class EditorInputSuite : public IntegrationHarnessWithAllow {
 public:
  EditorInputSuite() : IntegrationHarnessWithAllow("write") {}
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

TEST_F(EditorInputSuite, ClickInputMouseMissingPosition) {
  mcp::JsonValue result = call("click_input_mouse", make_args());
  assert_exact_error(result, "missing required parameter: position");
}

TEST_F(EditorInputSuite, ScrollInputMouseInvalidDirection) {
  mcp::JsonValue args = make_args();
  args["direction"] = mcp::JsonValue("sideways");
  mcp::JsonValue position(mcp::JsonValue::object_tag);
  position["x"] = mcp::JsonValue(1);
  position["y"] = mcp::JsonValue(1);
  args["position"] = std::move(position);
  mcp::JsonValue result = call("scroll_input_mouse", args);
  assert_exact_error(result, "invalid scroll direction: sideways");
}

TEST_F(EditorInputSuite, DragInputMouseMissingTo) {
  mcp::JsonValue args = make_args();
  mcp::JsonValue from(mcp::JsonValue::object_tag);
  from["x"] = mcp::JsonValue(0);
  from["y"] = mcp::JsonValue(0);
  args["from"] = std::move(from);
  mcp::JsonValue result = call("drag_input_mouse", args);
  assert_exact_error(result, "missing required parameter: to");
}

TEST_F(EditorInputSuite, TypeInputTextMissingText) {
  mcp::JsonValue result = call("type_input_text", make_args());
  assert_exact_error(result, "missing required parameter: text");
}

TEST_F(EditorInputSuite, ClickEditorElementMissingPath) {
  mcp::JsonValue result = call("click_editor_element", make_args());
  assert_exact_error(result, "missing required parameter: path");
}

TEST_F(EditorInputSuite, RunEditorShortcutUnknownKey) {
  mcp::JsonValue args = make_args();
  args["shortcut"] = mcp::JsonValue("ctrl+notakey");
  mcp::JsonValue result = call("run_editor_shortcut", args);
  assert_exact_error(result, "invalid shortcut key: notakey");
}

} // namespace
