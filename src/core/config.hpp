#ifndef GODOT_AUTOPILOT_CONFIG_HPP
#define GODOT_AUTOPILOT_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace godot_autopilot {

constexpr int GDA_DEFAULT_PORT = 9527;
constexpr int64_t GDA_HEALTHY_ACTIVITY_THRESHOLD_MS = 3000;
constexpr int64_t GDA_DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t GDA_MAX_TIMEOUT_MS = 30000;
constexpr int64_t GDA_RESPONSE_GRACE_MS = 2000;
constexpr int64_t GDA_MAX_GAME_OP_TIMEOUT_MS = 25000;
constexpr int64_t GDA_TRANSPORT_TIMEOUT_MS = 30000;
constexpr int64_t GDA_MAX_GAME_OP_HOST_WAIT_MS =
    GDA_MAX_GAME_OP_TIMEOUT_MS + GDA_RESPONSE_GRACE_MS;
static_assert(GDA_MAX_GAME_OP_HOST_WAIT_MS < GDA_TRANSPORT_TIMEOUT_MS,
              "game op host wait (timeout_ms + response grace) must stay below "
              "the HTTP transport timeout");
constexpr size_t GDA_LATE_RESULT_BUFFER_MAX = 5;
constexpr size_t GDA_LATE_RESULT_SUMMARY_CHARS = 200;
constexpr size_t GDA_ERROR_BUFFER_MAX = 200;
constexpr size_t GDA_OUTPUT_BUFFER_MAX = 500;
constexpr size_t GDA_EVAL_TRUNCATE_BYTES = 8192;
constexpr int64_t GDA_CAPTURE_MAX_DIMENSION = 4096;
constexpr size_t GDA_CAPTURE_MAX_PNG_BYTES = 8 * 1024 * 1024;
constexpr size_t GDA_VARIANT_MAX_STRING_BYTES = 64 * 1024;
constexpr size_t GDA_VARIANT_MAX_ARRAY_ELEMENTS = 10000;
constexpr size_t GDA_MAX_JSON_RESPONSE_BYTES = 4 * 1024 * 1024;
constexpr size_t GDA_SCAN_MAX_FILES = 10000;
constexpr size_t GDA_SCAN_MAX_FILE_BYTES = 2 * 1024 * 1024;
constexpr size_t GDA_SCAN_MAX_TOTAL_BYTES = 32 * 1024 * 1024;
constexpr size_t GDA_SCAN_MAX_DEPTH = 64;

} // namespace godot_autopilot

#endif
