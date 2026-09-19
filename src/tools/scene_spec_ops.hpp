#ifndef GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP
#define GODOT_AUTOPILOT_SCENE_SPEC_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace scene_spec_ops {

inline constexpr int SPEC_MAX_DEPTH = 32;
inline constexpr int SPEC_MAX_NODES = 500;

mcp::JsonValue handle_build_from_spec(const mcp::JsonValue &args);

} // namespace scene_spec_ops
} // namespace godot_autopilot

#endif
