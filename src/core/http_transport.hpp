#ifndef GODOT_AUTOPILOT_HTTP_TRANSPORT_HPP
#define GODOT_AUTOPILOT_HTTP_TRANSPORT_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace godot_autopilot {

struct HttpRequest {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse {
  int status_code{200};
  std::string status_text{"OK"};
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

using HttpRouteHandler = std::function<void(const HttpRequest &, HttpResponse &)>;

class HttpRouteTable {
public:
  void add(const std::string &method, const std::string &prefix,
           HttpRouteHandler handler);
  const HttpRouteHandler *find(const std::string &method,
                               const std::string &path) const;

  static std::string route_key(const std::string &method,
                                const std::string &prefix) {
    return method + " " + prefix;
  }

private:
  std::vector<std::pair<std::string, HttpRouteHandler>> routes_;
};

class HttpTransport {
public:
  bool listen(uint16_t port, const std::string &host);
  void stop();
  uint16_t bound_port() const;
  void register_route(const std::string &method, const std::string &prefix,
                      HttpRouteHandler handler);
  void pump();
};

} // namespace godot_autopilot

#endif
