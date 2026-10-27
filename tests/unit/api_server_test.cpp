#include "core/api_server.hpp"
#include "core/api_surface.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

using godot_autopilot::ApiServer;
using godot_autopilot::HttpRouteTable;
using godot_autopilot::api_surface::ApiReply;
using godot_autopilot::api_surface::BatchRequest;
using godot_autopilot::api_surface::ListQuery;
using godot_autopilot::api_surface::RouteKind;
using godot_autopilot::api_surface::RouteMatch;
using godot_autopilot::api_surface::ToolEntry;

namespace {

constexpr const char *kToken = "test-token";

ToolEntry make_entry(const std::string &name, const std::string &category,
                      const std::vector<std::string> &tags) {
  ToolEntry entry;
  entry.name = name;
  entry.description = "the " + name + " tool";
  entry.category = category;
  entry.tags = tags;
  entry.side_effect = "none";
  mcp::JsonValue schema(mcp::JsonValue::object_tag);
  schema["type"] = mcp::JsonValue("object");
  entry.input_schema = std::move(schema);
  return entry;
}

std::unordered_map<std::string, std::string> bearer_headers(
    const std::string &token) {
  std::unordered_map<std::string, std::string> headers;
  headers["authorization"] = "Bearer " + token;
  return headers;
}

class ApiSurfaceTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::api_surface::register_tool_lister(
        [this]() { return entries_; });
    godot_autopilot::api_surface::register_tool_executor(
        [this](const std::string &name, const mcp::JsonValue &args) {
          return execute(name, args);
        });
    godot_autopilot::api_surface::register_token_verifier(
        [](const std::unordered_map<std::string, std::string> &headers) {
          const auto found = headers.find("authorization");
          return found != headers.end() && found->second == "Bearer test-token";
        });
    godot_autopilot::api_surface::register_runtime_info_provider([]() {
      godot_autopilot::api_surface::RuntimeInfo info;
      info.version = "9.9.9";
      info.port = 9527;
      info.pid = 4242;
      return info;
    });
  }

  void TearDown() override {
    godot_autopilot::api_surface::register_tool_lister(nullptr);
    godot_autopilot::api_surface::register_tool_executor(nullptr);
    godot_autopilot::api_surface::register_token_verifier(nullptr);
    godot_autopilot::api_surface::register_runtime_info_provider(nullptr);
  }

  mcp::JsonValue execute(const std::string &name, const mcp::JsonValue &args) {
    last_tool = name;
    last_args = args;
    if (fail_next) {
      mcp::JsonValue failure(mcp::JsonValue::object_tag);
      failure["error"] = mcp::JsonValue("the stub tool failed");
      return failure;
    }
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    result["tool"] = mcp::JsonValue(name);
    result["echo"] = args;
    return result;
  }

  std::vector<ToolEntry> entries_;
  std::string last_tool;
  mcp::JsonValue last_args;
  bool fail_next{false};
};

} // namespace

TEST(ApiRouteTest, MatchesTheDocumentedRoutes) {
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/api").kind,
            RouteKind::kIndex);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/api/tools").kind,
            RouteKind::kTools);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/health").kind,
      RouteKind::kHealth);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/api/openapi.json")
                .kind,
            RouteKind::kOpenApi);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("POST", "/api/batch").kind,
      RouteKind::kBatch);
}

TEST(ApiRouteTest, ExtractsTheToolName) {
  const RouteMatch detail =
      godot_autopilot::api_surface::match_route("GET", "/api/tools/scene_open");
  EXPECT_EQ(detail.kind, RouteKind::kToolDetail);
  EXPECT_EQ(detail.tool_name, "scene_open");
  const RouteMatch call =
      godot_autopilot::api_surface::match_route("POST", "/api/tools/scene_open");
  EXPECT_EQ(call.kind, RouteKind::kToolCall);
  EXPECT_EQ(call.tool_name, "scene_open");
}

TEST(ApiRouteTest, RejectsMalformedToolNames) {
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/tools/").kind,
      RouteKind::kNotFound);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/tools/a/b").kind,
      RouteKind::kNotFound);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/api/tools/bad%2Fid")
                .kind,
            RouteKind::kNotFound);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/tools/bad name")
          .kind,
      RouteKind::kNotFound);
}

