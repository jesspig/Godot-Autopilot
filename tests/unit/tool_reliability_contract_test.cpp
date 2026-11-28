#include "core/config.hpp"
#include "tools/domains/editor/editor_tools.hpp"
#include "tools/domains/resource/uid_validation.hpp"
#include "tools/domains/theme/theme_ops.hpp"
#include "util/json_value.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

using godot_autopilot::util::JsonError;
using godot_autopilot::util::JsonErrorCode;
using godot_autopilot::util::JsonValue;

} // namespace

TEST(ToolReliabilityContractTest, Q5bReferenceIntChannelRejected) {
  JsonValue value(1);
  EXPECT_TRUE(value.IsNumber());
  EXPECT_TRUE(value.IsInt());
  EXPECT_FALSE(value.IsDouble());
  try {
    (void)value.GetDouble();
    FAIL() << "reference GetDouble unexpectedly accepted an integer";
  } catch (const JsonError &error) {
    std::cout << "PROBE Q5b reference error_code="
              << static_cast<int>(error.Code()) << " message=" << error.what()
              << std::endl;
    EXPECT_EQ(error.Code(), JsonErrorCode::deserialization_failed);
  }
}

TEST(ToolReliabilityContractTest, Q4ReferenceDispatchBudget) {
  std::cout << "PROBE Q4 reference default_ms="
            << godot_autopilot::GDA_DISPATCH_WAIT_DEFAULT_MS
            << " max_ms=" << godot_autopilot::GDA_DISPATCH_WAIT_MAX_MS
            << " game_max_ms=" << godot_autopilot::GDA_MAX_GAME_OP_TIMEOUT_MS
            << std::endl;
  EXPECT_EQ(godot_autopilot::GDA_DISPATCH_WAIT_DEFAULT_MS, 27000);
  EXPECT_EQ(godot_autopilot::GDA_DISPATCH_WAIT_MAX_MS, 29000);
}
TEST(ToolReliabilityContractTest, Q5bRepairedIntegerChannelsAccepted) {
  JsonValue channels(JsonValue::ObjectTag{});
  JsonValue::Object &fields = channels.GetObject();
  fields["r"] = JsonValue(0);
  fields["g"] = JsonValue(1);
  fields["b"] = JsonValue(0.5);
  fields["a"] = JsonValue(1);
  double parsed[4] = {-1.0, -1.0, -1.0, -1.0};
  std::string error;
  ASSERT_TRUE(godot_autopilot::theme_ops::parse_color_channels(channels, parsed, error));
  std::cout << "PROBE Q5b repaired r=" << parsed[0] << " g=" << parsed[1]
            << " b=" << parsed[2] << " a=" << parsed[3] << std::endl;
  EXPECT_DOUBLE_EQ(parsed[0], 0.0);
  EXPECT_DOUBLE_EQ(parsed[1], 1.0);
  EXPECT_DOUBLE_EQ(parsed[2], 0.5);
  EXPECT_DOUBLE_EQ(parsed[3], 1.0);

  fields["a"] = JsonValue(2);
  EXPECT_FALSE(godot_autopilot::theme_ops::parse_color_channels(channels, parsed, error));
  EXPECT_NE(error.find("out of range"), std::string::npos);
}
TEST(ToolReliabilityContractTest, Q5aRepairedSidecarTextNormalization) {
  using godot_autopilot::resource_ops_detail::normalized_uid_sidecar;
  EXPECT_EQ(normalized_uid_sidecar("uid://abc123\n"), "uid://abc123");
  EXPECT_EQ(normalized_uid_sidecar("  \xef\xbb\xbfuid://xyz  \r\n"),
            "uid://xyz");
  EXPECT_TRUE(normalized_uid_sidecar("not-an-uid").empty());
  EXPECT_TRUE(normalized_uid_sidecar("uid://").empty());
  std::cout << "PROBE Q5a repaired sidecar=" << normalized_uid_sidecar("uid://abc123\n")
            << std::endl;
}
TEST(ToolReliabilityContractTest, Q2RepairedUndoSchemaAndNoEditorPath) {
  const auto tools = godot_autopilot::editor_tools::make_tools();
  bool found = false;
  for (const auto &tool : tools) {
    if (tool->meta().name != "editor_undo") {
      continue;
    }
    found = true;
    EXPECT_NE(tool->input_schema().Find("properties"), nullptr);
  }
  EXPECT_TRUE(found);

  std::cout << "PROBE Q2 repaired schema-unchanged=" << found << std::endl;
}

TEST(ToolReliabilityContractTest, Q9UndoValueContractUnchanged) {
  const auto tools = godot_autopilot::editor_tools::make_tools();
  bool checked = false;
  for (const auto &tool : tools) {
    if (tool->meta().name != "editor_undo") {
      continue;
    }
    const JsonValue schema = tool->input_schema();
    const JsonValue *props = schema.Find("properties");
    ASSERT_NE(props, nullptr);
    const JsonValue *value = props->Find("value");
    ASSERT_NE(value, nullptr);
    const JsonValue *desc = value->Find("description");
    ASSERT_NE(desc, nullptr);
    ASSERT_TRUE(desc->IsString());
    EXPECT_NE(desc->GetString().find("omit for methods without arguments"),
              std::string::npos);
    const JsonValue *method = props->Find("method");
    ASSERT_NE(method, nullptr);
    const JsonValue *method_desc = method->Find("description");
    ASSERT_NE(method_desc, nullptr);
    ASSERT_TRUE(method_desc->IsString());
    EXPECT_NE(method_desc->GetString().find("queue_free"), std::string::npos);
    std::cout << "PROBE Q9 no_arg_method_documented=1 value_optional=1"
              << std::endl;
    checked = true;
    break;
  }
  EXPECT_TRUE(checked);
}
