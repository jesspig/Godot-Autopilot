#include "core/command_queue.hpp"
#include "core/export_guard.hpp"
#include "tools/base/runtime_ops.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/register_all.hpp"
#include "tools/base/tool_catalog.hpp"
#include "util/bm25_index.hpp"
#include "util/json_value.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace {

class ExportExecutionBoundaryTest : public ::testing::Test {
protected:
  void SetUp() override {
    godot_autopilot::runtime_ops::set_editor_queue(&queue);
    godot_autopilot::install_tool_handlers(queue, catalog, index, 9527);
    queue.drain();
  }

  void TearDown() override {
    godot_autopilot::runtime_ops::set_editor_queue(nullptr);
    godot_autopilot::dispatch::clear_handlers();
    godot_autopilot::clear_active_registry();
  }

  bool wait_for_queued_task() {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (queue.stats().pending == 0 &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return queue.stats().pending == 1u;
  }

  static void expect_export_blocked(const godot_autopilot::util::JsonValue &result) {
    const auto *error = result.Find("error");
    ASSERT_NE(error, nullptr);
    ASSERT_TRUE(error->IsString());
    EXPECT_EQ(error->GetString(),
              godot_autopilot::dispatch::export_blocked_error_text());
    const auto *code = result.Find("code");
    ASSERT_NE(code, nullptr);
    ASSERT_TRUE(code->IsString());
    EXPECT_EQ(code->GetString(), "export_in_progress");
    const auto *retryable = result.Find("retryable");
    ASSERT_NE(retryable, nullptr);
    ASSERT_TRUE(retryable->IsBool());
    EXPECT_TRUE(retryable->GetBool());
  }

  godot_autopilot::CommandQueue queue;
  godot_autopilot::ToolCatalog catalog;
  godot_autopilot::Bm25Index index;
};

TEST_F(ExportExecutionBoundaryTest,
       ToolCallStartingExportAfterSubmissionIsRejectedAtFinalBoundary) {
  godot_autopilot::ExportState export_state;
  std::unique_ptr<godot_autopilot::ExportState::Scope> export_scope;
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  auto completed = queue.submit([&args]() {
    return godot_autopilot::dispatch::call_handler("search_tools", args);
  });

  const bool queued = wait_for_queued_task();
  export_scope =
      std::make_unique<godot_autopilot::ExportState::Scope>(export_state);
  queue.drain();
  export_scope.reset();

  ASSERT_TRUE(queued);
  expect_export_blocked(completed.get());
}

TEST_F(ExportExecutionBoundaryTest,
       ToolCallRejectsExportAlreadyInProgressAtEntry) {
  godot_autopilot::ExportState export_state;
  godot_autopilot::ExportState::Scope scope(export_state);

  expect_export_blocked(godot_autopilot::dispatch::call_handler(
      "search_tools", godot_autopilot::util::JsonValue::Parse("{}")));
}

TEST_F(ExportExecutionBoundaryTest,
       QueuedToolCallCompletesWhenExportNeverStarts) {
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  auto completed = queue.submit([&args]() {
    return godot_autopilot::dispatch::call_handler("search_tools", args);
  });

  const bool queued = wait_for_queued_task();
  queue.drain();

  ASSERT_TRUE(queued);
  const auto result = completed.get();
  EXPECT_EQ(result.Find("error"), nullptr);
  const auto *results = result.Find("results");
  ASSERT_NE(results, nullptr);
  ASSERT_TRUE(results->IsArray());
  EXPECT_FALSE(results->GetArray().empty());
}

TEST(StableExportBlockedResultTest, PreservesShortMessageAcrossCalls) {
  const auto first = godot_autopilot::dispatch::export_blocked_json_result();
  const auto second = godot_autopilot::dispatch::export_blocked_json_result();

  EXPECT_EQ(first.Dump(), second.Dump());
  const auto *error = second.Find("error");
  ASSERT_NE(error, nullptr);
  EXPECT_STREQ(error->GetString().c_str(),
               godot_autopilot::dispatch::export_blocked_error_text().c_str());
}

} // namespace
