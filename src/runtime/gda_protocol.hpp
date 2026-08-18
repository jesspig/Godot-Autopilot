#pragma once

//

//

//

//

//

#include <string_view>

namespace godot_autopilot {

inline constexpr std::string_view GDA_PREFIX = "gda";
inline constexpr std::string_view GDA_MSG_READY = "gda:ready";
inline constexpr std::string_view GDA_MSG_RESPONSE = "gda:response";
inline constexpr std::string_view GDA_MSG_REQUEST = "gda:request";

inline constexpr std::string_view GDA_OP_PING = "ping";
inline constexpr std::string_view GDA_OP_CANCEL = "cancel";
inline constexpr std::string_view GDA_OP_STATUS = "status";
inline constexpr std::string_view GDA_OP_EVAL = "eval";
inline constexpr std::string_view GDA_OP_INPUT = "input";
inline constexpr std::string_view GDA_OP_INPUT_WAIT = "input_wait";
inline constexpr std::string_view GDA_OP_INPUT_STATUS = "input_status";
inline constexpr std::string_view GDA_OP_CAPTURE = "capture";
inline constexpr std::string_view GDA_OP_GET_ERRORS = "get_errors";
inline constexpr std::string_view GDA_OP_GET_OUTPUT = "get_output";
inline constexpr std::string_view GDA_OP_GET_TREE = "get_tree";

inline constexpr std::string_view GDA_FIELD_REQUEST_ID = "request_id";
inline constexpr std::string_view GDA_FIELD_OP = "op";
inline constexpr std::string_view GDA_FIELD_PARAMS = "params";

inline constexpr std::string_view GDA_FIELD_OK = "ok";
inline constexpr std::string_view GDA_FIELD_ERROR = "error";
inline constexpr std::string_view GDA_FIELD_RESULT = "result";
inline constexpr std::string_view GDA_FIELD_CANCELLED = "cancelled";

inline constexpr std::string_view GDA_FIELD_PARSED_PHYSICS_FRAME =
    "parsed_physics_frame";
inline constexpr std::string_view GDA_FIELD_PARSED_PROCESS_FRAME =
    "parsed_process_frame";
inline constexpr std::string_view GDA_FIELD_MATCHED_AT_PHYSICS_FRAME =
    "matched_at_physics_frame";

inline constexpr std::string_view GDA_FIELD_LAST_ACTIVITY_MS =
    "last_activity_ms";
inline constexpr std::string_view GDA_FIELD_READY = "ready";
inline constexpr std::string_view GDA_FIELD_HEALTHY = "healthy";

inline constexpr int GDA_READY_WAIT_MS = 5000;

inline constexpr int GDA_PLAY_READY_WAIT_MS = 2000;

inline constexpr std::string_view GDA_AUTO_CONTINUE_ENV = "GDA_AUTO_CONTINUE";
inline constexpr int GDA_AUTO_CONTINUE_MAX = 3;

inline constexpr int GDA_NEW_SCENE_SWITCH_WAIT_MS = 2000;
inline constexpr int GDA_NEW_SCENE_POLL_MS = 50;

} // namespace godot_autopilot
