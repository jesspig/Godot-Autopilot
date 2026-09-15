#include "tools/tilemap_ops.hpp"

#include <gtest/gtest.h>

#include <cstdint>

using godot_autopilot::tilemap_ops::kMaxFillCells;
using godot_autopilot::tilemap_ops::normalize_rect_range;
using godot_autopilot::tilemap_ops::rect_cell_count_bounded;
using godot_autopilot::tilemap_ops::rect_exceeds_cell_limit;
using godot_autopilot::tilemap_ops::RectRange;

TEST(TilemapOpsTest, NormalizeKeepsForwardCorners) {
  const RectRange r = normalize_rect_range(2, 3, 5, 9);
  EXPECT_EQ(2, r.min_x);
  EXPECT_EQ(3, r.min_y);
  EXPECT_EQ(5, r.max_x);
  EXPECT_EQ(9, r.max_y);
  EXPECT_EQ(4, r.width);
  EXPECT_EQ(7, r.height);
}

TEST(TilemapOpsTest, NormalizeSwapsReversedCorners) {
  const RectRange forward = normalize_rect_range(2, 3, 5, 9);
  const RectRange reversed = normalize_rect_range(5, 9, 2, 3);
  EXPECT_EQ(forward.min_x, reversed.min_x);
  EXPECT_EQ(forward.min_y, reversed.min_y);
  EXPECT_EQ(forward.max_x, reversed.max_x);
  EXPECT_EQ(forward.max_y, reversed.max_y);
  EXPECT_EQ(4, reversed.width);
  EXPECT_EQ(7, reversed.height);
}

TEST(TilemapOpsTest, NormalizeMixedReversedAxes) {
  const RectRange r = normalize_rect_range(-1, 5, 3, -2);
  EXPECT_EQ(-1, r.min_x);
  EXPECT_EQ(-2, r.min_y);
  EXPECT_EQ(3, r.max_x);
  EXPECT_EQ(5, r.max_y);
  EXPECT_EQ(5, r.width);
  EXPECT_EQ(8, r.height);
}

TEST(TilemapOpsTest, NormalizeSingleCell) {
  const RectRange r = normalize_rect_range(4, 7, 4, 7);
  EXPECT_EQ(4, r.min_x);
  EXPECT_EQ(7, r.min_y);
  EXPECT_EQ(4, r.max_x);
  EXPECT_EQ(7, r.max_y);
  EXPECT_EQ(1, r.width);
  EXPECT_EQ(1, r.height);
}

TEST(TilemapOpsTest, NormalizeHorizontalLineIsOneCellHigh) {
  const RectRange r = normalize_rect_range(0, 4, 9, 4);
  EXPECT_EQ(10, r.width);
  EXPECT_EQ(1, r.height);
}

TEST(TilemapOpsTest, NormalizeVerticalLineIsOneCellWide) {
  const RectRange r = normalize_rect_range(7, 0, 7, 2);
  EXPECT_EQ(1, r.width);
  EXPECT_EQ(3, r.height);
}

TEST(TilemapOpsTest, NormalizeNegativeCorners) {
  const RectRange r = normalize_rect_range(-3, -5, 2, 1);
  EXPECT_EQ(-3, r.min_x);
  EXPECT_EQ(-5, r.min_y);
  EXPECT_EQ(2, r.max_x);
  EXPECT_EQ(1, r.max_y);
  EXPECT_EQ(6, r.width);
  EXPECT_EQ(7, r.height);
}

TEST(TilemapOpsTest, CellCountBoundedWithinLimit) {
  const RectRange r = normalize_rect_range(0, 0, 99, 299);
  EXPECT_EQ(30000, rect_cell_count_bounded(r, kMaxFillCells));
  EXPECT_FALSE(rect_exceeds_cell_limit(r, kMaxFillCells));
}

TEST(TilemapOpsTest, CellCountBoundedAtExactLimit) {
  const RectRange r = normalize_rect_range(0, 0, 999, 99);
  EXPECT_EQ(kMaxFillCells, rect_cell_count_bounded(r, kMaxFillCells));
  EXPECT_FALSE(rect_exceeds_cell_limit(r, kMaxFillCells));
}

TEST(TilemapOpsTest, CellCountBoundedOverLimitStaysExactWhenSafe) {
  const RectRange r = normalize_rect_range(0, 0, 999, 199);
  EXPECT_EQ(200000, rect_cell_count_bounded(r, kMaxFillCells));
  EXPECT_TRUE(rect_exceeds_cell_limit(r, kMaxFillCells));
}

TEST(TilemapOpsTest, CellCountBoundedClampsWhenDimensionExceedsLimit) {
  const RectRange r = normalize_rect_range(0, 0, 199999, 0);
  EXPECT_EQ(kMaxFillCells + 1, rect_cell_count_bounded(r, kMaxFillCells));
  EXPECT_TRUE(rect_exceeds_cell_limit(r, kMaxFillCells));
}

TEST(TilemapOpsTest, OverLimitCheckIsOrderIndependent) {
  const RectRange forward = normalize_rect_range(0, 0, 1000, 200);
  const RectRange reversed = normalize_rect_range(1000, 200, 0, 0);
  EXPECT_TRUE(rect_exceeds_cell_limit(forward, kMaxFillCells));
  EXPECT_TRUE(rect_exceeds_cell_limit(reversed, kMaxFillCells));
}

TEST(TilemapOpsTest, SingleCellNeverExceedsLimit) {
  const RectRange r = normalize_rect_range(-1, -1, -1, -1);
  EXPECT_EQ(1, rect_cell_count_bounded(r, kMaxFillCells));
  EXPECT_FALSE(rect_exceeds_cell_limit(r, kMaxFillCells));
}
