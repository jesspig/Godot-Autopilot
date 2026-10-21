#include "core/api_envelope.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using godot_autopilot::ApiEnvelope;

mcp::JsonValue make_failure(const std::string &error_text) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue(error_text);
  return result;
}

} // namespace

TEST(ApiEnvelopeTest, NormalizeCodePrefersExplicitCodeField) {
  ApiEnvelope envelope;
  mcp::JsonValue result = make_failure("something went wrong");
  result["code"] = mcp::JsonValue("queue_unavailable");
  EXPECT_EQ(envelope.normalize_code(result), "queue_unavailable");
}

TEST(ApiEnvelopeTest, NormalizeCodePrefersAuthorizationRequiredField) {
  ApiEnvelope envelope;
  mcp::JsonValue result = make_failure("denied");
  result["authorization_required"] = mcp::JsonValue("write");
  EXPECT_EQ(envelope.normalize_code(result), "authorization_required");
}

TEST(ApiEnvelopeTest, NormalizeCodeReadsStructuredErrorCode) {
  ApiEnvelope envelope;
  mcp::JsonValue structured(mcp::JsonValue::object_tag);
  structured["code"] = mcp::JsonValue("main_thread_timeout");
  mcp::JsonValue result = make_failure("boom");
  result["structured_error"] = std::move(structured);
  EXPECT_EQ(envelope.normalize_code(result), "main_thread_timeout");
}

TEST(ApiEnvelopeTest, NormalizeCodeAppliesFeatureRules) {
  ApiEnvelope envelope;
  EXPECT_EQ(envelope.normalize_code(make_failure(
                "missing required parameter: source_code")),
            "invalid_argument");
  EXPECT_EQ(envelope.normalize_code(make_failure(
                "unknown parameter for code_execute: bogus")),
            "invalid_argument");
  EXPECT_EQ(
      envelope.normalize_code(make_failure("domain tool 'gda_absent' not found")),
      "tool_not_found");
  EXPECT_EQ(envelope.normalize_code(
                make_failure("editor is exporting; retry after export completes")),
            "export_in_progress");
  EXPECT_EQ(
      envelope.normalize_code(make_failure("main thread timeout after 27000 ms")),
      "main_thread_timeout");
  EXPECT_EQ(
      envelope.normalize_code(make_failure("editor queue unavailable right now")),
      "queue_unavailable");
  EXPECT_EQ(envelope.normalize_code(make_failure(
                "internal error in tool 'gda_probe': boom")),
            "internal_error");
  EXPECT_EQ(
      envelope.normalize_code(make_failure("authorization required for 'x'")),
      "authorization_required");
  EXPECT_EQ(envelope.normalize_code(make_failure("unauthorized request")),
            "unauthorized");
}

TEST(ApiEnvelopeTest, NormalizeCodeFallsBackToToolError) {
  ApiEnvelope envelope;
  EXPECT_EQ(envelope.normalize_code(make_failure("something odd happened")),
            "tool_error");
  mcp::JsonValue empty;
  EXPECT_EQ(envelope.normalize_code(empty), "tool_error");
}

TEST(ApiEnvelopeTest, StatusForCoversEveryKnownCode) {
  ApiEnvelope envelope;
  EXPECT_EQ(envelope.status_for("unauthorized"), 401);
  EXPECT_EQ(envelope.status_for("invalid_argument"), 400);
  EXPECT_EQ(envelope.status_for("tool_not_found"), 404);
  EXPECT_EQ(envelope.status_for("authorization_required"), 403);
  EXPECT_EQ(envelope.status_for("export_in_progress"), 503);
  EXPECT_EQ(envelope.status_for("queue_unavailable"), 503);
  EXPECT_EQ(envelope.status_for("main_thread_timeout"), 504);
  EXPECT_EQ(envelope.status_for("internal_error"), 500);
  EXPECT_EQ(envelope.status_for("tool_error"), 422);
}

TEST(ApiEnvelopeTest, StatusForUnknownCodeIsUnprocessableEntity) {
  ApiEnvelope envelope;
  EXPECT_EQ(envelope.status_for("exec_timeout"), 422);
  EXPECT_EQ(envelope.status_for(""), 422);
}

TEST(ApiEnvelopeTest, BuildLeavesSuccessfulResultUnwrapped) {
  ApiEnvelope envelope;
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["result"] = mcp::JsonValue("executed");
  const mcp::JsonValue built = envelope.build(result, "gda_probe_tool");
  EXPECT_EQ(built.Find("code"), nullptr);
  EXPECT_EQ(built.Find("warnings"), nullptr);
  EXPECT_EQ(built.Find("tool"), nullptr);
  const mcp::JsonValue *value = built.Find("result");
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsString());
  EXPECT_EQ(value->GetString(), "executed");
}

TEST(ApiEnvelopeTest, BuildKeepsOriginalErrorTextAndAddsStableCode) {
  ApiEnvelope envelope;
  const std::string message = "missing required parameter: source_code";
  const mcp::JsonValue built =
      envelope.build(make_failure(message), "gda_probe_tool");
  const mcp::JsonValue *error = built.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_EQ(error->GetString(), message);
  const mcp::JsonValue *code = built.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "invalid_argument");
  const mcp::JsonValue *warnings = built.Find("warnings");
  ASSERT_NE(warnings, nullptr);
  EXPECT_TRUE(warnings->IsArray());
  EXPECT_TRUE(warnings->Empty());
  const mcp::JsonValue *tool = built.Find("tool");
  ASSERT_NE(tool, nullptr);
  ASSERT_TRUE(tool->IsString());
  EXPECT_EQ(tool->GetString(), "gda_probe_tool");
}

TEST(ApiEnvelopeTest, BuildPreservesExtraFieldsOfFailedResult) {
  ApiEnvelope envelope;
  mcp::JsonValue result = make_failure("invalid parameter: probe");
  result["parameter"] = mcp::JsonValue("probe");
  const mcp::JsonValue built = envelope.build(result, "gda_probe_tool");
  const mcp::JsonValue *parameter = built.Find("parameter");
  ASSERT_NE(parameter, nullptr);
  ASSERT_TRUE(parameter->IsString());
  EXPECT_EQ(parameter->GetString(), "probe");
}

TEST(ApiEnvelopeTest, BuildKeepsExistingCodeWhenAlreadyPresent) {
  ApiEnvelope envelope;
  mcp::JsonValue result = make_failure("the editor queue is unavailable");
  result["code"] = mcp::JsonValue("queue_unavailable");
  const mcp::JsonValue built = envelope.build(result, "gda_probe_tool");
  const mcp::JsonValue *code = built.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "queue_unavailable");
  EXPECT_EQ(envelope.status_for(code->GetString()), 503);
}
