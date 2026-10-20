#ifndef GODOT_AUTOPILOT_API_SERVER_HPP
#define GODOT_AUTOPILOT_API_SERVER_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

#include <mcp/JsonValue.hpp>

#include "core/http_transport.hpp"

namespace godot_autopilot {

class ApiServer {
public:
  bool start(uint16_t port) const;
  void stop() const;
  void register_routes(HttpRouteTable &table) const;
  mcp::JsonValue handle(const std::string &method, const std::string &path,
                        const std::unordered_map<std::string, std::string> &headers,
                        const std::string &body) const;
};

} // namespace godot_autopilot

#endif
