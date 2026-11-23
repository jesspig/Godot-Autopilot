#ifndef GODOT_AUTOPILOT_CALL_DEPTH_GUARD_HPP
#define GODOT_AUTOPILOT_CALL_DEPTH_GUARD_HPP

#include <cstdint>

namespace godot_autopilot {
namespace meta_detail {

inline bool call_depth_exceeds(int64_t current_depth, int64_t max_depth) {
  return current_depth >= max_depth;
}

} // namespace meta_detail
} // namespace godot_autopilot

#endif
