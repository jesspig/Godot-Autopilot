


#include <gtest/gtest.h>

#include <mcp/client/McpClient.hpp>
#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/register_all.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_catalog.hpp"
#include "tools/tool_registry.hpp"
#include "util/bm25_index.hpp"

#include <string>
#include <thread>
#include <vector>

namespace {

constexpr size_t kMetaToolCount = 7;

const char *const kMetaToolNames[kMetaToolCount] = {
    "ping",      "search_tools",  "list_categories", "get_tool_detail",
    "call_tool", "batch_execute", "code_execute"};

const char *const kValidCategories[] = {
    "Analysis",   "Animation",  "Audio",      "Capture",    "Config",     "Debug",
    "Debugger",   "Display",    "Docs",       "Editor",     "Game",
    "Group",      "Input",      "Navigation", "OS",         "Physics",
    "Properties", "Render",     "Resources",  "Scene",      "Scripts",
    "SpriteFrames", "System",   "Testing",    "Text",       "Theme",
    "TileMap",
    "Meta",       "Auto"};

bool has_schema_params(const mcp::JsonValue &schema) {
  const auto *props = schema.Find("properties");
  return props != nullptr && props->IsObject() && !props->GetObject().empty();
}

bool is_valid_category(const std::string &category) {
  for (const char *c : kValidCategories) {
    if (category == c)
      return true;
  }
  return false;
}

struct RegisteredServerFixture : ::testing::Test {
  std::unique_ptr<mcp::McpServer> server;
  std::unique_ptr<mcp::McpClient> client;
  std::thread server_thread;
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    auto pair = mcp::InMemoryTransport::CreatePair();
    server = mcp::McpServer::Create(pair.server);
    godot_autopilot::register_all_tools(*server, queue, catalog, index,
                                           9527);
    server_thread = std::thread([this] { server->Run(); });

    mcp::ClientOptions cops;
    cops.client_info = mcp::Implementation{"GsdTestClient", "1.0.0"};
    cops.connect_mode = mcp::ConnectMode::Auto;
    client = mcp::McpClient::Create(pair.client, cops);

    queue.drain();
    godot_autopilot::runtime_ops::set_editor_queue(&queue);
  }

  void TearDown() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    if (client)
      client->Close();
    if (server)
      server->Close();
    if (server_thread.joinable())
      server_thread.join();
  }
};

} // namespace

TEST_F(RegisteredServerFixture, ListToolsReturnsExactlySevenMetaTools) {
  auto result = client->ListTools();
  ASSERT_EQ(result.tools.size(), kMetaToolCount);
}

TEST_F(RegisteredServerFixture, AllMetaToolNamesPresent) {
  auto result = client->ListTools();
  for (const char *name : kMetaToolNames) {
    bool found = false;
    for (const auto &tool : result.tools) {
      if (tool.name == name)
        found = true;
    }
    EXPECT_TRUE(found) << "meta tool missing from ListTools: " << name;
  }
}

TEST_F(RegisteredServerFixture, MetaToolAttributesNonEmpty) {
  auto result = client->ListTools();
  for (const auto &tool : result.tools) {
    EXPECT_FALSE(tool.name.empty());
    EXPECT_TRUE(tool.description.has_value())
        << "tool has no description: " << tool.name;
    if (tool.description.has_value()) {
      EXPECT_FALSE(tool.description->empty());
    }
  }
}

TEST_F(RegisteredServerFixture, CatalogToolCountMatches) {
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
}

TEST_F(RegisteredServerFixture, CatalogEntriesWellFormed) {
  auto tools = catalog.get_all_tools();
  ASSERT_EQ(tools.size(), catalog.size());
  for (const auto &tool : tools) {
    EXPECT_FALSE(tool.name.empty());
    EXPECT_FALSE(tool.description.empty())
        << "empty description: " << tool.name;
    EXPECT_FALSE(tool.category.empty()) << "empty category: " << tool.name;
    EXPECT_TRUE(is_valid_category(tool.category))
        << "invalid category '" << tool.category << "' for tool "
        << tool.name;
  }
}

TEST_F(RegisteredServerFixture, SchemaStatisticsBaseline) {
  size_t non_empty = 0;
  size_t empty = 0;
  const auto tools = catalog.get_all_tools();
  for (const auto &tool : tools) {
    if (has_schema_params(tool.input_schema)) {
      ++non_empty;
    } else {
      ++empty;
    }
  }
  EXPECT_EQ(tools.size(), 459);
  EXPECT_EQ(non_empty, 393);
  EXPECT_EQ(empty, 66);
  EXPECT_EQ(non_empty + empty, tools.size());
  EXPECT_GT(non_empty, empty);
  EXPECT_GT(empty, 0);
}

