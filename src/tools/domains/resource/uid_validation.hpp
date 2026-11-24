#ifndef GODOT_AUTOPILOT_UID_VALIDATION_HPP
#define GODOT_AUTOPILOT_UID_VALIDATION_HPP

#include <cstdint>

namespace godot_autopilot {
namespace resource_ops_detail {

enum class UidVerdict { kAcceptAdd, kAcceptSet, kReject };

enum class UidReassignFailure {
  kNone,
  kRequiresOverwrite,
  kConfirmationRequired,
  kScanTruncated,
};

inline UidReassignFailure check_uid_reassign(bool overwrite, bool confirm,
                                             bool already_registered,
                                             bool scan_truncated) {
  if (scan_truncated) return UidReassignFailure::kScanTruncated;
  if (!already_registered) return UidReassignFailure::kNone;
  if (!overwrite) return UidReassignFailure::kRequiresOverwrite;
  return confirm ? UidReassignFailure::kNone
                 : UidReassignFailure::kConfirmationRequired;
}

inline UidVerdict validate_uid_assignment(bool explicit_uid, int64_t uid,
                                          bool already_registered,
                                          bool overwrite) {
  if (explicit_uid && uid <= 0) return UidVerdict::kReject;
  if (!already_registered) return UidVerdict::kAcceptAdd;
  if (explicit_uid && !overwrite) return UidVerdict::kReject;
  return UidVerdict::kAcceptSet;
}

} // namespace resource_ops_detail
} // namespace godot_autopilot

#endif
