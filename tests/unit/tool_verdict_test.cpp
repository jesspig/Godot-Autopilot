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

TEST(ToolVerdictTest, DeferredMarkerIsDetectedByPendingId) {
  std::string key;
  const bool deferred = godot_autopilot::spec_detail::deferred_key_of(
      parse(R"({"__gda_pending":7,"timeout_ms":8020})"), &key);
  EXPECT_TRUE(deferred);
  EXPECT_EQ(key, "pending:7");
}

TEST(ToolVerdictTest, DeferredMarkerIsDetectedByWaitKindAndId) {
  std::string key;
  const bool deferred = godot_autopilot::spec_detail::deferred_key_of(
      parse(R"({"__gda_wait":{"kind":"os_process","id":12,"timeout_ms":5000}})"),
      &key);
  EXPECT_TRUE(deferred);
  EXPECT_EQ(key, "wait:os_process:12");
}

TEST(ToolVerdictTest, PlainResultHasNoDeferredKey) {
  std::string key;
  EXPECT_FALSE(godot_autopilot::spec_detail::deferred_key_of(
      parse(R"({"result":{"ok":true}})"), &key));
}

TEST(ToolVerdictTest, DeferredTraceStashAndTakeRoundTrip) {
  godot_autopilot::spec_detail::DeferredTrace record;
  record.event.tool = "execute_game_script";
  record.event.ok = true;
  godot_autopilot::spec_detail::stash_deferred_trace("pending:99", record);
  godot_autopilot::spec_detail::DeferredTrace taken;
  EXPECT_TRUE(godot_autopilot::spec_detail::take_deferred_trace("pending:99", &taken));
  EXPECT_EQ(taken.event.tool, "execute_game_script");
  EXPECT_FALSE(godot_autopilot::spec_detail::take_deferred_trace("pending:99", &taken));
}
