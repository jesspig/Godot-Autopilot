#ifndef GODOT_AUTOPILOT_EDITOR_READINESS_HPP
#define GODOT_AUTOPILOT_EDITOR_READINESS_HPP

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {

bool is_import_in_progress();

mcp::JsonValue busy_error();

} // namespace godot_autopilot

#endif
