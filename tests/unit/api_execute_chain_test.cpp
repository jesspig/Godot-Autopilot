#include "core/api_surface.hpp"
#include "tools/tool_execute.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "tools/register_all.hpp"
#include "tools/tool_base.hpp"
#include "tools/tool_registry.hpp"

namespace {

using godot_autopilot::ToolBase;
using godot_autopilot::ToolMeta;
using godot_autopilot::api_surface::ApiReply;
namespace tools = godot_autopilot::tools;

constexpr const char *kToken = "rest-chain-token";

class RecordingTool : public ToolBase {
public:
  explicit RecordingTool(const std::string &name) : name_(name) {
    meta_.name = name_;
    meta_.description = "l1 rest chain stub";
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

std::unordered_map<std::string, std::string> bearer_headers() {
  std::unordered_map<std::string, std::string> headers;
  headers["authorization"] = std::string("Bearer ") + kToken;
  return headers;
}

class RestExecuteChainTest : public ::testing::Test {
protected:
  void SetUp() override {
    previous_executor_ = tools::tool_executor_slot();
    previous_validator_ = tools::argument_validator_slot();
    previous_authorization_ = tools::authorization_policy_slot();
    previous_envelope_ = tools::envelope_builder_slot();
    previous_export_guard_ = tools::export_guard_slot();
    tools::register_tool_executor(&tools::execute_tool_chain);
    tools::register_export_guard([]() { return false; });
    tools::register_argument_validator(
        [](const std::string &, const mcp::JsonValue &) {
          return empty_result();
        });
    tools::register_authorization_policy(
        [](const std::string &) { return empty_result(); });
    tools::register_envelope_builder(
        [](const std::string &, const mcp::JsonValue &result) { return result; });
    godot_autopilot::api_surface::register_token_verifier(
        [](const std::unordered_map<std::string, std::string> &headers) {
          const auto found = headers.find("authorization");
          return found != headers.end() &&
                 found->second == std::string("Bearer ") + kToken;
        });
  }

  void TearDown() override {
    tools::register_tool_executor(previous_executor_);
    tools::register_argument_validator(previous_validator_);
    tools::register_authorization_policy(previous_authorization_);
    tools::register_envelope_builder(previous_envelope_);
    tools::register_export_guard(previous_export_guard_);
    godot_autopilot::api_surface::register_token_verifier(nullptr);
    godot_autopilot::clear_active_registry();
  }

  RecordingTool *register_recording_tool(const std::string &name) {
    auto tool = std::make_unique<RecordingTool>(name);
    RecordingTool *raw = tool.get();
    godot_autopilot::get_active_registry()->add(std::move(tool));
    return raw;
  }

  ApiReply call_tool(const std::string &name) {
    return godot_autopilot::api_surface::handle_request(
        "POST", "/api/tools/" + name, bearer_headers(), "{}");
  }

  tools::ToolExecutorFn previous_executor_;
  tools::ArgumentValidatorFn previous_validator_;
  tools::AuthorizationFn previous_authorization_;
  tools::EnvelopeFn previous_envelope_;
  tools::ExportGuardFn previous_export_guard_;
};

} // namespace

TEST_F(RestExecuteChainTest, ExportInProgressShortCircuitsTheRestCall) {
  RecordingTool *tool = register_recording_tool("gda_rest_export_probe_tool");
  tools::register_export_guard([]() { return true; });

  const ApiReply reply = call_tool("gda_rest_export_probe_tool");

  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "export_in_progress");
  EXPECT_EQ(reply.status, 503);
  EXPECT_FALSE(tool->executed);
}

TEST_F(RestExecuteChainTest, AuthorizationDenialShortCircuitsTheRestCall) {
  RecordingTool *tool = register_recording_tool("gda_rest_auth_probe_tool");
  const mcp::JsonValue injected = make_error_object(
      "authorization required for 'gda_rest_auth_probe_tool'",
      "authorization_required");
  tools::register_authorization_policy(
      [injected](const std::string &) { return injected; });

  const ApiReply reply = call_tool("gda_rest_auth_probe_tool");

  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "authorization_required");
  EXPECT_EQ(reply.status, 403);
  EXPECT_FALSE(tool->executed);
}

TEST_F(RestExecuteChainTest, UnknownToolIsReportedByTheRestCall) {
  const ApiReply reply = call_tool("gda_rest_absent_probe_tool");

  const mcp::JsonValue *code = reply.body.Find("code");
  ASSERT_NE(code, nullptr);
  ASSERT_TRUE(code->IsString());
  EXPECT_EQ(code->GetString(), "tool_not_found");
  EXPECT_EQ(reply.status, 404);
}

TEST_F(RestExecuteChainTest, TheRestCallRejectsAMissingToken) {
  register_recording_tool("gda_rest_token_probe_tool");

  const ApiReply reply = godot_autopilot::api_surface::handle_request(
      "POST", "/api/tools/gda_rest_token_probe_tool", {}, "{}");

  EXPECT_EQ(reply.status, 401);
}
