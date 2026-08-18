#ifndef GODOT_AUTOPILOT_CONFIG_HPP
#define GODOT_AUTOPILOT_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace godot_autopilot {

constexpr int GDA_DEFAULT_PORT = 9527;
constexpr int64_t GDA_HEALTHY_ACTIVITY_THRESHOLD_MS = 3000;
constexpr int64_t GDA_DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t GDA_MAX_TIMEOUT_MS = 30000;
constexpr size_t GDA_ERROR_BUFFER_MAX = 200;
constexpr size_t GDA_OUTPUT_BUFFER_MAX = 500;
constexpr size_t GDA_EVAL_TRUNCATE_BYTES = 8192;

} // namespace godot_autopilot

#endif
