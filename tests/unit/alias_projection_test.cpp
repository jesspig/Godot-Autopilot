#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <mcp/JsonValue.hpp>

#include "tools/base/tool_spec.hpp"
#include "tools/domains/audio/audio_tools.hpp"
#include "tools/domains/debug/debug_tools.hpp"
#include "tools/domains/doc/doc_tools.hpp"

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
  for (const auto &tool : tools) {
    if (tool->meta().name != canonical) {
      continue;
    }
    const auto *spec_tool = dynamic_cast<const SpecTool *>(tool.get());
    if (spec_tool == nullptr) {
      continue;
    }
    for (const auto &alias : spec_tool->spec().aliases) {
      if (alias.name == alias_name) {
        return &alias;
      }
    }
  }
  return nullptr;
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
  EXPECT_EQ(count_args["action"].GetString(), "count");

  const ToolAlias *name =
      find_alias(tools, "get_audio_bus_layout", "get_audio_bus_name");
  ASSERT_NE(name, nullptr);
  mcp::JsonValue name_input = object();
  name_input["bus_index"] = mcp::JsonValue(static_cast<int64_t>(2));
  const mcp::JsonValue name_args = apply_alias_args(*name, name_input);
  EXPECT_EQ(name_args["action"].GetString(), "name");
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
  EXPECT_TRUE(catalog_args["catalog"].GetBool());

  const ToolAlias *objects = find_alias(
      tools, "get_debug_monitors", "get_debug_object_count");
  const ToolAlias *memory = find_alias(
      tools, "get_debug_monitors", "get_debug_memory_usage");
  const ToolAlias *nodes = find_alias(
      tools, "get_debug_monitors", "get_debug_node_count");
  ASSERT_NE(objects, nullptr);
  ASSERT_NE(memory, nullptr);
  ASSERT_NE(nodes, nullptr);
  EXPECT_EQ(apply_alias_args(*objects, object())["ids"][0].GetInt(), 7);
  EXPECT_EQ(apply_alias_args(*memory, object())["ids"][0].GetInt(), 4);
  EXPECT_EQ(apply_alias_args(*nodes, object())["ids"][0].GetInt(), 9);

  const ToolAlias *custom = find_alias(
      tools, "get_debug_monitors", "get_debug_custom_monitor_names");
  ASSERT_NE(custom, nullptr);
  const mcp::JsonValue custom_args = apply_alias_args(*custom, object());
  EXPECT_TRUE(custom_args["include_custom"].GetBool());
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

} // namespace
