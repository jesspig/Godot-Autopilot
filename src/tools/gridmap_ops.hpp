#ifndef GODOT_AUTOPILOT_GRIDMAP_OPS_HPP
#define GODOT_AUTOPILOT_GRIDMAP_OPS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace gridmap_ops {

struct BoxRange {
  int64_t min_x = 0;
  int64_t min_y = 0;
  int64_t min_z = 0;
  int64_t max_x = 0;
  int64_t max_y = 0;
  int64_t max_z = 0;
  int64_t size_x = 0;
  int64_t size_y = 0;
  int64_t size_z = 0;
};

constexpr int64_t kMaxFillCells = 100000;
constexpr int kMinOrientation = 0;
constexpr int kMaxOrientation = 23;
constexpr int kInvalidCellItem = -1;

BoxRange normalize_box_range(int64_t from_x, int64_t from_y, int64_t from_z,
                             int64_t to_x, int64_t to_y, int64_t to_z);

inline int64_t box_cell_count_bounded(const BoxRange &range, int64_t limit) {
  if (range.size_x > limit || range.size_y > limit || range.size_z > limit)
    return limit + 1;
  return range.size_x * range.size_y * range.size_z;
}

inline bool box_exceeds_cell_limit(const BoxRange &range, int64_t limit) {
  return box_cell_count_bounded(range, limit) > limit;
}

mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_create_library_item(const mcp::JsonValue &args);
mcp::JsonValue handle_set_cell(const mcp::JsonValue &args);
mcp::JsonValue handle_fill_box(const mcp::JsonValue &args);
mcp::JsonValue handle_get_used_cells(const mcp::JsonValue &args);

} // namespace gridmap_ops
} // namespace godot_autopilot

#endif
