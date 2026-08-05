#ifndef GODOT_SELF_DRIVING_TILESET_OPS_HPP
#define GODOT_SELF_DRIVING_TILESET_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace tileset_ops {

mcp::JsonValue handle_add_atlas_source(const mcp::JsonValue& args);
mcp::JsonValue handle_add_physics_layer(const mcp::JsonValue& args);
mcp::JsonValue handle_set_tile_collision(const mcp::JsonValue& args);

} // namespace tileset_ops
} // namespace godot_self_driving

#endif
