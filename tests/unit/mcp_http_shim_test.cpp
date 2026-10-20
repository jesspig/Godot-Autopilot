#include "core/godot_transport.hpp"
#include "core/mcp_http_shim.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>

using godot_autopilot::HttpRequest;
using godot_autopilot::HttpResponse;
using godot_autopilot::McpHttpShim;
using godot_autopilot::mcp_wire::ParsedRequest;
using godot_autopilot::mcp_wire::build_error_response_text;
using godot_autopilot::mcp_wire::build_response_text;
using godot_autopilot::mcp_wire::is_supported_protocol_version;
using godot_autopilot::mcp_wire::parse_request;
using godot_autopilot::mcp_wire::request_id_key;

namespace {

const char *kInitializeBody =
    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{)"
    R"("protocolVersion":"2026-07-28","capabilities":{},)"
    R"("clientInfo":{"name":"gate-probe","version":"1.0"},)"
    R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

} // namespace

TEST(McpWireTest, ParsesInitializeRequestIntoDispatchableMessage) {
  const ParsedRequest parsed = parse_request(kInitializeBody);

  ASSERT_TRUE(parsed.ok);
  EXPECT_TRUE(parsed.is_request);
  EXPECT_FALSE(parsed.is_notification);
  EXPECT_EQ(parsed.method, "initialize");
  EXPECT_EQ(parsed.request_id, "i:1");
  ASSERT_TRUE(mcp::IsRequest(parsed.message));
  const mcp::JsonRpcRequest *request = mcp::AsRequest(parsed.message);
  ASSERT_NE(request, nullptr);
  EXPECT_EQ(request->method, "initialize");
  EXPECT_EQ(request->id, mcp::RequestId{int64_t{1}});
  ASSERT_TRUE(request->params.has_value());
  ASSERT_TRUE(request->params->IsObject());
  EXPECT_TRUE(request->params->Contains("protocolVersion"));
  EXPECT_TRUE(request->params->Contains("clientInfo"));
}

TEST(McpWireTest, ParsesNotificationWithoutRequestId) {
  const ParsedRequest parsed =
      parse_request(R"({"jsonrpc":"2.0","method":"notifications/initialized"})");

  ASSERT_TRUE(parsed.ok);
  EXPECT_FALSE(parsed.is_request);
  EXPECT_TRUE(parsed.is_notification);
  EXPECT_EQ(parsed.method, "notifications/initialized");
  EXPECT_TRUE(parsed.request_id.empty());
}

TEST(McpWireTest, RejectsMalformedBody) {
  const ParsedRequest parsed = parse_request("{\"jsonrpc\":");

  EXPECT_FALSE(parsed.ok);
  EXPECT_FALSE(parsed.error.empty());
}

TEST(McpWireTest, RejectsBodyThatIsNotAClientRequest) {
  const ParsedRequest parsed =
      parse_request(R"({"jsonrpc":"2.0","id":1,"result":{"tools":[]}})");

  EXPECT_FALSE(parsed.ok);
  EXPECT_FALSE(parsed.error.empty());
}

TEST(McpWireTest, BuildsResponseTextFromJsonRpcMessage) {
  mcp::JsonRpcResponse response;
  response.id = mcp::RequestId{int64_t{7}};
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["ok"] = mcp::JsonValue(true);
  response.result = std::move(result);
  const mcp::JsonRpcMessage message{std::move(response)};

  EXPECT_EQ(build_response_text(message),
            R"({"id":7,"jsonrpc":"2.0","result":{"ok":true}})");
}

TEST(McpWireTest, BuildsErrorResponseText) {
  EXPECT_EQ(build_error_response_text(-32700, "Parse error"),
            R"({"error":{"code":-32700,"message":"Parse error"},"jsonrpc":"2.0"})");
}

TEST(McpWireTest, MapsRequestIdToStableKey) {
  EXPECT_EQ(request_id_key(mcp::RequestId{int64_t{12}}), "i:12");
  EXPECT_EQ(request_id_key(mcp::RequestId{std::string("abc")}), "s:abc");
}

TEST(McpWireTest, AcceptsOnlyKnownProtocolVersions) {
  EXPECT_TRUE(is_supported_protocol_version(""));
  EXPECT_TRUE(is_supported_protocol_version("2025-06-18"));
  EXPECT_TRUE(is_supported_protocol_version("2026-07-28"));
  EXPECT_FALSE(is_supported_protocol_version("1999-01-01"));
}

TEST(McpHttpShimTest, ValidatesProtocolVersionHeader) {
  const McpHttpShim shim;

  EXPECT_TRUE(shim.validate_protocol_version(""));
  EXPECT_TRUE(shim.validate_protocol_version("2025-03-26"));
  EXPECT_FALSE(shim.validate_protocol_version("2000-01-01"));
}

TEST(McpHttpShimTest, RejectsNonPostRequest) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "GET";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 405);
}

TEST(McpHttpShimTest, RejectsUnparsableBody) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "POST";
  request.body = "not-json";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 400);
  EXPECT_NE(response.body.find("\"error\""), std::string::npos);
}
