#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>
#include <tools/base/tool_spec.hpp>

namespace {

mcp::JsonValue parse(const char *text) {
  mcp::JsonValue value = mcp::JsonValue::Parse(text);
  EXPECT_TRUE(value.IsObject()) << text;
  return value;
}

godot_autopilot::spec_detail::ToolVerdict verdict(const char *text) {
  return godot_autopilot::spec_detail::verdict_of(parse(text));
}

} // namespace

TEST(ToolVerdictTest, PlainErrorIsFailureWithGenericCode) {
  const auto v = verdict(R"({"error":"invalid shortcut key: -"})");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "error");
}

TEST(ToolVerdictTest, TopLevelCodeWinsForPlainError) {
  const auto v = verdict(R"({"error":"editor is exporting","code":"export_blocked"})");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "export_blocked");
}

TEST(ToolVerdictTest, StructuredCodeIsKeptWhenTopLevelErrorAlsoPresent) {
  const auto v = verdict(R"({
    "error": "capture condition not met within 4000 ms",
    "structured_error": {"code": "when_timeout", "timeout_ms": 4000}
  })");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "when_timeout");
}

TEST(ToolVerdictTest, StructuredErrorAloneIsFailure) {
  const auto v = verdict(R"({"structured_error":{"code":"when_timeout","frames_waited":240}})");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "when_timeout");
}

TEST(ToolVerdictTest, RuntimeErrorAloneIsFailure) {
  const auto v = verdict(R"({
    "result": {"value": null},
    "runtime_error": true,
    "error_details": "Invalid call. Nonexistent function 'get_node' in base 'Node2D'."
  })");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "runtime_error");
}

TEST(ToolVerdictTest, StructuredCodeWinsOverRuntimeError) {
  const auto v = verdict(R"({
    "runtime_error": true,
    "error_details": "boom",
    "structured_error": {"code": "tool_error"}
  })");
  EXPECT_FALSE(v.ok);
  EXPECT_EQ(v.error_code, "tool_error");
}

TEST(ToolVerdictTest, RuntimeErrorFalseIsSuccess) {
  const auto v = verdict(R"({"runtime_error":false,"result":{"value":1}})");
  EXPECT_TRUE(v.ok);
  EXPECT_TRUE(v.error_code.empty());
}

TEST(ToolVerdictTest, StructuredErrorWithoutCodeIsSuccess) {
  const auto v = verdict(R"({"structured_error":{"action":"retry"},"result":{"value":1}})");
  EXPECT_TRUE(v.ok);
  EXPECT_TRUE(v.error_code.empty());
}

TEST(ToolVerdictTest, StructuredErrorWithEmptyCodeIsSuccess) {
  const auto v = verdict(R"({"structured_error":{"code":""},"result":{"value":1}})");
  EXPECT_TRUE(v.ok);
  EXPECT_TRUE(v.error_code.empty());
}

TEST(ToolVerdictTest, NormalResultIsSuccess) {
  const auto v = verdict(R"({"result":{"path":"res://a.tscn"}})");
  EXPECT_TRUE(v.ok);
  EXPECT_TRUE(v.error_code.empty());
}

TEST(ToolVerdictTest, NonObjectResultIsSuccess) {
  const auto v = godot_autopilot::spec_detail::verdict_of(mcp::JsonValue("text"));
  EXPECT_TRUE(v.ok);
  EXPECT_TRUE(v.error_code.empty());
}
