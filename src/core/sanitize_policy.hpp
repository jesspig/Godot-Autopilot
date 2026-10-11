#ifndef GODOT_AUTOPILOT_SANITIZE_POLICY_HPP
#define GODOT_AUTOPILOT_SANITIZE_POLICY_HPP

#include <atomic>

namespace godot_autopilot {
namespace sanitize_policy {

namespace detail {

inline std::atomic<bool> &cached_enabled() {
  static std::atomic<bool> value{true};
  return value;
}

} // namespace detail

inline bool enabled() { return detail::cached_enabled().load(); }

inline void set_enabled(bool value) {
  detail::cached_enabled().store(value);
}

void initialize();

} // namespace sanitize_policy
} // namespace godot_autopilot

#endif
