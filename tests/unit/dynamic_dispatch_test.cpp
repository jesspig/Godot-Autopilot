#include <gtest/gtest.h>

#include <mcp/server/McpServer.hpp>
#include <mcp/transport/InMemoryTransport.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "core/command_queue.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/dynamic_spec_store.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/tool_catalog.hpp"
#include "tools/base/tool_registry.hpp"
#include "tools/base/tool_spec.hpp"
#include "util/bm25_index.hpp"

namespace {

using godot_autopilot::ToolCatalog;
using godot_autopilot::ToolSpec;
using godot_autopilot::ToolSurface;

std::string code_of(const mcp::JsonValue &value) {
  const mcp::JsonValue *code = value.Find("code");
  return code != nullptr && code->IsString() ? code->GetString()
                                             : std::string();
}

class DynamicDispatchFixture : public ::testing::Test {
protected:
  void SetUp() override {
    pair_ = mcp::InMemoryTransport::CreatePair();
    server_ = mcp::McpServer::Create(pair_.server);
    queue_.drain();
    godot_autopilot::register_all_tools(*server_, queue_, catalog_, index_, 9540,
                                        ToolSurface::Meta);
    godot_autopilot::runtime_ops::set_editor_queue(&queue_);
    godot_autopilot::dynamic_specs::enabled().store(true);
  }

  void TearDown() override {
    {
      std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
      godot_autopilot::dynamic_specs::store().clear();
    }
    godot_autopilot::dynamic_specs::enabled().store(true);
    godot_autopilot::refresh_dynamic_tools();
    godot_autopilot::clear_active_registry();
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    if (server_) {
      server_->Close();
    }
  }

  void add_dynamic(
      const std::string &name,
      std::function<mcp::JsonValue(const mcp::JsonValue &)> handler) {
    ToolSpec spec{name, "Dynamic test tool", "Testing", {"dynamic"}};
    spec.side_effect = godot_autopilot::SideEffect::None;
    spec.flags = godot_autopilot::tool_flags::kDynamic;
    spec.handler = std::move(handler);
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::dynamic_specs::store().push_back(std::move(spec));
  }

  mcp::InMemoryTransport::Pair pair_;
  std::unique_ptr<mcp::McpServer> server_;
  godot_autopilot::CommandQueue queue_;
  ToolCatalog catalog_;
  godot_autopilot::Bm25Index index_;
};

} // namespace

TEST_F(DynamicDispatchFixture, CallToolRecursionUsesUnifiedDispatchDepth) {
  mcp::JsonValue args(mcp::JsonValue::object_tag);
  for (int i = 0; i < 10; ++i) {
    mcp::JsonValue wrapper(mcp::JsonValue::object_tag);
    wrapper["name"] = mcp::JsonValue("call_tool");
    wrapper["arguments"] = std::move(args);
    args = std::move(wrapper);
  }

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "call_tool", args, godot_autopilot::dispatch::capture_depth_context());
  EXPECT_EQ(code_of(result), "depth_exceeded");
  EXPECT_EQ(godot_autopilot::tools::invoke_depth(), 0);
}

TEST_F(DynamicDispatchFixture, BatchRecursionUsesUnifiedDispatchDepth) {
  mcp::JsonValue args = mcp::JsonValue::Parse(
      R"({"operations":[],"confirm":true})");
  for (int i = 0; i < 10; ++i) {
    mcp::JsonValue operation(mcp::JsonValue::object_tag);
    operation["tool"] = mcp::JsonValue("batch_execute");
    operation["args"] = std::move(args);
    mcp::JsonValue wrapper(mcp::JsonValue::object_tag);
    mcp::JsonValue operations(mcp::JsonValue::array_tag);
    operations.PushBack(std::move(operation));
    wrapper["operations"] = std::move(operations);
    wrapper["confirm"] = mcp::JsonValue(true);
    args = std::move(wrapper);
  }

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "batch_execute", args,
      godot_autopilot::dispatch::capture_depth_context());
  EXPECT_NE(result.Dump().find("tool invoke depth exceeded"), std::string::npos);
  EXPECT_EQ(godot_autopilot::tools::invoke_depth(), 0);
}

