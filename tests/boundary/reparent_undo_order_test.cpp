#include "tools/domains/scene/reparent_undo_order.hpp"

#include <gtest/gtest.h>

#include <vector>

using godot_autopilot::scene_ops::ReparentUndoStep;
using godot_autopilot::scene_ops::reparent_undo_order;

namespace {

std::vector<ReparentUndoStep> without_transform(bool transform_managed) {
  std::vector<ReparentUndoStep> steps = reparent_undo_order(transform_managed);
  std::vector<ReparentUndoStep> result;
  for (ReparentUndoStep step : steps) {
    if (step != ReparentUndoStep::kRestoreTransform) result.push_back(step);
  }
  return result;
}

} // namespace

TEST(ReparentUndoOrderTest, DetachesFromNewParentBeforeReattaching) {
  const std::vector<ReparentUndoStep> steps = without_transform(false);
  ASSERT_EQ(steps.size(), 4u);
  EXPECT_EQ(steps.front(), ReparentUndoStep::kDetachFromNewParent)
      << "the node still belongs to the new parent when undo begins; it must "
         "be detached first";
  EXPECT_EQ(steps[1], ReparentUndoStep::kAttachToOldParent);
  EXPECT_EQ(steps[2], ReparentUndoStep::kMoveChildToOldIndex);
  EXPECT_EQ(steps.back(), ReparentUndoStep::kRestoreOwner);
}

TEST(ReparentUndoOrderTest, TransformIsRestoredAfterReattach) {
  const std::vector<ReparentUndoStep> steps = reparent_undo_order(true);
  std::size_t attach = steps.size();
  std::size_t transform = steps.size();
  for (std::size_t i = 0; i < steps.size(); ++i) {
    if (steps[i] == ReparentUndoStep::kAttachToOldParent) attach = i;
    if (steps[i] == ReparentUndoStep::kRestoreTransform) transform = i;
  }
  ASSERT_LT(attach, steps.size());
  ASSERT_LT(transform, steps.size());
  EXPECT_LT(attach, transform)
      << "transform must be restored after the node is back in the tree";
}

TEST(ReparentUndoOrderTest, NoTransformStepWhenNotManaged) {
  const std::vector<ReparentUndoStep> steps = reparent_undo_order(false);
  for (ReparentUndoStep step : steps) {
    EXPECT_NE(step, ReparentUndoStep::kRestoreTransform);
  }
}
