#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/runtime_ops.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

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
        [&handler_called](const godot_autopilot::util::JsonValue &) {
          handler_called = true;
          return godot_autopilot::util::JsonValue::Parse(R"({"result": 1})");
        }}},
      {});

  const godot_autopilot::util::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_probe", godot_autopilot::util::JsonValue::Parse("{}"));

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
  const godot_autopilot::util::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_absent_tool", godot_autopilot::util::JsonValue::Parse("{}"));
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  const std::string message = result["error"].GetString();
  EXPECT_EQ(message.find("not found"), std::string::npos);
}

TEST_F(DispatchQueueUnavailableTest, MainThreadExecutesHandlerInPlace) {
  bool handler_called = false;
  godot_autopilot::dispatch::replace_handlers(
      {{"gda_queue_probe",
        [&handler_called](const godot_autopilot::util::JsonValue &) {
          handler_called = true;
          return godot_autopilot::util::JsonValue::Parse(R"({"result": 7})");
        }}},
      {});

  queue.drain();
  godot_autopilot::runtime_ops::set_editor_queue(&queue);

  const godot_autopilot::util::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_probe", godot_autopilot::util::JsonValue::Parse("{}"));

  EXPECT_TRUE(handler_called);
  EXPECT_EQ(result.Find("error"), nullptr);
  ASSERT_NE(result.Find("result"), nullptr);
  EXPECT_EQ(result["result"].GetInt(), 7);
}

TEST_F(DispatchQueueUnavailableTest, ClosedFormerMainQueueRejectsHandler) {
  bool handler_called = false;
  godot_autopilot::dispatch::replace_handlers(
      {{"gda_queue_probe",
        [&handler_called](const godot_autopilot::util::JsonValue &) {
          handler_called = true;
          return godot_autopilot::util::JsonValue::Parse(R"({"result": 7})");
        }}},
      {});

  queue.drain();
  queue.close();
  godot_autopilot::runtime_ops::set_editor_queue(&queue);

  const godot_autopilot::util::JsonValue result = godot_autopilot::dispatch::call_handler(
      "gda_queue_probe", godot_autopilot::util::JsonValue::Parse("{}"));

  EXPECT_FALSE(handler_called);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
}

TEST_F(DispatchQueueUnavailableTest,
       ExportStartingAfterSubmissionBlocksFinalMainThreadExecution) {
  bool handler_called = false;
  godot_autopilot::dispatch::replace_handlers(
      {{"gda_queue_probe",
        [&handler_called](const godot_autopilot::util::JsonValue &) {
          handler_called = true;
          return godot_autopilot::util::JsonValue::Parse(R"({"result": 7})");
        }}},
      {});
  queue.drain();
  godot_autopilot::runtime_ops::set_editor_queue(&queue);
  godot_autopilot::ExportState export_state;
  std::unique_ptr<godot_autopilot::ExportState::Scope> export_scope;

  std::promise<godot_autopilot::util::JsonValue> completed;
  auto completed_future = completed.get_future();
  std::thread worker([&] {
    completed.set_value(godot_autopilot::dispatch::call_handler(
        "gda_queue_probe", godot_autopilot::util::JsonValue::Parse("{}")));
  });

  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(2);
  while (queue.stats().pending == 0 &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const bool queued = queue.stats().pending == 1u;
  export_scope =
      std::make_unique<godot_autopilot::ExportState::Scope>(export_state);
  queue.drain();
  export_scope.reset();

  const auto completed_status =
      completed_future.wait_for(std::chrono::seconds(2));
  godot_autopilot::util::JsonValue result;
  if (completed_status == std::future_status::ready) {
    result = completed_future.get();
  }
  worker.join();
  ASSERT_EQ(completed_status, std::future_status::ready);
  ASSERT_TRUE(queued);
  EXPECT_FALSE(handler_called);
  EXPECT_EQ(result["code"].GetString(), "export_in_progress");
  EXPECT_TRUE(result["retryable"].GetBool());
}
