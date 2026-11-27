


#include <gtest/gtest.h>


#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>

#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_registry.hpp"
#include "support/tool_catalog_baseline.hpp"
#include "util/json_value.hpp"
#include "util/bm25_index.hpp"

#include <string>
#include <vector>

namespace {

constexpr size_t kMetaToolCount = 5;

const char *const kMetaToolNames[kMetaToolCount] = {
    "search_tools", "get_tool_detail", "call_tool", "batch_execute",
    "code_execute"};

const char *const kValidCategories[] = {
    "Analysis",   "Animation",  "Audio",      "Capture",    "Config",     "Debug",
    "Debugger",   "Display",    "Docs",       "Editor",     "Game",
    "Group",      "Input",      "Monitor",    "Navigation", "OS",         "Physics",
    "Properties", "Render",     "Resources",  "Scene",      "Scripts",
    "SpriteFrames", "System",   "Testing",    "Text",       "Theme",
    "TileMap",
    "Meta",       "Auto"};

bool has_schema_params(const godot_autopilot::util::JsonValue &schema) {
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

struct NativeRegistrationFixture : ::testing::Test {
  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;

  void SetUp() override {
    godot_autopilot::install_tool_handlers(queue, catalog, index, 9527);
    queue.drain();
    godot_autopilot::runtime_ops::set_editor_queue(&queue);
  }

  void TearDown() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::clear_active_registry();
  }
};

} // namespace

TEST_F(NativeRegistrationFixture, RegistryExposesExactlyFiveMetaTools) {
  const auto registry = godot_autopilot::get_active_registry();
  ASSERT_NE(registry, nullptr);
  ASSERT_EQ(registry->meta_size(), kMetaToolCount);
  EXPECT_EQ(registry->all_meta().size(), kMetaToolCount);
}

TEST_F(NativeRegistrationFixture, AllMetaToolNamesPresent) {
  const auto registry = godot_autopilot::get_active_registry();
  ASSERT_NE(registry, nullptr);
  for (const char *name : kMetaToolNames) {
    const auto tool = registry->find_meta(name);
    ASSERT_NE(tool, nullptr) << "meta tool missing from registry: " << name;
    EXPECT_EQ(tool->meta().name, name);
  }
}

TEST_F(NativeRegistrationFixture, MetaToolAttributesNonEmpty) {
  const auto registry = godot_autopilot::get_active_registry();
  ASSERT_NE(registry, nullptr);
  for (const auto &tool : registry->all_meta()) {
    EXPECT_FALSE(tool->meta().name.empty());
    EXPECT_FALSE(tool->meta().description.empty())
        << "meta tool has no description: " << tool->meta().name;
    EXPECT_TRUE(catalog.get_tool(tool->meta().name).has_value())
        << "meta tool missing from catalog: " << tool->meta().name;
  }
}

TEST_F(NativeRegistrationFixture, CatalogToolCountMatches) {
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
}

