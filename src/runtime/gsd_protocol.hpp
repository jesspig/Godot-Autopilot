#pragma once

// ============================================================================
// gsd 调试通道协议契约（唯一权威来源）
// ============================================================================
// 本文件收敛编辑器<->游戏进程 gsd 调试通道的全部协议常量与字段名，
// 防止各任务各自定义字符串字面量导致漂移。后续任务（游戏侧 / 编辑器侧 /
// 注册层）必须引用本文件，不得重复定义字符串字面量。
//
// 协议语义：
//
// 1. ready 握手时序：
//    - 编辑器启动游戏进程后等待游戏就绪（GSD_READY_WAIT_MS 上限）。
//    - 游戏侧在 EngineDebugger register_listener("gsd") 成功后，主动发送
//      gsd:ready 消息（消息体含 GSD_FIELD_LAST_ACTIVITY_MS）。
//    - 编辑器侧收到 gsd:ready 后标记该 session 为 ready，此后才可发送请求。
//    - 游戏首次运行自动继续播放时，等待窗口缩短为 GSD_PLAY_READY_WAIT_MS。
//
// 2. cancel 语义：
//    - 编辑器侧请求 cancel，游戏侧在后台线程收到后取消对应请求。
//    - cancel 幂等：游戏侧以 done_ 标志防重复完成，重复 cancel / 已完成的
//      cancel 均不产生副作用，响应中带 GSD_FIELD_CANCELLED 标记。
//
// 3. ping 用途：
//    - 编辑器侧周期性发送 ping 对游戏进程做物理健康检测（基于
//      GSD_FIELD_LAST_ACTIVITY_MS 判断是否存活/卡死）。
//
// 4. 消息路由：
//    - 编辑器 -> 游戏：gsd:request（消息体含 op / request_id / params）。
//    - 游戏 -> 编辑器：gsd:response（消息体含 request_id / ok / result /
//      error / cancelled）。
//    - 消息体字段命名统一使用本文件 GSD_FIELD_* 常量。
// ============================================================================

#include <string_view>

namespace godot_self_driving {

// ---- 消息前缀与消息名 ----
inline constexpr std::string_view GSD_PREFIX = "gsd";
inline constexpr std::string_view GSD_MSG_READY = "gsd:ready";
inline constexpr std::string_view GSD_MSG_RESPONSE = "gsd:response";
inline constexpr std::string_view GSD_MSG_REQUEST = "gsd:request";

// ---- 操作名 ----
inline constexpr std::string_view GSD_OP_PING = "ping";
inline constexpr std::string_view GSD_OP_CANCEL = "cancel";
inline constexpr std::string_view GSD_OP_STATUS = "status";
inline constexpr std::string_view GSD_OP_EVAL = "eval";
inline constexpr std::string_view GSD_OP_INPUT = "input";
inline constexpr std::string_view GSD_OP_INPUT_WAIT = "input_wait";
inline constexpr std::string_view GSD_OP_INPUT_STATUS = "input_status";
inline constexpr std::string_view GSD_OP_CAPTURE = "capture";
inline constexpr std::string_view GSD_OP_GET_ERRORS = "get_errors";
inline constexpr std::string_view GSD_OP_GET_OUTPUT = "get_output";
inline constexpr std::string_view GSD_OP_GET_TREE = "get_tree";

// ---- 请求字段（gsd:request 消息体） ----
inline constexpr std::string_view GSD_FIELD_REQUEST_ID = "request_id";
inline constexpr std::string_view GSD_FIELD_OP = "op";
inline constexpr std::string_view GSD_FIELD_PARAMS = "params";

// ---- 响应字段（gsd:response 消息体） ----
inline constexpr std::string_view GSD_FIELD_OK = "ok";
inline constexpr std::string_view GSD_FIELD_ERROR = "error";
inline constexpr std::string_view GSD_FIELD_RESULT = "result";
inline constexpr std::string_view GSD_FIELD_CANCELLED = "cancelled";

// ---- 输入回读字段（input 请求结果） ----
inline constexpr std::string_view GSD_FIELD_PARSED_PHYSICS_FRAME = "parsed_physics_frame";
inline constexpr std::string_view GSD_FIELD_PARSED_PROCESS_FRAME = "parsed_process_frame";
inline constexpr std::string_view GSD_FIELD_MATCHED_AT_PHYSICS_FRAME = "matched_at_physics_frame";

// ---- 状态字段 ----
inline constexpr std::string_view GSD_FIELD_LAST_ACTIVITY_MS = "last_activity_ms";
inline constexpr std::string_view GSD_FIELD_READY = "ready";
inline constexpr std::string_view GSD_FIELD_HEALTHY = "healthy";

// ---- 超时与窗口值 ----
// 编辑器侧等待游戏就绪的上限
inline constexpr int GSD_READY_WAIT_MS = 5000;
// 游戏首次自动继续播放时的缩短就绪等待
inline constexpr int GSD_PLAY_READY_WAIT_MS = 2000;
// 自动 continue 次数的环境变量名与上限
inline constexpr std::string_view GSD_AUTO_CONTINUE_ENV = "GSD_AUTO_CONTINUE";
inline constexpr int GSD_AUTO_CONTINUE_MAX = 3;
// 新场景切换后的等待与轮询间隔
inline constexpr int GSD_NEW_SCENE_SWITCH_WAIT_MS = 2000;
inline constexpr int GSD_NEW_SCENE_POLL_MS = 50;

} // namespace godot_self_driving
