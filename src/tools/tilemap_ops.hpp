#ifndef GODOT_SELF_DRIVING_TILEMAP_OPS_HPP
#define GODOT_SELF_DRIVING_TILEMAP_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace tilemap_ops {

mcp::JsonValue handle_create(const mcp::JsonValue& args);
mcp::JsonValue handle_set_cell(const mcp::JsonValue& args);
mcp::JsonValue handle_set_cells(const mcp::JsonValue& args);
mcp::JsonValue handle_tileset_create(const mcp::JsonValue& args);

} // namespace tilemap_ops
} // namespace godot_self_driving

#endif