TEST_F(DynamicDispatchFixture, DynamicHandlerRecursionUsesUnifiedDispatchDepth) {
  add_dynamic("dynamic_recursive", [](const mcp::JsonValue &args) {
    return godot_autopilot::dispatch::call_handler(
        "dynamic_recursive", args,
        godot_autopilot::dispatch::capture_depth_context());
  });
  godot_autopilot::refresh_dynamic_tools();

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "dynamic_recursive", mcp::JsonValue::Parse("{}"),
      godot_autopilot::dispatch::capture_depth_context());
  EXPECT_EQ(code_of(result), "depth_exceeded");
}

TEST_F(DynamicDispatchFixture, DisabledToolsDisappearAndCallsAreRejected) {
  std::atomic<int> calls{0};
  add_dynamic("toggle_tool", [&calls](const mcp::JsonValue &) {
    calls.fetch_add(1);
    return mcp::JsonValue::Parse(R"({"result":"ok"})");
  });
  godot_autopilot::refresh_dynamic_tools();

  ASSERT_TRUE(catalog_.get_tool("toggle_tool").has_value());
  ASSERT_NE(godot_autopilot::get_active_registry()->find("toggle_tool"),
            nullptr);
  EXPECT_EQ(code_of(godot_autopilot::dispatch::call_handler(
                "toggle_tool", mcp::JsonValue::Parse("{}"))),
            std::string());
  EXPECT_EQ(calls.load(), 1);

  godot_autopilot::dynamic_specs::enabled().store(false);
  godot_autopilot::refresh_dynamic_tools();

  EXPECT_FALSE(catalog_.get_tool("toggle_tool").has_value());
  EXPECT_EQ(godot_autopilot::get_active_registry()->find("toggle_tool"),
            nullptr);
  const mcp::JsonValue disabled = godot_autopilot::dispatch::call_handler(
      "toggle_tool", mcp::JsonValue::Parse("{}"));
  EXPECT_NE(disabled.Dump().find("not found"), std::string::npos);
  EXPECT_EQ(calls.load(), 1);

  godot_autopilot::dynamic_specs::enabled().store(true);
  godot_autopilot::refresh_dynamic_tools();
  EXPECT_TRUE(catalog_.get_tool("toggle_tool").has_value());
  EXPECT_EQ(code_of(godot_autopilot::dispatch::call_handler(
                "toggle_tool", mcp::JsonValue::Parse("{}"))),
            std::string());
  EXPECT_EQ(calls.load(), 2);
}

TEST_F(DynamicDispatchFixture, ZeroParamDynamicToolRejectsNonObjectArguments) {
  add_dynamic("zero_param", [](const mcp::JsonValue &) {
    return mcp::JsonValue::Parse(R"({"result":"ok"})");
  });
  godot_autopilot::refresh_dynamic_tools();

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "zero_param", mcp::JsonValue::Parse("[]"));
  EXPECT_EQ(code_of(result), "invalid_arguments");
}

TEST_F(DynamicDispatchFixture, InvalidDynamicDefinitionsFailClosedAtRuntime) {
  ToolSpec duplicate{"duplicate_params", "d", "Testing", {}};
  duplicate.side_effect = godot_autopilot::SideEffect::None;
  duplicate.flags = godot_autopilot::tool_flags::kDynamic;
  duplicate.params = {{"value", "string", "", false},
                      {"value", "integer", "", false}};
  duplicate.handler = [](const mcp::JsonValue &) {
    return mcp::JsonValue::Parse(R"({"result":"ok"})");
  };

  ToolSpec unsupported{"unsupported_param", "d", "Testing", {}};
  unsupported.side_effect = godot_autopilot::SideEffect::None;
  unsupported.flags = godot_autopilot::tool_flags::kDynamic;
  unsupported.params = {{"value", "unknown_kind", "", false}};
  unsupported.handler = duplicate.handler;

  {
    std::lock_guard<std::mutex> lock(godot_autopilot::dynamic_specs::mutex());
    godot_autopilot::dynamic_specs::store().push_back(std::move(duplicate));
    godot_autopilot::dynamic_specs::store().push_back(std::move(unsupported));
  }
  godot_autopilot::refresh_dynamic_tools();

  EXPECT_EQ(code_of(godot_autopilot::dispatch::call_handler(
                "duplicate_params", mcp::JsonValue::Parse("{}"))),
            "invalid_tool_definition");
  EXPECT_EQ(code_of(godot_autopilot::dispatch::call_handler(
                "unsupported_param", mcp::JsonValue::Parse("{}"))),
            "invalid_tool_definition");
}
