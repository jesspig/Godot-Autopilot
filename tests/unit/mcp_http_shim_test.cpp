#include "core/godot_transport.hpp"
#include "core/http_stream_registry.hpp"
#include "core/http_transport_ext.hpp"
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
using godot_autopilot::mcp_wire::build_event_frame;
using godot_autopilot::mcp_wire::build_response_text;
using godot_autopilot::mcp_wire::is_supported_protocol_version;
using godot_autopilot::mcp_wire::parse_request;
using godot_autopilot::mcp_wire::request_id_key;
using godot_autopilot::mcp_http::evaluate_accept_header;
using godot_autopilot::mcp_http::evaluate_accept_media_type;
using godot_autopilot::mcp_http::evaluate_content_type;
using godot_autopilot::mcp_http::evaluate_protocol_version;
using godot_autopilot::mcp_http::evaluate_session_header;
using godot_autopilot::mcp_http::media_type_accepted;
using godot_autopilot::mcp_http::status_for_jsonrpc_error;
using godot_autopilot::mcp_http::supported_protocol_versions_json;

namespace {

const char *kInitializeBody =
    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{)"
    R"("protocolVersion":"2026-07-28","capabilities":{},)"
    R"("clientInfo":{"name":"gate-probe","version":"1.0"},)"
    R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})";

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

TEST(McpWireTest, BuildsEventFrameInServerSentEventFormat) {
  EXPECT_EQ(build_event_frame("endpoint", "/mcp"),
            "event: endpoint\ndata: /mcp\n\n");
}

TEST(McpWireTest, EscapesNewlinesInsideEventFrameData) {
  EXPECT_EQ(build_event_frame("message", "a\nb"),
            "event: message\ndata: a\ndata: b\n\n");
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

TEST(McpHttpTest, AcceptsBothRequiredMediaTypes) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_accept_header("application/json, text/event-stream");

  EXPECT_TRUE(decision.accepted);
  EXPECT_EQ(decision.status_code, 200);
}

TEST(McpHttpTest, RejectsAcceptHeaderWithoutEventStream) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_accept_header("application/json");

  EXPECT_FALSE(decision.accepted);
  EXPECT_EQ(decision.status_code, 406);
}

TEST(McpHttpTest, RejectsAcceptHeaderWithoutJson) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_accept_header("text/event-stream");

  EXPECT_FALSE(decision.accepted);
  EXPECT_EQ(decision.status_code, 406);
}

TEST(McpHttpTest, AcceptsWildcardAcceptHeader) {
  EXPECT_TRUE(media_type_accepted("*/*", "application/json"));
  EXPECT_TRUE(media_type_accepted("*/*", "text/event-stream"));
  EXPECT_TRUE(media_type_accepted("text/*", "text/event-stream"));
  EXPECT_FALSE(media_type_accepted("text/*", "application/json"));
}

TEST(McpHttpTest, AcceptsSingleMediaTypeForEventStream) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_accept_media_type("text/event-stream", "text/event-stream");

  EXPECT_TRUE(decision.accepted);
}

TEST(McpHttpTest, AcceptsJsonContentType) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_content_type("application/json");

  EXPECT_TRUE(decision.accepted);
}

TEST(McpHttpTest, AcceptsJsonContentTypeWithCharsetParameter) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_content_type("application/json; charset=utf-8");

  EXPECT_TRUE(decision.accepted);
}

TEST(McpHttpTest, RejectsNonJsonContentType) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_content_type("text/plain");

  EXPECT_FALSE(decision.accepted);
  EXPECT_EQ(decision.status_code, 415);
}

TEST(McpHttpTest, AcceptsMissingProtocolVersionHeader) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_protocol_version("");

  EXPECT_TRUE(decision.accepted);
}

TEST(McpHttpTest, AcceptsSupportedProtocolVersionHeader) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_protocol_version("2025-11-25");

  EXPECT_TRUE(decision.accepted);
}

TEST(McpHttpTest, RejectsUnsupportedProtocolVersionHeader) {
  const godot_autopilot::mcp_http::HeaderDecision decision =
      evaluate_protocol_version("1999-01-01");

  EXPECT_FALSE(decision.accepted);
  EXPECT_EQ(decision.status_code, 400);
  EXPECT_NE(decision.message.find("1999-01-01"), std::string::npos);
}

TEST(McpHttpTest, ListsEverySupportedProtocolVersion) {
  const std::string versions = supported_protocol_versions_json();

  EXPECT_NE(versions.find("2024-11-05"), std::string::npos);
  EXPECT_NE(versions.find("2025-03-26"), std::string::npos);
  EXPECT_NE(versions.find("2025-06-18"), std::string::npos);
  EXPECT_NE(versions.find("2025-11-25"), std::string::npos);
  EXPECT_NE(versions.find("2026-07-28"), std::string::npos);
}

