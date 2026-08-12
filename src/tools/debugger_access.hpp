#ifndef GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {

// runtime_ops 对 debugger 能力的自由函数接口，实现于 debugger_access.cpp。
// 依赖方向：runtime_ops.cpp -> debugger_access.hpp（实现于 debugger_access.cpp）；
// debugger_ops.cpp -> runtime_ops.hpp。无环。

// 捕获插件是否已初始化（等价 DebugCapturePlugin::get_instance() != nullptr）。
bool debugger_capture_initialized();

// 将 payload 作为 gda:request 发给全部 active 且 ready 的会话。
// 返回是否至少发出一个会话；out_first_session_id 接收第一个会话 id（可为空）。
bool debugger_broadcast_request(const std::string &payload,
                                int32_t *out_first_session_id);

// 若会话存在且 active，向其发送 cancel 请求（request_id 对应超时的请求）。
void debugger_send_cancel(int32_t session_id, int64_t request_id);

// 返回全部 active 且 breaked 的会话 id 快照。
std::vector<int32_t> debugger_breaked_session_ids();

// 若会话存在且 active 且 breaked，向其发送 continue。
void debugger_continue_session(int32_t session_id);

namespace debugger_ops {

// runtime_ops 仅需该 capture 自由函数；与 debugger_ops.hpp 声明一致，实现同源。
std::string capture_get_errors_text(size_t limit);

} // namespace debugger_ops
} // namespace godot_autopilot

#endif
