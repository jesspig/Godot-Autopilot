#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <mcp/JsonValue.hpp>

#include "tools/base/tool_spec.hpp"
#include "tools/domains/audio/audio_tools.hpp"
#include "tools/domains/capture/capture_tools.hpp"
#include "tools/domains/scene/scene_tools.hpp"
#include "tools/domains/debug/debug_tools.hpp"
#include "tools/domains/doc/doc_tools.hpp"
#include "tools/domains/gridmap/gridmap_tools.hpp"
#include "tools/domains/input_map/input_map_tools.hpp"
#include "tools/domains/property/property_tools.hpp"
#include "tools/domains/resource/resource_tools.hpp"
#include "tools/domains/theme/theme_tools.hpp"
#include "tools/domains/tilemap/tilemap_tools.hpp"

namespace {

using godot_autopilot::SpecTool;
using godot_autopilot::ToolAlias;
using godot_autopilot::ToolBase;
using godot_autopilot::apply_alias_args;
using godot_autopilot::apply_alias_result;

mcp::JsonValue object() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

const ToolAlias *find_alias(
    const std::vector<std::unique_ptr<ToolBase>> &tools,
    const std::string &canonical, const std::string &alias_name) {
  const ToolAlias *fallback = nullptr;
  for (const auto &tool : tools) {
    const auto *spec_tool = dynamic_cast<const SpecTool *>(tool.get());
    if (spec_tool == nullptr) {
      continue;
    }
    for (const auto &alias : spec_tool->spec().aliases) {
      if (alias.name != alias_name) {
        continue;
      }
      if (tool->meta().name == canonical) {
        return &alias;
      }
      fallback = &alias;
    }
  }
  return fallback;
}

std::string string_field(const mcp::JsonValue &value, const char *key) {
  const mcp::JsonValue *found = value.Find(key);
  return found != nullptr && found->IsString() ? found->GetString()
                                               : std::string();
}

std::string error_text(const mcp::JsonValue &value) {
  const mcp::JsonValue *error = value.Find("error");
  return error != nullptr && error->IsString() ? error->GetString()
                                               : std::string();
}

TEST(AliasProjectionTest, AudioAliasesProjectLegacyShapes) {
  const auto tools = godot_autopilot::audio_tools::make_tools();
  const ToolAlias *count =
      find_alias(tools, "get_audio_bus_layout", "get_audio_bus_count");
  ASSERT_NE(count, nullptr);
  const mcp::JsonValue count_args = apply_alias_args(*count, object());
  EXPECT_EQ(count_args["action"].GetString(), "get_info");

  const ToolAlias *name =
      find_alias(tools, "get_audio_bus_layout", "get_audio_bus_name");
  ASSERT_NE(name, nullptr);
  mcp::JsonValue name_input = object();
  name_input["bus_index"] = mcp::JsonValue(static_cast<int64_t>(2));
  const mcp::JsonValue name_args = apply_alias_args(*name, name_input);
  EXPECT_EQ(name_args["action"].GetString(), "get_info");
  EXPECT_EQ(name_args["bus_index"].GetInt(), 2);

  const ToolAlias *devices = find_alias(
      tools, "get_audio_input_devices", "get_audio_device_inputs");
  ASSERT_NE(devices, nullptr);
  mcp::JsonValue devices_result = object();
  mcp::JsonValue devices_inner = object();
  mcp::JsonValue device_names(mcp::JsonValue::array_tag);
  device_names.PushBack(mcp::JsonValue("mic"));
  devices_inner["devices"] = std::move(device_names);
  devices_inner["current"] = mcp::JsonValue("mic");
  devices_result["result"] = std::move(devices_inner);
  const mcp::JsonValue projected =
      apply_alias_result(*devices, devices_result);
  const mcp::JsonValue *projected_result = projected.Find("result");
  ASSERT_NE(projected_result, nullptr);
  ASSERT_TRUE(projected_result->IsArray());
  ASSERT_EQ(projected_result->Size(), 1u);
  EXPECT_EQ((*projected_result)[0].GetString(), "mic");
}

TEST(AliasProjectionTest, DebugAliasesProjectLegacyShapes) {
  const auto tools = godot_autopilot::debug_tools::make_tools();
  const ToolAlias *monitor =
      find_alias(tools, "get_debug_monitors", "get_debug_monitor");
  ASSERT_NE(monitor, nullptr);
  ASSERT_NE(monitor->validate_args, nullptr);
  const mcp::JsonValue missing = monitor->validate_args(object());
  EXPECT_NE(error_text(missing).find("monitor"), std::string::npos);

  mcp::JsonValue monitor_input = object();
  monitor_input["monitor"] = mcp::JsonValue(static_cast<int64_t>(9));
  const mcp::JsonValue monitor_args =
      apply_alias_args(*monitor, monitor_input);
  EXPECT_EQ(monitor_args["action"].GetString(), "monitors");
  const mcp::JsonValue *ids = monitor_args.Find("ids");
  ASSERT_NE(ids, nullptr);
  ASSERT_TRUE(ids->IsArray());
  ASSERT_EQ(ids->Size(), 1u);
  EXPECT_EQ((*ids)[0].GetInt(), 9);

  mcp::JsonValue monitor_result = object();
  mcp::JsonValue monitors(mcp::JsonValue::array_tag);
  mcp::JsonValue monitor_item = object();
  monitor_item["value"] = mcp::JsonValue(42.5);
  monitors.PushBack(std::move(monitor_item));
  monitor_result["result"] = std::move(monitors);
  const mcp::JsonValue projected =
      apply_alias_result(*monitor, monitor_result);
  const mcp::JsonValue *value = projected.Find("result");
  ASSERT_NE(value, nullptr);
  EXPECT_DOUBLE_EQ(value->GetDouble(), 42.5);

  const ToolAlias *catalog = find_alias(
      tools, "get_debug_monitors", "get_debug_monitor_catalog");
  ASSERT_NE(catalog, nullptr);
  const mcp::JsonValue catalog_args = apply_alias_args(*catalog, object());
  EXPECT_EQ(catalog_args["action"].GetString(), "catalog");

  const ToolAlias *objects = find_alias(
      tools, "get_debug_monitors", "get_debug_object_count");
  const ToolAlias *memory = find_alias(
      tools, "get_debug_monitors", "get_debug_memory_usage");
  const ToolAlias *nodes = find_alias(
      tools, "get_debug_monitors", "get_debug_node_count");
  ASSERT_NE(objects, nullptr);
  ASSERT_NE(memory, nullptr);
  ASSERT_NE(nodes, nullptr);
  EXPECT_EQ(apply_alias_args(*objects, object())["action"].GetString(),
            "object");
  EXPECT_EQ(apply_alias_args(*memory, object())["action"].GetString(),
            "memory");
  EXPECT_EQ(apply_alias_args(*nodes, object())["action"].GetString(), "node");

  const ToolAlias *custom = find_alias(
      tools, "get_debug_monitors", "get_debug_custom_monitor_names");
  ASSERT_NE(custom, nullptr);
  const mcp::JsonValue custom_args = apply_alias_args(*custom, object());
  EXPECT_EQ(custom_args["action"].GetString(), "custom");
  mcp::JsonValue custom_result = object();
  mcp::JsonValue custom_inner = object();
  mcp::JsonValue custom_names(mcp::JsonValue::array_tag);
  custom_names.PushBack(mcp::JsonValue("custom"));
  custom_inner["custom_names"] = std::move(custom_names);
  custom_result["result"] = std::move(custom_inner);
  const mcp::JsonValue custom_projected =
      apply_alias_result(*custom, custom_result);
  const mcp::JsonValue *custom_output = custom_projected.Find("result");
  ASSERT_NE(custom_output, nullptr);
  ASSERT_TRUE(custom_output->IsArray());
  EXPECT_EQ((*custom_output)[0].GetString(), "custom");
}

TEST(AliasProjectionTest, DocsAliasesProjectLegacyShapes) {
  const auto tools = godot_autopilot::doc_tools::make_tools();
  const ToolAlias *method =
      find_alias(tools, "get_docs_class", "get_docs_method");
  ASSERT_NE(method, nullptr);
  ASSERT_NE(method->validate_args, nullptr);
  const mcp::JsonValue missing = method->validate_args(object());
  EXPECT_NE(error_text(missing).find("class"), std::string::npos);

  mcp::JsonValue method_input = object();
  method_input["class"] = mcp::JsonValue("Node2D");
  method_input["method"] = mcp::JsonValue("queue_free");
  const mcp::JsonValue method_args =
      apply_alias_args(*method, method_input);
  EXPECT_EQ(method_args["section"].GetString(), "methods");
  EXPECT_EQ(method_args["member"].GetString(), "queue_free");
  EXPECT_TRUE(method_args["exact"].GetBool());

  mcp::JsonValue method_result = object();
  mcp::JsonValue method_inner = object();
  mcp::JsonValue methods(mcp::JsonValue::array_tag);
  mcp::JsonValue method_item = object();
  method_item["name"] = mcp::JsonValue("queue_free");
  methods.PushBack(std::move(method_item));
  method_inner["methods"] = std::move(methods);
  method_result["result"] = std::move(method_inner);
  const mcp::JsonValue method_projected =
      apply_alias_result(*method, method_result);
  const mcp::JsonValue *method_output = method_projected.Find("result");
  ASSERT_NE(method_output, nullptr);
  EXPECT_EQ((*method_output)["name"].GetString(), "queue_free");
  EXPECT_EQ((*method_output)["note"].GetString(),
            "method signature without docstrings");

  const ToolAlias *property =
      find_alias(tools, "get_docs_class", "get_docs_property");
  ASSERT_NE(property, nullptr);
  mcp::JsonValue property_input = object();
  property_input["class"] = mcp::JsonValue("Node2D");
  property_input["property"] = mcp::JsonValue("position");
  const mcp::JsonValue property_args =
      apply_alias_args(*property, property_input);
  EXPECT_EQ(property_args["section"].GetString(), "properties");
  EXPECT_EQ(property_args["member"].GetString(), "position");
  EXPECT_TRUE(property_args["exact"].GetBool());

  mcp::JsonValue property_result = object();
  mcp::JsonValue property_inner = object();
  mcp::JsonValue properties(mcp::JsonValue::array_tag);
  mcp::JsonValue property_item = object();
  property_item["name"] = mcp::JsonValue("position");
  properties.PushBack(std::move(property_item));
  property_inner["properties"] = std::move(properties);
  property_result["result"] = std::move(property_inner);
  const mcp::JsonValue property_projected =
      apply_alias_result(*property, property_result);
  const mcp::JsonValue *property_output = property_projected.Find("result");
  ASSERT_NE(property_output, nullptr);
  EXPECT_EQ((*property_output)["name"].GetString(), "position");
}

TEST(AliasProjectionTest, ThemeAliasesProjectLegacyShapes) {
  const auto tools = godot_autopilot::theme_tools::make_tools();
  const ToolAlias *color = find_alias(
      tools, "set_theme_item_bundle", "set_theme_color");
  ASSERT_NE(color, nullptr);
  mcp::JsonValue color_input = object();
  color_input["path"] = mcp::JsonValue("res://theme.tres");
  color_input["theme_type"] = mcp::JsonValue("Button");
  color_input["varname"] = mcp::JsonValue("font_color");
  color_input["color"] = mcp::JsonValue("#ffffff");
  const mcp::JsonValue color_args = apply_alias_args(*color, color_input);
  const mcp::JsonValue *colors = color_args.Find("colors");
  ASSERT_NE(colors, nullptr);
  const mcp::JsonValue *button = colors->Find("Button");
  ASSERT_NE(button, nullptr);
  EXPECT_EQ((*button)["font_color"].GetString(), "#ffffff");

  mcp::JsonValue color_result = object();
  color_result["path"] = mcp::JsonValue("res://theme.tres");
  color_result["saved"] = mcp::JsonValue(true);
  color_result["verified"] = mcp::JsonValue(true);
  const mcp::JsonValue projected =
      apply_alias_result(*color, color_result, color_input);
  EXPECT_EQ(string_field(projected, "theme_type"), "Button");
  EXPECT_EQ(string_field(projected, "varname"), "font_color");
  EXPECT_TRUE(projected["saved"].GetBool());

  const ToolAlias *anchor = find_alias(
      tools, "set_control_layout", "set_control_anchor_preset");
  ASSERT_NE(anchor, nullptr);
  mcp::JsonValue anchor_input = object();
  anchor_input["control_path"] = mcp::JsonValue("HUD");
  anchor_input["preset"] = mcp::JsonValue(static_cast<int64_t>(8));
  const mcp::JsonValue anchor_args = apply_alias_args(*anchor, anchor_input);
  EXPECT_EQ(string_field(anchor_args, "path"), "HUD");
  const mcp::JsonValue anchor_projected = apply_alias_result(
      *anchor, mcp::JsonValue(mcp::JsonValue::object_tag), anchor_input);
  EXPECT_EQ(string_field(anchor_projected, "control_path"), "HUD");
  EXPECT_EQ(anchor_projected["preset"].GetInt(), 8);
}

TEST(AliasProjectionTest, TileGridAliasesProjectLegacyShapes) {
  const auto tilemap = godot_autopilot::tilemap_tools::make_tools();
  const ToolAlias *cell =
      find_alias(tilemap, "set_tilemap_cells", "set_tilemap_cell");
  ASSERT_NE(cell, nullptr);
  mcp::JsonValue cell_input = object();
  cell_input["path"] = mcp::JsonValue("TileMap");
  cell_input["x"] = mcp::JsonValue(static_cast<int64_t>(1));
  cell_input["y"] = mcp::JsonValue(static_cast<int64_t>(2));
  const mcp::JsonValue cell_args = apply_alias_args(*cell, cell_input);
  EXPECT_EQ(string_field(cell_args, "node_path"), "TileMap");
  const mcp::JsonValue *cells = cell_args.Find("cells");
  ASSERT_NE(cells, nullptr);
  ASSERT_EQ(cells->Size(), 1u);
  EXPECT_EQ((*cells)[0]["source_id"].GetInt(), 0);
  const mcp::JsonValue projected =
      apply_alias_result(*cell, mcp::JsonValue(mcp::JsonValue::object_tag));
  EXPECT_EQ(string_field(projected, "result"), "ok");

  const auto gridmap = godot_autopilot::gridmap_tools::make_tools();
  const ToolAlias *grid_cell =
      find_alias(gridmap, "fill_gridmap_box", "set_gridmap_cell");
  ASSERT_NE(grid_cell, nullptr);
  mcp::JsonValue grid_input = object();
  grid_input["node_path"] = mcp::JsonValue("GridMap");
  mcp::JsonValue position = object();
  position["x"] = mcp::JsonValue(static_cast<int64_t>(1));
  position["y"] = mcp::JsonValue(static_cast<int64_t>(2));
  position["z"] = mcp::JsonValue(static_cast<int64_t>(3));
  grid_input["position"] = position;
  grid_input["item"] = mcp::JsonValue(static_cast<int64_t>(-1));
  const mcp::JsonValue grid_args = apply_alias_args(*grid_cell, grid_input);
  EXPECT_TRUE(grid_args["erase"].GetBool());
  EXPECT_EQ(grid_args["from"]["x"].GetInt(), 1);
}

TEST(AliasProjectionTest, InputMapAliasesProjectLegacyShapes) {
  const auto tools = godot_autopilot::input_map_tools::make_tools();
  const ToolAlias *add = find_alias(
      tools, "set_input_map_action_bundle", "add_input_map_action");
  ASSERT_NE(add, nullptr);
  mcp::JsonValue add_input = object();
  add_input["action"] = mcp::JsonValue("jump");
  const mcp::JsonValue add_args = apply_alias_args(*add, add_input);
  EXPECT_TRUE(add_args["persist"].GetBool());
  EXPECT_DOUBLE_EQ(add_args["deadzone"].GetDouble(), 0.5);

  const ToolAlias *has = find_alias(
      tools, "get_input_map_actions", "has_input_map_action");
  ASSERT_NE(has, nullptr);
  mcp::JsonValue has_result = object();
  mcp::JsonValue names(mcp::JsonValue::array_tag);
  names.PushBack(mcp::JsonValue("jump"));
  has_result["result"] = std::move(names);
  const mcp::JsonValue projected = apply_alias_result(*has, has_result);
  EXPECT_TRUE(projected["result"].GetBool());
}

TEST(AliasProjectionTest, ResourceScriptPropertyAliasesProjectLegacyShapes) {
  const auto resources = godot_autopilot::resource_tools::make_tools();
  const ToolAlias *dependency = find_alias(
      resources, "get_resource_dependencies", "has_resource_dependency");
  ASSERT_NE(dependency, nullptr);
  mcp::JsonValue dependency_input = object();
  dependency_input["path"] = mcp::JsonValue("res://a.tscn");
  dependency_input["dependency"] = mcp::JsonValue("res://b.tres");
  const mcp::JsonValue dependency_args =
      apply_alias_args(*dependency, dependency_input);
  mcp::JsonValue dependency_result = object();
  mcp::JsonValue dependencies(mcp::JsonValue::array_tag);
  dependencies.PushBack(mcp::JsonValue("res://b.tres"));
  dependency_result["result"] = std::move(dependencies);
  const mcp::JsonValue dependency_projected = apply_alias_result(
      *dependency, dependency_result, dependency_input);
  EXPECT_TRUE(dependency_projected["result"].GetBool());

  const ToolAlias *move =
      find_alias(resources, "rename_resource_file", "move_resource_file");
  ASSERT_NE(move, nullptr);

  const ToolAlias *load_script =
      find_alias(resources, "load_resource", "load_script");
  ASSERT_NE(load_script, nullptr);
  mcp::JsonValue script_input = object();
  script_input["path"] = mcp::JsonValue("res://player.gd");
  script_input["fresh"] = mcp::JsonValue(true);
  const mcp::JsonValue script_args =
      apply_alias_args(*load_script, script_input);
  EXPECT_EQ(string_field(script_args, "type_hint"), "Script");
  EXPECT_EQ(string_field(script_args, "cache_mode"), "ignore");

  const auto properties = godot_autopilot::property_tools::make_tools();
  const ToolAlias *batch =
      find_alias(properties, "property_set", "set_properties_batch");
  ASSERT_NE(batch, nullptr);
  mcp::JsonValue batch_input = object();
  batch_input["path"] = mcp::JsonValue("Player");
  mcp::JsonValue batch_properties = object();
  batch_properties["visible"] = mcp::JsonValue(false);
  batch_input["properties"] = std::move(batch_properties);
  const mcp::JsonValue batch_args = apply_alias_args(*batch, batch_input);
  EXPECT_TRUE(batch_args["atomic"].GetBool());
}

TEST(AliasProjectionTest, SceneAndCaptureAliasesProjectLegacyShapes) {
  const auto scene_tools = godot_autopilot::scene_tools::make_tools();
  const ToolAlias *snapshot =
      find_alias(scene_tools, "get_scene_tree", "get_scene_snapshot");
  ASSERT_NE(snapshot, nullptr);
  const mcp::JsonValue snapshot_args =
      apply_alias_args(*snapshot, object());
  EXPECT_TRUE(snapshot_args["verify"].GetBool());

  mcp::JsonValue explicit_verify = object();
  explicit_verify["verify"] = mcp::JsonValue(false);
  const mcp::JsonValue kept_verify =
      apply_alias_args(*snapshot, explicit_verify);
  EXPECT_FALSE(kept_verify["verify"].GetBool());

  const ToolAlias *rect = find_alias(
      scene_tools, "get_scene_tree", "get_scene_node_screen_rect");
  ASSERT_NE(rect, nullptr);
  ASSERT_NE(rect->validate_args, nullptr);
  const mcp::JsonValue empty_paths = rect->validate_args(object());
  EXPECT_EQ(error_text(empty_paths),
            "missing required parameter: paths (array)");
  mcp::JsonValue bad_paths = object();
  mcp::JsonValue empty_array(mcp::JsonValue::array_tag);
  bad_paths["paths"] = std::move(empty_array);
  EXPECT_EQ(error_text(rect->validate_args(bad_paths)),
            "parameter 'paths' must contain between 1 and 50 entries");

  mcp::JsonValue rect_input = object();
  mcp::JsonValue paths(mcp::JsonValue::array_tag);
  paths.PushBack(mcp::JsonValue("Player"));
  rect_input["paths"] = std::move(paths);
  rect_input["viewport"] = mcp::JsonValue("2d");
  const mcp::JsonValue rect_args = apply_alias_args(*rect, rect_input);
  EXPECT_TRUE(rect_args["paths"].IsArray());
  EXPECT_EQ(string_field(rect_args, "viewport"), "2d");

  mcp::JsonValue rect_canonical = object();
  mcp::JsonValue rect_inner = object();
  rect_inner["tree"] = object();
  mcp::JsonValue nodes = object();
  mcp::JsonValue items(mcp::JsonValue::array_tag);
  items.PushBack(object());
  nodes["items"] = std::move(items);
  nodes["viewport"] = mcp::JsonValue("2d");
  rect_inner["nodes"] = std::move(nodes);
  rect_canonical["result"] = std::move(rect_inner);
  const mcp::JsonValue rect_projected =
      apply_alias_result(*rect, rect_canonical);
  const mcp::JsonValue *rect_result = rect_projected.Find("result");
  ASSERT_NE(rect_result, nullptr);
  ASSERT_TRUE(rect_result->IsObject());
  EXPECT_EQ(string_field(*rect_result, "viewport"), "2d");
  EXPECT_EQ((*rect_result)["items"].Size(), 1u);

  mcp::JsonValue nodes_error_canonical = object();
  mcp::JsonValue nodes_error_inner = object();
  nodes_error_inner["nodes_error"] = mcp::JsonValue("lookup failed");
  nodes_error_canonical["result"] = std::move(nodes_error_inner);
  EXPECT_EQ(error_text(apply_alias_result(*rect, nodes_error_canonical)),
            "lookup failed");

  const ToolAlias *verify =
      find_alias(scene_tools, "get_scene_tree", "verify_scene_saved");
  ASSERT_NE(verify, nullptr);
  const mcp::JsonValue verify_args =
      apply_alias_args(*verify, object());
  EXPECT_TRUE(verify_args["verify"].GetBool());

  mcp::JsonValue verify_canonical = object();
  mcp::JsonValue verify_inner = object();
  mcp::JsonValue verify_summary = object();
  verify_summary["available"] = mcp::JsonValue(true);
  verify_summary["match"] = mcp::JsonValue(true);
  verify_summary["path"] = mcp::JsonValue("res://main.tscn");
  verify_summary["memory_nodes"] = mcp::JsonValue(static_cast<int64_t>(3));
  verify_summary["disk_nodes"] = mcp::JsonValue(static_cast<int64_t>(3));
  verify_summary["memory_hash"] = mcp::JsonValue("00ff");
  verify_inner["verify"] = std::move(verify_summary);
  verify_canonical["result"] = std::move(verify_inner);
  const mcp::JsonValue verify_projected =
      apply_alias_result(*verify, verify_canonical);
  EXPECT_EQ(string_field(verify_projected, "result"), "match");
  EXPECT_EQ(string_field(verify_projected, "memory_hash"), "00ff");

  mcp::JsonValue unavailable_canonical = object();
  mcp::JsonValue unavailable_inner = object();
  mcp::JsonValue unavailable = object();
  unavailable["available"] = mcp::JsonValue(false);
  unavailable["error"] = mcp::JsonValue("scene file is unavailable");
  unavailable_inner["verify"] = std::move(unavailable);
  unavailable_canonical["result"] = std::move(unavailable_inner);
  EXPECT_EQ(error_text(apply_alias_result(*verify, unavailable_canonical)),
            "scene file is unavailable");

  const auto capture_tools = godot_autopilot::capture_tools::make_tools();
  const ToolAlias *capture = find_alias(
      capture_tools, "capture_editor_viewport", "capture_game_viewport");
  ASSERT_NE(capture, nullptr);
  ASSERT_NE(capture->validate_args, nullptr);
  const mcp::JsonValue capture_args =
      apply_alias_args(*capture, object());
  EXPECT_EQ(string_field(capture_args, "target"), "game");
  mcp::JsonValue bad_nodes = object();
  mcp::JsonValue bad_nodes_array(mcp::JsonValue::array_tag);
  bad_nodes["annotate_nodes"] = std::move(bad_nodes_array);
  EXPECT_EQ(error_text(capture->validate_args(bad_nodes)),
            "annotate_nodes must be an array of 1-50 strings");
  mcp::JsonValue unknown = object();
  unknown["bogus"] = mcp::JsonValue(true);
  EXPECT_EQ(error_text(capture->validate_args(unknown)),
            "unknown parameter for capture_game_viewport: bogus");
}

} // namespace
