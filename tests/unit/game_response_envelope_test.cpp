#include "runtime/game_response_envelope.hpp"

#include "tools/base/debugger_access.hpp"
#include "tools/base/runtime_ops.hpp"

#include <gtest/gtest.h>
#include "util/json_value.hpp"
#include <string>
#include <utility>

namespace {

using godot_autopilot::build_cancel_request_body;
using godot_autopilot::runtime::response_envelope::carry_transport_fields;
using godot_autopilot::runtime_ops::build_game_timeout_receipt;
using godot_autopilot::runtime_ops::GameTimeoutReceiptInput;
using godot_autopilot::util::JsonValue;

GameTimeoutReceiptInput eval_timeout_input() {
  GameTimeoutReceiptInput input;
  input.op = "eval";
  input.request_id = 406;
  input.timeout_ms = 7000;
  input.session_count = 1;
  input.pending_count = 1;
  return input;
}

bool game_cancel_contract_accepts(const JsonValue &request) {
  const JsonValue *params = request.Find("params");
  if (params == nullptr || !params->IsObject())
    return false;
  const JsonValue *rid = params->Find("request_id");
  return rid != nullptr && rid->IsInt();
}

} // namespace

TEST(GameResponseEnvelopeTest, ScalarPayloadGainsWrappedBesideResult) {
  const JsonValue game_body =
      JsonValue::Parse(R"({"result":2,"data":2,"wrapped":true,"ok":true})");
  const JsonValue payload =
      carry_transport_fields(game_body, JsonValue::Parse("2"));

  ASSERT_TRUE(payload.IsObject());
  ASSERT_NE(payload.Find("result"), nullptr);
  EXPECT_TRUE(payload.Find("result")->IsInt());
  EXPECT_EQ(payload.Find("result")->GetInt(), 2);
  ASSERT_NE(payload.Find("wrapped"), nullptr);
  EXPECT_TRUE(payload.Find("wrapped")->IsBool());
  EXPECT_TRUE(payload.Find("wrapped")->GetBool());
  EXPECT_EQ(payload.Find("data"), nullptr);
  EXPECT_EQ(payload.Find("ok"), nullptr);
}

TEST(GameResponseEnvelopeTest, ArrayPayloadKeepsListUnderResultAndGainsNextSeq) {
  const JsonValue game_body = JsonValue::Parse(
      R"({"result":[{"seq":1},{"seq":2}],"next_seq":2,"ok":true})");
  const JsonValue payload =
      carry_transport_fields(game_body, JsonValue::Parse(R"([{"seq":1},{"seq":2}])"));

  ASSERT_TRUE(payload.IsObject());
  const JsonValue *list = payload.Find("result");
  ASSERT_NE(list, nullptr);
  ASSERT_TRUE(list->IsArray());
  EXPECT_EQ(list->Size(), 2u);
  EXPECT_EQ((*list)[static_cast<size_t>(1)].Find("seq")->GetInt(), 2);
  ASSERT_NE(payload.Find("next_seq"), nullptr);
  EXPECT_EQ(payload.Find("next_seq")->GetInt(), 2);
  EXPECT_EQ(payload.Find("ok"), nullptr);
}

TEST(GameResponseEnvelopeTest, ObjectPayloadMergesFieldsWithoutRewritingIt) {
  const JsonValue game_body = JsonValue::Parse(
      R"({"result":{"checked":3,"possibly_occluded_count":2},"wrapped":true})");
  const JsonValue payload = carry_transport_fields(
      game_body, JsonValue::Parse(R"({"checked":3,"possibly_occluded_count":2})"));

  ASSERT_TRUE(payload.IsObject());
  EXPECT_EQ(payload.Find("checked")->GetInt(), 3);
  EXPECT_EQ(payload.Find("possibly_occluded_count")->GetInt(), 2);
  EXPECT_TRUE(payload.Find("wrapped")->GetBool());
  EXPECT_EQ(payload.Size(), 3u);
}

TEST(GameResponseEnvelopeTest, PayloadOwnValuesWinOverTransportFields) {
  const JsonValue game_body =
      JsonValue::Parse(R"({"result":{"next_seq":9,"wrapped":false},"next_seq":4})");
  const JsonValue payload = carry_transport_fields(
      game_body, JsonValue::Parse(R"({"next_seq":9,"wrapped":false})"));

  ASSERT_TRUE(payload.IsObject());
  EXPECT_EQ(payload.Find("next_seq")->GetInt(), 9);
  EXPECT_FALSE(payload.Find("wrapped")->GetBool());
}

