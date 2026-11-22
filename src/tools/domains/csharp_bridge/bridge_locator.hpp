#ifndef GODOT_AUTOPILOT_BRIDGE_LOCATOR_HPP
#define GODOT_AUTOPILOT_BRIDGE_LOCATOR_HPP

#include <string>

namespace godot_autopilot {
namespace csharp_bridge_detail {

inline std::string bridge_ready_expression(const std::string &holder_var) {
  return holder_var + " != null and " + holder_var +
         ".has_meta(\"bridge\") and " + holder_var +
         ".get_meta(\"bridge\") != null";
}

} // namespace csharp_bridge_detail
} // namespace godot_autopilot

#endif