TEST(McpHttpTest, IgnoresSessionIdentifierInStatelessMode) {
  EXPECT_EQ(evaluate_session_header(""),
            godot_autopilot::mcp_http::SessionDisposition::kIgnore);
  EXPECT_EQ(evaluate_session_header("session-123"),
            godot_autopilot::mcp_http::SessionDisposition::kIgnore);
}

TEST(McpHttpTest, MapsJsonRpcErrorCodesToHttpStatus) {
  EXPECT_EQ(status_for_jsonrpc_error(-32700), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32600), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32602), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32020), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32022), 400);
  EXPECT_EQ(status_for_jsonrpc_error(-32601), 404);
  EXPECT_EQ(status_for_jsonrpc_error(-32000), 200);
}

TEST(McpHttpTest, BuildsUnsupportedVersionBodyWithSupportedList) {
  const std::string body =
      godot_autopilot::mcp_http::build_unsupported_version_body("1999-01-01");

  EXPECT_NE(body.find("\"code\":-32022"), std::string::npos);
  EXPECT_NE(body.find("\"requested\":\"1999-01-01\""), std::string::npos);
  EXPECT_NE(body.find("\"supported\""), std::string::npos);
  EXPECT_NE(body.find("\"id\":null"), std::string::npos);
}

TEST(McpHttpTest, BuildsErrorBodyWithNullIdentifier) {
  const std::string body =
      godot_autopilot::mcp_http::build_error_body(-32600, "invalid request");

  EXPECT_NE(body.find("\"code\":-32600"), std::string::npos);
  EXPECT_NE(body.find("\"message\":\"invalid request\""), std::string::npos);
  EXPECT_NE(body.find("\"id\":null"), std::string::npos);
}

TEST(McpHttpShimTest, ValidatesProtocolVersionHeader) {
  const McpHttpShim shim;

  EXPECT_TRUE(shim.validate_protocol_version(""));
  EXPECT_TRUE(shim.validate_protocol_version("2025-03-26"));
  EXPECT_FALSE(shim.validate_protocol_version("2000-01-01"));
}

TEST(McpHttpShimTest, RejectsUnknownMethod) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "PUT";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 405);
}

TEST(McpHttpShimTest, RejectsUnparsableBody) {
  const McpHttpShim shim;
  HttpRequest request = make_post_request("not-json");
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 400);
  EXPECT_NE(response.body.find("\"error\""), std::string::npos);
}

TEST(McpHttpShimTest, RejectsPostWithoutEventStreamAccept) {
  const McpHttpShim shim;
  HttpRequest request = make_post_request("{}");
  request.headers["accept"] = "application/json";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 406);
}

TEST(McpHttpShimTest, RejectsPostWithUnsupportedContentType) {
  const McpHttpShim shim;
  HttpRequest request = make_post_request("{}");
  request.headers["content-type"] = "text/plain";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 415);
}

TEST(McpHttpShimTest, RejectsPostWithUnsupportedProtocolVersion) {
  const McpHttpShim shim;
  HttpRequest request = make_post_request("{}");
  request.headers["mcp-protocol-version"] = "1999-01-01";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 400);
  EXPECT_NE(response.body.find("\"supported\""), std::string::npos);
}

TEST(McpHttpShimTest, RejectsDeleteWithoutSession) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "DELETE";
  request.path = "/mcp";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 405);
}

TEST(McpHttpShimTest, RejectsEventStreamWithoutConnectionIdentifier) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "GET";
  request.path = "/mcp";
  request.headers["accept"] = "text/event-stream";
  HttpResponse response;

  EXPECT_FALSE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 400);
}

TEST(McpHttpShimTest, ServesEventStreamForGetRequest) {
  const McpHttpShim shim;
  HttpRequest request;
  request.method = "GET";
  request.path = "/mcp";
  request.headers["accept"] = "text/event-stream";
  request.headers[godot_autopilot::kConnectionIdHeader] = "unit-test-stream";
  HttpResponse response;

  EXPECT_TRUE(shim.handle(request, response));
  EXPECT_EQ(response.status_code, 200);
  EXPECT_EQ(response.headers.at("Content-Type"), "text/event-stream");
  EXPECT_EQ(response.body, "event: endpoint\ndata: /mcp\n\n");
  godot_autopilot::http_stream::release("unit-test-stream");
}
