#include "core/transport/godot_transport.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <variant>

using godot_autopilot::GodotTransport;
using godot_autopilot::mcp_wire::make_pending_key;
using godot_autopilot::mcp_wire::parse_request;
using godot_autopilot::mcp_wire::request_id_key;

namespace {

mcp::JsonRpcMessage take_channel_message(GodotTransport &transport) {
  mcp::JsonRpcMessage message;
  transport.GetMessageChannel().AsyncReceive(
      [&](std::error_code ec, mcp::JsonRpcMessage received) {
        EXPECT_FALSE(ec);
        message = std::move(received);
      });
  return message;
}

} // namespace

TEST(GodotTransportPendingTest, MakePendingKeySeparatesConnectionAndId) {
  const std::string key_a = make_pending_key("conn-1", "i:7");
  const std::string key_b = make_pending_key("conn-2", "i:7");

  EXPECT_NE(key_a, key_b);
  EXPECT_NE(key_a.find("conn-1"), std::string::npos);
  EXPECT_NE(key_a.find("i:7"), std::string::npos);
}

TEST(GodotTransportPendingTest, MakePendingKeyStableForSameInputs) {
  EXPECT_EQ(make_pending_key("conn-1", "i:7"), make_pending_key("conn-1", "i:7"));
  EXPECT_EQ(make_pending_key("conn-1", "s:abc"),
            make_pending_key("conn-1", "s:abc"));
  EXPECT_NE(make_pending_key("conn-1", "i:7"), make_pending_key("conn-1", "i:8"));
}

TEST(GodotTransportPendingTest, RequestIdKeyDistinguishesIntAndString) {
  EXPECT_EQ(request_id_key(mcp::RequestId{int64_t{7}}), "i:7");
  EXPECT_EQ(request_id_key(mcp::RequestId{std::string("i:7")}), "s:i:7");
  EXPECT_NE(request_id_key(mcp::RequestId{int64_t{7}}),
            request_id_key(mcp::RequestId{std::string("7")}));
}

TEST(GodotTransportPendingTest, CrossConnectionSameIdBothAccepted) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);
  ASSERT_TRUE(parsed.is_request);

  std::string error;
  EXPECT_TRUE(transport.submit(parsed, "conn-1", error));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(transport.submit(parsed, "conn-2", error));
  EXPECT_TRUE(error.empty());
}

TEST(GodotTransportPendingTest, SameConnectionDuplicateIdRejected) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  EXPECT_FALSE(transport.submit(parsed, "conn-1", error));
  EXPECT_NE(error.find("request id already in flight"), std::string::npos);
  EXPECT_NE(error.find("conn-1"), std::string::npos);
}

TEST(GodotTransportPendingTest, GracePeriodSameConnectionIdRejected) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));

  mcp::JsonRpcMessage message;
  EXPECT_FALSE(transport.await_response("conn-1", parsed.request_id, message,
                                        std::chrono::milliseconds(0)));

  EXPECT_FALSE(transport.submit(parsed, "conn-1", error));
  EXPECT_NE(error.find("request id already in flight"), std::string::npos);
  EXPECT_NE(error.find("conn-1"), std::string::npos);
}

TEST(GodotTransportPendingTest, ResponseRoutesToSubmittingConnection) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));

  const auto rewritten = take_channel_message(transport);
  const auto *request = std::get_if<mcp::JsonRpcRequest>(&rewritten);
  ASSERT_NE(request, nullptr);

  mcp::JsonRpcResponse response;
  response.id = request->id;
  response.result = mcp::JsonValue(mcp::JsonValue::object_tag);
  transport.SendMessageAsync(mcp::JsonRpcMessage(response));

  mcp::JsonRpcMessage message;
  ASSERT_TRUE(transport.await_response("conn-1", parsed.request_id, message,
                                       std::chrono::milliseconds(100)));
  const auto *delivered = std::get_if<mcp::JsonRpcResponse>(&message);
  ASSERT_NE(delivered, nullptr);
  EXPECT_EQ(std::get<int64_t>(delivered->id), int64_t{2});
}

