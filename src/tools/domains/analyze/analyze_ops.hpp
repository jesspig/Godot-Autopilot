#ifndef GODOT_AUTOPILOT_ANALYZE_OPS_HPP
#define GODOT_AUTOPILOT_ANALYZE_OPS_HPP

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace analyze_ops {

godot_autopilot::util::JsonValue handle_analyze_project(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_validate_scene_file(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_find_unused_resources(const godot_autopilot::util::JsonValue &args);
godot_autopilot::util::JsonValue handle_trace_signal_flow(const godot_autopilot::util::JsonValue &args);

} // namespace analyze_ops
} // namespace godot_autopilot

#endif
