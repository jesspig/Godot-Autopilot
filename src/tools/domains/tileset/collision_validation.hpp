#ifndef GODOT_AUTOPILOT_COLLISION_VALIDATION_HPP
#define GODOT_AUTOPILOT_COLLISION_VALIDATION_HPP

namespace godot_autopilot {
namespace tileset_detail {

inline bool physics_layer_valid(int layer, int layer_count) {
  return layer >= 0 && layer < layer_count;
}

} // namespace tileset_detail
} // namespace godot_autopilot

#endif