TEST(GameResponseEnvelopeTest, AbsentTransportFieldsLeavePayloadShapeUntouched) {
  const JsonValue scalar_body = JsonValue::Parse(R"({"result":"triggered"})");
  const JsonValue scalar_payload =
      carry_transport_fields(scalar_body, JsonValue("triggered"));
  ASSERT_TRUE(scalar_payload.IsString());
  EXPECT_EQ(scalar_payload.GetString(), "triggered");

  const JsonValue array_body = JsonValue::Parse(R"({"result":[1,2]})");
  const JsonValue array_payload =
      carry_transport_fields(array_body, JsonValue::Parse("[1,2]"));
  ASSERT_TRUE(array_payload.IsArray());
  EXPECT_EQ(array_payload.Size(), 2u);

  const JsonValue grouped_body =
      JsonValue::Parse(R"({"result":{"grouped":true,"next_seq":1}})");
  const JsonValue grouped_payload = carry_transport_fields(
      grouped_body, JsonValue::Parse(R"({"grouped":true,"next_seq":1})"));
  ASSERT_TRUE(grouped_payload.IsObject());
  EXPECT_EQ(grouped_payload.Size(), 2u);
  EXPECT_EQ(grouped_payload.Find("next_seq")->GetInt(), 1);
}

TEST(GameResponseEnvelopeTest, NonObjectGameBodyReturnsPayloadUnchanged) {
  const JsonValue game_body("not-an-object");
  const JsonValue payload =
      carry_transport_fields(game_body, JsonValue(static_cast<int64_t>(7)));
  ASSERT_TRUE(payload.IsInt());
  EXPECT_EQ(payload.GetInt(), 7);
}

TEST(GameCancelContractTest, CancelBodyCarriesRequestIdInParamsMatchingTopLevel) {
  const JsonValue body = build_cancel_request_body(42);

  ASSERT_TRUE(body.IsObject());
  const JsonValue *top_id = body.Find("request_id");
  ASSERT_NE(top_id, nullptr);
  ASSERT_TRUE(top_id->IsInt());
  EXPECT_EQ(top_id->GetInt(), 42);

  const JsonValue *op = body.Find("op");
  ASSERT_NE(op, nullptr);
  ASSERT_TRUE(op->IsString());
  EXPECT_EQ(op->GetString(), "cancel");

  const JsonValue *params = body.Find("params");
  ASSERT_NE(params, nullptr);
  ASSERT_TRUE(params->IsObject());
  const JsonValue *params_id = params->Find("request_id");
  ASSERT_NE(params_id, nullptr);
  EXPECT_TRUE(params_id->IsInt());
  EXPECT_EQ(params_id->GetInt(), top_id->GetInt());
}

TEST(GameCancelContractTest, CancelRequestIdStaysIntegerAcrossJsonRoundTrip) {
  const JsonValue body = build_cancel_request_body(7001);
  const JsonValue reparsed = JsonValue::Parse(body.Dump());

  ASSERT_TRUE(reparsed.IsObject());
  const JsonValue *params = reparsed.Find("params");
  ASSERT_NE(params, nullptr);
  ASSERT_TRUE(params->IsObject());
  const JsonValue *params_id = params->Find("request_id");
  ASSERT_NE(params_id, nullptr);
  EXPECT_TRUE(params_id->IsInt());
  EXPECT_EQ(params_id->GetInt(), 7001);
  EXPECT_EQ(reparsed.Find("request_id")->GetInt(), 7001);
}

TEST(GameCancelContractTest, CancelParamsSatisfyGameBridgeOpCancelContract) {
  const JsonValue body = build_cancel_request_body(288);
  const JsonValue wire = JsonValue::Parse(body.Dump());
  EXPECT_TRUE(game_cancel_contract_accepts(wire));

  JsonValue legacy_body(JsonValue::object_tag);
  legacy_body["request_id"] = JsonValue(static_cast<int64_t>(288));
  legacy_body["op"] = JsonValue("cancel");
  legacy_body["params"] = JsonValue(JsonValue::object_tag);
  EXPECT_FALSE(game_cancel_contract_accepts(legacy_body));
}