TEST_F(RegisteredServerFixture, ToolDetailSideEffectIsMachineReadable) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["name"] = mcp::JsonValue("code_execute");
  auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
  const auto *tool = result.Find("tool");
  ASSERT_NE(tool, nullptr);
  ASSERT_TRUE(tool->IsObject());
  const auto *side_effect = tool->Find("side_effect");
  ASSERT_NE(side_effect, nullptr);
  ASSERT_TRUE(side_effect->IsString());
  EXPECT_EQ(side_effect->GetString(), "code_execute");
}

TEST_F(RegisteredServerFixture, EveryToolDetailReportsNonEmptySideEffect) {
  const auto tools = catalog.get_all_tools();
  ASSERT_GT(tools.size(), size_t{0});
  for (const auto &tool : tools) {
    mcp::JsonValue args(mcp::JsonValue::object_tag);
    args["name"] = mcp::JsonValue(tool.name);
    auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
    const auto *info = result.Find("tool");
    ASSERT_NE(info, nullptr) << tool.name;
    const auto *side_effect = info->Find("side_effect");
    ASSERT_NE(side_effect, nullptr) << tool.name;
    ASSERT_TRUE(side_effect->IsString()) << tool.name;
    EXPECT_FALSE(side_effect->GetString().empty()) << tool.name;
  }
}

TEST_F(RegisteredServerFixture, SchemaSampledTools) {
  const char *const kNonEmpty[] = {
      "property_get",          "intersect_physics_2d_ray",
      "execute_script",        "call_scene_tree_group",
      "set_tilemap_cell",      "queue_game_input",
      "get_scene_tree",        "batch_execute",
      "call_tool",             "code_execute",
      "search_tools",          "get_tool_detail"};
  const char *const kEmpty[] = {"get_engine_version",   "create_physics_2d_body",
                                "ping",                 "system_status",
                                "list_categories"};
  for (const char *name : kNonEmpty) {
    auto info = catalog.get_tool(name);
    ASSERT_TRUE(info.has_value()) << name;
    EXPECT_TRUE(has_schema_params(info->input_schema)) << name;
  }
  for (const char *name : kEmpty) {
    auto info = catalog.get_tool(name);
    ASSERT_TRUE(info.has_value()) << name;
    EXPECT_FALSE(has_schema_params(info->input_schema)) << name;
  }
}

TEST_F(RegisteredServerFixture, CatalogCoversServerTools) {
  auto result = client->ListTools();
  for (const auto &tool : result.tools) {
    EXPECT_TRUE(catalog.get_tool(tool.name).has_value())
        << "tool registered on server but missing from catalog: " << tool.name;
  }
}

TEST(ToolResultIsErrorTest, DetectsErrorField) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["error"] = mcp::JsonValue("boom");
  EXPECT_TRUE(godot_autopilot::tool_result_is_error(result));
}

TEST(ToolResultIsErrorTest, IgnoresPlainObjectWithoutError) {
  mcp::JsonValue result(mcp::JsonValue::object_tag);
  result["result"] = mcp::JsonValue("ok");
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(result));
}

TEST(ToolResultIsErrorTest, IgnoresNonObjectResult) {
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(mcp::JsonValue("text")));
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(mcp::JsonValue()));
}

TEST_F(RegisteredServerFixture, BatchExecuteSchemaDeclaresRollbackOnError) {
  auto info = catalog.get_tool("batch_execute");
  ASSERT_TRUE(info.has_value());
  const auto *properties = info->input_schema.Find("properties");
  ASSERT_NE(properties, nullptr);
  const auto *rollback = properties->Find("rollback_on_error");
  ASSERT_NE(rollback, nullptr);
  ASSERT_TRUE(rollback->IsObject());
  const auto *type = rollback->Find("type");
  ASSERT_NE(type, nullptr);
  ASSERT_TRUE(type->IsString());
  EXPECT_EQ(type->GetString(), "boolean");
  const auto *default_value = rollback->Find("default");
  ASSERT_NE(default_value, nullptr);
  ASSERT_TRUE(default_value->IsBool());
  EXPECT_FALSE(default_value->GetBool());
  const auto *description = rollback->Find("description");
  ASSERT_NE(description, nullptr);
  ASSERT_TRUE(description->IsString());
  EXPECT_NE(description->GetString().find("rolled_back"), std::string::npos);
  const auto *required = info->input_schema.Find("required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsArray());
  for (const auto &entry : required->GetArray()) {
    ASSERT_TRUE(entry.IsString());
    EXPECT_NE(entry.GetString(), "rollback_on_error");
  }
}

TEST_F(RegisteredServerFixture, CallHandlerUnknownToolReturnsError) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  auto result = godot_autopilot::dispatch::call_handler("no_such_tool_xyz", args);
  const auto *err = result.Find("error");
  ASSERT_NE(err, nullptr);
  ASSERT_TRUE(err->IsString());
  const std::string &msg = err->GetString();
  EXPECT_NE(msg.find("domain tool 'no_such_tool_xyz' not found"),
            std::string::npos);
}

