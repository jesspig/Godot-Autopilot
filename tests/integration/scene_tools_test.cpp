

#include "godot_fixture.hpp"
#include "mcp_test_client.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <mcp/JsonValue.hpp>
#include <string>
#include <thread>

using gsd_test::McpTestClient;

namespace {

mcp::JsonValue parse_json(const std::string &text) {
  try {
    return mcp::JsonValue::Parse(text);
  } catch (...) {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  }
}

bool new_scene(McpTestClient &client, const std::string &name) {
  client.call_tool("call_tool",
                   R"({"name":"editor_new_scene","arguments":{"name":")" +
                       name + R"("}})");
  return !client.last_call_was_error();
}

bool create_node(McpTestClient &client, const std::string &name,
                 const std::string &type, const std::string &parent_path) {
  const std::string args = R"({"name":")" + name + R"(","type":")" + type +
                           R"(","parent_path":")" + parent_path + R"("})";
  client.call_tool("call_tool",
                   R"({"name":"scene_node_create","arguments":)" + args + "}");
  return !client.last_call_was_error();
}

bool tree_contains_name(const std::string &tree_resp, const std::string &name) {
  return tree_resp.find("\"name\":\"" + name + "\"") != std::string::npos;
}

} // namespace

class SceneToolsTest : public gsd_test::GodotEditorFixture {};

TEST_F(SceneToolsTest, SceneNodeCreateRoot) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());

  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_create","arguments":{"name":"MyRoot","type":"Node"}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());
  auto *path = result->Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "MyRoot");
  EXPECT_NE(result->Find("undo"), nullptr);
  const std::string tree =
      client.call_tool("call_tool", R"({"name":"scene_get_tree"})");
  ASSERT_FALSE(client.last_call_was_error()) << tree;
  EXPECT_TRUE(tree_contains_name(tree, "MyRoot"));
}

TEST_F(SceneToolsTest, SceneNodeCreateDefaults) {
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

TEST_F(SceneToolsTest, SceneNodeCreateChild) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_create","arguments":{"name":"Child","type":"Node2D","parent_path":"Root"}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());

  auto *path = result->Find("path");
  ASSERT_NE(path, nullptr);
  EXPECT_EQ(path->GetString(), "Child");
}

TEST_F(SceneToolsTest, SceneNodeCreateInvalidType) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_create","arguments":{"name":"Bad","type":"NoSuchNodeType123"}})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("is not a Node subclass"), std::string::npos) << resp;
}

TEST_F(SceneToolsTest, SceneNodeCreateSecondRootFails) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_create","arguments":{"name":"SecondRoot"}})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("scene already has a root"), std::string::npos) << resp;
}

TEST_F(SceneToolsTest, SceneNodeDelete) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  ASSERT_TRUE(create_node(client, "Doomed", "Node", "Root"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_delete","arguments":{"path":"Doomed"}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetString(), "deleted");
  EXPECT_NE(j.Find("undo"), nullptr);

  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  const std::string tree =
      client.call_tool("call_tool", R"({"name":"scene_get_tree"})");
  ASSERT_FALSE(client.last_call_was_error()) << tree;
  EXPECT_FALSE(tree_contains_name(tree, "Doomed"));
}

TEST_F(SceneToolsTest, SceneNodeDeleteMissing) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_delete","arguments":{"path":"NoSuchNode"}})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("node not found"), std::string::npos) << resp;
}

TEST_F(SceneToolsTest, SceneNodeDeleteRootForbidden) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_node_delete","arguments":{"path":"Root"}})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("cannot delete the scene root"), std::string::npos)
      << resp;
}

TEST_F(SceneToolsTest, SceneGetTree) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  ASSERT_TRUE(create_node(client, "ChildA", "Node2D", "Root"));
  ASSERT_TRUE(create_node(client, "ChildB", "Label", "ChildA"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"scene_get_tree","arguments":{"include_properties":true}})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());
  auto *result_name = result->Find("name");
  ASSERT_NE(result_name, nullptr);
  EXPECT_EQ(result_name->GetString(), "Root");
  auto *result_type = result->Find("type");
  ASSERT_NE(result_type, nullptr);
  EXPECT_EQ(result_type->GetString(), "Node");
  auto *result_path = result->Find("path");
  ASSERT_NE(result_path, nullptr);
  EXPECT_EQ(result_path->GetString(), "Root");
  auto *children = result->Find("children");
  ASSERT_NE(children, nullptr);
  ASSERT_TRUE(children->IsArray());
  ASSERT_EQ(children->GetArray().size(), 1u);
  const mcp::JsonValue &child_a = children->GetArray()[0];
  auto *child_a_name = child_a.Find("name");
  ASSERT_NE(child_a_name, nullptr);
  EXPECT_EQ(child_a_name->GetString(), "ChildA");
  auto *child_a_type = child_a.Find("type");
  ASSERT_NE(child_a_type, nullptr);
  EXPECT_EQ(child_a_type->GetString(), "Node2D");
  auto *child_a_path = child_a.Find("path");
  ASSERT_NE(child_a_path, nullptr);

  EXPECT_EQ(child_a_path->GetString(), "ChildA");
  auto *grand_children = child_a.Find("children");
  ASSERT_NE(grand_children, nullptr);
  ASSERT_TRUE(grand_children->IsArray());
  ASSERT_EQ(grand_children->GetArray().size(), 1u);
  auto *grand_name = grand_children->GetArray()[0].Find("name");
  ASSERT_NE(grand_name, nullptr);
  EXPECT_EQ(grand_name->GetString(), "ChildB");

  EXPECT_TRUE(child_a.Find("properties") != nullptr);
}

