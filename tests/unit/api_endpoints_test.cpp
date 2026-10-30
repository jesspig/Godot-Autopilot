#include "core/api_endpoints.hpp"
#include "core/api_surface.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/project_store.hpp"

namespace {

using godot_autopilot::EndpointEntry;
using godot_autopilot::api_endpoints::EndpointInfo;
using godot_autopilot::api_surface::ApiReply;

constexpr const char *kToken = "endpoints-token";

EndpointInfo make_info() {
  EndpointInfo info;
  info.instance_id = "gda-probe-4242-1";
  info.version = "9.9.9";
  info.pid = 4242;
  info.project_path = "/tmp/gda-probe";
  info.mcp = EndpointEntry{"127.0.0.1", 9527, "/mcp", "mcp", "none"};
  info.api = EndpointEntry{"127.0.0.1", 9527, "/api", "http", "bearer"};
  return info;
}

std::unordered_map<std::string, std::string> bearer_headers() {
  std::unordered_map<std::string, std::string> headers;
  headers["authorization"] = std::string("Bearer ") + kToken;
  return headers;
}

class ApiEndpointsTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::api_endpoints::register_endpoint_info_provider(
        []() { return make_info(); });
    godot_autopilot::api_surface::register_token_verifier(
        [](const std::unordered_map<std::string, std::string> &headers) {
          const auto found = headers.find("authorization");
          return found != headers.end() &&
                 found->second == std::string("Bearer ") + kToken;
        });
  }

  void TearDown() override {
    godot_autopilot::api_endpoints::register_endpoint_info_provider(nullptr);
    godot_autopilot::api_surface::register_token_verifier(nullptr);
  }
};

} // namespace

TEST_F(ApiEndpointsTest, TheRestAndMcpSourcesShareTheSameTable) {
  const std::string mcp_payload =
      godot_autopilot::api_endpoints::endpoint_document_json();

  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/endpoints", bearer_headers(), "");

  ASSERT_EQ(reply.status, 200);
  EXPECT_EQ(reply.body.Dump(), mcp_payload);

  const mcp::JsonValue rest = reply.body;
  const mcp::JsonValue mcp_doc = mcp::JsonValue::Parse(mcp_payload);

  const mcp::JsonValue *rest_instance = rest.Find("instance_id");
  const mcp::JsonValue *mcp_instance = mcp_doc.Find("instance_id");
  ASSERT_NE(rest_instance, nullptr);
  ASSERT_NE(mcp_instance, nullptr);
  EXPECT_EQ(rest_instance->GetString(), mcp_instance->GetString());
  EXPECT_EQ(rest_instance->GetString(), "gda-probe-4242-1");

  const mcp::JsonValue *rest_version = rest.Find("version");
  const mcp::JsonValue *mcp_version = mcp_doc.Find("version");
  ASSERT_NE(rest_version, nullptr);
  ASSERT_NE(mcp_version, nullptr);
  EXPECT_EQ(rest_version->GetString(), mcp_version->GetString());
  EXPECT_EQ(rest_version->GetString(), "9.9.9");

  const mcp::JsonValue *rest_pid = rest.Find("pid");
  const mcp::JsonValue *mcp_pid = mcp_doc.Find("pid");
  ASSERT_NE(rest_pid, nullptr);
  ASSERT_NE(mcp_pid, nullptr);
  EXPECT_EQ(rest_pid->GetInt(), mcp_pid->GetInt());
  EXPECT_EQ(rest_pid->GetInt(), 4242);

  const mcp::JsonValue *rest_project = rest.Find("project_path");
  const mcp::JsonValue *mcp_project = mcp_doc.Find("project_path");
  ASSERT_NE(rest_project, nullptr);
  ASSERT_NE(mcp_project, nullptr);
  EXPECT_EQ(rest_project->GetString(), mcp_project->GetString());
  EXPECT_EQ(rest_project->GetString(), "/tmp/gda-probe");

  const mcp::JsonValue *rest_endpoints = rest.Find("endpoints");
  const mcp::JsonValue *mcp_endpoints = mcp_doc.Find("endpoints");
  ASSERT_NE(rest_endpoints, nullptr);
  ASSERT_NE(mcp_endpoints, nullptr);
  ASSERT_TRUE(rest_endpoints->IsArray());
  ASSERT_TRUE(mcp_endpoints->IsArray());
  ASSERT_EQ(rest_endpoints->GetArray().size(), 2u);
  ASSERT_EQ(mcp_endpoints->GetArray().size(), 2u);

  const char *names[] = {"mcp", "api"};
  const char *paths[] = {"/mcp", "/api"};
  const char *protocols[] = {"mcp", "http"};
  const char *auths[] = {"none", "bearer"};
  for (size_t index = 0; index < 2; ++index) {
    const mcp::JsonValue &rest_entry = rest_endpoints->GetArray()[index];
    const mcp::JsonValue &mcp_entry = mcp_endpoints->GetArray()[index];
    EXPECT_EQ(rest_entry.Find("name")->GetString(),
              mcp_entry.Find("name")->GetString());
    EXPECT_EQ(rest_entry.Find("name")->GetString(), names[index]);
    EXPECT_EQ(rest_entry.Find("host")->GetString(),
              mcp_entry.Find("host")->GetString());
    EXPECT_EQ(rest_entry.Find("host")->GetString(), "127.0.0.1");
    EXPECT_EQ(rest_entry.Find("port")->GetInt(),
              mcp_entry.Find("port")->GetInt());
    EXPECT_EQ(rest_entry.Find("port")->GetInt(), 9527);
    EXPECT_EQ(rest_entry.Find("path")->GetString(),
              mcp_entry.Find("path")->GetString());
    EXPECT_EQ(rest_entry.Find("path")->GetString(), paths[index]);
    EXPECT_EQ(rest_entry.Find("protocol")->GetString(),
              mcp_entry.Find("protocol")->GetString());
    EXPECT_EQ(rest_entry.Find("protocol")->GetString(), protocols[index]);
    EXPECT_EQ(rest_entry.Find("auth")->GetString(),
              mcp_entry.Find("auth")->GetString());
    EXPECT_EQ(rest_entry.Find("auth")->GetString(), auths[index]);
  }
}

TEST_F(ApiEndpointsTest, TheEndpointTableRequiresAToken) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/endpoints", {}, "");

  EXPECT_EQ(reply.status, 401);
  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "unauthorized");
}

TEST_F(ApiEndpointsTest, TheEndpointRouteIsAdvertised) {
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/api/endpoints")
                .kind,
            godot_autopilot::api_surface::RouteKind::kEndpoints);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("POST", "/api/endpoints")
                .kind,
            godot_autopilot::api_surface::RouteKind::kMethodNotAllowed);

  const ApiReply index = godot_autopilot::api_surface::handle_request(
      "GET", "/api", bearer_headers(), "");
  ASSERT_EQ(index.status, 200);
  const mcp::JsonValue *endpoints = index.body.Find("endpoints");
  ASSERT_NE(endpoints, nullptr);
  ASSERT_TRUE(endpoints->IsArray());
  bool found = false;
  for (const mcp::JsonValue &entry : endpoints->GetArray()) {
    const mcp::JsonValue *path = entry.Find("path");
    if (path != nullptr && path->GetString() == "/api/endpoints") {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}
