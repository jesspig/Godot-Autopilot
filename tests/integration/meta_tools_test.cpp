

#include "godot_fixture.hpp"
#include "mcp_test_client.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <mcp/JsonValue.hpp>
#include <string>

using gsd_test::McpTestClient;

namespace {

mcp::JsonValue parse_json(const std::string &text) {
  try {
    return mcp::JsonValue::Parse(text);
  } catch (...) {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  }
}

bool has_error(const mcp::JsonValue &j) { return j.Find("error") != nullptr; }

} // namespace

class MetaToolsTest : public gsd_test::GodotEditorFixture {};

TEST_F(MetaToolsTest, PingReturnsOk) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool("ping");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "pong");
}

TEST_F(MetaToolsTest, ListToolsContainsMetaTools) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const auto tools = client.list_tools();
  ASSERT_FALSE(tools.empty());
  for (const char *meta :
       {"ping", "search_tools", "list_categories", "get_tool_detail",
        "call_tool", "batch_execute", "code_execute"}) {
    const bool found = std::any_of(
        tools.begin(), tools.end(),
        [meta](const gsd_test::ToolSummary &t) { return t.name == meta; });
    EXPECT_TRUE(found) << "meta tool missing from tools/list: " << meta;
  }
}

TEST_F(MetaToolsTest, SearchToolsByKeyword) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  const std::string resp =
      client.call_tool("search_tools", R"({"query":"scene_node_create"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *results = j.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_FALSE(results->GetArray().empty());
  bool found_scene_create = false;
  for (const auto &item : results->GetArray()) {
    ASSERT_TRUE(item.IsObject());
    auto *name = item.Find("name");
    ASSERT_NE(name, nullptr);
    EXPECT_TRUE(name->IsString());
    if (name->IsString() && name->GetString() == "scene_node_create") {
      found_scene_create = true;
    }
    EXPECT_NE(item.Find("score"), nullptr);
  }
  EXPECT_TRUE(found_scene_create);
}

TEST_F(MetaToolsTest, SearchToolsByCategory) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  const std::string resp = client.call_tool(
      "search_tools", R"({"query":"scene_node_create","category":"Scene"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *results = j.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_FALSE(results->GetArray().empty());
  bool found_scene_create = false;
  for (const auto &item : results->GetArray()) {
    auto *name = item.Find("name");
    if (name && name->IsString() && name->GetString() == "scene_node_create") {
      found_scene_create = true;
    }
  }
  EXPECT_TRUE(found_scene_create);
}

TEST_F(MetaToolsTest, SearchToolsByTags) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp =
      client.call_tool("search_tools", R"({"query":"node","tags":["node"]})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *results = j.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_FALSE(results->GetArray().empty());
  bool found_scene_create = false;
  for (const auto &item : results->GetArray()) {
    auto *name = item.Find("name");
    if (name && name->IsString() && name->GetString() == "scene_node_create") {
      found_scene_create = true;
    }
  }
  EXPECT_TRUE(found_scene_create);
}

TEST_F(MetaToolsTest, SearchToolsEmptyQueryReturnsEmpty) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool("search_tools", R"({"query":""})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *results = j.Find("results");
  ASSERT_NE(results, nullptr);
  EXPECT_TRUE(results->IsArray());
  EXPECT_TRUE(results->GetArray().empty());
}

TEST_F(MetaToolsTest, SearchToolsMissingQueryFails) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool("search_tools", R"({})");
  const mcp::JsonValue j = parse_json(resp);
  EXPECT_TRUE(has_error(j));
  EXPECT_NE(resp.find("missing required parameter: query"), std::string::npos);
}

TEST_F(MetaToolsTest, ListCategoriesReturnsNonEmpty) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool("list_categories");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *categories = j.Find("categories");
  ASSERT_NE(categories, nullptr);
  ASSERT_TRUE(categories->IsArray());
  ASSERT_FALSE(categories->GetArray().empty());
  bool found_scene = false;
  for (const auto &item : categories->GetArray()) {
    ASSERT_TRUE(item.IsObject());
    EXPECT_NE(item.Find("id"), nullptr);
    EXPECT_NE(item.Find("name"), nullptr);
    EXPECT_NE(item.Find("tool_count"), nullptr);
    auto *id = item.Find("id");
    if (id && id->IsString() && id->GetString() == "Scene") {
      found_scene = true;
    }
  }
  EXPECT_TRUE(found_scene);
}