TEST(ApiRouteTest, ReportsMethodMismatch) {
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("POST", "/api/tools").kind,
      RouteKind::kMethodNotAllowed);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/batch").kind,
      RouteKind::kMethodNotAllowed);
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("POST", "/api/health").kind,
      RouteKind::kMethodNotAllowed);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("DELETE", "/api/tools/x")
                .kind,
            RouteKind::kMethodNotAllowed);
}

TEST(ApiRouteTest, RejectsUnknownRoutes) {
  EXPECT_EQ(
      godot_autopilot::api_surface::match_route("GET", "/api/unknown").kind,
      RouteKind::kNotFound);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/mcp").kind,
            RouteKind::kNotFound);
  EXPECT_EQ(godot_autopilot::api_surface::match_route("GET", "/apifoo").kind,
            RouteKind::kNotFound);
}

TEST(ApiRouteTest, ValidatesToolNames) {
  EXPECT_TRUE(godot_autopilot::api_surface::is_valid_tool_name("scene_open"));
  EXPECT_TRUE(godot_autopilot::api_surface::is_valid_tool_name("code.execute-1"));
  EXPECT_FALSE(godot_autopilot::api_surface::is_valid_tool_name(""));
  EXPECT_FALSE(godot_autopilot::api_surface::is_valid_tool_name("a/b"));
  EXPECT_FALSE(godot_autopilot::api_surface::is_valid_tool_name("a b"));
  EXPECT_FALSE(godot_autopilot::api_surface::is_valid_tool_name(std::string(
      200, 'a')));
}

TEST(ApiListQueryTest, ParsesTheFilterParameters) {
  std::unordered_map<std::string, std::string> params;
  params["query"] = " scene ";
  params["category"] = "Scene";
  params["tags"] = "editing, ui";
  ListQuery query;
  std::string error;
  ASSERT_TRUE(godot_autopilot::api_surface::parse_list_query(params, query,
                                                              error));
  EXPECT_EQ(query.query, "scene");
  EXPECT_EQ(query.category, "Scene");
  ASSERT_EQ(query.tags.size(), 2u);
  EXPECT_EQ(query.tags[0], "editing");
  EXPECT_EQ(query.tags[1], "ui");
  EXPECT_EQ(query.limit, godot_autopilot::api_surface::kDefaultToolListLimit);
  EXPECT_EQ(query.offset, 0u);
}

TEST(ApiListQueryTest, ClampsThePaginationWindow) {
  std::unordered_map<std::string, std::string> params;
  params["limit"] = "100000";
  params["offset"] = "5";
  ListQuery query;
  std::string error;
  ASSERT_TRUE(godot_autopilot::api_surface::parse_list_query(params, query,
                                                              error));
  EXPECT_EQ(query.limit, godot_autopilot::api_surface::kMaxToolListLimit);
  EXPECT_EQ(query.offset, 5u);
}

TEST(ApiListQueryTest, RejectsMalformedPaginationValues) {
  std::unordered_map<std::string, std::string> params;
  params["limit"] = "many";
  ListQuery query;
  std::string error;
  EXPECT_FALSE(
      godot_autopilot::api_surface::parse_list_query(params, query, error));
  EXPECT_FALSE(error.empty());
}

TEST(ApiFilterTest, KeepsTheMatchingTools) {
  std::vector<ToolEntry> entries;
  entries.push_back(make_entry("scene_open", "Scene", {"editing", "ui"}));
  entries.push_back(make_entry("scene_save", "Scene", {"editing"}));
  entries.push_back(make_entry("audio_play", "Audio", {"media"}));

  ListQuery query;
  ASSERT_TRUE(godot_autopilot::api_surface::filter_tools(entries, query).size() ==
              3u);
  query.category = "scene";
  EXPECT_EQ(godot_autopilot::api_surface::filter_tools(entries, query).size(),
            2u);
  query = ListQuery();
  query.tags = {"editing"};
  EXPECT_EQ(godot_autopilot::api_surface::filter_tools(entries, query).size(),
            2u);
  query = ListQuery();
  query.tags = {"editing", "media"};
  EXPECT_EQ(godot_autopilot::api_surface::filter_tools(entries, query).size(),
            0u);
  query = ListQuery();
  query.query = "audio";
  EXPECT_EQ(godot_autopilot::api_surface::filter_tools(entries, query).size(),
            1u);
}