TEST(GodotTransportPendingTest, OutOfOrderResponsesRouteByInternalId) {
  GodotTransport transport;
  const auto parsed_a =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  const auto parsed_b = parse_request(
      R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"probe"}})");
  ASSERT_TRUE(parsed_a.ok);
  ASSERT_TRUE(parsed_b.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed_a, "conn-1", error));
  ASSERT_TRUE(transport.submit(parsed_b, "conn-2", error));

  const auto message_a = take_channel_message(transport);
  const auto message_b = take_channel_message(transport);
  const auto *request_a = std::get_if<mcp::JsonRpcRequest>(&message_a);
  const auto *request_b = std::get_if<mcp::JsonRpcRequest>(&message_b);
  ASSERT_NE(request_a, nullptr);
  ASSERT_NE(request_b, nullptr);
  ASSERT_EQ(request_a->method, "tools/list");
  ASSERT_EQ(request_b->method, "tools/call");
  const auto internal_a = std::get<int64_t>(request_a->id);
  const auto internal_b = std::get<int64_t>(request_b->id);
  EXPECT_NE(internal_a, internal_b);

  mcp::JsonRpcResponse response_b;
  response_b.id = mcp::RequestId{internal_b};
  response_b.result = mcp::JsonValue("result-b");
  transport.SendMessageAsync(mcp::JsonRpcMessage(response_b));
  mcp::JsonRpcResponse response_a;
  response_a.id = mcp::RequestId{internal_a};
  response_a.result = mcp::JsonValue("result-a");
  transport.SendMessageAsync(mcp::JsonRpcMessage(response_a));

  mcp::JsonRpcMessage delivered_a;
  ASSERT_TRUE(transport.await_response("conn-1", parsed_a.request_id,
                                       delivered_a,
                                       std::chrono::milliseconds(100)));
  const auto *result_a = std::get_if<mcp::JsonRpcResponse>(&delivered_a);
  ASSERT_NE(result_a, nullptr);
  EXPECT_EQ(std::get<int64_t>(result_a->id), int64_t{2});
  EXPECT_EQ(result_a->result.GetString(), "result-a");

  mcp::JsonRpcMessage delivered_b;
  ASSERT_TRUE(transport.await_response("conn-2", parsed_b.request_id,
                                       delivered_b,
                                       std::chrono::milliseconds(100)));
  const auto *result_b = std::get_if<mcp::JsonRpcResponse>(&delivered_b);
  ASSERT_NE(result_b, nullptr);
  EXPECT_EQ(std::get<int64_t>(result_b->id), int64_t{2});
  EXPECT_EQ(result_b->result.GetString(), "result-b");
}

TEST(GodotTransportPendingTest, LateResponseForExpiredRequestDropped) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  const auto first = take_channel_message(transport);
  const auto *first_request = std::get_if<mcp::JsonRpcRequest>(&first);
  ASSERT_NE(first_request, nullptr);
  const auto internal_a = std::get<int64_t>(first_request->id);

  mcp::JsonRpcMessage expired_message;
  EXPECT_FALSE(transport.await_response("conn-1", parsed.request_id,
                                        expired_message,
                                        std::chrono::milliseconds(0)));

  const auto cancellation = take_channel_message(transport);
  ASSERT_TRUE(std::holds_alternative<mcp::JsonRpcNotification>(cancellation));

  ASSERT_TRUE(transport.submit(parsed, "conn-2", error));
  const auto second = take_channel_message(transport);
  const auto *second_request = std::get_if<mcp::JsonRpcRequest>(&second);
  ASSERT_NE(second_request, nullptr);
  const auto internal_b = std::get<int64_t>(second_request->id);
  EXPECT_NE(internal_a, internal_b);

  mcp::JsonRpcResponse late_response;
  late_response.id = mcp::RequestId{internal_a};
  late_response.result = mcp::JsonValue("late-a");
  transport.SendMessageAsync(mcp::JsonRpcMessage(late_response));

  mcp::JsonRpcResponse response_b;
  response_b.id = mcp::RequestId{internal_b};
  response_b.result = mcp::JsonValue("result-b");
  transport.SendMessageAsync(mcp::JsonRpcMessage(response_b));

  mcp::JsonRpcMessage delivered_b;
  ASSERT_TRUE(transport.await_response("conn-2", parsed.request_id,
                                       delivered_b,
                                       std::chrono::milliseconds(100)));
  const auto *result_b = std::get_if<mcp::JsonRpcResponse>(&delivered_b);
  ASSERT_NE(result_b, nullptr);
  EXPECT_EQ(std::get<int64_t>(result_b->id), int64_t{2});
  EXPECT_EQ(result_b->result.GetString(), "result-b");

  mcp::JsonRpcMessage delivered_a;
  EXPECT_FALSE(transport.await_response("conn-1", parsed.request_id,
                                        delivered_a,
                                        std::chrono::milliseconds(0)));
}

TEST(GodotTransportPendingTest, CancelNotificationRewritesRequestId) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  const auto request_message = take_channel_message(transport);
  const auto *request = std::get_if<mcp::JsonRpcRequest>(&request_message);
  ASSERT_NE(request, nullptr);
  const auto internal_id = std::get<int64_t>(request->id);

  const auto cancel = parse_request(
      R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":2}})");
  ASSERT_TRUE(cancel.ok);
  ASSERT_TRUE(cancel.is_notification);
  ASSERT_TRUE(transport.submit(cancel, "conn-1", error));

  const auto cancel_message = take_channel_message(transport);
  const auto *notification =
      std::get_if<mcp::JsonRpcNotification>(&cancel_message);
  ASSERT_NE(notification, nullptr);
  ASSERT_TRUE(notification->params.has_value());
  const mcp::JsonValue *target = notification->params->Find("requestId");
  ASSERT_NE(target, nullptr);
  ASSERT_TRUE(target->IsInt());
  EXPECT_EQ(target->GetInt(), internal_id);
}

