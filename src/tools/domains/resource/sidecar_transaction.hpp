#ifndef GODOT_AUTOPILOT_SIDECAR_TRANSACTION_HPP
#define GODOT_AUTOPILOT_SIDECAR_TRANSACTION_HPP

#include "core/config.hpp"

#include <string>

namespace godot_autopilot {
namespace resource_ops_detail {

struct ResourceScanBudget {
  size_t files_listed = 0;
  size_t files_scanned = 0;
  size_t bytes_scanned = 0;
  std::string limit_reason;
  bool truncated = false;
  bool hit_limit(size_t file_bytes, size_t depth) {
    if (depth > GDA_SCAN_MAX_DEPTH) {
      limit_reason = "directory_depth";
      truncated = true;
      return true;
    }
    if (files_listed >= GDA_SCAN_MAX_FILES) {
      limit_reason = "file_count";
      truncated = true;
      return true;
    }
    if (file_bytes > GDA_SCAN_MAX_FILE_BYTES) {
      limit_reason = "single_file_bytes";
      truncated = true;
      return true;
    }
    if (file_bytes > GDA_SCAN_MAX_TOTAL_BYTES - bytes_scanned) {
      limit_reason = "total_bytes";
      truncated = true;
      return true;
    }
    return false;
  }
};

inline bool scan_must_abort(const ResourceScanBudget &budget,
                            bool truncated) {
  return budget.truncated || truncated;
}

} // namespace resource_ops_detail
} // namespace godot_autopilot

#endif
