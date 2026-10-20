#include "tools/tool_execute.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "tools/register_all.hpp"
#include "tools/tool_base.hpp"
#include "tools/tool_registry.hpp"

namespace {

using godot_autopilot::ToolBase;
using godot_autopilot::ToolMeta;
namespace tools = godot_autopilot::tools;

class RecordingTool : public ToolBase {
public:
  explicit RecordingTool(const std::string &name) : name_(name) {
    meta_.name = name_;
    meta_.description = "l1 recording stub";
    meta_.category = "Test";
  }

  const ToolMeta &meta() const override { return meta_; }

  mcp::JsonValue execute(const mcp::JsonValue &) override {
    executed = true;
    mcp::JsonValue result(mcp::JsonValue::object_tag);
    result["result"] = mcp::JsonValue("executed");
    return result;
  }

  mcp::JsonValue input_schema() const override {
    return mcp::JsonValue(mcp::JsonValue::object_tag);
  }

  bool executed = false;

private:
  std::string name_;
  ToolMeta meta_;
};

mcp::JsonValue empty_result() { return mcp::JsonValue(); }

mcp::JsonValue make_error_object(const std::string &message,
                                 const std::string &code) {
  mcp::JsonValue error(mcp::JsonValue::object_tag);
  error["error"] = mcp::JsonValue(message);
  error["code"] = mcp::JsonValue(code);
  return error;
}

class ToolExecuteTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_executor_ = tools::tool_executor_slot();
    previous_validator_ = tools::argument_validator_slot();
    previous_authorization_ = tools::authorization_policy_slot();
    previous_envelope_ = tools::envelope_builder_slot();
    previous_export_guard_ = tools::export_guard_slot();
    tools::register_export_guard([]() { return false; });
    tools::register_argument_validator(
        [](const std::string &, const mcp::JsonValue &) {
          return empty_result();
        });
    tools::register_authorization_policy(
        [](const std::string &) { return empty_result(); });
    tools::register_envelope_builder(
        [](const std::string &, const mcp::JsonValue &result) { return result; });
  }

  void TearDown() override {
    tools::register_tool_executor(previous_executor_);
    tools::register_argument_validator(previous_validator_);
    tools::register_authorization_policy(previous_authorization_);
    tools::register_envelope_builder(previous_envelope_);
    tools::register_export_guard(previous_export_guard_);
    godot_autopilot::clear_active_registry();
  }

  RecordingTool *register_recording_tool(const std::string &name) {
    auto tool = std::make_unique<RecordingTool>(name);
    RecordingTool *raw = tool.get();
    godot_autopilot::get_active_registry()->add(std::move(tool));
    return raw;
  }

  tools::ToolExecutorFn previous_executor_;
  tools::ArgumentValidatorFn previous_validator_;
  tools::AuthorizationFn previous_authorization_;
  tools::EnvelopeFn previous_envelope_;
  tools::ExportGuardFn previous_export_guard_;
};

} // namespace

TEST_F(ToolExecuteTest, ExportInProgressShortCircuitsBeforeToolLookup) {
  tools::register_export_guard([]() { return true; });

  const mcp::JsonValue result = tools::execute_tool(
      "gda_export_probe_tool", mcp::JsonValue::Parse("{}"));

  const mcp::JsonValue *error = result.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  const mcp::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "export_in_progress");
}

TEST_F(ToolExecuteTest, UnknownToolReturnsToolNotFoundWithSearchHint) {
  const mcp::JsonValue result = tools::execute_tool(
      "gda_absent_tool_xyz", mcp::JsonValue::Parse("{}"));

  const mcp::JsonValue *error = result.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_NE(error->GetString().find("gda_absent_tool_xyz"), std::string::npos);
  const mcp::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "tool_not_found");
  const mcp::JsonValue *hint = result.Find("hint");
  ASSERT_NE(hint, nullptr);
  ASSERT_TRUE(hint->IsString());
  EXPECT_NE(hint->GetString().find("search_tools"), std::string::npos);
}

TEST_F(ToolExecuteTest, ValidationFailureShortCircuitsBeforeDispatch) {
  RecordingTool *tool = register_recording_tool("gda_validator_probe_tool");
  const mcp::JsonValue injected =
      make_error_object("invalid parameter: probe must be a string",
                         "invalid_argument");
  tools::register_argument_validator(
      [injected](const std::string &, const mcp::JsonValue &) {
        return injected;
      });

  const mcp::JsonValue result = tools::execute_tool(
      "gda_validator_probe_tool", mcp::JsonValue::Parse("{}"));

  const mcp::JsonValue *error = result.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_EQ(error->GetString(), "invalid parameter: probe must be a string");
  const mcp::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "invalid_argument");
  EXPECT_FALSE(tool->executed);
}

TEST_F(ToolExecuteTest, AuthorizationDenialShortCircuitsBeforeDispatch) {
  RecordingTool *tool = register_recording_tool("gda_authorization_probe_tool");
  const mcp::JsonValue injected =
      make_error_object("authorization required for 'gda_authorization_probe_tool'",
                         "authorization_denied");
  tools::register_authorization_policy(
      [injected](const std::string &) { return injected; });

  const mcp::JsonValue result = tools::execute_tool(
      "gda_authorization_probe_tool", mcp::JsonValue::Parse("{}"));

  const mcp::JsonValue *error = result.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_EQ(error->GetString(),
            "authorization required for 'gda_authorization_probe_tool'");
  const mcp::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "authorization_denied");
  EXPECT_FALSE(tool->executed);
}

TEST_F(ToolExecuteTest, UnregisteredExecutorReportsExecutorUnavailable) {
  tools::register_tool_executor(nullptr);

  const mcp::JsonValue result = tools::execute_tool(
      "gda_absent_tool_xyz", mcp::JsonValue::Parse("{}"));

  const mcp::JsonValue *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "executor_unavailable");
}
