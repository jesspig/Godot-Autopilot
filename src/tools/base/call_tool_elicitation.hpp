#ifndef GODOT_AUTOPILOT_CALL_TOOL_ELICITATION_HPP
#define GODOT_AUTOPILOT_CALL_TOOL_ELICITATION_HPP

#include "tools/base/confirmation.hpp"

namespace godot_autopilot {
namespace elicitation_detail {

inline SideEffect effective_side_effect(bool is_call_tool,
                                        SideEffect surface_effect,
                                        SideEffect target_effect) {
  return is_call_tool ? target_effect : surface_effect;
}

inline bool should_elicit(SideEffect effect, bool confirm_provided,
                          bool client_supports_elicitation) {
  return confirmation::required_for(effect) && !confirm_provided &&
         client_supports_elicitation;
}

} // namespace elicitation_detail
} // namespace godot_autopilot

#endif
