#include "core/api_server.hpp"

#include <string>
#include <unordered_map>

#include "core/api_surface.hpp"
#include "core/config.hpp"

namespace godot_autopilot {

namespace {

void api_route_handler(const HttpRequest &request, HttpResponse &response) {
  const api_surface::ApiReply reply = api_surface::handle_request(
      request.method, request.path, request.headers, request.body);
  response.status_code = reply.status;
  response.status_text = reply.status_text;
  response.headers["Content-Type"] = "application/json";
  response.body = reply.body.Dump();
}

} // namespace

void ApiServer::register_routes(HttpRouteTable &table) const {
  const HttpRouteHandler handler = api_route_handler;
  table.add("GET", GDA_API_PATH, handler);
  table.add("POST", GDA_API_PATH, handler);
  table.add("PUT", GDA_API_PATH, handler);
  table.add("PATCH", GDA_API_PATH, handler);
  table.add("DELETE", GDA_API_PATH, handler);
  table.add("HEAD", GDA_API_PATH, handler);
  table.add("OPTIONS", GDA_API_PATH, handler);
}

mcp::JsonValue ApiServer::handle(
    const std::string &method, const std::string &path,
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &body) const {
  return api_surface::handle_request(method, path, headers, body).body;
}

} // namespace godot_autopilot
