#include "core/config.hpp"
#include "tools/dispatch.hpp"
#include "tools/tool_spec.hpp"

#include <gtest/gtest.h>

using godot_autopilot::CommandQueue;
using godot_autopilot::dispatch::main_thread_wait_budget_ms;
using godot_autopilot::dispatch::run_on_main_thread_with_budget;
using godot_autopilot::GDA_DISPATCH_WAIT_DEFAULT_MS;
using godot_autopilot::GDA_DISPATCH_WAIT_HEALTH_MS;
using godot_autopilot::GDA_DISPATCH_WAIT_LONG_BLOCKING_MS;
using godot_autopilot::GDA_DISPATCH_WAIT_MAX_MS;
using godot_autopilot::GDA_DISPATCH_WAIT_MIN_MS;
using godot_autopilot::GDA_TRANSPORT_TIMEOUT_MS;
using godot_autopilot::tool_flags::kCaptureImage;
using godot_autopilot::tool_flags::kDynamic;
using godot_autopilot::tool_flags::kHealthProbe;
using godot_autopilot::tool_flags::kLongBlocking;
using godot_autopilot::tool_flags::kMeta;
using godot_autopilot::tool_flags::kMutating;
using godot_autopilot::tool_flags::kNone;
using godot_autopilot::tool_flags::kObserve;
using godot_autopilot::tool_flags::kSceneTarget;
using godot_autopilot::tool_flags::kUndoable;

namespace {

constexpr int64_t kDefaultMs = 27000;
constexpr int64_t kHealthMs = 5000;
constexpr int64_t kLongBlockingMs = 30000;

} // namespace

TEST(DispatchBudgetTest, NoneReturnsDefaultBudget) {
  EXPECT_EQ(
      main_thread_wait_budget_ms(kNone, kDefaultMs, kHealthMs, kLongBlockingMs),
      kDefaultMs);
}

TEST(DispatchBudgetTest, HealthProbeReturnsHealthBudget) {
  EXPECT_EQ(main_thread_wait_budget_ms(kHealthProbe, kDefaultMs, kHealthMs,
                                       kLongBlockingMs),
            kHealthMs);
}

TEST(DispatchBudgetTest, HealthProbeWithMetaReturnsHealthBudget) {
  EXPECT_EQ(main_thread_wait_budget_ms(kMeta | kHealthProbe, kDefaultMs,
                                       kHealthMs, kLongBlockingMs),
            kHealthMs);
}

TEST(DispatchBudgetTest, LongBlockingReturnsLongBlockingBudget) {
  EXPECT_EQ(main_thread_wait_budget_ms(kLongBlocking, kDefaultMs, kHealthMs,
                                       kLongBlockingMs),
            kLongBlockingMs);
}

TEST(DispatchBudgetTest, LongBlockingOverridesHealthProbe) {
  EXPECT_EQ(main_thread_wait_budget_ms(kLongBlocking | kHealthProbe, kDefaultMs,
                                       kHealthMs, kLongBlockingMs),
            kLongBlockingMs);
}

TEST(DispatchBudgetTest, LongBlockingWithMetaReturnsLongBlockingBudget) {
  EXPECT_EQ(main_thread_wait_budget_ms(kMeta | kLongBlocking, kDefaultMs,
                                       kHealthMs, kLongBlockingMs),
            kLongBlockingMs);
}

TEST(DispatchBudgetTest, LongBlockingFallsBackToDefaultWhenBudgetNotPositive) {
  EXPECT_EQ(main_thread_wait_budget_ms(kLongBlocking, kDefaultMs, kHealthMs, 0),
            kDefaultMs);
}

TEST(DispatchBudgetTest, UnrelatedFlagCombinationsReturnDefaultBudget) {
  const uint32_t combos[] = {
      kMeta,
      kDynamic,
      kMutating,
      kObserve,
      kCaptureImage,
      kSceneTarget,
      kUndoable,
      kMeta | kDynamic | kMutating | kObserve | kCaptureImage | kSceneTarget |
          kUndoable,
  };
  for (uint32_t flags : combos) {
    EXPECT_EQ(
        main_thread_wait_budget_ms(flags, kDefaultMs, kHealthMs, kLongBlockingMs),
        kDefaultMs);
  }
}

TEST(DispatchBudgetTest, CustomBudgetsArePassedThrough) {
  EXPECT_EQ(main_thread_wait_budget_ms(kNone, 12345, 6789, kLongBlockingMs),
            12345);
  EXPECT_EQ(main_thread_wait_budget_ms(kHealthProbe, 12345, 6789, kLongBlockingMs),
            6789);
}

TEST(DispatchBudgetTest, SharedBudgetConstantsStayInsideTheClampInterval) {
  EXPECT_LE(GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_MAX_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_DEFAULT_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_DEFAULT_MS, GDA_DISPATCH_WAIT_MAX_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_HEALTH_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_HEALTH_MS, GDA_DISPATCH_WAIT_MAX_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_MIN_MS, GDA_DISPATCH_WAIT_LONG_BLOCKING_MS);
  EXPECT_LE(GDA_DISPATCH_WAIT_LONG_BLOCKING_MS, GDA_DISPATCH_WAIT_MAX_MS);
}

TEST(DispatchBudgetTest, LongBlockingConstantEqualsTheClampCeiling) {
  EXPECT_EQ(GDA_DISPATCH_WAIT_LONG_BLOCKING_MS, GDA_DISPATCH_WAIT_MAX_MS);
}

TEST(DispatchBudgetTest, ClampCeilingStaysBelowTheTransportTimeout) {
  EXPECT_LT(GDA_DISPATCH_WAIT_MAX_MS, GDA_TRANSPORT_TIMEOUT_MS);
  EXPECT_LT(GDA_DISPATCH_WAIT_LONG_BLOCKING_MS, GDA_TRANSPORT_TIMEOUT_MS);
  EXPECT_LT(GDA_DISPATCH_WAIT_DEFAULT_MS, GDA_TRANSPORT_TIMEOUT_MS);
}

TEST(DispatchBudgetTest, ClosedQueueReturnsQueueUnavailable) {
  CommandQueue queue;
  queue.close();
  const mcp::JsonValue result = run_on_main_thread_with_budget(
      queue, "ping", kHealthProbe,
      [] { return mcp::JsonValue(mcp::JsonValue::object_tag); });
  EXPECT_TRUE(result.Find("error") != nullptr);
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_TRUE(result["retryable"].GetBool());
  EXPECT_EQ(result.Find("cancelled"), nullptr);
}

TEST(DispatchBudgetTest, FullQueueReturnsQueueUnavailable) {
  CommandQueue queue(1);
  auto pending = queue.submit([] { return 7; });
  const mcp::JsonValue result = run_on_main_thread_with_budget(
      queue, "ping", kHealthProbe,
      [] { return mcp::JsonValue(mcp::JsonValue::object_tag); });
  EXPECT_EQ(result["code"].GetString(), "queue_unavailable");
  EXPECT_NE(result["error"].GetString().find("full"), std::string::npos);
  queue.drain();
  EXPECT_EQ(pending.get(), 7);
}
