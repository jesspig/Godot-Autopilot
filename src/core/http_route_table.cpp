#include "core/http_transport.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "core/http_parse.hpp"

namespace godot_autopilot {

void HttpRouteTable::add(const std::string &method, const std::string &prefix,
                         HttpRouteHandler handler) {
  const std::string key =
      HttpRouteTable::route_key(http_parse::to_lower(method), prefix);
  for (std::pair<std::string, HttpRouteHandler> &entry : routes_) {
    if (entry.first == key) {
      entry.second = std::move(handler);
      return;
    }
  }
  routes_.emplace_back(key, std::move(handler));
}

const HttpRouteHandler *HttpRouteTable::find(const std::string &method,
                                             const std::string &path) const {
  const std::string normalized = http_parse::to_lower(method);
  const HttpRouteHandler *best = nullptr;
  size_t best_prefix_length = 0;
  for (const std::pair<std::string, HttpRouteHandler> &entry : routes_) {
    const size_t separator = entry.first.find(' ');
    if (separator == std::string::npos) {
      continue;
    }
    if (entry.first.compare(0, separator, normalized) != 0) {
      continue;
    }
    const std::string prefix = entry.first.substr(separator + 1);
    if (prefix.empty() || path.size() < prefix.size()) {
      continue;
    }
    if (path.compare(0, prefix.size(), prefix) != 0) {
      continue;
    }
    if (best == nullptr || prefix.size() > best_prefix_length) {
      best = &entry.second;
      best_prefix_length = prefix.size();
    }
  }
  return best;
}

} // namespace godot_autopilot