TEST_F(MetaToolsTest, GetToolDetailExisting) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp =
      client.call_tool("get_tool_detail", R"({"name":"scene_node_create"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *tool = j.Find("tool");
  ASSERT_NE(tool, nullptr);
  ASSERT_TRUE(tool->IsObject());
  auto *name = tool->Find("name");
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->GetString(), "scene_node_create");
  EXPECT_NE(tool->Find("description"), nullptr);
  EXPECT_NE(tool->Find("category"), nullptr);
  EXPECT_NE(tool->Find("tags"), nullptr);
  auto *schema = tool->Find("input_schema");
  ASSERT_NE(schema, nullptr);
  ASSERT_TRUE(schema->IsObject());
  auto *schema_type = schema->Find("type");
  ASSERT_NE(schema_type, nullptr);
  EXPECT_EQ(schema_type->GetString(), "object");
  auto *properties = schema->Find("properties");
  ASSERT_NE(properties, nullptr);
  EXPECT_TRUE(properties->IsObject());
  auto *required = schema->Find("required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsArray());
  bool required_has_name = false;
  bool required_has_type = false;
  for (const auto &r : required->GetArray()) {
    if (!r.IsString())
      continue;
    if (r.GetString() == "name")
      required_has_name = true;
    if (r.GetString() == "type")
      required_has_type = true;
  }
  EXPECT_TRUE(required_has_name);
  EXPECT_TRUE(required_has_type);
}

TEST_F(MetaToolsTest, GetToolDetailMissing) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp =
      client.call_tool("get_tool_detail", R"({"name":"no_such_tool"})");
  EXPECT_NE(resp.find("tool not found"), std::string::npos) << resp;
  EXPECT_NE(resp.find("\"error\""), std::string::npos) << resp;
}

TEST_F(MetaToolsTest, CallToolUnknownTool) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp =
      client.call_tool("call_tool", R"({"name":"no_such_tool"})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("domain tool"), std::string::npos) << resp;
  EXPECT_NE(resp.find("not found"), std::string::npos) << resp;
}

TEST_F(MetaToolsTest, CallToolWithEmptyArguments) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool(
      "call_tool", R"({"name":"scene_node_create","arguments":{}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());
  auto *path = result->Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "NewNode");
}

TEST_F(MetaToolsTest, BatchExecuteSequential) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool("batch_execute", R"({
        "operations": [
            {"tool": "editor_new_scene", "args": {"name": "BatchRoot"}},
            {"tool": "scene_node_create", "args": {"name": "BatchChild", "parent_path": "BatchRoot"}},
            {"tool": "scene_get_tree", "args": {}}
        ]
    })");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *succeeded = j.Find("succeeded");
  ASSERT_NE(succeeded, nullptr);
  EXPECT_EQ(succeeded->GetInt(), 3);
  auto *failed = j.Find("failed");
  ASSERT_NE(failed, nullptr);
  EXPECT_EQ(failed->GetInt(), 0);
  auto *results = j.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  ASSERT_EQ(results->GetArray().size(), 3u);
  const auto &ops = results->GetArray();
  auto *status0 = ops[0].Find("status");
  ASSERT_NE(status0, nullptr);
  EXPECT_EQ(status0->GetString(), "ok");
  auto *data0 = ops[0].Find("data");
  ASSERT_NE(data0, nullptr);
  auto *result0 = data0->Find("result");
  ASSERT_NE(result0, nullptr);
  auto *path0 = result0->Find("path");
  ASSERT_NE(path0, nullptr);
  EXPECT_EQ(path0->GetString(), "BatchRoot");
  auto *data1 = ops[1].Find("data");
  ASSERT_NE(data1, nullptr);
  auto *result1 = data1->Find("result");
  ASSERT_NE(result1, nullptr);
  auto *path1 = result1->Find("path");
  ASSERT_NE(path1, nullptr);
  EXPECT_EQ(path1->GetString(), "BatchChild");
  auto *data2 = ops[2].Find("data");
  ASSERT_NE(data2, nullptr);
  auto *result2 = data2->Find("result");
  ASSERT_NE(result2, nullptr);
  auto *name2 = result2->Find("name");
  ASSERT_NE(name2, nullptr);
  EXPECT_EQ(name2->GetString(), "BatchRoot");
}

TEST_F(MetaToolsTest, CodeExecuteBasic) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  const std::string resp =
      client.call_tool("code_execute", R"({"source_code":"return 21 * 2"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->IsNumber());
  EXPECT_EQ(result->GetInt(), 42);
  EXPECT_NE(j.Find("execution_time_ms"), nullptr);
}

TEST_F(MetaToolsTest, CodeExecuteSyntaxError) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp =
      client.call_tool("code_execute", R"({"source_code":"var x ="})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("GDScript compilation failed"), std::string::npos)
      << resp;

  EXPECT_TRUE(editor_alive());
}