TEST(ApiPaginationTest, SlicesTheRequestedWindow) {
  std::vector<ToolEntry> entries;
  entries.push_back(make_entry("a_tool", "Tools", {}));
  entries.push_back(make_entry("b_tool", "Tools", {}));
  entries.push_back(make_entry("c_tool", "Tools", {}));
  const std::vector<ToolEntry> page =
      godot_autopilot::api_surface::paginate_tools(entries, 1, 1);
  ASSERT_EQ(page.size(), 1u);
  EXPECT_EQ(page[0].name, "b_tool");
  EXPECT_TRUE(
      godot_autopilot::api_surface::paginate_tools(entries, 9, 2).empty());
}

TEST(ApiResponseTest, AssemblesTheToolListEnvelope) {
  std::vector<ToolEntry> entries;
  entries.push_back(make_entry("scene_open", "Scene", {"editing"}));
  ListQuery query;
  const mcp::JsonValue body =
      godot_autopilot::api_surface::build_tool_list_response(entries, 1, query);
  const mcp::JsonValue *tools = body.Find("tools");
  ASSERT_NE(tools, nullptr);
  ASSERT_TRUE(tools->IsArray());
  ASSERT_EQ(tools->GetArray().size(), 1u);
  const mcp::JsonValue *entry = tools->GetArray()[0].Find("name");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetString(), "scene_open");
  const mcp::JsonValue *count = body.Find("count");
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->GetInt(), 1);
}

TEST(ApiStatusTest, MapsTheErrorCodesToStatusCodes) {
  EXPECT_EQ(godot_autopilot::api_surface::status_for_code("unauthorized"), 401);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("invalid_argument"), 400);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("tool_not_found"), 404);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("authorization_required"),
      403);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("method_not_allowed"), 405);
  EXPECT_EQ(godot_autopilot::api_surface::status_for_code("conflict"), 409);
  EXPECT_EQ(godot_autopilot::api_surface::status_for_code("tool_error"), 422);
  EXPECT_EQ(godot_autopilot::api_surface::status_for_code("internal_error"),
            500);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("main_thread_timeout"),
      504);
  EXPECT_EQ(
      godot_autopilot::api_surface::status_for_code("queue_unavailable"), 503);
}

TEST(ApiBatchTest, ParsesTheOperationList) {
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  mcp::JsonValue first(mcp::JsonValue::object_tag);
  first["tool"] = mcp::JsonValue("scene_open");
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["path"] = mcp::JsonValue("res://scene.tscn");
  first["args"] = std::move(args);
  operations.PushBack(std::move(first));
  body["operations"] = std::move(operations);
  body["stop_on_error"] = mcp::JsonValue(false);

  BatchRequest request;
  std::string error;
  ASSERT_TRUE(
      godot_autopilot::api_surface::parse_batch_request(body, request, error));
  ASSERT_EQ(request.operations.size(), 1u);
  EXPECT_EQ(request.operations[0].tool, "scene_open");
  EXPECT_FALSE(request.stop_on_error);
}

TEST(ApiBatchTest, RejectsMalformedOperationLists) {
  BatchRequest request;
  std::string error;
  mcp::JsonValue empty(mcp::JsonValue::object_tag);
  EXPECT_FALSE(godot_autopilot::api_surface::parse_batch_request(empty, request,
                                                                 error));
  EXPECT_FALSE(error.empty());

  mcp::JsonValue not_an_array(mcp::JsonValue::object_tag);
  not_an_array["operations"] = mcp::JsonValue("nope");
  EXPECT_FALSE(godot_autopilot::api_surface::parse_batch_request(not_an_array,
                                                                 request, error));

  mcp::JsonValue bad_flag(mcp::JsonValue::object_tag);
  mcp::JsonValue empty_operations(mcp::JsonValue::array_tag);
  bad_flag["operations"] = std::move(empty_operations);
  bad_flag["stop_on_error"] = mcp::JsonValue("yes");
  EXPECT_FALSE(
      godot_autopilot::api_surface::parse_batch_request(bad_flag, request, error));

  mcp::JsonValue bad_operation(mcp::JsonValue::object_tag);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  operations.PushBack(mcp::JsonValue("scene_open"));
  bad_operation["operations"] = std::move(operations);
  EXPECT_FALSE(godot_autopilot::api_surface::parse_batch_request(
      bad_operation, request, error));

  mcp::JsonValue missing_tool(mcp::JsonValue::object_tag);
  mcp::JsonValue unnamed(mcp::JsonValue::object_tag);
  unnamed["args"] = mcp::JsonValue(mcp::JsonValue::object_tag);
  mcp::JsonValue unnamed_operations(mcp::JsonValue::array_tag);
  unnamed_operations.PushBack(std::move(unnamed));
  missing_tool["operations"] = std::move(unnamed_operations);
  EXPECT_FALSE(godot_autopilot::api_surface::parse_batch_request(
      missing_tool, request, error));

  mcp::JsonValue bad_args(mcp::JsonValue::object_tag);
  mcp::JsonValue named(mcp::JsonValue::object_tag);
  named["tool"] = mcp::JsonValue("scene_open");
  named["args"] = mcp::JsonValue("nope");
  mcp::JsonValue named_operations(mcp::JsonValue::array_tag);
  named_operations.PushBack(std::move(named));
  bad_args["operations"] = std::move(named_operations);
  EXPECT_FALSE(
      godot_autopilot::api_surface::parse_batch_request(bad_args, request, error));
}

