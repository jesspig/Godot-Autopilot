#ifndef GODOT_SELF_DRIVING_SCENE_OPS_HPP
#define GODOT_SELF_DRIVING_SCENE_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_self_driving {
namespace scene_ops {

mcp::JsonValue handle_create(const mcp::JsonValue& args);
mcp::JsonValue handle_delete(const mcp::JsonValue& args);
mcp::JsonValue handle_get_tree(const mcp::JsonValue& args);

} // namespace scene_ops
} // namespace godot_self_driving

#endif
