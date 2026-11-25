#ifndef GODOT_AUTOPILOT_EDITOR_SAFETY_HPP
#define GODOT_AUTOPILOT_EDITOR_SAFETY_HPP

namespace godot_autopilot {
namespace editor_safety {

enum class SaveAsTargetFailure {
  kNone,
  kInvalidPath,
  kInvalidExtension,
  kTargetExists,
  kConfirmationRequired,
};

inline SaveAsTargetFailure check_save_as_target(bool path_valid,
                                                bool scene_extension,
                                                bool exists, bool overwrite,
                                                bool confirm) {
  if (!path_valid) return SaveAsTargetFailure::kInvalidPath;
  if (!scene_extension) return SaveAsTargetFailure::kInvalidExtension;
  if (!exists) return SaveAsTargetFailure::kNone;
  if (!overwrite) return SaveAsTargetFailure::kTargetExists;
  return confirm ? SaveAsTargetFailure::kNone
                 : SaveAsTargetFailure::kConfirmationRequired;
}

enum class ReloadDiscardFailure { kNone, kConfirmationRequired };

inline ReloadDiscardFailure check_reload_discard(bool has_unsaved,
                                                 bool discard_unsaved,
                                                 bool confirm) {
  if (!has_unsaved) return ReloadDiscardFailure::kNone;
  return (discard_unsaved && confirm) ? ReloadDiscardFailure::kNone
                                      : ReloadDiscardFailure::kConfirmationRequired;
}

} // namespace editor_safety
} // namespace godot_autopilot

#endif