TEST_F(ApiSurfaceTest, ListsTheToolsWithTheEnvelopeShape) {
  entries_.push_back(make_entry("scene_open", "Scene", {"editing"}));
  entries_.push_back(make_entry("audio_play", "Audio", {"media"}));

  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/tools", bearer_headers(kToken), "");
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *tools = reply.body.Find("tools");
  ASSERT_NE(tools, nullptr);
  ASSERT_TRUE(tools->IsArray());
  EXPECT_EQ(tools->GetArray().size(), 2u);
  const mcp::JsonValue *entry = tools->GetArray()[0].Find("side_effect");
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->GetString(), "none");
  const mcp::JsonValue *schema = tools->GetArray()[0].Find("input_schema");
  ASSERT_NE(schema, nullptr);
  EXPECT_TRUE(schema->IsObject());
}

TEST_F(ApiSurfaceTest, AppliesTheServerSideFilters) {
  entries_.push_back(make_entry("scene_open", "Scene", {"editing"}));
  entries_.push_back(make_entry("audio_play", "Audio", {"media"}));

  std::unordered_map<std::string, std::string> headers = bearer_headers(kToken);
  headers[std::string("x-gda-query-") + "category"] = "Audio";
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/tools", headers, "");
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *tools = reply.body.Find("tools");
  ASSERT_NE(tools, nullptr);
  ASSERT_EQ(tools->GetArray().size(), 1u);
  EXPECT_EQ(tools->GetArray()[0].Find("name")->GetString(), "audio_play");
  const mcp::JsonValue *total = reply.body.Find("total");
  ASSERT_NE(total, nullptr);
  EXPECT_EQ(total->GetInt(), 1);
}

TEST_F(ApiSurfaceTest, RejectsTheUnauthenticatedRequests) {
  const ApiReply reply =
      godot_autopilot::api_surface::handle_request("GET", "/api/tools", {}, "");
  EXPECT_EQ(reply.status, 401);
  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "unauthorized");
}

TEST_F(ApiSurfaceTest, ServesTheHealthProbeWithoutAToken) {
  const ApiReply reply =
      godot_autopilot::api_surface::handle_request("GET", "/api/health", {}, "");
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *status = reply.body.Find("status");
  ASSERT_NE(status, nullptr);
  EXPECT_EQ(status->GetString(), "ok");
  const mcp::JsonValue *port = reply.body.Find("port");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->GetInt(), 9527);
  const mcp::JsonValue *pid = reply.body.Find("pid");
  ASSERT_NE(pid, nullptr);
  EXPECT_EQ(pid->GetInt(), 4242);
  EXPECT_EQ(reply.body.Find("project_path"), nullptr);
  EXPECT_EQ(reply.body.Find("endpoints"), nullptr);
}

TEST_F(ApiSurfaceTest, ReadsOneToolDescription) {
  entries_.push_back(make_entry("scene_open", "Scene", {"editing"}));
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/tools/scene_open", bearer_headers(kToken), "");
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *name = reply.body.Find("name");
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->GetString(), "scene_open");
  const mcp::JsonValue *tags = reply.body.Find("tags");
  ASSERT_NE(tags, nullptr);
  ASSERT_TRUE(tags->IsArray());
  EXPECT_EQ(tags->GetArray().size(), 1u);
}

