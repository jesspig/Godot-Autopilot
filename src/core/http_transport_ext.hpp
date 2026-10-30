#ifndef GODOT_AUTOPILOT_HTTP_TRANSPORT_EXT_HPP
#define GODOT_AUTOPILOT_HTTP_TRANSPORT_EXT_HPP

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/http_parse.hpp"
#include "core/http_transport.hpp"

namespace godot_autopilot {

constexpr const char *kStreamIdHeader = "x-gda-stream-id";
constexpr const char *kRequestIdHeader = "x-gda-request-id";

struct HttpPumpLimits {
  size_t max_accepts_per_frame{8};
  size_t max_connections_per_frame{8};
  size_t max_read_bytes_per_connection{32768};
  size_t max_write_bytes_per_frame{262144};
  size_t max_open_connections{64};
  size_t max_outbox_bytes_per_connection{1024 * 1024};
};

inline const HttpPumpLimits &http_pump_limits() {
  static const HttpPumpLimits limits;
  return limits;
}

inline size_t http_pump_service_index(size_t cursor, size_t offset,
                                      size_t count) {
  if (count == 0) {
    return 0;
  }
  return (cursor + offset) % count;
}

inline size_t http_pump_connection_write_budget(size_t total, size_t count,
                                                size_t max_per_frame) {
  if (count == 0) {
    return 0;
  }
  const size_t per_cap = std::max<size_t>(1, max_per_frame / count);
  return std::min(total, per_cap);
}

inline std::string transport_final_chunk() { return "0\r\n\r\n"; }

inline bool transport_idle_expired(
    std::chrono::steady_clock::time_point last_activity,
    std::chrono::steady_clock::time_point now,
    std::chrono::milliseconds budget) {
  return now - last_activity > budget;
}

HttpTransport &http_transport_instance();

HttpRouteTable &http_transport_route_table();

bool http_transport_is_listening();

size_t http_transport_reap_parked_workers();

bool http_transport_bind(uint16_t port, const std::string &host,
                         std::string &error);

inline std::unordered_map<std::string, std::string>
http_request_query_params(const HttpRequest &request) {
  std::unordered_map<std::string, std::string> params;
  const std::string prefix = http_parse::kQueryHeaderPrefix;
  for (const auto &entry : request.headers) {
    if (entry.first.size() <= prefix.size() ||
        entry.first.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    params[entry.first.substr(prefix.size())] = entry.second;
  }
  return params;
}

inline std::string http_request_query_string(const HttpRequest &request) {
  const auto found = request.headers.find(http_parse::kQueryRawHeader);
  if (found == request.headers.end()) {
    return std::string();
  }
  return found->second;
}

} // namespace godot_autopilot

#endif
