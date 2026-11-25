
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

std::string error_of(const godot_autopilot::util::JsonValue &value) {
  if (!value.IsObject()) {
    return "<non-object>";
  }
  const godot_autopilot::util::JsonValue *err = value.Find("error");
  if (err == nullptr) {
    return "<no error>";
  }
  return err->GetString();
}

std::string code_of(const godot_autopilot::util::JsonValue &value) {
  if (!value.IsObject()) {
    return "<non-object>";
  }
  const godot_autopilot::util::JsonValue *code = value.Find("code");
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
  const godot_autopilot::util::JsonValue result = invoke_tool("x", godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_NE(error_of(result).find("domain tool 'x' not found"),
            std::string::npos);
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, SuccessPassthrough) {
  godot_autopilot::dispatch::replace_handlers(
      {{"echo", [](const godot_autopilot::util::JsonValue &) {
          return godot_autopilot::util::JsonValue::Parse(R"({"result": 42})");
        }}},
      {});
  const godot_autopilot::util::JsonValue result =
      invoke_tool("echo", godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(error_of(result), "<no error>");
  const godot_autopilot::util::JsonValue *value = result.Find("result");
  ASSERT_NE(value, nullptr);
  ASSERT_TRUE(value->IsInt());
  EXPECT_EQ(value->GetInt(), 42);
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, PendingGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"p", [](const godot_autopilot::util::JsonValue &) {
          return godot_autopilot::util::JsonValue::Parse(R"({"__gda_pending": 7})");
        }}},
      {});
  const godot_autopilot::util::JsonValue result = invoke_tool("p", godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(error_of(result),
            "cannot invoke async tool 'p' from within another tool on the "
            "main thread");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, DirectDispatchRecursionUsesTheSameGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"direct_a",
        [](const godot_autopilot::util::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "direct_b", args,
              godot_autopilot::dispatch::capture_depth_context());
        }},
       {"direct_b",
        [](const godot_autopilot::util::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "direct_a", args,
              godot_autopilot::dispatch::capture_depth_context());
        }}},
      {});

  const godot_autopilot::util::JsonValue result =
      godot_autopilot::dispatch::call_handler("direct_a",
                                               godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "depth_exceeded");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, ExplicitDepthSurvivesDispatchToTheMainThread) {
  godot_autopilot::dispatch::replace_handlers(
      {{"outer",
        [](const godot_autopilot::util::JsonValue &) {
          EXPECT_EQ(invoke_depth(), 8);
          EXPECT_EQ(godot_autopilot::tools::current_trace_id(),
                    "trace-cross-thread");
          EXPECT_EQ(godot_autopilot::tools::current_span_id(),
                    "span-parent");
          EXPECT_EQ(godot_autopilot::monitor::current_request_id(),
                    "request-cross-thread");
          return godot_autopilot::dispatch::call_handler(
              "inner", godot_autopilot::util::JsonValue::Parse("{}"),
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
        "outer", godot_autopilot::util::JsonValue::Parse("{}"), ctx);
  });

  bool ready = false;
  for (int i = 0; i < 1000 && !ready; ++i) {
    ready = future.wait_for(std::chrono::milliseconds(1)) ==
            std::future_status::ready;
    queue.drain();
  }
  ASSERT_TRUE(ready);
  const godot_autopilot::util::JsonValue result = future.get();
  EXPECT_EQ(code_of(result), "depth_exceeded");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, DepthResetsAfterOverflow) {
  godot_autopilot::dispatch::replace_handlers(
      {{"ok", [](const godot_autopilot::util::JsonValue &) {
          return godot_autopilot::util::JsonValue::Parse(R"({"result":true})");
        }}},
      {});
  const godot_autopilot::util::JsonValue overflow =
      godot_autopilot::dispatch::call_handler(
          "ok", godot_autopilot::util::JsonValue::Parse("{}"),
          godot_autopilot::dispatch::DepthContext{8});
  EXPECT_EQ(code_of(overflow), "depth_exceeded");

  const godot_autopilot::util::JsonValue result =
      godot_autopilot::dispatch::call_handler("ok",
                                               godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "<no code>");
  EXPECT_EQ(invoke_depth(), 0);
}

TEST_F(ToolInvokeTest, CodeExecuteHandlerNestedCallsUseTheSameGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"code_execute", [](const godot_autopilot::util::JsonValue &args) {
          return godot_autopilot::dispatch::call_handler(
              "code_execute", args,
              godot_autopilot::dispatch::capture_depth_context());
        }}},
      {});
  const godot_autopilot::util::JsonValue result =
      godot_autopilot::dispatch::call_handler("code_execute",
                                               godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(code_of(result), "depth_exceeded");
}

TEST_F(ToolInvokeTest, DepthGuard) {
  godot_autopilot::dispatch::replace_handlers(
      {{"a",
        [](const godot_autopilot::util::JsonValue &args) { return invoke_tool("b", args); }},
       {"b",
        [](const godot_autopilot::util::JsonValue &args) { return invoke_tool("a", args); }}},
      {});
  const godot_autopilot::util::JsonValue result = invoke_tool("a", godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_NE(error_of(result).find("tool invoke depth exceeded (max 8)"),
            std::string::npos);
  EXPECT_EQ(invoke_depth(), 0);
}
