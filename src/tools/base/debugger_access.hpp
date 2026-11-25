#ifndef GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP
#define GODOT_AUTOPILOT_DEBUGGER_ACCESS_HPP

#include <cstdint>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

namespace godot_autopilot {

mcp::JsonValue build_cancel_request_body(int64_t request_id);

bool debugger_capture_initialized();

struct DebuggerSessionSnapshot {
  int32_t id = -1;
  bool active = false;
  bool ready = false;
  bool scene_ready = false;
};

class DebuggerRequestBackend {
public:
  virtual ~DebuggerRequestBackend() = default;
  virtual std::vector<DebuggerSessionSnapshot> sessions() const = 0;
  virtual bool send_request(int32_t session_id,
                            const std::string &payload) const = 0;
  virtual void send_cancel(int32_t session_id, int64_t request_id) const = 0;
};

void set_debugger_request_backend(
    std::shared_ptr<const DebuggerRequestBackend> backend);
void clear_debugger_request_backend();

bool debugger_broadcast_engine_command(const godot::String &command,
                                       const godot::Array &data);

bool debugger_send_request(const std::string &payload, int32_t session_id);

std::vector<int32_t> debugger_active_session_ids();
std::vector<int32_t> debugger_request_ready_session_ids();

int32_t debugger_active_session_count();

int32_t debugger_ready_session_count();

int32_t debugger_scene_ready_session_count();

void debugger_send_cancel(int32_t session_id, int64_t request_id);

std::vector<int32_t> debugger_breaked_session_ids();

void debugger_continue_session(int32_t session_id);

bool debugger_reload_scripts(int32_t session_id,
                              const std::vector<std::string> &script_paths);

namespace debugger_ops {

std::string capture_get_errors_text(size_t limit);

} // namespace debugger_ops
} // namespace godot_autopilot

#endif