TEST_F(NativeRegistrationFixture, CatalogEntriesWellFormed) {
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

TEST_F(NativeRegistrationFixture, SchemaStatisticsBaseline) {
  size_t non_empty = 0;
  size_t empty = 0;
  std::string empty_names;
  const auto tools = catalog.get_all_tools();
  for (const auto &tool : tools) {
    if (has_schema_params(tool.input_schema)) {
      ++non_empty;
    } else {
      ++empty;
      if (!empty_names.empty()) {
        empty_names += ", ";
      }
      empty_names += tool.name;
    }
  }
  const gda_test::ToolCatalogBaseline &baseline =
      gda_test::load_tool_catalog_baseline();
  EXPECT_EQ(tools.size(), baseline.expected_catalog_total);
  EXPECT_EQ(non_empty, baseline.expected_non_empty);
  EXPECT_EQ(empty, baseline.expected_empty) << empty_names;
  EXPECT_EQ(non_empty + empty, tools.size());
  EXPECT_GT(non_empty, empty);
  EXPECT_GT(empty, 0);
}

TEST_F(NativeRegistrationFixture, ReloadScriptSchemaExposesKeepStateOptional) {
  auto info = catalog.get_tool("reload_script");
  ASSERT_TRUE(info.has_value());
  const godot_autopilot::util::JsonValue *props = info->input_schema.Find("properties");
  ASSERT_NE(props, nullptr);
  ASSERT_TRUE(props->IsObject());
  const godot_autopilot::util::JsonValue *keep = props->Find("keep_state");
  ASSERT_NE(keep, nullptr);
  ASSERT_TRUE(keep->IsObject());
  EXPECT_EQ(keep->At("type").GetString(), "boolean");
  const godot_autopilot::util::JsonValue *required = info->input_schema.Find("required");
  ASSERT_NE(required, nullptr);
  ASSERT_TRUE(required->IsArray());
  for (const auto &item : required->GetArray()) {
    EXPECT_NE(item.GetString(), "keep_state");
  }
}

TEST_F(NativeRegistrationFixture, ToolDetailSideEffectIsMachineReadable) {
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["name"] = godot_autopilot::util::JsonValue("code_execute");
  auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
  const auto *tool = result.Find("tool");
  ASSERT_NE(tool, nullptr);
  ASSERT_TRUE(tool->IsObject());
  const auto *side_effect = tool->Find("side_effect");
  ASSERT_NE(side_effect, nullptr);
  ASSERT_TRUE(side_effect->IsString());
  EXPECT_EQ(side_effect->GetString(), "code_execute");
}

TEST_F(NativeRegistrationFixture, EveryToolDetailReportsNonEmptySideEffect) {
  const auto tools = catalog.get_all_tools();
  ASSERT_GT(tools.size(), size_t{0});
  for (const auto &tool : tools) {
    godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
    args["name"] = godot_autopilot::util::JsonValue(tool.name);
    auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
    const auto *info = result.Find("tool");
    ASSERT_NE(info, nullptr) << tool.name;
    const auto *side_effect = info->Find("side_effect");
    ASSERT_NE(side_effect, nullptr) << tool.name;
    ASSERT_TRUE(side_effect->IsString()) << tool.name;
    EXPECT_FALSE(side_effect->GetString().empty()) << tool.name;
  }
}

TEST_F(NativeRegistrationFixture, SchemaSampledTools) {
  const char *const kNonEmpty[] = {
      "property_get",          "intersect_physics_2d_ray",
      "execute_script",        "call_scene_tree_group",
      "set_tilemap_cells",     "game_input",
      "get_scene_tree",        "batch_execute",
      "call_tool",             "code_execute",
      "search_tools",          "get_tool_detail"};
  const char *const kEmpty[] = {"get_os_unique_id",
                                "create_physics_2d_body", "system_status"};
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

TEST_F(NativeRegistrationFixture, CatalogCoversMetaTools) {
  for (const char *name : kMetaToolNames) {
    EXPECT_TRUE(catalog.get_tool(name).has_value())
        << "meta tool registered but missing from catalog: " << name;
  }
  const auto registry = godot_autopilot::get_active_registry();
  ASSERT_NE(registry, nullptr);
  for (const auto &tool : registry->all_any()) {
    EXPECT_TRUE(catalog.get_tool(tool->meta().name).has_value())
        << "registry tool missing from catalog: " << tool->meta().name;
  }
}

TEST(ToolResultIsErrorTest, DetectsErrorField) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["error"] = godot_autopilot::util::JsonValue("boom");
  EXPECT_TRUE(godot_autopilot::tool_result_is_error(result));
}

TEST(ToolResultIsErrorTest, IgnoresPlainObjectWithoutError) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["result"] = godot_autopilot::util::JsonValue("ok");
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(result));
}

TEST(ToolResultIsErrorTest, IgnoresNonObjectResult) {
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(godot_autopilot::util::JsonValue("text")));
  EXPECT_FALSE(godot_autopilot::tool_result_is_error(godot_autopilot::util::JsonValue()));
}

TEST(ToolResultIsErrorTest, DetectsStructuredErrorCode) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  godot_autopilot::util::JsonValue structured(godot_autopilot::util::JsonValue::object_tag);
  structured["code"] = godot_autopilot::util::JsonValue("when_timeout");
  result["structured_error"] = std::move(structured);
  EXPECT_TRUE(godot_autopilot::tool_result_is_error(result));
}

TEST(ToolResultIsErrorTest, DetectsRuntimeErrorFlag) {
  godot_autopilot::util::JsonValue result(godot_autopilot::util::JsonValue::object_tag);
  result["runtime_error"] = godot_autopilot::util::JsonValue(true);
  EXPECT_TRUE(godot_autopilot::tool_result_is_error(result));
}

