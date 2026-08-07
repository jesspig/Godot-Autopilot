#ifndef GODOT_SELF_DRIVING_CONFIG_HPP
#define GODOT_SELF_DRIVING_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace godot_self_driving {

constexpr int GSD_DEFAULT_PORT = 9527;
constexpr int64_t GSD_HEALTHY_ACTIVITY_THRESHOLD_MS = 3000;
constexpr int64_t GSD_DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t GSD_MAX_TIMEOUT_MS = 30000;
constexpr size_t GSD_ERROR_BUFFER_MAX = 200;
constexpr size_t GSD_OUTPUT_BUFFER_MAX = 500;
constexpr size_t GSD_EVAL_TRUNCATE_BYTES = 8192;

} // namespace godot_self_driving

#endif
