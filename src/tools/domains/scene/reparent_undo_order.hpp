#ifndef GODOT_AUTOPILOT_REPARENT_UNDO_ORDER_HPP
#define GODOT_AUTOPILOT_REPARENT_UNDO_ORDER_HPP

#include <vector>

namespace godot_autopilot {
namespace scene_ops {

enum class ReparentUndoStep {
  kMoveChildToOldIndex,
  kRestoreTransform,
  kRestoreOwner,
  kAttachToOldParent,
  kDetachFromNewParent,
};

inline std::vector<ReparentUndoStep> reparent_undo_order(bool transform_managed) {
  std::vector<ReparentUndoStep> steps;
  steps.push_back(ReparentUndoStep::kDetachFromNewParent);
  steps.push_back(ReparentUndoStep::kAttachToOldParent);
  steps.push_back(ReparentUndoStep::kMoveChildToOldIndex);
  if (transform_managed) {
    steps.push_back(ReparentUndoStep::kRestoreTransform);
  }
  steps.push_back(ReparentUndoStep::kRestoreOwner);
  return steps;
}

} // namespace scene_ops
} // namespace godot_autopilot

#endif
