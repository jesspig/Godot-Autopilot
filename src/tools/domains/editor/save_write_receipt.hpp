#ifndef GODOT_AUTOPILOT_SAVE_WRITE_RECEIPT_HPP
#define GODOT_AUTOPILOT_SAVE_WRITE_RECEIPT_HPP

#include <cstdint>
#include <string>

namespace godot_autopilot {
namespace editor_ops_detail {

struct FileStamp {
  bool exists{false};
  uint64_t modified_ns{0};
  int64_t size_bytes{0};
};

enum class WriteOutcome { kVerified, kUnverifiable };

inline WriteOutcome decide_write_outcome(const FileStamp &before, const FileStamp &after) {
  if (!after.exists) return WriteOutcome::kUnverifiable;
  if (!before.exists) return WriteOutcome::kVerified;
  if (after.modified_ns != before.modified_ns) return WriteOutcome::kVerified;
  if (after.size_bytes != before.size_bytes) return WriteOutcome::kVerified;
  return WriteOutcome::kUnverifiable;
}

FileStamp file_stamp(const std::string &path);

} // namespace editor_ops_detail
} // namespace godot_autopilot

#endif
