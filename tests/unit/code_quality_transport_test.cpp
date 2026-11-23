#include <gtest/gtest.h>

#include <mcp/JsonValue.hpp>

#include <cstdint>
#include <string>

#include "core/error_watermark.hpp"
#include "core/observability/trace_recorder.hpp"
#include "core/transport/http_transport_ext.hpp"

namespace {

using godot_autopilot::error_watermark::consume_new_errors;
using godot_autopilot::error_watermark::count_response_errors;
using godot_autopilot::error_watermark::record_error;
using godot_autopilot::TraceRecorder;

mcp::JsonValue parse(const std::string &text) {
  return mcp::JsonValue::Parse(text);
}

} // namespace

TEST(CodeQualityA3Test, OutboxAppendRespectsCap) {
  EXPECT_TRUE(godot_autopilot::http_outbox_can_append(0, 100, 100));
  EXPECT_TRUE(godot_autopilot::http_outbox_can_append(40, 60, 100));
  EXPECT_FALSE(godot_autopilot::http_outbox_can_append(41, 60, 100));
  EXPECT_FALSE(godot_autopilot::http_outbox_can_append(0, 101, 100));
  EXPECT_TRUE(godot_autopilot::http_outbox_can_append(0, 0, 0));
}

TEST(CodeQualityA5Test, CountsTopLevelStringError) {
  EXPECT_EQ(count_response_errors(parse(R"({"error":"boom"})")), 1);
}

TEST(CodeQualityA5Test, CountsTopLevelObjectError) {
  EXPECT_EQ(count_response_errors(parse(R"({"error":{"code":"E1"}})")), 1);
}

TEST(CodeQualityA5Test, CountsStructuredAndRuntimeError) {
  EXPECT_EQ(count_response_errors(
                parse(R"({"structured_error":{"code":"E1"}})")),
            1);
  EXPECT_EQ(count_response_errors(
                parse(R"({"runtime_error":true})")),
            1);
  EXPECT_EQ(count_response_errors(
                parse(R"({"runtime_error":false})")),
            0);
}

TEST(CodeQualityA5Test, CountsObjectErrorsInsideResults) {
  const mcp::JsonValue response = parse(
      R"({"results":[{"error":{"code":"E1"}},{"error":"boom"},{"result":1}]})");
  EXPECT_EQ(count_response_errors(response), 2);
}

TEST(CodeQualityA5Test, IgnoresNonObjects) {
  EXPECT_EQ(count_response_errors(mcp::JsonValue("boom")), 0);
  EXPECT_EQ(count_response_errors(parse(R"({"result":1})")), 0);
}

TEST(CodeQualityA4Test, ErrorsAreAttributedPerRequest) {
  record_error(2, "req-a");
  record_error(3, "req-b");
  EXPECT_EQ(consume_new_errors("req-a"), 2);
  EXPECT_EQ(consume_new_errors("req-b"), 3);
  EXPECT_EQ(consume_new_errors("req-a"), 0);
}

TEST(CodeQualityA4Test, UnattributedErrorsGoToFallbackBucket) {
  record_error(4);
  EXPECT_EQ(consume_new_errors("req-x"), 4);
  EXPECT_EQ(consume_new_errors("req-y"), 0);
}

TEST(CodeQualityA4Test, ConsumingOneRequestDoesNotStealAnother) {
  record_error(1, "req-a");
  record_error(5, "req-b");
  EXPECT_EQ(consume_new_errors("req-a"), 1);
  record_error(2, "req-a");
  EXPECT_EQ(consume_new_errors("req-b"), 5);
  EXPECT_EQ(consume_new_errors("req-a"), 2);
}

TEST(CodeQualityA7Test, StripsNumericSensitiveValue) {
  const std::string payload = R"({"token":1234567890,"name":"kept"})";
  const auto result = TraceRecorder::sanitize_args(payload, true);
  EXPECT_EQ(result.text.find("1234567890"), std::string::npos);
  EXPECT_NE(result.text.find("<stripped len=10>"), std::string::npos);
  EXPECT_NE(result.text.find("\"name\":\"kept\""), std::string::npos);
}

TEST(CodeQualityA7Test, StripsBooleanAndNullSensitiveValues) {
  const auto boolean = TraceRecorder::sanitize_args(R"({"secret":true})", true);
  EXPECT_EQ(boolean.text.find("true"), std::string::npos);
  EXPECT_NE(boolean.text.find("<stripped len=4>"), std::string::npos);
  const auto null_value =
      TraceRecorder::sanitize_args(R"({"api_key":null})", true);
  EXPECT_EQ(null_value.text.find("null"), std::string::npos);
  EXPECT_NE(null_value.text.find("<stripped len=4>"), std::string::npos);
}

TEST(CodeQualityA7Test, StripsObjectAndArraySensitiveValues) {
  const auto object =
      TraceRecorder::sanitize_args(R"({"password":{"inner":"hidden"}})",
                                   true);
  EXPECT_EQ(object.text.find("hidden"), std::string::npos);
  EXPECT_EQ(object.text.find("inner"), std::string::npos);
  EXPECT_NE(object.text.find("<stripped len=18>"), std::string::npos);

  const auto array =
      TraceRecorder::sanitize_args(R"({"credential":[1,2,3]})", true);
  EXPECT_EQ(array.text.find("[1,2,3]"), std::string::npos);
  EXPECT_NE(array.text.find("<stripped len=7>"), std::string::npos);
}

TEST(CodeQualityA7Test, RawModeKeepsAllValueTypes) {
  const std::string payload = R"({"token":12345,"password":{"a":1}})";
  const auto result = TraceRecorder::sanitize_args(payload, false);
  EXPECT_EQ(result.text, payload);
}