TEST_F(RegisteredServerFixture, CallHandlerOversizedArgsKeepsFixedError) {
  std::string big(600, 'x');
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  args["payload"] = mcp::JsonValue(big);

  auto result = godot_autopilot::dispatch::call_handler("no_such_tool_xyz", args);
  const auto *err = result.Find("error");
  ASSERT_NE(err, nullptr);
  ASSERT_TRUE(err->IsString());
  const std::string &msg = err->GetString();
  EXPECT_NE(msg.find("domain tool 'no_such_tool_xyz' not found"),
            std::string::npos);
  EXPECT_EQ(msg.find(big), std::string::npos)
      << "error message must not echo back oversized arguments";
}

TEST_F(RegisteredServerFixture, ReRegisterIsIdempotentForServerAndCatalog) {
  godot_autopilot::register_all_tools(*server, queue, catalog, index, 9527);

  auto result = client->ListTools();
  EXPECT_EQ(result.tools.size(), kMetaToolCount);
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
  EXPECT_EQ(index.size(), catalog.size());
}

TEST_F(RegisteredServerFixture, TwoIndependentServersRegisterIdentically) {
  auto pair2 = mcp::InMemoryTransport::CreatePair();
  auto server2 = mcp::McpServer::Create(pair2.server);

  godot_autopilot::CommandQueue queue2;
  godot_autopilot::ToolCatalog catalog2;
  godot_autopilot::Bm25Index index2;
  godot_autopilot::register_all_tools(*server2, queue2, catalog2, index2,
                                         9528);

  std::thread thread2([&server2] { server2->Run(); });

  mcp::ClientOptions cops2;
  cops2.client_info = mcp::Implementation{"GsdTestClient2", "1.0.0"};
  cops2.connect_mode = mcp::ConnectMode::Auto;
  auto client2 = mcp::McpClient::Create(pair2.client, cops2);

  auto result1 = client->ListTools();
  auto result2 = client2->ListTools();
  EXPECT_EQ(result1.tools.size(), kMetaToolCount);
  EXPECT_EQ(result2.tools.size(), kMetaToolCount);
  EXPECT_EQ(catalog2.size(), catalog.size());

  client2->Close();
  server2->Close();
  if (thread2.joinable())
    thread2.join();
}

TEST_F(RegisteredServerFixture, Bm25IndexPopulatedAfterRegistration) {
  EXPECT_EQ(index.size(), catalog.size());
}

TEST_F(RegisteredServerFixture, JsonValueParametersOmitTypeInSchema) {
  const char *const kJsonValueTools[] = {
      "property_set",
      "set_script_property",
      "execute_game_script",
      "csharp_set_property",
      "set_project_settings",
      "set_editor_settings",
      "insert_animation_keyframe",
      "set_physics_2d_body_state",
      "set_physics_3d_body_state",
      "set_physics_3d_area_param",
      "set_render_material_param",
      "set_render_shader_parameter_global",
      "set_resource_property",
      "add_editor_undo_redo_do",
      "add_editor_undo_redo_undo"};
  for (const char *name : kJsonValueTools) {
    auto info = catalog.get_tool(name);
    ASSERT_TRUE(info.has_value()) << name;
    const auto *props = info->input_schema.Find("properties");
    ASSERT_NE(props, nullptr) << name;
    const auto *value = props->Find("value");
    ASSERT_NE(value, nullptr) << name;
    EXPECT_EQ(value->Find("type"), nullptr) << name;
    EXPECT_NE(value->Find("description"), nullptr) << name;
  }
}

TEST_F(RegisteredServerFixture, SceneWritingToolsDeclareWritesScene) {
  const char *const kSceneWriters[] = {
      "property_set",
      "set_properties_batch",
      "set_node_transform",
      "create_scene_node",
      "rename_scene_node",
      "reparent_node",
      "instantiate_scene",
      "build_nodes_from_spec",
      "spawn_node_bundle",
      "signal_connect",
      "signal_disconnect",
      "attach_script_to_node",
      "detach_script_from_node",
      "add_group_node",
      "remove_group_node",
      "create_scene_animation_player",
      "insert_animation_keyframe",
      "create_tilemap",
      "set_tilemap_cell",
      "fill_tilemap_rect",
      "create_gridmap",
      "assign_collision_shape",
      "apply_theme_to_control",
      "set_control_layout",
      "set_particles_params",
      "assign_mesh_to_instance",
      "tween_editor_property"};
  for (const char *name : kSceneWriters) {
    mcp::JsonValue args(mcp::JsonValue::object_tag);
    args["name"] = mcp::JsonValue(name);
    auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
    const auto *tool = result.Find("tool");
    ASSERT_NE(tool, nullptr) << name;
    const auto *side_effect = tool->Find("side_effect");
    ASSERT_NE(side_effect, nullptr) << name;
    ASSERT_TRUE(side_effect->IsString()) << name;
    EXPECT_EQ(side_effect->GetString(), "writes_scene") << name;
  }
}