TEST(GodotTransportPendingTest, CancelNotificationWithoutPendingPassesThrough) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  take_channel_message(transport);

  const auto cancel = parse_request(
      R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{"requestId":2}})");
  ASSERT_TRUE(cancel.ok);
  ASSERT_TRUE(transport.submit(cancel, "conn-2", error));

  const auto cancel_message = take_channel_message(transport);
  const auto *notification =
      std::get_if<mcp::JsonRpcNotification>(&cancel_message);
  ASSERT_NE(notification, nullptr);
  ASSERT_TRUE(notification->params.has_value());
  const mcp::JsonValue *target = notification->params->Find("requestId");
  ASSERT_NE(target, nullptr);
  ASSERT_TRUE(target->IsInt());
  EXPECT_EQ(target->GetInt(), int64_t{2});
}

TEST(GodotTransportPendingTest, TimeoutSendsCancellationWithInternalRequestId) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":"client-7","method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  const auto request_message = take_channel_message(transport);
  const auto *request = std::get_if<mcp::JsonRpcRequest>(&request_message);
  ASSERT_NE(request, nullptr);
  const auto internal_id = std::get<int64_t>(request->id);

  mcp::JsonRpcMessage response;
  bool cancellation_sent = false;
  EXPECT_FALSE(transport.await_response(
      "conn-1", parsed.request_id, response,
      std::chrono::milliseconds(0), &cancellation_sent));
  EXPECT_TRUE(cancellation_sent);

  const auto cancel_message = take_channel_message(transport);
  const auto *notification =
      std::get_if<mcp::JsonRpcNotification>(&cancel_message);
  ASSERT_NE(notification, nullptr);
  EXPECT_EQ(notification->method, "notifications/cancelled");
  ASSERT_TRUE(notification->params.has_value());
  const mcp::JsonValue *target = notification->params->Find("requestId");
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->GetInt(), internal_id);
  const mcp::JsonValue *reason = notification->params->Find("reason");
  ASSERT_NE(reason, nullptr);
  EXPECT_EQ(reason->GetString(), "timeout");
}

TEST(GodotTransportPendingTest, ClientRequestIdTextMapsInternalKey) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  const auto parsed_string_id =
      parse_request(R"({"jsonrpc":"2.0","id":"abc","method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);
  ASSERT_TRUE(parsed_string_id.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  ASSERT_TRUE(transport.submit(parsed_string_id, "conn-1", error));

  const auto message = take_channel_message(transport);
  const auto *request = std::get_if<mcp::JsonRpcRequest>(&message);
  ASSERT_NE(request, nullptr);
  const std::string internal_key = request_id_key(request->id);
  EXPECT_EQ(transport.client_request_id_text(internal_key), "2");

  const auto string_message = take_channel_message(transport);
  const auto *string_request = std::get_if<mcp::JsonRpcRequest>(&string_message);
  ASSERT_NE(string_request, nullptr);
  const std::string string_internal_key = request_id_key(string_request->id);
  EXPECT_EQ(transport.client_request_id_text(string_internal_key), "abc");

  EXPECT_EQ(transport.client_request_id_text("i:999999"), "999999");
}

TEST(GodotTransportPendingTest, ErrorResponseRestoresClientId) {
  GodotTransport transport;
  const auto parsed =
      parse_request(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  ASSERT_TRUE(parsed.ok);

  std::string error;
  ASSERT_TRUE(transport.submit(parsed, "conn-1", error));
  const auto rewritten = take_channel_message(transport);
  const auto *request = std::get_if<mcp::JsonRpcRequest>(&rewritten);
  ASSERT_NE(request, nullptr);

  mcp::JsonRpcErrorResponse failure;
  failure.id = request->id;
  failure.error.code = mcp::McpErrorCode::InternalError;
  failure.error.message = "probe failure";
  transport.SendMessageAsync(mcp::JsonRpcMessage(failure));

  mcp::JsonRpcMessage message;
  ASSERT_TRUE(transport.await_response("conn-1", parsed.request_id, message,
                                       std::chrono::milliseconds(100)));
  const auto *delivered = std::get_if<mcp::JsonRpcErrorResponse>(&message);
  ASSERT_NE(delivered, nullptr);
  ASSERT_TRUE(delivered->id.has_value());
  EXPECT_EQ(std::get<int64_t>(*delivered->id), int64_t{2});
  EXPECT_EQ(delivered->error.message, "probe failure");
}
