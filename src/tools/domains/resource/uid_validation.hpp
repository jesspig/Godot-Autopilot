#ifndef GODOT_AUTOPILOT_UID_VALIDATION_HPP
#define GODOT_AUTOPILOT_UID_VALIDATION_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace godot_autopilot {
namespace resource_ops_detail {

enum class UidVerdict { kAcceptAdd, kAcceptSet, kReject };

inline std::string normalized_uid_sidecar(std::string_view text) {
  constexpr std::string_view kPrefix = "uid://";
  size_t begin = 0;
  while (begin < text.size() &&
         static_cast<unsigned char>(text[begin]) <= 0x20) {
    ++begin;
  }
  if (text.size() - begin >= 3 &&
      static_cast<unsigned char>(text[begin]) == 0xef &&
      static_cast<unsigned char>(text[begin + 1]) == 0xbb &&
      static_cast<unsigned char>(text[begin + 2]) == 0xbf) {
    begin += 3;
  }
  while (begin < text.size() &&
         static_cast<unsigned char>(text[begin]) <= 0x20) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin &&
         static_cast<unsigned char>(text[end - 1]) <= 0x20) {
    --end;
  }
  if (end <= begin + kPrefix.size() ||
      text.compare(begin, kPrefix.size(), kPrefix) != 0) {
    return {};
  }
  return std::string(text.substr(begin, end - begin));
}

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
