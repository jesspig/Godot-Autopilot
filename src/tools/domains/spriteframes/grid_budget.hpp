#ifndef GODOT_AUTOPILOT_GRID_BUDGET_HPP
#define GODOT_AUTOPILOT_GRID_BUDGET_HPP

#include <cstdint>

namespace godot_autopilot {
namespace grid_detail {

constexpr int64_t kMaxGridFrames = 10000;

inline int64_t grid_frame_count(int64_t hframes, int64_t vframes) {
  if (hframes <= 0 || vframes <= 0) return -1;
  if (hframes > kMaxGridFrames / vframes) return -1;
  return hframes * vframes;
}

} // namespace grid_detail
} // namespace godot_autopilot

#endif
