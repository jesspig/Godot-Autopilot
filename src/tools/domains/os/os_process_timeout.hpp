#ifndef GODOT_AUTOPILOT_OS_PROCESS_TIMEOUT_HPP
#define GODOT_AUTOPILOT_OS_PROCESS_TIMEOUT_HPP

#include <string>

namespace godot_autopilot {
namespace os_ops_detail {

inline std::string termination_detail(bool termination_confirmed) {
  return termination_confirmed
      ? "it was terminated and the call is cancelled (not retryable)"
      : "termination was not confirmed; the process may still be running "
        "(not retryable - inspect the process before acting again)";
}

} // namespace os_ops_detail
} // namespace godot_autopilot

#endif
