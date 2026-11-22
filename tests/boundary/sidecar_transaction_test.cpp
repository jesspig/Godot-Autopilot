#include "tools/domains/resource/sidecar_transaction.hpp"

#include <gtest/gtest.h>

#include <vector>

using godot_autopilot::resource_ops_detail::rollback_order;
using godot_autopilot::resource_ops_detail::SidecarKind;
using godot_autopilot::resource_ops_detail::SidecarMove;

TEST(SidecarTransactionTest, RollbackIsReverseOfMoves) {
  std::vector<SidecarMove> moves = {
      {SidecarKind::kUid, true},
      {SidecarKind::kImport, true},
  };
  const auto order = rollback_order(moves);
  ASSERT_EQ(order.size(), 2u);
  EXPECT_EQ(order[0], SidecarKind::kImport);
  EXPECT_EQ(order[1], SidecarKind::kUid);
}

TEST(SidecarTransactionTest, UnmovedSidecarsAreSkipped) {
  std::vector<SidecarMove> moves = {
      {SidecarKind::kUid, false},
      {SidecarKind::kImport, true},
  };
  const auto order = rollback_order(moves);
  ASSERT_EQ(order.size(), 1u);
  EXPECT_EQ(order[0], SidecarKind::kImport);
}

TEST(SidecarTransactionTest, EmptyMovesYieldNoRollback) {
  EXPECT_TRUE(rollback_order({}).empty());
}
