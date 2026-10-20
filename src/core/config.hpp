#ifndef GODOT_AUTOPILOT_CONFIG_HPP
#define GODOT_AUTOPILOT_CONFIG_HPP

#include <cstddef>
#include <cstdint>

namespace godot_autopilot {

constexpr int GDA_DEFAULT_PORT = 9527;
constexpr int GDA_PORT_CANDIDATE_BASE = 20000;
constexpr int GDA_PORT_CANDIDATE_LIMIT = 29999;
constexpr int GDA_PORT_PROBE_LIMIT = 256;
constexpr int GDA_CONTROL_PORT_BASE = 19900;
constexpr int GDA_CONTROL_PORT_LIMIT = 19999;
constexpr int GDA_PRIVATE_PORT_BASE = 32768;
constexpr const char *GDA_MCP_PATH = "/mcp";
constexpr const char *GDA_API_PATH = "/api";
constexpr const char *GDA_HOME_DIR_NAME = ".godot-autopilot";
constexpr int GDA_PROJECT_HASH_LENGTH = 8;
constexpr int GDA_API_TOKEN_BYTES = 32;
static_assert(GDA_PORT_CANDIDATE_BASE <= GDA_PORT_CANDIDATE_LIMIT,
              "the auto assigned port candidate floor must not exceed the "
              "candidate ceiling");
static_assert(GDA_CONTROL_PORT_BASE <= GDA_CONTROL_PORT_LIMIT,
              "the control plane port floor must not exceed the control "
              "plane port ceiling");
static_assert(GDA_CONTROL_PORT_LIMIT < GDA_PORT_CANDIDATE_BASE,
              "the auto assigned port candidate range must not overlap the "
              "control plane port range");
static_assert(GDA_PORT_CANDIDATE_LIMIT < GDA_PRIVATE_PORT_BASE,
              "the auto assigned port candidate range must stay below the "
              "private dynamic port range");
static_assert(GDA_PORT_PROBE_LIMIT > 0 &&
                  GDA_PORT_PROBE_LIMIT <=
                      GDA_PORT_CANDIDATE_LIMIT - GDA_PORT_CANDIDATE_BASE + 1,
              "the port probe limit must be positive and must not exceed the "
              "auto assigned port candidate count");
static_assert(GDA_PROJECT_HASH_LENGTH > 0 && GDA_PROJECT_HASH_LENGTH <= 16,
              "the project path hash length must be positive and must fit the "
              "hexadecimal digest width");
static_assert(GDA_API_TOKEN_BYTES >= 16,
              "the API token size must stay at least 128 bits of entropy");
constexpr int64_t GDA_HEALTHY_ACTIVITY_THRESHOLD_MS = 3000;
constexpr int64_t GDA_DEFAULT_TIMEOUT_MS = 5000;
constexpr int64_t GDA_MAX_TIMEOUT_MS = 30000;
constexpr int64_t GDA_OS_PROCESS_MIN_TIMEOUT_MS = 100;
constexpr int64_t GDA_OS_PROCESS_MAX_TIMEOUT_MS = 25000;
static_assert(GDA_OS_PROCESS_MIN_TIMEOUT_MS > 0 &&
                  GDA_OS_PROCESS_MAX_TIMEOUT_MS <= GDA_MAX_TIMEOUT_MS,
              "the external process timeout bounds must stay inside the shared "
              "timeout range");
constexpr int64_t GDA_RESPONSE_GRACE_MS = 2000;
constexpr int64_t GDA_MAX_GAME_OP_TIMEOUT_MS = 25000;
constexpr int64_t GDA_TRANSPORT_TIMEOUT_MS = 30000;
constexpr int64_t GDA_DISPATCH_WAIT_MIN_MS = 1000;
constexpr int64_t GDA_DISPATCH_WAIT_MAX_MS = 29000;
constexpr int64_t GDA_DISPATCH_WAIT_DEFAULT_MS = 27000;
constexpr int64_t GDA_DISPATCH_WAIT_HEALTH_MS = 5000;
constexpr int64_t GDA_DISPATCH_WAIT_LONG_BLOCKING_MS = GDA_DISPATCH_WAIT_MAX_MS;
static_assert(GDA_DISPATCH_WAIT_MIN_MS <= GDA_DISPATCH_WAIT_MAX_MS,
              "the dispatch wait clamp floor must not exceed the clamp "
              "ceiling");
static_assert(GDA_DISPATCH_WAIT_MAX_MS < GDA_TRANSPORT_TIMEOUT_MS,
              "the dispatch wait clamp ceiling must stay below the HTTP "
              "transport timeout so a structured timeout result can reach "
              "the client");
static_assert(GDA_DISPATCH_WAIT_MIN_MS <= GDA_DISPATCH_WAIT_DEFAULT_MS &&
                  GDA_DISPATCH_WAIT_DEFAULT_MS <= GDA_DISPATCH_WAIT_MAX_MS,
              "the default dispatch wait budget must stay inside the clamp "
              "interval so the constant equals the effective budget");
static_assert(GDA_DISPATCH_WAIT_MIN_MS <= GDA_DISPATCH_WAIT_HEALTH_MS &&
                  GDA_DISPATCH_WAIT_HEALTH_MS <= GDA_DISPATCH_WAIT_MAX_MS,
              "the health probe dispatch wait budget must stay inside the "
              "clamp interval so the constant equals the effective budget");
static_assert(GDA_DISPATCH_WAIT_MIN_MS <=
                      GDA_DISPATCH_WAIT_LONG_BLOCKING_MS &&
                  GDA_DISPATCH_WAIT_LONG_BLOCKING_MS <= GDA_DISPATCH_WAIT_MAX_MS,
              "the long blocking dispatch wait budget must stay inside the "
              "clamp interval so the constant equals the effective budget");
static_assert(GDA_DISPATCH_WAIT_LONG_BLOCKING_MS < GDA_TRANSPORT_TIMEOUT_MS,
              "the effective long blocking dispatch wait budget must stay "
              "below the HTTP transport timeout so a structured timeout "
              "result can reach the client");
constexpr int64_t GDA_CODE_EXEC_MAX_TIMEOUT_MS = 27000;
constexpr int64_t GDA_CODE_EXEC_BUDGET_MARGIN_MS = 2000;
constexpr int64_t GDA_CODE_EXEC_TRANSPORT_MARGIN_MS = 2000;
static_assert(GDA_CODE_EXEC_MAX_TIMEOUT_MS <= GDA_DISPATCH_WAIT_MAX_MS,
              "the code_execute timeout ceiling must not exceed the dispatch "
              "wait clamp ceiling so the script deadline can fire before the "
              "host wait budget expires");
static_assert(GDA_CODE_EXEC_MAX_TIMEOUT_MS +
                      GDA_CODE_EXEC_BUDGET_MARGIN_MS ==
                  GDA_DISPATCH_WAIT_MAX_MS,
              "the code_execute timeout ceiling plus its budget margin must "
              "equal the dispatch wait clamp ceiling");
static_assert(GDA_CODE_EXEC_MAX_TIMEOUT_MS +
                      GDA_CODE_EXEC_TRANSPORT_MARGIN_MS <=
                  GDA_TRANSPORT_TIMEOUT_MS,
              "the code_execute timeout ceiling must keep one transport margin "
              "below the HTTP transport timeout so a structured timeout "
              "result can reach the client");
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
