#include "core/command_queue.hpp"
#include "core/observability/sanitize_policy.hpp"
#include "tools/base/dispatch.hpp"
#include "tools/base/tool_spec.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "util/console_encoding.hpp"

using godot_autopilot::CommandQueue;
using godot_autopilot::dispatch::BoundedWaitOutcome;
using godot_autopilot::dispatch::dispatch_exception_message;
using godot_autopilot::dispatch::dispatch_timeout_result;
using godot_autopilot::dispatch::queue_unavailable_result;
using godot_autopilot::dispatch::run_bounded_on_main_thread;
using godot_autopilot::dispatch::run_main_thread_slice;
using godot_autopilot::dispatch::run_on_main_thread_with_budget;
using godot_autopilot::tool_flags::kHealthProbe;

namespace {

godot_autopilot::util::JsonValue empty_payload() {
  return godot_autopilot::util::JsonValue(godot_autopilot::util::JsonValue::object_tag);
}

class SanitizePolicyGuard {
public:
  explicit SanitizePolicyGuard(bool enabled)
      : previous_(godot_autopilot::sanitize_policy::enabled()) {
    godot_autopilot::sanitize_policy::set_enabled(enabled);
  }
  ~SanitizePolicyGuard() {
    godot_autopilot::sanitize_policy::set_enabled(previous_);
  }
  SanitizePolicyGuard(const SanitizePolicyGuard &) = delete;
  SanitizePolicyGuard &operator=(const SanitizePolicyGuard &) = delete;

private:
  bool previous_;
};

} // namespace

TEST(DispatchSemanticsTest, ClosedQueueRejectionBecomesStructuredError) {
  CommandQueue queue;
  queue.close();

  godot_autopilot::util::JsonValue result;
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

  godot_autopilot::util::JsonValue result;
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

TEST(DispatchSemanticsTest, BoundedHelperRunsInlineOnMainThreadWithinBudget) {
  CommandQueue queue;
  queue.drain();
  auto state = std::make_shared<int>(0);
  const CommandQueue::Stats before = queue.stats();

  const BoundedWaitOutcome outcome = run_bounded_on_main_thread(
      queue, "gda_inline_probe", state,
      [](int &value) { value = 7; }, 1000);

  EXPECT_TRUE(outcome.ok);
  EXPECT_FALSE(outcome.timed_out);
  EXPECT_FALSE(outcome.cancelled);
  EXPECT_GE(outcome.waited_ms, 0);
  EXPECT_LE(outcome.waited_ms, 1000);
  EXPECT_TRUE(outcome.error.empty());
  EXPECT_EQ(*state, 7);
  EXPECT_EQ(queue.stats().submitted, before.submitted);
  EXPECT_EQ(queue.stats().executed, before.executed);
}

TEST(DispatchSemanticsTest, BoundedHelperInlineTimeoutReportsUnpreemptableTask) {
  CommandQueue queue;
  queue.drain();
  auto state = std::make_shared<bool>(false);

  const BoundedWaitOutcome outcome = run_bounded_on_main_thread(
      queue, "gda_inline_probe", state,
      [](bool &ran) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ran = true;
      },
      1);

  EXPECT_FALSE(outcome.ok);
  EXPECT_TRUE(outcome.timed_out);
  EXPECT_FALSE(outcome.cancelled);
  EXPECT_GE(outcome.waited_ms, 10);
  EXPECT_TRUE(*state);
  EXPECT_NE(outcome.error.find("gda_inline_probe"), std::string::npos);
  EXPECT_NE(outcome.error.find("could not be preempted"), std::string::npos);
}

TEST(DispatchSemanticsTest, QueueUnavailableResultCarriesDiagnosticFields) {
  CommandQueue::Stats stats;
  stats.pending = 3;
  stats.capacity = 8;

  const godot_autopilot::util::JsonValue result = queue_unavailable_result(
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

  const godot_autopilot::util::JsonValue result =
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

  const godot_autopilot::util::JsonValue result =
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

  const godot_autopilot::util::JsonValue result = run_main_thread_slice(
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

  godot_autopilot::util::JsonValue result;
  ASSERT_NO_THROW(result = run_main_thread_slice(
      queue, "gda_probe", [] { return empty_payload(); }, 50));

  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
}

TEST(DispatchSemanticsTest, MainThreadSliceTimeoutReportsRealCancelOutcome) {
  CommandQueue queue;
  bool slice_ran = false;
  const int64_t budget_ms = 50;

  const godot_autopilot::util::JsonValue result = run_main_thread_slice(
      queue, "gda_probe",
      [&slice_ran]() {
        slice_ran = true;
        return empty_payload();
      },
      budget_ms);

  EXPECT_FALSE(slice_ran);
  ASSERT_NE(result.Find("error"), nullptr);
  EXPECT_EQ(result["code"].GetString(), "main_thread_timeout");
  EXPECT_TRUE(result["cancelled"].GetBool());
  ASSERT_NE(result.Find("cancelled_before_execution"), nullptr);
  EXPECT_TRUE(result["cancelled_before_execution"].GetBool());
  ASSERT_NE(result.Find("waited_ms"), nullptr);
  EXPECT_LE(result["waited_ms"].GetInt(), budget_ms);
  EXPECT_GE(result["waited_ms"].GetInt(), 0);
}

TEST(DispatchSemanticsTest, ExceptionMessageDropsSensitiveArgValues) {
  SanitizePolicyGuard policy(true);
  const std::string sentinel = "GDA_SENTINEL_4711";
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["password"] = godot_autopilot::util::JsonValue(sentinel);
  args["path"] = godot_autopilot::util::JsonValue("/tmp/visible.txt");

  const std::string message =
      dispatch_exception_message("gda_probe", "boom", args);

  EXPECT_EQ(message.find(sentinel), std::string::npos);
  EXPECT_NE(message.find("gda_probe"), std::string::npos);
  EXPECT_NE(message.find("boom"), std::string::npos);
  EXPECT_NE(message.find("/tmp/visible.txt"), std::string::npos);
}

TEST(DispatchSemanticsTest, ExceptionMessageKeepsTruncatedMultibyteArgsValid) {
  std::string payload;
  for (int i = 0; i < 200; ++i) {
    payload += "\xE4\xB8\xAD";
  }
  godot_autopilot::util::JsonValue args(godot_autopilot::util::JsonValue::object_tag);
  args["text"] = godot_autopilot::util::JsonValue(payload);

  const std::string message =
      dispatch_exception_message("gda_probe", "boom", args);

  EXPECT_TRUE(godot_autopilot::util::is_valid_utf8(message));
  EXPECT_NE(message.find("\xE4\xB8\xAD"), std::string::npos);
  EXPECT_LT(message.size(), payload.size());
}
