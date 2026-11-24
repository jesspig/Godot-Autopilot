#include "runtime/runtime_budget.hpp"

#include <gtest/gtest.h>

using godot_autopilot::runtime_budget::remaining_timeout_ms;
using godot_autopilot::runtime_budget::SequencePlanFailure;
using godot_autopilot::runtime_budget::tween_capacity_available;
using godot_autopilot::runtime_budget::validate_sequence_plan;

TEST(RuntimeLifecycleTest, AwaitReusesRemainingBudget) {
  EXPECT_EQ(remaining_timeout_ms(5000, 0), 5000);
  EXPECT_EQ(remaining_timeout_ms(5000, 1200), 3800);
  EXPECT_EQ(remaining_timeout_ms(5000, 5000), 0);
  EXPECT_EQ(remaining_timeout_ms(5000, 8000), 0);
  EXPECT_EQ(remaining_timeout_ms(0, 0), 0);
}

TEST(RuntimeLifecycleTest, SequencePlanRejectsOversizedInputs) {
  const auto ok = validate_sequence_plan(10, 200, 2200, 256, 1000, 30000);
  EXPECT_EQ(ok.failure, SequencePlanFailure::kNone);
  EXPECT_EQ(ok.timeout_ms, 2200);

  EXPECT_EQ(validate_sequence_plan(0, 100, 2000, 256, 1000, 30000).failure,
            SequencePlanFailure::kTooManyItems);
  EXPECT_EQ(validate_sequence_plan(300, 100, 2000, 256, 1000, 30000).failure,
            SequencePlanFailure::kTooManyItems);
  EXPECT_EQ(validate_sequence_plan(10, 1001, 2200, 256, 1000, 30000).failure,
            SequencePlanFailure::kStepTooLong);
  EXPECT_EQ(validate_sequence_plan(10, 100, 31000, 256, 1000, 30000).failure,
            SequencePlanFailure::kTotalTooLong);
  EXPECT_EQ(validate_sequence_plan(10, 100, 0, 256, 1000, 30000).failure,
            SequencePlanFailure::kTotalTooLong);
}

TEST(RuntimeLifecycleTest, TweenRegistryCapacity) {
  EXPECT_TRUE(tween_capacity_available(0, 256));
  EXPECT_TRUE(tween_capacity_available(255, 256));
  EXPECT_FALSE(tween_capacity_available(256, 256));
  EXPECT_FALSE(tween_capacity_available(300, 256));
}