TEST_F(SceneToolsTest, SceneTreeGet) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  ASSERT_TRUE(create_node(client, "Leaf", "Node", "Root"));
  const std::string resp =
      client.call_tool("call_tool", R"({"name":"scene_tree_get"})");
  ASSERT_FALSE(client.last_call_was_error()) << resp;
  const mcp::JsonValue j = parse_json(resp);
  ASSERT_TRUE(j.IsObject());
  auto *result = j.Find("result");
  ASSERT_NE(result, nullptr);
  ASSERT_TRUE(result->IsObject());
  auto *result_name = result->Find("name");
  ASSERT_NE(result_name, nullptr);
  EXPECT_EQ(result_name->GetString(), "Root");
  EXPECT_NE(result->Find("type"), nullptr);
  EXPECT_NE(result->Find("path"), nullptr);
  auto *children = result->Find("children");
  ASSERT_NE(children, nullptr);
  ASSERT_TRUE(children->IsArray());
  ASSERT_EQ(children->GetArray().size(), 1u);
  auto *leaf_name = children->GetArray()[0].Find("name");
  ASSERT_NE(leaf_name, nullptr);
  EXPECT_EQ(leaf_name->GetString(), "Leaf");
}

TEST_F(SceneToolsTest, EditorNewSceneCloseCurrentRefusesUnsaved) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "First"));
  const std::string resp = client.call_tool(
      "call_tool",
      R"({"name":"editor_new_scene","arguments":{"name":"Second","close_current":true}})");
  EXPECT_TRUE(client.last_call_was_error()) << resp;
  EXPECT_NE(resp.find("unsaved"), std::string::npos) << resp;

  const std::string tree =
      client.call_tool("call_tool", R"({"name":"scene_get_tree"})");
  ASSERT_FALSE(client.last_call_was_error()) << tree;
  EXPECT_TRUE(tree_contains_name(tree, "First"));
}

TEST_F(SceneToolsTest, GroupAddHasRemoveRoundTrip) {
  McpTestClient client(port());
  ASSERT_TRUE(client.connect());
  ASSERT_TRUE(new_scene(client, "Root"));
  ASSERT_TRUE(create_node(client, "Member", "Node", "Root"));
  const std::string add = client.call_tool(
      "call_tool",
      R"({"name":"group_add_node_to_group","arguments":{"node_path":"Member","group_name":"gsd_test_group"}})");
  ASSERT_FALSE(client.last_call_was_error()) << add;
  EXPECT_NE(add.find("\"result\":\"added\""), std::string::npos) << add;
  const std::string has = client.call_tool(
      "call_tool",
      R"({"name":"group_has_node_in_group","arguments":{"node_path":"Member","group_name":"gsd_test_group"}})");
  ASSERT_FALSE(client.last_call_was_error()) << has;
  const mcp::JsonValue has_j = parse_json(has);
  auto *has_result = has_j.Find("result");
  ASSERT_NE(has_result, nullptr);
  EXPECT_TRUE(has_result->GetBool());
  const std::string remove = client.call_tool(
      "call_tool",
      R"({"name":"group_remove_node_from_group","arguments":{"node_path":"Member","group_name":"gsd_test_group"}})");
  ASSERT_FALSE(client.last_call_was_error()) << remove;
  EXPECT_NE(remove.find("\"result\":\"removed\""), std::string::npos) << remove;
  const std::string has_after = client.call_tool(
      "call_tool",
      R"({"name":"group_has_node_in_group","arguments":{"node_path":"Member","group_name":"gsd_test_group"}})");
  ASSERT_FALSE(client.last_call_was_error()) << has_after;
  const mcp::JsonValue has_after_j = parse_json(has_after);
  auto *has_after_result = has_after_j.Find("result");
  ASSERT_NE(has_after_result, nullptr);
  EXPECT_FALSE(has_after_result->GetBool());
}
