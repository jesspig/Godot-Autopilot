#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/runtime_ops.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

class DispatchQueueUnavailableTest : public ::testing::Test {
protected:
  godot_autopilot::CommandQueue queue;

  void SetUp() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    godot_autopilot::dispatch::replace_handlers({}, {});
  }

  void TearDown() override {
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
  }
};

} // namespace

TEST_F(DispatchQueueUnavailableTest, MissingEditorQueueReturnsStructuredError) {
  bool handler_called = false;
  godot_autopilot::dispatch::replace_handlers(
      {{"gda_queue_probe",
        [&handler_called](const mcp::JsonValue &) {
          handler_called = true;
          return mcp::JsonValue::Parse(R"({"result": 1})");
        }}},
      {});

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_probe", mcp::JsonValue::Parse("{}"));

  EXPECT_FALSE(handler_called);
  ASSERT_NE(result.Find("error"), nullptr);
  ASSERT_TRUE(result["error"].IsString());
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
  EXPECT_EQ(result.Find("tool"), nullptr);
  EXPECT_EQ(result.Find("waited_ms"), nullptr);
  const std::string message = result["error"].GetString();
  EXPECT_NE(message.find("editor main thread queue unavailable"),
            std::string::npos);
  EXPECT_NE(message.find("not attached"), std::string::npos);
  EXPECT_NE(message.find("gda_queue_probe"), std::string::npos);
}

TEST_F(DispatchQueueUnavailableTest, MissingEditorQueueSkipsUnknownToolLookup) {
  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_absent_tool", mcp::JsonValue::Parse("{}"));
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  const std::string message = result["error"].GetString();
  EXPECT_EQ(message.find("not found"), std::string::npos);
}

TEST_F(DispatchQueueUnavailableTest, MainThreadExecutesHandlerInPlace) {
  bool handler_called = false;
  godot_autopilot::dispatch::replace_handlers(
      {{"gda_queue_probe",
        [&handler_called](const mcp::JsonValue &) {
          handler_called = true;
          return mcp::JsonValue::Parse(R"({"result": 7})");
        }}},
      {});

  queue.drain();
  godot_autopilot::runtime_ops::set_editor_queue(&queue);

  const mcp::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_probe", mcp::JsonValue::Parse("{}"));

  EXPECT_TRUE(handler_called);
  EXPECT_EQ(result.Find("error"), nullptr);
  ASSERT_NE(result.Find("result"), nullptr);
  EXPECT_EQ(result["result"].GetInt(), 7);
}
