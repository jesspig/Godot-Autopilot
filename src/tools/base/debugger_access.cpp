#include "tools/base/debugger_access.hpp"
#include "tools/domains/debugger/debugger_ops.hpp"
#include "tools/base/ready_policy.hpp"
#include "runtime/gda_protocol.hpp"
#include <cstdint>
#include <godot_cpp/classes/editor_debugger_session.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include "util/json_value.hpp"
#include <mutex>
#include <string>
#include <vector>

namespace godot_autopilot {

namespace {

std::mutex g_request_backend_mtx;
std::shared_ptr<const DebuggerRequestBackend> g_request_backend;

std::shared_ptr<const DebuggerRequestBackend> request_backend() {
  std::lock_guard<std::mutex> lock(g_request_backend_mtx);
  return g_request_backend;
}

std::vector<DebuggerSessionSnapshot> engine_sessions() {
  std::vector<DebuggerSessionSnapshot> snapshots;
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin) {
    return snapshots;
  }
  for (int32_t id : plugin->get_session_ids()) {
    auto session = plugin->get_session(id);
    if (!session.is_valid()) {
      continue;
    }
    snapshots.push_back({id, session->is_active(), plugin->is_session_ready(id),
                         plugin->is_session_scene_ready(id)});
  }
  return snapshots;
}

} // namespace

void set_debugger_request_backend(
    std::shared_ptr<const DebuggerRequestBackend> backend) {
  std::lock_guard<std::mutex> lock(g_request_backend_mtx);
  g_request_backend = std::move(backend);
}

void clear_debugger_request_backend() {
  std::lock_guard<std::mutex> lock(g_request_backend_mtx);
  g_request_backend.reset();
}

bool debugger_capture_initialized() {
  return request_backend() != nullptr ||
         debugger_ops::DebugCapturePlugin::get_instance() != nullptr;
}

bool debugger_send_request(const std::string &payload, int32_t session_id) {
  if (auto backend = request_backend()) {
    return backend->send_request(session_id, payload);
  }
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin) {
    return false;
  }
  auto session = plugin->get_session(session_id);
  if (!(session.is_valid() && session->is_active())) {
    return false;
  }
  godot::Array arr;
  arr.push_back(godot::String::utf8(payload.c_str()));
  session->send_message(godot::String(std::string(GDA_MSG_REQUEST).c_str()),
                        arr);
  return true;
}

std::vector<int32_t> debugger_active_session_ids() {
  auto backend = request_backend();
  const auto snapshots = backend ? backend->sessions() : engine_sessions();
  std::vector<int32_t> ids;
  for (const auto &snapshot : snapshots) {
    if (snapshot.active) {
      ids.push_back(snapshot.id);
    }
  }
  return ids;
}

std::vector<int32_t> debugger_request_ready_session_ids() {
  auto backend = request_backend();
  const auto snapshots = backend ? backend->sessions() : engine_sessions();
  std::vector<int32_t> ids;
  for (const auto &snapshot : snapshots) {
    if (snapshot.active &&
        is_broadcast_eligible(snapshot.ready, snapshot.scene_ready)) {
      ids.push_back(snapshot.id);
    }
  }
  return ids;
}

int32_t debugger_active_session_count() {
  return static_cast<int32_t>(debugger_active_session_ids().size());
}

int32_t debugger_ready_session_count() {
  auto backend = request_backend();
  const auto snapshots = backend ? backend->sessions() : engine_sessions();
  int32_t count = 0;
  for (const auto &snapshot : snapshots) {
    if (snapshot.active && snapshot.ready) {
      ++count;
    }
  }
  return count;
}

int32_t debugger_scene_ready_session_count() {
  auto backend = request_backend();
  const auto snapshots = backend ? backend->sessions() : engine_sessions();
  int32_t count = 0;
  for (const auto &snapshot : snapshots) {
    if (snapshot.active && snapshot.scene_ready) {
      ++count;
    }
  }
  return count;
}

bool debugger_broadcast_engine_command(const godot::String &command,
                                       const godot::Array &data) {
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return false;
  bool sent = false;
  for (int32_t id : plugin->get_session_ids()) {
    auto session = plugin->get_session(id);
    if (!(session.is_valid() && session->is_active())) {
      if (session.is_valid())
        plugin->forget_ready_session(id);
      continue;
    }
    if (!plugin->is_session_ready(id))
      continue;
    session->send_message(command, data);
    sent = true;
  }
  return sent;
}

godot_autopilot::util::JsonValue build_cancel_request_body(int64_t request_id) {
  godot_autopilot::util::JsonValue cancel_body(godot_autopilot::util::JsonValue::object_tag);
  cancel_body[GDA_FIELD_REQUEST_ID] = godot_autopilot::util::JsonValue(request_id);
  cancel_body[GDA_FIELD_OP] = godot_autopilot::util::JsonValue(std::string(GDA_OP_CANCEL));
  godot_autopilot::util::JsonValue params(godot_autopilot::util::JsonValue::object_tag);
  params[GDA_FIELD_REQUEST_ID] = godot_autopilot::util::JsonValue(request_id);
  cancel_body[GDA_FIELD_PARAMS] = std::move(params);
  return cancel_body;
}

void debugger_send_cancel(int32_t session_id, int64_t request_id) {
  if (auto backend = request_backend()) {
    backend->send_cancel(session_id, request_id);
    return;
  }
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return;
  auto session = plugin->get_session(session_id);
  if (!(session.is_valid() && session->is_active()))
    return;
  const godot_autopilot::util::JsonValue cancel_body = build_cancel_request_body(request_id);
  godot::Array arr;
  arr.push_back(godot::String(cancel_body.Dump().c_str()));
  session->send_message(
      godot::String(std::string(GDA_MSG_REQUEST).c_str()), arr);
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

bool debugger_reload_scripts(int32_t session_id,
                              const std::vector<std::string> &script_paths) {
  auto *plugin = debugger_ops::DebugCapturePlugin::get_instance();
  if (!plugin)
    return false;
  auto session = plugin->get_session(session_id);
  if (!(session.is_valid() && session->is_active() &&
        plugin->is_session_ready(session_id)))
    return false;
  godot::Array arr;
  if (script_paths.empty()) {
    session->send_message(godot::String("reload_all_scripts"), godot::Array());
    return true;
  }
  for (const auto &path : script_paths) {
    arr.push_back(godot::String::utf8(path.c_str()));
  }
  session->send_message(godot::String("reload_scripts"), arr);
  return true;
}

} // namespace godot_autopilot
