#include "debugger_access.hpp"
#include "debugger_ops.hpp"
#include "runtime/gsd_protocol.hpp"
#include <cstdint>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <string>
#include <vector>

namespace godot_self_driving {

bool debugger_capture_initialized() {
  return debugger_ops::DebugCapturePlugin::get_instance() != nullptr;
}

bool debugger_broadcast_request(const std::string &payload,
                                int32_t *out_first_session_id) {
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return false;
  int32_t first_id = -1;
  std::vector<godot::Ref<godot::EditorDebuggerSession>> sessions;
  for (int32_t id : plugin->get_session_ids()) {
    auto session = plugin->get_session(id);
    if (!(session.is_valid() && session->is_active()))
      continue;
    if (!plugin->is_session_ready(id))
      continue;
    sessions.push_back(session);
    if (first_id < 0)
      first_id = id;
  }
  if (sessions.empty())
    return false;
  godot::Array arr;
  arr.push_back(godot::String(payload.c_str()));
  for (auto &session : sessions) {
    session->send_message(
        godot::String(std::string(GSD_MSG_REQUEST).c_str()), arr);
  }
  if (out_first_session_id)
    *out_first_session_id = first_id;
  return true;
}

void debugger_send_cancel(int32_t session_id, int64_t request_id) {
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return;
  auto session = plugin->get_session(session_id);
  if (!(session.is_valid() && session->is_active()))
    return;
  mcp::JsonValue cancel_body(mcp::JsonValue::object_tag);
  cancel_body[GSD_FIELD_REQUEST_ID] = mcp::JsonValue(request_id);
  cancel_body[GSD_FIELD_OP] = mcp::JsonValue(std::string(GSD_OP_CANCEL));
  cancel_body[GSD_FIELD_PARAMS] = mcp::JsonValue(mcp::JsonValue::object_tag);
  godot::Array arr;
  arr.push_back(godot::String(cancel_body.Dump().c_str()));
  session->send_message(
      godot::String(std::string(GSD_MSG_REQUEST).c_str()), arr);
}

std::vector<int32_t> debugger_breaked_session_ids() {
  std::vector<int32_t> ids;
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return ids;
  for (int32_t id : plugin->get_session_ids()) {
    auto session = plugin->get_session(id);
    if (session.is_valid() && session->is_active() && session->is_breaked())
      ids.push_back(id);
  }
  return ids;
}

void debugger_continue_session(int32_t session_id) {
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return;
  auto session = plugin->get_session(session_id);
  if (!(session.is_valid() && session->is_active() && session->is_breaked()))
    return;
  session->send_message(godot::String("continue"), godot::Array());
}

} // namespace godot_self_driving
