#include "runtime/game_response_envelope.hpp"

#include <gtest/gtest.h>
#include <mcp/JsonValue.hpp>
#include <string>
#include <utility>

namespace {

using godot_autopilot::runtime::response_envelope::carry_transport_fields;
using mcp::JsonValue;

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