TEST_F(ApiSurfaceTest, ReportsTheMissingTool) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/tools/absent_tool", bearer_headers(kToken), "");
  EXPECT_EQ(reply.status, 404);
  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "tool_not_found");
}

TEST_F(ApiSurfaceTest, CallsTheToolWithTheArgumentObject) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/tools/scene_open", bearer_headers(kToken),
      "{\"path\":\"res://scene.tscn\"}");
  EXPECT_EQ(reply.status, 200);
  EXPECT_EQ(last_tool, "scene_open");
  const mcp::JsonValue *path = last_args.Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "res://scene.tscn");
  const mcp::JsonValue *tool = reply.body.Find("tool");
  ASSERT_NE(tool, nullptr);
  EXPECT_EQ(tool->GetString(), "scene_open");
}

TEST_F(ApiSurfaceTest, RejectsANonObjectArgumentBody) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/tools/scene_open", bearer_headers(kToken), "[1,2,3]");
  EXPECT_EQ(reply.status, 400);
  EXPECT_TRUE(last_tool.empty());
}

TEST_F(ApiSurfaceTest, MapsTheToolFailureToAnErrorStatus) {
  fail_next = true;
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/tools/scene_open", bearer_headers(kToken), "{}");
  EXPECT_EQ(reply.status, 422);
  const mcp::JsonValue *error = reply.body.Find("error");
  ASSERT_NE(error, nullptr);
  EXPECT_EQ(error->GetString(), "the stub tool failed");
}

TEST_F(ApiSurfaceTest, RunsTheBatchOperationsInOrder) {
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  for (const char *name : {"first_tool", "second_tool"}) {
    mcp::JsonValue operation(mcp::JsonValue::object_tag);
    operation["tool"] = mcp::JsonValue(name);
    operations.PushBack(std::move(operation));
  }
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["operations"] = std::move(operations);

  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/batch", bearer_headers(kToken), body.Dump());
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *results = reply.body.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_EQ(results->GetArray().size(), 2u);
  EXPECT_EQ(results->GetArray()[0].Find("tool")->GetString(), "first_tool");
  EXPECT_EQ(results->GetArray()[0].Find("status")->GetString(), "ok");
  const mcp::JsonValue *total = reply.body.Find("total");
  ASSERT_NE(total, nullptr);
  EXPECT_EQ(total->GetInt(), 2);
}

TEST_F(ApiSurfaceTest, StopsTheBatchOnTheFirstFailure) {
  fail_next = true;
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  for (const char *name : {"first_tool", "second_tool"}) {
    mcp::JsonValue operation(mcp::JsonValue::object_tag);
    operation["tool"] = mcp::JsonValue(name);
    operations.PushBack(std::move(operation));
  }
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  body["operations"] = std::move(operations);

  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/batch", bearer_headers(kToken), body.Dump());
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *results = reply.body.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_EQ(results->GetArray().size(), 1u);
  EXPECT_EQ(results->GetArray()[0].Find("status")->GetString(), "error");
  const mcp::JsonValue *skipped = reply.body.Find("skipped");
  ASSERT_NE(skipped, nullptr);
  EXPECT_EQ(skipped->GetInt(), 1);
}

TEST_F(ApiSurfaceTest, RejectsAMalformedBatchBody) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/batch", bearer_headers(kToken), "{\"operations\":\"nope\"}");
  EXPECT_EQ(reply.status, 400);
}

TEST(ApiBatchTest, ParseBatchRequestAcceptsRollbackOnError) {
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  mcp::JsonValue first(mcp::JsonValue::object_tag);
  first["tool"] = mcp::JsonValue("scene_open");
  operations.PushBack(std::move(first));
  body["operations"] = std::move(operations);
  body["rollback_on_error"] = mcp::JsonValue(true);

  BatchRequest request;
  std::string error;
  ASSERT_TRUE(
      godot_autopilot::api_surface::parse_batch_request(body, request, error));
  EXPECT_TRUE(request.rollback_on_error);

  body["rollback_on_error"] = mcp::JsonValue(false);
  ASSERT_TRUE(
      godot_autopilot::api_surface::parse_batch_request(body, request, error));
  EXPECT_FALSE(request.rollback_on_error);

  BatchRequest defaults;
  EXPECT_FALSE(defaults.rollback_on_error);
}

