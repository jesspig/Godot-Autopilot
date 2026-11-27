#ifndef GODOT_AUTOPILOT_BATCH_VERDICT_HPP
#define GODOT_AUTOPILOT_BATCH_VERDICT_HPP

#include "tools/base/tool_spec.hpp"

#include "util/json_value.hpp"

namespace godot_autopilot {
namespace batch_detail {

inline bool op_succeeded(const godot_autopilot::util::JsonValue &result) {
  return spec_detail::verdict_of(result).ok;
}

} // namespace batch_detail
} // namespace godot_autopilot

#endif