TEST_F(RegisteredServerFixture, ClickGameUiElementSchemaDeclaresStrictHit) {
  auto info = catalog.get_tool("click_game_ui_element");
  ASSERT_TRUE(info.has_value());
  const auto *props = info->input_schema.Find("properties");
  ASSERT_NE(props, nullptr);
  ASSERT_TRUE(props->IsObject());
  const auto *strict_hit = props->Find("strict_hit");
  ASSERT_NE(strict_hit, nullptr);
  ASSERT_TRUE(strict_hit->IsObject());
  const auto *type = strict_hit->Find("type");
  ASSERT_NE(type, nullptr);
  ASSERT_TRUE(type->IsString());
  EXPECT_EQ(type->GetString(), "boolean");
  const auto *description = strict_hit->Find("description");
  ASSERT_NE(description, nullptr);
  ASSERT_TRUE(description->IsString());
  EXPECT_NE(description->GetString().find("click_occluded"), std::string::npos);
  const auto *required = info->input_schema.Find("required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsArray());
  for (const auto &entry : required->GetArray()) {
    if (entry.IsString()) {
      EXPECT_NE(entry.GetString(), "strict_hit");
    }
  }
}

TEST_F(RegisteredServerFixture, GetGameUiElementsDescriptionDocumentsMouseFilterAndZIndex) {
  auto info = catalog.get_tool("get_game_ui_elements");
  ASSERT_TRUE(info.has_value());
  EXPECT_NE(info->description.find("mouse_filter"), std::string::npos);
  EXPECT_NE(info->description.find("IGNORE"), std::string::npos);
  EXPECT_NE(info->description.find("z_index"), std::string::npos);
}

TEST_F(RegisteredServerFixture, ClickGameUiElementDescriptionDocumentsHitDiagnostics) {
  auto info = catalog.get_tool("click_game_ui_element");
  ASSERT_TRUE(info.has_value());
  const char *const kExpected[] = {"hit_path",   "hit_mismatch", "warning",
                                   "strict_hit", "mouse_motion", "click_occluded"};
  for (const char *token : kExpected) {
    EXPECT_NE(info->description.find(token), std::string::npos) << token;
  }
}

TEST_F(RegisteredServerFixture, ValidateGameUiLayoutDescriptionDocumentsOccluderFields) {
  auto info = catalog.get_tool("validate_game_ui_layout");
  ASSERT_TRUE(info.has_value());
  const char *const kExpected[] = {
      "mouse_filter", "IGNORE", "effective z_index", "occluder_path",
      "occluder_mouse_filter", "possibly_occluded_count"};
  for (const char *token : kExpected) {
    EXPECT_NE(info->description.find(token), std::string::npos) << token;
  }
  EXPECT_EQ(info->description.find("ignores canvas_layer/z_index"),
            std::string::npos)
      << "stale occlusion wording must be gone";
  const auto *props = info->input_schema.Find("properties");
  ASSERT_NE(props, nullptr);
  const auto *ratio = props->Find("occlude_ratio");
  ASSERT_NE(ratio, nullptr);
  const auto *ratio_description = ratio->Find("description");
  ASSERT_NE(ratio_description, nullptr);
  ASSERT_TRUE(ratio_description->IsString());
  EXPECT_NE(ratio_description->GetString().find("effective z_index"),
            std::string::npos);
  EXPECT_NE(ratio_description->GetString().find("mouse_filter"),
            std::string::npos);
  EXPECT_EQ(ratio_description->GetString().find(
                "ignoring canvas_layer and z_index"),
            std::string::npos);
}

TEST_F(RegisteredServerFixture, RegistryIsSingleSourceOfTools) {
  auto reg = godot_autopilot::get_active_registry();

  EXPECT_EQ(reg->meta_size(), kMetaToolCount);

  EXPECT_EQ(reg->all_any().size(), catalog.size());

  for (const char* name : kMetaToolNames) {
    EXPECT_NE(reg->find_meta(name), nullptr) << "missing meta tool: " << name;
  }

  EXPECT_NE(reg->find("system_status"), nullptr);

  for (auto t : reg->all_any()) {
    EXPECT_TRUE(catalog.get_tool(t->meta().name).has_value())
        << "registry tool missing from catalog: " << t->meta().name;
  }

  for (const auto &tool : catalog.get_all_tools()) {
    EXPECT_NE(reg->find_any(tool.name), nullptr)
        << "catalog entry missing from registry: " << tool.name;
  }
}