TEST(ApiBatchTest, ParseBatchRequestRejectsNonBooleanRollbackOnError) {
  mcp::JsonValue body(mcp::JsonValue::object_tag);
  mcp::JsonValue operations(mcp::JsonValue::array_tag);
  mcp::JsonValue first(mcp::JsonValue::object_tag);
  first["tool"] = mcp::JsonValue("scene_open");
  operations.PushBack(std::move(first));
  body["operations"] = std::move(operations);
  body["rollback_on_error"] = mcp::JsonValue("yes");

  BatchRequest request;
  std::string error;
  EXPECT_FALSE(
      godot_autopilot::api_surface::parse_batch_request(body, request, error));
  EXPECT_EQ(error, "rollback_on_error must be a boolean");
}

TEST_F(ApiSurfaceTest, HandleRequestRejectsUnacceptableAccept) {
  std::unordered_map<std::string, std::string> headers = bearer_headers(kToken);
  headers["accept"] = "text/plain";
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/health", headers, "");
  EXPECT_EQ(reply.status, 406);
  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "invalid_accept");

  std::unordered_map<std::string, std::string> wildcard =
      bearer_headers(kToken);
  wildcard["accept"] = "*/*";
  const ApiReply accepted = godot_autopilot::api_surface::handle_request(
      "GET", "/api/health", wildcard, "");
  EXPECT_EQ(accepted.status, 200);
}

TEST_F(ApiSurfaceTest, ServesTheOpenApiDocument) {
  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "GET", "/api/openapi.json", bearer_headers(kToken), "");
  EXPECT_EQ(reply.status, 200);
  const mcp::JsonValue *openapi = reply.body.Find("openapi");
  ASSERT_NE(openapi, nullptr);
  EXPECT_EQ(openapi->GetString(), "3.0.3");
  const mcp::JsonValue *paths = reply.body.Find("paths");
  ASSERT_NE(paths, nullptr);
  EXPECT_NE(paths->Find("/api/tools"), nullptr);
  EXPECT_NE(paths->Find("/api/tools/{tool}"), nullptr);
  EXPECT_NE(paths->Find("/api/batch"), nullptr);
  EXPECT_NE(paths->Find("/api/health"), nullptr);
}

TEST_F(ApiSurfaceTest, ReportsTheUnknownRouteAndTheMethodMismatch) {
  const ApiReply unknown = godot_autopilot::api_surface::handle_request(
      "GET", "/api/unknown", bearer_headers(kToken), "");
  EXPECT_EQ(unknown.status, 404);
  const ApiReply wrong_method = godot_autopilot::api_surface::handle_request(
      "GET", "/api/batch", bearer_headers(kToken), "");
  EXPECT_EQ(wrong_method.status, 405);
}

TEST(ApiServerTest, RegistersTheApiRoutesOnTheRouteTable) {
  HttpRouteTable table;
  const ApiServer server;
  server.register_routes(table);
  EXPECT_NE(table.find("GET", "/api/tools"), nullptr);
  EXPECT_NE(table.find("POST", "/api/tools"), nullptr);
  EXPECT_NE(table.find("POST", "/api/batch"), nullptr);
  EXPECT_NE(table.find("GET", "/api/health"), nullptr);
  EXPECT_NE(table.find("GET", "/api/openapi.json"), nullptr);
  EXPECT_EQ(table.find("GET", "/mcp"), nullptr);
}

TEST(ApiServerTest, HandleReturnsTheResponseBody) {
  godot_autopilot::api_surface::register_runtime_info_provider([]() {
    godot_autopilot::api_surface::RuntimeInfo info;
    info.version = "1.2.3";
    info.port = 9527;
    info.pid = 7;
    return info;
  });
  const ApiServer server;
  const mcp::JsonValue body =
      server.handle("GET", "/api/health", {}, "");
  const mcp::JsonValue *version = body.Find("version");
  ASSERT_NE(version, nullptr);
  EXPECT_EQ(version->GetString(), "1.2.3");
  godot_autopilot::api_surface::register_runtime_info_provider(nullptr);
}
