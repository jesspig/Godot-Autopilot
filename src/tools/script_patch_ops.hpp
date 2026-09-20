#ifndef GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP
#define GODOT_AUTOPILOT_SCRIPT_PATCH_OPS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace script_patch_ops {

mcp::JsonValue handle_patch(const mcp::JsonValue &args);

} // namespace script_patch_ops
} // namespace godot_autopilot

#endif
