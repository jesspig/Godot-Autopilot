#ifndef GODOT_AUTOPILOT_TWEEN_OPS_HPP
#define GODOT_AUTOPILOT_TWEEN_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace tween_ops {

mcp::JsonValue handle_create_game_tween(const mcp::JsonValue &args);
mcp::JsonValue handle_control_game_tween(const mcp::JsonValue &args);
mcp::JsonValue handle_get_game_tween_list(const mcp::JsonValue &args);
mcp::JsonValue handle_tween_editor_property(const mcp::JsonValue &args);

} // namespace tween_ops
} // namespace godot_autopilot

#endif
