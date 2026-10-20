#ifndef GODOT_AUTOPILOT_HTTP_TRANSPORT_EXT_HPP
#define GODOT_AUTOPILOT_HTTP_TRANSPORT_EXT_HPP

#include <cstddef>
#include <string>
#include <unordered_map>

#include "core/http_parse.hpp"
#include "core/http_transport.hpp"

namespace godot_autopilot {

struct HttpPumpLimits {
  size_t max_accepts_per_frame{8};
  size_t max_connections_per_frame{8};
  size_t max_read_bytes_per_connection{32768};
  size_t max_write_bytes_per_frame{262144};
  size_t max_open_connections{64};
};

inline const HttpPumpLimits &http_pump_limits() {
  static const HttpPumpLimits limits;
  return limits;
}

HttpTransport &http_transport_instance();

bool http_transport_is_listening();

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
