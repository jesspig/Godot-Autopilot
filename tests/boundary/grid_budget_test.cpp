#include "tools/domains/spriteframes/grid_budget.hpp"

#include <gtest/gtest.h>

using godot_autopilot::grid_detail::grid_frame_count;
using godot_autopilot::grid_detail::kMaxGridFrames;

TEST(GridBudgetTest, NormalGrid) {
  EXPECT_EQ(grid_frame_count(4, 3), 12);
}

TEST(GridBudgetTest, OversizedProductRejected) {
  EXPECT_EQ(grid_frame_count(100000, 100000), -1);
  EXPECT_EQ(grid_frame_count(kMaxGridFrames + 1, 1), -1);
}

TEST(GridBudgetTest, NonPositiveRejected) {
  EXPECT_EQ(grid_frame_count(0, 5), -1);
  EXPECT_EQ(grid_frame_count(5, -1), -1);
}
