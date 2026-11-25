#ifndef GODOT_AUTOPILOT_TOOL_PIPELINE_HPP
#define GODOT_AUTOPILOT_TOOL_PIPELINE_HPP

#include "util/json_value.hpp"

#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace pipeline {

godot_autopilot::util::JsonValue run_post(const ToolSpec &spec, const godot_autopilot::util::JsonValue &args,
                        godot_autopilot::util::JsonValue result);

} // namespace pipeline
} // namespace godot_autopilot

#endif