TEST(GameTimeoutReceiptTest, SilentChannelTimeoutIsChannelLost) {
  const GameTimeoutReceiptInput input = eval_timeout_input();
  const JsonValue receipt = build_game_timeout_receipt(input);

  ASSERT_TRUE(receipt.IsObject());
  ASSERT_TRUE(receipt.Contains("error"));
  const JsonValue *details = receipt.Find("structured_error");
  ASSERT_NE(details, nullptr);
  EXPECT_EQ(details->Find("code")->GetString(), "eval_timeout");
  EXPECT_EQ(details->Find("timeout_cause")->GetString(), "channel_lost");
  EXPECT_TRUE(details->Find("game_may_be_busy")->GetBool());
  EXPECT_EQ(details->Find("timeout_ms")->GetInt(), 7000);

  const std::string text = receipt.Find("error")->GetString();
  EXPECT_NE(text.find("no gda:response received on the channel yet"),
            std::string::npos);
  EXPECT_NE(text.find("pending: 1"), std::string::npos);
  EXPECT_EQ(text.find("responses received: "), std::string::npos);
}

TEST(GameTimeoutReceiptTest, LateResponseEvidenceMarksScriptError) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.last_channel_response_age_ms = 120;
  input.late_response_for_request = true;
  const JsonValue receipt = build_game_timeout_receipt(input);

  const JsonValue *details = receipt.Find("structured_error");
  ASSERT_NE(details, nullptr);
  EXPECT_EQ(details->Find("code")->GetString(), "eval_timeout");
  EXPECT_EQ(details->Find("timeout_cause")->GetString(), "script_error");
}

TEST(GameTimeoutReceiptTest, GameErrorStreamDuringWaitMarksScriptError) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.last_channel_response_age_ms = 120;
  input.game_error_during_wait = true;
  const JsonValue receipt = build_game_timeout_receipt(input);

  const JsonValue *details = receipt.Find("structured_error");
  ASSERT_NE(details, nullptr);
  EXPECT_EQ(details->Find("timeout_cause")->GetString(), "script_error");
}

TEST(GameTimeoutReceiptTest, SelfCheckResponseNotCountedForEvalRequest) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.last_channel_response_age_ms = 230;
  input.responses_received = 0;
  const JsonValue receipt = build_game_timeout_receipt(input);

  const JsonValue *details = receipt.Find("structured_error");
  ASSERT_NE(details, nullptr);
  EXPECT_EQ(details->Find("timeout_cause")->GetString(), "unknown");

  const std::string text = receipt.Find("error")->GetString();
  EXPECT_NE(text.find("responses received for this request: 0"),
            std::string::npos);
  EXPECT_NE(text.find("last channel response 230 ms ago"), std::string::npos);
  EXPECT_NE(text.find("pending: 1"), std::string::npos);
}

TEST(GameTimeoutReceiptTest, PerRequestCountAndPendingAreReported) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.last_channel_response_age_ms = 50;
  input.responses_received = 2;
  input.pending_count = 3;
  const JsonValue receipt = build_game_timeout_receipt(input);

  const std::string text = receipt.Find("error")->GetString();
  EXPECT_NE(text.find("responses received for this request: 2"),
            std::string::npos);
  EXPECT_NE(text.find("pending: 3"), std::string::npos);
}

TEST(GameTimeoutReceiptTest, NonEvalOpHasNoEvalTimeoutDetails) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.op = "status";
  const JsonValue receipt = build_game_timeout_receipt(input);

  EXPECT_TRUE(receipt.Contains("error"));
  EXPECT_EQ(receipt.Find("structured_error"), nullptr);
}

TEST(GameTimeoutReceiptTest, LateResultsArrayIsAttached) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  JsonValue late(JsonValue::array_tag);
  JsonValue item(JsonValue::object_tag);
  item["request_id"] = JsonValue(static_cast<int64_t>(406));
  late.PushBack(std::move(item));
  input.late_results = std::move(late);
  const JsonValue receipt = build_game_timeout_receipt(input);

  const JsonValue *late_results = receipt.Find("late_results");
  ASSERT_NE(late_results, nullptr);
  ASSERT_TRUE(late_results->IsArray());
  EXPECT_EQ(late_results->Size(), 1u);
}

TEST(GameTimeoutReceiptTest, QueuedForReadyTimeoutIsStructuredRetryable) {
  GameTimeoutReceiptInput input = eval_timeout_input();
  input.awaiting_ready = true;
  const JsonValue receipt = build_game_timeout_receipt(input);

  ASSERT_TRUE(receipt.Contains("error"));
  const std::string text = receipt.Find("error")->GetString();
  EXPECT_NE(text.find("never reported gda:ready"), std::string::npos);

  const JsonValue *details = receipt.Find("structured_error");
  ASSERT_NE(details, nullptr);
  EXPECT_EQ(details->Find("code")->GetString(), "game_ready_timeout");
  EXPECT_TRUE(details->Find("retryable")->GetBool());
  EXPECT_EQ(details->Find("timeout_ms")->GetInt(), 7000);
}
