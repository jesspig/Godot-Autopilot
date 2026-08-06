

#include "godot_fixture.hpp"
#include "mcp_test_client.hpp"
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

double as_number(const mcp::JsonValue &v) {
  return v.IsInt() ? static_cast<double>(v.GetInt()) : v.GetDouble();
}

int64_t as_int(const mcp::JsonValue &v) {
  return v.IsInt() ? v.GetInt() : static_cast<int64_t>(v.GetDouble());
}

std::string call_domain(McpTestClient &client, const std::string &name,
                        const std::string &args_json) {
  return client.call_tool("call_tool", R"({"name":")" + name +
                                           R"(","arguments":)" + args_json +
                                           "}");
}

bool prepare_scene(McpTestClient &client) {
  call_domain(client, "editor_new_scene", R"({"name":"Root"})");
  if (client.last_call_was_error())
    return false;
  call_domain(client, "scene_node_create",
              R"({"name":"LabelNode","type":"Label","parent_path":"Root"})");
  return !client.last_call_was_error();
}

} // namespace

class PropertyToolsTest : public gsd_test::GodotEditorFixture {};

TEST_F(PropertyToolsTest, PropertyGetInt) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  std::string resp =
      call_domain(client, "property_set",
                  R"({"path":"LabelNode","property":"z_index","value":5})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  resp = call_domain(client, "property_get",
                     R"({"path":"LabelNode","property":"z_index"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(as_int(*result), 5);
}

TEST_F(PropertyToolsTest, PropertySetAndGetString) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  std::string resp = call_domain(
      client, "property_set",
      R"({"path":"LabelNode","property":"text","value":"Hello GSD"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  resp = call_domain(client, "property_get",
                     R"({"path":"LabelNode","property":"text"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->IsString());
  EXPECT_EQ(result->GetString(), "Hello GSD");
}

TEST_F(PropertyToolsTest, PropertySetVector2) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  std::string resp = call_domain(
      client, "property_set",
      R"({"path":"LabelNode","property":"position","value":{"x":12.5,"y":-3.25}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  resp = call_domain(client, "property_get",
                     R"({"path":"LabelNode","property":"position"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());

  auto *x = result->Find("x");
  auto *y = result->Find("y");
  ASSERT_NE(x, nullptr);
  ASSERT_NE(y, nullptr);
  EXPECT_EQ(as_number(*x), 12.5);
  EXPECT_EQ(as_number(*y), -3.25);
}

TEST_F(PropertyToolsTest, PropertySetColor) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  std::string resp = call_domain(
      client, "property_set",
      R"({"path":"LabelNode","property":"modulate","value":{"r":1,"g":0.5,"b":0.25,"a":1}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  resp = call_domain(client, "property_get",
                     R"({"path":"LabelNode","property":"modulate"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());

  auto *r = result->Find("r");
  auto *g = result->Find("g");
  auto *b = result->Find("b");
  auto *a = result->Find("a");
  ASSERT_NE(r, nullptr);
  ASSERT_NE(g, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(as_number(*r), 1.0);
  EXPECT_EQ(as_number(*g), 0.5);
  EXPECT_EQ(as_number(*b), 0.25);
  EXPECT_EQ(as_number(*a), 1.0);
}

TEST_F(PropertyToolsTest, PropertyGetMissingProperty) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp =
      call_domain(client, "property_get",
                  R"({"path":"LabelNode","property":"no_such_prop"})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("property not found"), std::string::npos) << resp;
}

TEST_F(PropertyToolsTest, PropertySetNonexistentProperty) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp = call_domain(
      client, "property_set",
      R"({"path":"LabelNode","property":"no_such_prop","value":1})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;

  EXPECT_NE(resp.find("does not exist"), std::string::npos) << resp;
  EXPECT_NE(resp.find("position:"), std::string::npos) << resp;
}

TEST_F(PropertyToolsTest, PropertySetInvalidValue) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp =
      call_domain(client, "property_set",
                  R"({"path":"LabelNode","property":"z_index","value":"abc"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetString(), "ok");
  EXPECT_FALSE(has_error(j));
}

TEST_F(PropertyToolsTest, PropertyGetMissingNode) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp = call_domain(
      client, "property_get", R"({"path":"NoSuchNode","property":"z_index"})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("node not found"), std::string::npos) << resp;
}

TEST_F(PropertyToolsTest, ReadbackMatchedValue) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  std::string resp =
      call_domain(client, "property_set",
                  R"({"path":"Root","property":"name","value":"My Root"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());

  EXPECT_EQ(j.Find("warning"), nullptr) << resp;
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetString(), "ok");

  resp = call_domain(client, "property_get",
                     R"({"path":"My Root","property":"name"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue get_j = parse_json(resp);
  ASSERT_TRUE(get_j.IsObject());
  auto *get_result = get_j.Find("result");
  ASSERT_NE(get_result, nullptr);
  EXPECT_TRUE(get_result->IsString());
  EXPECT_EQ(get_result->GetString(), "My Root");
}

TEST_F(PropertyToolsTest, PropertyGetList) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp =
      call_domain(client, "property_get_list", R"({"path":"LabelNode"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsArray());
  ASSERT_FALSE(result->GetArray().empty());
  bool found_z_index = false;
  for (const auto &item : result->GetArray()) {
    ASSERT_TRUE(item.IsObject());
    EXPECT_NE(item.Find("name"), nullptr);
    EXPECT_NE(item.Find("type"), nullptr);
    auto *name = item.Find("name");
    if (name && name->IsString() && name->GetString() == "z_index") {
      found_z_index = true;
    }
  }
  EXPECT_TRUE(found_z_index);
}

TEST_F(PropertyToolsTest, PropertySetMissingValue) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(prepare_scene(client));
  const std::string resp = call_domain(
      client, "property_set", R"({"path":"LabelNode","property":"z_index"})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("missing required parameter: value"), std::string::npos)
      << resp;
}
