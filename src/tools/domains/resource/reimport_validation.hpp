#ifndef GODOT_AUTOPILOT_REIMPORT_VALIDATION_HPP
#define GODOT_AUTOPILOT_REIMPORT_VALIDATION_HPP

#include <cstddef>

namespace godot_autopilot {
namespace resource_ops_detail {

struct ReimportCounts {
  bool has_files{false};
  bool files_is_array{false};
  std::size_t string_count{0};
  bool path_is_string{false};
};

inline bool validate_reimport_counts(const ReimportCounts &c, std::size_t max_files) {
  if (c.has_files) {
    return c.files_is_array && c.string_count > 0 && c.string_count <= max_files;
  }
  return c.path_is_string;
}

} // namespace resource_ops_detail
} // namespace godot_autopilot

#endif
