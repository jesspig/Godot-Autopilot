#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include <mcp/JsonValue.hpp>

#include "tools/domains/capture/capture_ops.hpp"

namespace {

using godot_autopilot::capture_ops::is_pending_subcall_response;
using godot_autopilot::capture_ops::unwrap_subcall_section;
using mcp::JsonValue;

JsonValue parse(const std::string &text) {
  return JsonValue::Parse(text);
}

} // namespace

TEST(CaptureSubcallTest, DetectsPendingShapeWithTimeout) {
  int64_t request_id = 0;
  int64_t timeout_ms = 0;
  ASSERT_TRUE(is_pending_subcall_response(
      parse(R"({"__gda_pending":7,"timeout_ms":8020})"), &request_id, &timeout_ms));
  EXPECT_EQ(request_id, 7);
  EXPECT_EQ(timeout_ms, 8020);
}

TEST(CaptureSubcallTest, PendingShapeRequiresAnIntegerId) {
  int64_t request_id = 0;
  int64_t timeout_ms = 0;
  EXPECT_FALSE(is_pending_subcall_response(parse(R"("text")"), &request_id, &timeout_ms));
  EXPECT_FALSE(is_pending_subcall_response(
      parse(R"({"__gda_pending":"7"})"), &request_id, &timeout_ms));
  EXPECT_FALSE(is_pending_subcall_response(parse(R"({"result":{}})"), &request_id, &timeout_ms));
}

TEST(CaptureSubcallTest, PendingTimeoutDefaultsWhenMissing) {
  int64_t timeout_ms = 0;
  ASSERT_TRUE(is_pending_subcall_response(parse(R"({"__gda_pending":9})"), nullptr, &timeout_ms));
  EXPECT_GT(timeout_ms, 0);
}

TEST(CaptureSubcallTest, UnwrapKeepsExistingResultAndErrorShapes) {
  const JsonValue ok =
      unwrap_subcall_section(parse(R"({"result":{"path":"res://a.png"}})"), true);
  ASSERT_TRUE(ok.IsObject());
  const JsonValue *path = ok.Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "res://a.png");

  const JsonValue bad = unwrap_subcall_section(parse(R"({"error":"boom"})"), false);
  ASSERT_TRUE(bad.IsObject());
  const JsonValue *error = bad.Find("error");
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->GetString(), "boom");
}

TEST(CaptureSubcallTest, PendingWithoutAwaitStaysDiagnosticInsteadOfSilentlyDropped) {
  const JsonValue section = unwrap_subcall_section(
      parse(R"({"__gda_pending":7,"timeout_ms":8020})"), false);
  ASSERT_TRUE(section.IsObject());
  const JsonValue *error = section.Find("error");
  ASSERT_NE(error, nullptr);
  EXPECT_NE(error->GetString().find("unexpected sub-call response shape"), std::string::npos);
}

TEST(CaptureSubcallTest, AwaitPendingStopsReportingShapeMismatch) {
  const JsonValue section = unwrap_subcall_section(
      parse(R"({"__gda_pending":999999,"timeout_ms":5})"), true);
  ASSERT_TRUE(section.IsObject());
  EXPECT_NE(section.Find("error"), nullptr);
  EXPECT_EQ(section.Dump().find("unexpected sub-call response shape"), std::string::npos);
}
