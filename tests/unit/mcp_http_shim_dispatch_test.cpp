#include "core/godot_transport.hpp"
#include "core/mcp_http_shim.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::HttpRequest;
using godot_autopilot::HttpResponse;
using godot_autopilot::McpHttpShim;
using godot_autopilot::mcp_http::DispatchKind;
using godot_autopilot::mcp_http::build_accepted_response;
using godot_autopilot::mcp_http::classify_dispatch;
using godot_autopilot::mcp_http::status_for_jsonrpc_error;
using godot_autopilot::mcp_wire::parse_request;

namespace {

const char *kInitializedNotificationBody =
    R"({"jsonrpc":"2.0","method":"notifications/initialized",)"
    R"("params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

const char *kToolsListRequestBody =
    R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})";

HttpRequest make_post_request(const std::string &body) {
  HttpRequest request;
  request.method = "POST";
  request.path = "/mcp";
  request.headers["accept"] = "application/json, text/event-stream";
  request.headers["content-type"] = "application/json";
  request.body = body;
  return request;
}

} // namespace

TEST(McpHttpDispatchTest, ClassifiesInitializedNotificationAsAcceptOnly) {
  const godot_autopilot::mcp_wire::ParsedRequest parsed =
      parse_request(kInitializedNotificationBody);

  ASSERT_TRUE(parsed.ok);
  EXPECT_TRUE(parsed.is_notification);
  EXPECT_TRUE(parsed.request_id.empty());
  EXPECT_EQ(classify_dispatch(parsed), DispatchKind::kNotification);
}

TEST(McpHttpDispatchTest, ClassifiesRequestWithIdentifierAsAwaited) {
  const godot_autopilot::mcp_wire::ParsedRequest parsed =
      parse_request(kToolsListRequestBody);

  ASSERT_TRUE(parsed.ok);
  EXPECT_TRUE(parsed.is_request);
  EXPECT_EQ(parsed.request_id, "i:2");
  EXPECT_EQ(classify_dispatch(parsed), DispatchKind::kRequest);
}

TEST(McpHttpDispatchTest, ClassifiesUnusableBodyAsInvalid) {
  const godot_autopilot::mcp_wire::ParsedRequest parsed =
      parse_request("not-json");

  EXPECT_EQ(classify_dispatch(parsed), DispatchKind::kInvalid);
}

TEST(McpHttpDispatchTest, ClassifiesClientResponseAsInvalid) {
  const godot_autopilot::mcp_wire::ParsedRequest parsed =
      parse_request(R"({"jsonrpc":"2.0","id":1,"result":{"tools":[]}})");

  EXPECT_EQ(classify_dispatch(parsed), DispatchKind::kInvalid);
}

TEST(McpHttpDispatchTest, BuildsEmptyAcceptedResponseForNotification) {
  HttpResponse response;

  build_accepted_response(response);

  EXPECT_EQ(response.status_code, 202);
  EXPECT_EQ(response.status_text, "Accepted");
  EXPECT_TRUE(response.body.empty());
  EXPECT_TRUE(response.headers.find("Content-Type") == response.headers.end());
}

TEST(McpHttpDispatchTest, AcceptsNotificationWithoutAwaitingAnyResponse) {
  const McpHttpShim shim;
  HttpRequest request = make_post_request(kInitializedNotificationBody);
  HttpResponse response;

  EXPECT_TRUE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 202);
  EXPECT_TRUE(response.body.empty());
}

TEST(McpHttpDispatchTest, RejectsBodyThatCarriesNoClientMessage) {
  const McpHttpShim shim;
  HttpRequest request =
      make_post_request(R"({"jsonrpc":"2.0","id":1,"result":{"tools":[]}})");
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 400);
  EXPECT_NE(response.body.find("\"error\""), std::string::npos);
}

TEST(McpHttpDispatchTest, MapsServerSideRejectionToBadRequest) {
  EXPECT_EQ(status_for_jsonrpc_error(-32600), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32601), 404);
  EXPECT_EQ(status_for_jsonrpc_error(-32602), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32000), 200);
}
