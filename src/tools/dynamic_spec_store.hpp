#ifndef GODOT_AUTOPILOT_DYNAMIC_SPEC_STORE_HPP
#define GODOT_AUTOPILOT_DYNAMIC_SPEC_STORE_HPP

#include <mutex>
#include <vector>

#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace dynamic_specs {

inline std::vector<ToolSpec> &store() {
  static std::vector<ToolSpec> v;
  return v;
}

inline std::mutex &mutex() {
  static std::mutex m;
  return m;
}

} // namespace dynamic_specs
} // namespace godot_autopilot

#endif
