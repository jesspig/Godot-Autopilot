#include <gtest/gtest.h>

#include "util/json_value.hpp"

#include <string>

#include "integration/harness.hpp"

namespace {

using gda_integration::IntegrationHarnessWithAllow;

class ContractAssertSuite : public IntegrationHarnessWithAllow {
 public:
  ContractAssertSuite() : IntegrationHarnessWithAllow("code_execute") {}
};

godot_autopilot::util::JsonValue make_args() {
  return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
}

TEST_F(ContractAssertSuite, CallToolGetToolDetailCodeExecuteContract) {
  godot_autopilot::util::JsonValue args = make_args();
  args["name"] = godot_autopilot::util::JsonValue("get_tool_detail");
  godot_autopilot::util::JsonValue arguments = make_args();
  arguments["name"] = godot_autopilot::util::JsonValue("code_execute");
  args["arguments"] = std::move(arguments);
  godot_autopilot::util::JsonValue result = call("call_tool", args);
  const auto *tool = result.Find("tool");
  ASSERT_NE(tool, nullptr) << result.Dump();
  ASSERT_TRUE(tool->IsObject()) << result.Dump();
  const auto *name = tool->Find("name");
  ASSERT_NE(name, nullptr) << result.Dump();
  ASSERT_TRUE(name->IsString()) << result.Dump();
  EXPECT_EQ(name->GetString(), "code_execute");
  const auto *side_effect = tool->Find("side_effect");
  ASSERT_NE(side_effect, nullptr) << result.Dump();
  ASSERT_TRUE(side_effect->IsString()) << result.Dump();
  EXPECT_FALSE(side_effect->GetString().empty());
  EXPECT_EQ(side_effect->GetString(), "code_execute");
}

} // namespace
