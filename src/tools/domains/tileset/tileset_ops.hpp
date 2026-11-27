#ifndef GODOT_AUTOPILOT_TILESET_OPS_HPP
#define GODOT_AUTOPILOT_TILESET_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace tileset_ops {

godot_autopilot::util::JsonValue handle_add_atlas_source(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_add_physics_layer(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_set_tile_collision(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_update_tileset(const godot_autopilot::util::JsonValue &args);

} // namespace tileset_ops
} // namespace godot_autopilot

#endif
