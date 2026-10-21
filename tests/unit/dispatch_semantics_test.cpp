#include "core/command_queue.hpp"
#include "tools/dispatch.hpp"
#include "tools/tool_spec.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::CommandQueue;
using godot_autopilot::dispatch::dispatch_timeout_result;
using godot_autopilot::dispatch::queue_unavailable_result;
using godot_autopilot::dispatch::run_main_thread_slice;
using godot_autopilot::dispatch::run_on_main_thread_with_budget;
using godot_autopilot::tool_flags::kHealthProbe;

namespace {

mcp::JsonValue empty_payload() {
  return mcp::JsonValue(mcp::JsonValue::object_tag);
}

} // namespace

TEST(DispatchSemanticsTest, ClosedQueueRejectionBecomesStructuredError) {
  CommandQueue queue;
  queue.close();

  mcp::JsonValue result;
  ASSERT_NO_THROW(result = run_on_main_thread_with_budget(
      queue, "gda_probe", kHealthProbe, [] { return empty_payload(); }));

  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
  EXPECT_EQ(result.Find("cancelled"), nullptr);
  EXPECT_EQ(result["tool"].GetString(), "gda_probe");
  EXPECT_NE(result.Find("waited_ms"), nullptr);
  EXPECT_NE(result.Find("queue_depth"), nullptr);
  EXPECT_NE(result.Find("queue_capacity"), nullptr);
  EXPECT_NE(result.Find("oldest_pending_ms"), nullptr);
  EXPECT_NE(result.Find("last_drain_age_ms"), nullptr);
}

TEST(DispatchSemanticsTest, FullQueueRejectionBecomesStructuredError) {
  CommandQueue queue(1);
  auto pending = queue.submit([] { return 7; });

  mcp::JsonValue result;
  ASSERT_NO_THROW(result = run_on_main_thread_with_budget(
      queue, "gda_probe", kHealthProbe, [] { return empty_payload(); }));

  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
  const std::string message = result["error"].GetString();
  EXPECT_NE(message.find("full"), std::string::npos);

  queue.drain();
  EXPECT_EQ(pending.get(), 7);
}

TEST(DispatchSemanticsTest, QueueRejectionCarriesTheDedicatedExceptionType) {
  CommandQueue queue;
  queue.close();

  auto submission = queue.submit_tracked([] { return 1; });
  EXPECT_THROW(submission.future.get(), CommandQueue::Rejected);
}

TEST(DispatchSemanticsTest, QueueUnavailableResultCarriesDiagnosticFields) {
  CommandQueue::Stats stats;
  stats.pending = 3;
  stats.capacity = 8;

  const mcp::JsonValue result = queue_unavailable_result(
      "gda_probe", "the command queue is full", stats, 120, 45, 120);

  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
  EXPECT_EQ(result["tool"].GetString(), "gda_probe");
  EXPECT_EQ(result["waited_ms"].GetInt(), 120);
  EXPECT_EQ(result["queue_depth"].GetInt(), 3);
  EXPECT_EQ(result["queue_capacity"].GetInt(), 8);
  EXPECT_EQ(result["oldest_pending_ms"].GetInt(), 120);
  EXPECT_EQ(result["last_drain_age_ms"].GetInt(), 45);
}

TEST(DispatchSemanticsTest, TimeoutResultIsCancelledAndNotRetryable) {
  CommandQueue::Stats stats;
  stats.pending = 1;
  stats.capacity = 1024;

  const mcp::JsonValue result =
      dispatch_timeout_result("gda_probe", 27000, stats, 100, 200, false,
                              false);

  EXPECT_EQ(result["code"].GetString(), "main_thread_timeout");
  EXPECT_TRUE(result["cancelled"].GetBool());
  EXPECT_FALSE(result["retryable"].GetBool());
  EXPECT_FALSE(result["cancelled_before_execution"].GetBool());
  EXPECT_EQ(result["tool"].GetString(), "gda_probe");
  EXPECT_EQ(result["waited_ms"].GetInt(), 27000);
  const std::string message = result["error"].GetString();
  EXPECT_NE(message.find("may still execute"), std::string::npos);
}

TEST(DispatchSemanticsTest, TimeoutResultKeepsTheCancelHitDiagnostic) {
  CommandQueue::Stats stats;

  const mcp::JsonValue result =
      dispatch_timeout_result("gda_probe", 5000, stats, -1, -1, true, true);

  EXPECT_EQ(result["code"].GetString(), "main_thread_timeout");
  EXPECT_TRUE(result["cancelled"].GetBool());
  EXPECT_FALSE(result["retryable"].GetBool());
  EXPECT_TRUE(result["cancelled_before_execution"].GetBool());
  const std::string message = result["error"].GetString();
  EXPECT_NE(message.find("cancelled before execution"), std::string::npos);
}

TEST(DispatchSemanticsTest, MainThreadSliceTimeoutIsCancelledAndNotRetryable) {
  CommandQueue queue;
  bool slice_ran = false;

  const mcp::JsonValue result = run_main_thread_slice(
      queue, "gda_probe",
      [&slice_ran]() {
        slice_ran = true;
        return empty_payload();
      },
      50);

  EXPECT_FALSE(slice_ran);
  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "main_thread_timeout");
  EXPECT_TRUE(result["cancelled"].GetBool());
  EXPECT_FALSE(result["retryable"].GetBool());
  EXPECT_EQ(result["target"].GetString(), "gda_probe");
}

TEST(DispatchSemanticsTest, MainThreadSliceRejectionBecomesStructuredError) {
  CommandQueue queue;
  queue.close();

  mcp::JsonValue result;
  ASSERT_NO_THROW(result = run_main_thread_slice(
      queue, "gda_probe", [] { return empty_payload(); }, 50));

  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
}
