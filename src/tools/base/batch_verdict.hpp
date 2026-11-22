#ifndef GODOT_AUTOPILOT_BATCH_VERDICT_HPP
#define GODOT_AUTOPILOT_BATCH_VERDICT_HPP

#include "tools/base/tool_spec.hpp"

#include <mcp/JsonValue.hpp>

namespace godot_autopilot {
namespace batch_detail {

inline bool op_succeeded(const mcp::JsonValue &result) {
  return spec_detail::verdict_of(result).ok;
}

} // namespace batch_detail
} // namespace godot_autopilot

#endif
