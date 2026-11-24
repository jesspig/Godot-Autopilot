#include "tools/domains/resource/sidecar_transaction.hpp"

#include <gtest/gtest.h>

using godot_autopilot::resource_ops_detail::ResourceScanBudget;
using godot_autopilot::resource_ops_detail::scan_must_abort;

TEST(SidecarTransactionTest, FileCountLimitFailsClosed) {
  ResourceScanBudget budget;
  budget.files_listed = godot_autopilot::GDA_SCAN_MAX_FILES;
  EXPECT_TRUE(budget.hit_limit(0, 0));
  EXPECT_TRUE(budget.truncated);
  EXPECT_TRUE(scan_must_abort(budget, false));
  EXPECT_EQ(budget.limit_reason, "file_count");
}

TEST(SidecarTransactionTest, SingleFileLimitFailsClosed) {
  ResourceScanBudget budget;
  EXPECT_TRUE(budget.hit_limit(godot_autopilot::GDA_SCAN_MAX_FILE_BYTES + 1, 0));
  EXPECT_TRUE(budget.truncated);
  EXPECT_TRUE(scan_must_abort(budget, false));
  EXPECT_EQ(budget.limit_reason, "single_file_bytes");
}

TEST(SidecarTransactionTest, TotalBytesLimitFailsClosed) {
  ResourceScanBudget budget;
  budget.bytes_scanned = godot_autopilot::GDA_SCAN_MAX_TOTAL_BYTES;
  EXPECT_TRUE(budget.hit_limit(1, 0));
  EXPECT_TRUE(budget.truncated);
  EXPECT_TRUE(scan_must_abort(budget, false));
  EXPECT_EQ(budget.limit_reason, "total_bytes");
}

TEST(SidecarTransactionTest, DepthLimitFailsClosed) {
  ResourceScanBudget budget;
  EXPECT_TRUE(budget.hit_limit(0, godot_autopilot::GDA_SCAN_MAX_DEPTH + 1));
  EXPECT_TRUE(budget.truncated);
  EXPECT_TRUE(scan_must_abort(budget, false));
  EXPECT_EQ(budget.limit_reason, "directory_depth");
}

TEST(SidecarTransactionTest, ExplicitTruncationAlwaysAborts) {
  EXPECT_TRUE(scan_must_abort(ResourceScanBudget(), true));
  EXPECT_FALSE(scan_must_abort(ResourceScanBudget(), false));
}
