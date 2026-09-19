
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"
#include "tools/tool_invoke.hpp"

#include <gtest/gtest.h>

#include <string>

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

} // namespace

class ToolInvokeTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
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
