#ifndef GODOT_AUTOPILOT_SANITIZE_POLICY_HPP
#define GODOT_AUTOPILOT_SANITIZE_POLICY_HPP

#include <atomic>
#include <optional>
#include <string>

namespace godot_autopilot {
namespace sanitize_policy {

constexpr const char *kEnvKey = "GODOT_AUTOPILOT_DESENSITIZE";

namespace detail {

inline std::atomic<bool> &cached_enabled() {
  static std::atomic<bool> value{true};
  return value;
}

inline std::optional<bool> env_declared(const char *raw) {
  if (raw == nullptr || *raw == '\0') {
    return std::nullopt;
  }
  const std::string value(raw);
  return !(value == "0" || value == "false" || value == "off");
}

} // namespace detail

inline bool resolve_enabled(const char *env_raw, bool config_value) {
  return detail::env_declared(env_raw).value_or(config_value);
}

inline bool enabled() { return detail::cached_enabled().load(); }

inline void set_enabled(bool value) {
  detail::cached_enabled().store(value);
}

void initialize();

} // namespace sanitize_policy
} // namespace godot_autopilot

#endif
