#include "core/api_server.hpp"

#include <cstdint>
#include <string>

#include "core/http_transport.hpp"
#include "core/http_transport_ext.hpp"
#include "core/log_system.hpp"

namespace godot_autopilot {

bool ApiServer::start(uint16_t port) const {
  if (!http_transport_is_listening()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::Transport,
        "the REST API routes need a bound HTTP transport");
    return false;
  }
  const uint16_t bound = http_transport_instance().bound_port();
  if (bound != 0 && bound != port) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::Transport,
        "the REST API routes need the transport port " +
            std::to_string(port) + " but the transport is bound to " +
            std::to_string(bound));
    return false;
  }
  register_routes(http_transport_route_table());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "the REST API routes are registered on port " +
                                std::to_string(bound));
  return true;
}

void ApiServer::stop() const {}

} // namespace godot_autopilot
