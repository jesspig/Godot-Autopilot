#ifndef GODOT_AUTOPILOT_SCENE_SAFETY_HPP
#define GODOT_AUTOPILOT_SCENE_SAFETY_HPP

#include <string>

namespace godot_autopilot {
namespace scene_safety {

enum class PackRecoveryAction {
  kSucceeded,
  kAbortBeforeCommit,
  kRestoreBackup,
  kDiscardStaging,
};

inline PackRecoveryAction pack_recovery_action(bool existed, bool verified,
                                               bool commit_succeeded) {
  if (!verified) return PackRecoveryAction::kAbortBeforeCommit;
  if (commit_succeeded) return PackRecoveryAction::kSucceeded;
  return existed ? PackRecoveryAction::kRestoreBackup
                 : PackRecoveryAction::kDiscardStaging;
}

inline bool is_scene_file(const std::string &path) {
  return (path.size() >= 5 &&
          path.compare(path.size() - 5, 5, ".tscn") == 0) ||
         (path.size() >= 4 && path.compare(path.size() - 4, 4, ".scn") == 0);
}

enum class ReplaceConfirmFailure { kNone, kConfirmationRequired };

inline ReplaceConfirmFailure check_replace_confirm(bool confirm) {
  return confirm ? ReplaceConfirmFailure::kNone
                 : ReplaceConfirmFailure::kConfirmationRequired;
}

} // namespace scene_safety
} // namespace godot_autopilot

#endif
