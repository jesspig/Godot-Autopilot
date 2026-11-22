#ifndef GODOT_AUTOPILOT_TRASH_CONFIRM_HPP
#define GODOT_AUTOPILOT_TRASH_CONFIRM_HPP

#include <string>

namespace godot_autopilot {
namespace os_ops_detail {

inline bool trash_needs_confirm(const std::string &action, bool confirm_provided) {
  return action == "trash" && !confirm_provided;
}

} // namespace os_ops_detail
} // namespace godot_autopilot

#endif
