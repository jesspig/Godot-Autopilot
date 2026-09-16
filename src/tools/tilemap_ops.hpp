#ifndef GODOT_AUTOPILOT_TILEMAP_OPS_HPP
#define GODOT_AUTOPILOT_TILEMAP_OPS_HPP

#include <cstdint>
#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace tilemap_ops {

struct RectRange {
  int64_t min_x = 0;
  int64_t min_y = 0;
  int64_t max_x = 0;
  int64_t max_y = 0;
  int64_t width = 0;
  int64_t height = 0;
};

constexpr int64_t kMaxFillCells = 100000;

RectRange normalize_rect_range(int64_t from_x, int64_t from_y, int64_t to_x,
                               int64_t to_y);

inline int64_t rect_cell_count_bounded(const RectRange &range, int64_t limit) {
  if (range.width > limit || range.height > limit)
    return limit + 1;
  return range.width * range.height;
}

inline bool rect_exceeds_cell_limit(const RectRange &range, int64_t limit) {
  return rect_cell_count_bounded(range, limit) > limit;
}

mcp::JsonValue handle_create(const mcp::JsonValue &args);
mcp::JsonValue handle_set_cell(const mcp::JsonValue &args);
mcp::JsonValue handle_set_cells(const mcp::JsonValue &args);
mcp::JsonValue handle_fill_rect(const mcp::JsonValue &args);
mcp::JsonValue handle_tileset_create(const mcp::JsonValue &args);

} // namespace tilemap_ops
} // namespace godot_autopilot

#endif
