
#include "core/command_queue.hpp"
#include "core/observability/monitor.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_invoke.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <string>
#include <thread>

using godot_autopilot::tools::invoke_depth;
using godot_autopilot::tools::invoke_tool;

namespace {

std::string error_of(const mcp::JsonValue &value) {
  if (!value.IsObject()) {
    return "<non-object>";
  }
  const mcp::JsonValue *err = value.Find("error");
  if (err == nullptr) {
    return "<no error>";
  }
  return err->GetString();
}

std::string code_of(const mcp::JsonValue &value) {
  if (!value.IsObject()) {
    return "<non-object>";
  }
  const mcp::JsonValue *code = value.Find("code");
  return code != nullptr && code->IsString() ? code->GetString()
                                             : std::string("<no code>");
}

} // namespace

class ToolInvokeTest : public ::testing::Test {
protected:
  godot_autopilot::CommandQueue queue;

  void SetUp() override {
    queue.drain();
    godot_autopilot::runtime_ops::set_editor_queue(&queue);
    godot_autopilot::dispatch::replace_handlers({}, {});
  }

  void TearDown() override {
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
  }
};

TEST_F(ToolInvokeTest, UnknownToolPassthrough) {
  const mcp::JsonValue result = invoke_tool("x", mcp::JsonValue::Parse("{}"));
  EXPECT_NE(error_of(result).find("domain tool 'x' not found"),
            std::string::npos);
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, SuccessPassthrough) {
  godot_autopilot::dispatch::replace_handlers(
      {{"echo", [](const mcp::JsonValue &) {
          return mcp::JsonValue::Parse(R"({"result": 42})");
        }}},
      {});
  const mcp::JsonValue result =
      invoke_tool("echo", mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(error_of(result), "<no error>");
  const mcp::JsonValue *value = result.Find("result");
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsInt());
  EXPECT_EQ(value->GetInt(), 42);
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, PendingGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"p", [](const mcp::JsonValue &) {
          return mcp::JsonValue::Parse(R"({"__gda_pending": 7})");
        }}},
      {});
  const mcp::JsonValue result = invoke_tool("p", mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(error_of(result),
            "cannot invoke async tool 'p' from within another tool on the "
            "main thread");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, DirectDispatchRecursionUsesTheSameGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"direct_a",
        [](const mcp::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "direct_b", args,
              godot_autopilot::dispatch::capture_depth_context());
        }},
       {"direct_b",
        [](const mcp::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "direct_a", args,
              godot_autopilot::dispatch::capture_depth_context());
        }}},
      {});

  const mcp::JsonValue result =
      godot_autopilot::dispatch::call_handler("direct_a",
                                               mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "depth_exceeded");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, ExplicitDepthSurvivesDispatchToTheMainThread) {
  godot_autopilot::dispatch::replace_handlers(
      {{"outer",
        [](const mcp::JsonValue &) {
          EXPECT_EQ(invoke_depth(), 8);
          EXPECT_EQ(godot_autopilot::tools::current_trace_id(),
                    "trace-cross-thread");
          EXPECT_EQ(godot_autopilot::tools::current_span_id(),
                    "span-parent");
          EXPECT_EQ(godot_autopilot::monitor::current_request_id(),
                    "request-cross-thread");
          return godot_autopilot::dispatch::call_handler(
              "inner", mcp::JsonValue::Parse("{}"),
              godot_autopilot::dispatch::capture_depth_context());
        }}},
      {});

  godot_autopilot::dispatch::DepthContext ctx;
  ctx.depth = 7;
  ctx.trace_id = "trace-cross-thread";
  ctx.span_id = "span-parent";
  ctx.request_id = "request-cross-thread";
  auto future = std::async(std::launch::async, [ctx] {
    return godot_autopilot::dispatch::call_handler(
        "outer", mcp::JsonValue::Parse("{}"), ctx);
  });

  bool ready = false;
  for (int i = 0; i < 1000 && !ready; ++i) {
    ready = future.wait_for(std::chrono::milliseconds(1)) ==
            std::future_status::ready;
    queue.drain();
  }
  ASSERT_TRUE(ready);
  const mcp::JsonValue result = future.get();
  EXPECT_EQ(code_of(result), "depth_exceeded");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, DepthResetsAfterOverflow) {
  godot_autopilot::dispatch::replace_handlers(
      {{"ok", [](const mcp::JsonValue &) {
          return mcp::JsonValue::Parse(R"({"result":true})");
        }}},
      {});
  const mcp::JsonValue overflow =
      godot_autopilot::dispatch::call_handler(
          "ok", mcp::JsonValue::Parse("{}"),
          godot_autopilot::dispatch::DepthContext{8});
  EXPECT_EQ(code_of(overflow), "depth_exceeded");

  const mcp::JsonValue result =
      godot_autopilot::dispatch::call_handler("ok",
                                               mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "<no code>");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, CodeExecuteHandlerNestedCallsUseTheSameGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"code_execute", [](const mcp::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "code_execute", args,
              godot_autopilot::dispatch::capture_depth_context());
        }}},
      {});
  const mcp::JsonValue result =
      godot_autopilot::dispatch::call_handler("code_execute",
                                               mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "depth_exceeded");
}

TEST_F(ToolInvokeTest, DepthGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"a",
        [](const mcp::JsonValue &args) { return invoke_tool("b", args); }},
       {"b",
        [](const mcp::JsonValue &args) { return invoke_tool("a", args); }}},
      {});
  const mcp::JsonValue result = invoke_tool("a", mcp::JsonValue::Parse("{}"));
  EXPECT_NE(error_of(result).find("tool invoke depth exceeded (max 8)"),
            std::string::npos);
  EXPECT_EQ(invoke_depth(), 0);
}