TEST_F(NativeRegistrationFixture, BatchExecuteSchemaDeclaresRollbackOnError) {
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

TEST_F(NativeRegistrationFixture,
       ExportStartingAfterSubmissionBlocksFinalToolExecution) {
  godot_autopilot::ExportState export_state;
  std::unique_ptr<godot_autopilot::ExportState::Scope> export_scope;
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["query"] = godot_autopilot::util::JsonValue("scene");
  auto completed = queue.submit([&args]() {
    return godot_autopilot::dispatch::call_handler("search_tools", args);
  });

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(2);
  while (queue.stats().pending == 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const bool queued = queue.stats().pending == 1u;
  export_scope =
      std::make_unique<godot_autopilot::ExportState::Scope>(export_state);
  queue.drain();
  export_scope.reset();

  const std::string response = completed.get().Dump();

  ASSERT_TRUE(queued);
  EXPECT_NE(response.find(godot_autopilot::dispatch::export_blocked_error_text()),
            std::string::npos);
}

TEST_F(NativeRegistrationFixture, CallHandlerUnknownToolReturnsError) {
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  auto result = godot_autopilot::dispatch::call_handler("no_such_tool_xyz", args);
  const auto *err = result.Find("error");
  ASSERT_NE(err, nullptr);
  ASSERT_TRUE(err->IsString());
  const std::string &msg = err->GetString();
  EXPECT_NE(msg.find("domain tool 'no_such_tool_xyz' not found"),
            std::string::npos);
}

TEST_F(NativeRegistrationFixture, CallHandlerOversizedArgsKeepsFixedError) {
  std::string big(600, 'x');
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["payload"] = godot_autopilot::util::JsonValue(big);

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

TEST_F(NativeRegistrationFixture, ReinstallHandlersIsIdempotent) {
  const auto registry = godot_autopilot::install_tool_handlers(
      queue, catalog, index, 9527);
  ASSERT_NE(registry, nullptr);
  EXPECT_EQ(registry->meta_size(), kMetaToolCount);
  EXPECT_EQ(catalog.size(), catalog.get_all_tools().size());
  EXPECT_EQ(index.size(), catalog.size());

  const auto result = godot_autopilot::dispatch::call_handler(
      "get_tool_detail",
      godot_autopilot::util::JsonValue::Parse(R"({"name":"search_tools"})"));
  const auto *tool = result.Find("tool");
  ASSERT_NE(tool, nullptr);
  EXPECT_EQ((*tool)["name"].GetString(), "search_tools");
}

TEST_F(NativeRegistrationFixture, TwoIndependentRegistrationsAreIsolated) {
  godot_autopilot::CommandQueue queue2;
  godot_autopilot::ToolCatalog catalog2;
  godot_autopilot::Bm25Index index2;
  const auto registry2 = godot_autopilot::install_tool_handlers(queue2, catalog2, index2, 9528);
  ASSERT_NE(registry2, nullptr);
  EXPECT_EQ(registry2->meta_size(), kMetaToolCount);
  EXPECT_EQ(registry2->all_any().size(), catalog2.size());
  EXPECT_EQ(catalog2.size(), catalog.size());
  EXPECT_EQ(index2.size(), catalog2.size());
  godot_autopilot::dispatch::clear_handlers();
  godot_autopilot::clear_active_registry();
}

TEST(RegisterAllSurfaceTest, FullSurfaceRegistersAllTools) {
  godot_autopilot::CommandQueue full_queue;
  godot_autopilot::ToolCatalog full_catalog;
  godot_autopilot::Bm25Index full_index;
  const auto registry = godot_autopilot::install_tool_handlers(
      full_queue, full_catalog, full_index, 9530,
      godot_autopilot::ToolSurface::Full);
  ASSERT_NE(registry, nullptr);
  EXPECT_EQ(full_catalog.size(), full_index.size());
  EXPECT_EQ(registry->all_any().size(), full_catalog.size());
  EXPECT_GT(full_catalog.size(), static_cast<size_t>(7));
  EXPECT_TRUE(full_catalog.get_tool("create_scene_node").has_value());
  godot_autopilot::dispatch::clear_handlers();
  godot_autopilot::clear_active_registry();
}

TEST(RegisterAllSurfaceTest, FullSurfaceDynamicRefreshKeepsRegistryInSync) {
  godot_autopilot::CommandQueue full_queue;
  godot_autopilot::ToolCatalog full_catalog;
  godot_autopilot::Bm25Index full_index;
  godot_autopilot::install_tool_handlers(
      full_queue, full_catalog, full_index, 9531,
      godot_autopilot::ToolSurface::Full);
  const size_t before = full_catalog.size();
  {
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::ToolSpec spec{
        "gda_test_dynamic_tool", "Dynamic test tool", "Testing", {"dynamic"}};
    spec.side_effect = godot_autopilot::SideEffect::None;
    spec.handler = [](const godot_autopilot::util::JsonValue &) {
      return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
    };
    godot_autopilot::dynamic_specs::store().push_back(std::move(spec));
  }
  godot_autopilot::refresh_dynamic_tools();
  auto registry = godot_autopilot::get_active_registry();
  EXPECT_TRUE(full_catalog.get_tool("gda_test_dynamic_tool").has_value());
  EXPECT_NE(registry->find_any("gda_test_dynamic_tool"), nullptr);
  {
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::dynamic_specs::store().clear();
  }
  godot_autopilot::refresh_dynamic_tools();
  registry = godot_autopilot::get_active_registry();
  EXPECT_FALSE(full_catalog.get_tool("gda_test_dynamic_tool").has_value());
  EXPECT_EQ(registry->find_any("gda_test_dynamic_tool"), nullptr);
  EXPECT_EQ(full_catalog.size(), before);
  godot_autopilot::dispatch::clear_handlers();
  godot_autopilot::clear_active_registry();
}

TEST(RegisterAllSurfaceTest, CallToolMetaToolIsRegisteredForNativeTasks) {
  godot_autopilot::CommandQueue task_queue;
  godot_autopilot::ToolCatalog task_catalog;
  godot_autopilot::Bm25Index task_index;
  const auto registry = godot_autopilot::install_tool_handlers(
      task_queue, task_catalog, task_index, 9532);
  ASSERT_NE(registry, nullptr);
  ASSERT_NE(registry->find_meta("call_tool"), nullptr);
  const auto info = task_catalog.get_tool("call_tool");
  ASSERT_TRUE(info.has_value());
  const auto *props = info->input_schema.Find("properties");
  ASSERT_NE(props, nullptr);
  ASSERT_TRUE(props->IsObject());
  EXPECT_NE(props->Find("name"), nullptr);
  EXPECT_NE(props->Find("arguments"), nullptr);
  godot_autopilot::dispatch::clear_handlers();
  godot_autopilot::clear_active_registry();
}

TEST_F(NativeRegistrationFixture, Bm25IndexPopulatedAfterRegistration) {
  EXPECT_EQ(index.size(), catalog.size());
}

TEST_F(NativeRegistrationFixture, JsonValueParametersOmitTypeInSchema) {
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

TEST_F(NativeRegistrationFixture, SceneWritingToolsDeclareWritesScene) {
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
    godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
    args["name"] = godot_autopilot::util::JsonValue(name);
    auto result = godot_autopilot::dispatch::call_handler("get_tool_detail", args);
    const auto *tool = result.Find("tool");
    ASSERT_NE(tool, nullptr) << name;
    const auto *side_effect = tool->Find("side_effect");
    ASSERT_NE(side_effect, nullptr) << name;
    ASSERT_TRUE(side_effect->IsString()) << name;
    EXPECT_EQ(side_effect->GetString(), "writes_scene") << name;
  }
}

TEST_F(NativeRegistrationFixture, ClickGameUiElementSchemaDeclaresStrictHit) {
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

TEST_F(NativeRegistrationFixture, GetGameUiElementsDescriptionDocumentsMouseFilterAndZIndex) {
  auto info = catalog.get_tool("get_game_ui_elements");
  ASSERT_TRUE(info.has_value());
  EXPECT_NE(info->description.find("mouse_filter"), std::string::npos);
  EXPECT_NE(info->description.find("IGNORE"), std::string::npos);
  EXPECT_NE(info->description.find("z_index"), std::string::npos);
}

TEST_F(NativeRegistrationFixture, ClickGameUiElementDescriptionDocumentsHitDiagnostics) {
  auto info = catalog.get_tool("click_game_ui_element");
  ASSERT_TRUE(info.has_value());
  const char *const kExpected[] = {"hit_path",   "hit_mismatch", "warning",
                                   "strict_hit", "mouse_motion", "click_occluded"};
  for (const char *token : kExpected) {
    EXPECT_NE(info->description.find(token), std::string::npos) << token;
  }
}

TEST_F(NativeRegistrationFixture, ValidateGameUiLayoutDescriptionDocumentsOccluderFields) {
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

TEST_F(NativeRegistrationFixture, RegistryIsSingleSourceOfTools) {
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
