#include "core/http_transport.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "core/http_parse.hpp"
#include "core/http_transport_ext.hpp"
#include "core/log_system.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kTransportContentType = "application/json";

struct TransportConnection {
  godot::Ref<godot::StreamPeerTCP> peer;
  std::string inbox;
  std::string outbox;
  bool close_after_write{false};
  bool dead{false};
};

struct TransportState {
  godot::Ref<godot::TCPServer> server;
  uint16_t bound_port{0};
  std::vector<TransportConnection> connections;
  HttpRouteTable routes;
};

TransportState &transport_state() {
  static TransportState state;
  return state;
}

bool transport_host_is_loopback(const std::string &host) {
  return host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

std::string transport_json_escape(const std::string &text) {
  std::string result;
  result.reserve(text.size());
  for (char c : text) {
    if (c == '"' || c == '\\') {
      result.push_back('\\');
      result.push_back(c);
    } else if (c == '\n') {
      result += "\\n";
    } else if (c == '\r') {
      result += "\\r";
    } else if (c == '\t') {
      result += "\\t";
    } else if (static_cast<unsigned char>(c) < 0x20) {
      result += " ";
    } else {
      result.push_back(c);
    }
  }
  return result;
}

std::string transport_find_header(
    const std::unordered_map<std::string, std::string> &headers,
    const std::string &name) {
  const std::string lowered = http_parse::to_lower(name);
  for (const auto &entry : headers) {
    if (http_parse::to_lower(entry.first) == lowered) {
      return entry.second;
    }
  }
  return std::string();
}

std::string transport_build_response(const HttpResponse &response,
                                     bool keep_alive) {
  std::string head = "HTTP/1.1 " + std::to_string(response.status_code) + " " +
                     response.status_text + "\r\n";
  std::string content_type =
      transport_find_header(response.headers, "Content-Type");
  if (content_type.empty()) {
    content_type = kTransportContentType;
  }
  head += "Content-Type: " + content_type + "\r\n";
  head += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
  for (const auto &entry : response.headers) {
    const std::string lowered = http_parse::to_lower(entry.first);
    if (lowered == "content-type" || lowered == "content-length" ||
        lowered == "connection") {
      continue;
    }
    head += entry.first + ": " + entry.second + "\r\n";
  }
  head += std::string("Connection: ") + (keep_alive ? "keep-alive" : "close") +
          "\r\n\r\n";
  return head + response.body;
}

void transport_enqueue_error(TransportConnection &connection, int status_code,
                             const std::string &status_text,
                             const std::string &message) {
  HttpResponse response;
  response.status_code = status_code;
  response.status_text = status_text;
  response.headers["Content-Type"] = kTransportContentType;
  response.body = "{\"error\":\"" + transport_json_escape(message) + "\"}";
  connection.outbox += transport_build_response(response, false);
  connection.close_after_write = true;
}

void transport_read(TransportConnection &connection) {
  const HttpPumpLimits &limits = http_pump_limits();
  const int32_t available = connection.peer->get_available_bytes();
  if (available <= 0) {
    return;
  }
  size_t wanted = static_cast<size_t>(available);
  if (wanted > limits.max_read_bytes_per_connection) {
    wanted = limits.max_read_bytes_per_connection;
  }
  godot::Array result =
      connection.peer->get_partial_data(static_cast<int32_t>(wanted));
  if (result.size() < 2) {
    return;
  }
  const int64_t error = static_cast<int64_t>(result[0]);
  if (error != static_cast<int64_t>(godot::OK)) {
    connection.dead = true;
    return;
  }
  const godot::PackedByteArray chunk =
      static_cast<godot::PackedByteArray>(result[1]);
  const int64_t chunk_size = chunk.size();
  if (chunk_size <= 0) {
    return;
  }
  connection.inbox.append(reinterpret_cast<const char *>(chunk.ptr()),
                           static_cast<size_t>(chunk_size));
}

void transport_dispatch(TransportState &state, TransportConnection &connection) {
  while (true) {
    const size_t header_end = connection.inbox.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      if (connection.inbox.size() > http_parse::kMaxHeaderBlockBytes) {
        transport_enqueue_error(connection, 400, "Bad Request",
                                "header block exceeds the length limit");
        connection.inbox.clear();
      }
      return;
    }
    const std::string head = connection.inbox.substr(0, header_end);
    const size_t body_start = header_end + 4;
    const size_t line_end = head.find("\r\n");
    const std::string request_line_text =
        line_end == std::string::npos ? head : head.substr(0, line_end);
    const std::string header_text =
        line_end == std::string::npos ? std::string() : head.substr(line_end + 2);

    http_parse::RequestLine request_line;
    if (!http_parse::parse_request_line(request_line_text, request_line)) {
      transport_enqueue_error(connection, 400, "Bad Request", request_line.error);
      connection.inbox.clear();
      return;
    }
    http_parse::HeaderBlock headers;
    if (!http_parse::parse_headers(header_text, headers)) {
      transport_enqueue_error(connection, 400, "Bad Request", headers.error);
      connection.inbox.clear();
      return;
    }
    size_t content_length = 0;
    if (!http_parse::parse_content_length(headers, content_length)) {
      transport_enqueue_error(connection, 400, "Bad Request",
                              "content-length is not a valid length");
      connection.inbox.clear();
      return;
    }
    if (http_parse::decide_body(content_length) ==
        http_parse::BodyDecision::kTooLarge) {
      transport_enqueue_error(connection, 413, "Payload Too Large",
                              "request body exceeds the accepted limit");
      connection.inbox.clear();
      return;
    }
    if (connection.inbox.size() - body_start < content_length) {
      return;
    }

    HttpRequest request;
    request.method = request_line.method;
    request.path = request_line.path;
    request.headers = headers.headers;
    request.body = connection.inbox.substr(body_start, content_length);
    connection.inbox.erase(0, body_start + content_length);
    request.headers[http_parse::kQueryRawHeader] = request_line.query;
    for (const auto &entry : request_line.query_params) {
      request.headers[std::string(http_parse::kQueryHeaderPrefix) + entry.first] =
          entry.second;
    }

    const bool keep_alive =
        http_parse::request_keeps_alive(request_line.version, headers);
    HttpResponse response;
    const HttpRouteHandler *handler =
        state.routes.find(request.method, request.path);
    if (handler != nullptr) {
      (*handler)(request, response);
    } else {
      response.status_code = 404;
      response.status_text = "Not Found";
      response.headers["Content-Type"] = kTransportContentType;
      response.body = "{\"error\":\"no route for " +
                      transport_json_escape(request.path) + "\"}";
    }
    connection.outbox += transport_build_response(response, keep_alive);
    if (!keep_alive) {
      connection.close_after_write = true;
      return;
    }
  }
}

void transport_flush(TransportConnection &connection, size_t &write_budget) {
  while (!connection.outbox.empty() && write_budget > 0) {
    size_t chunk_size = connection.outbox.size();
    if (chunk_size > write_budget) {
      chunk_size = write_budget;
    }
    godot::PackedByteArray payload;
    payload.resize(static_cast<int64_t>(chunk_size));
    std::memcpy(payload.ptrw(), connection.outbox.data(), chunk_size);
    godot::Array result = connection.peer->put_partial_data(payload);
    if (result.size() < 2) {
      break;
    }
    const int64_t error = static_cast<int64_t>(result[0]);
    const int64_t written = static_cast<int64_t>(result[1]);
    if (error == static_cast<int64_t>(godot::ERR_BUSY)) {
      break;
    }
    if (error != static_cast<int64_t>(godot::OK)) {
      connection.dead = true;
      return;
    }
    if (written <= 0) {
      break;
    }
    size_t consumed = static_cast<size_t>(written);
    if (consumed > connection.outbox.size()) {
      consumed = connection.outbox.size();
    }
    connection.outbox.erase(0, consumed);
    write_budget -= consumed;
  }
}

void transport_process(TransportState &state, TransportConnection &connection,
                       size_t &write_budget) {
  if (connection.peer.is_null()) {
    connection.dead = true;
    return;
  }
  connection.peer->poll();
  if (connection.peer->get_status() !=
      godot::StreamPeerSocket::STATUS_CONNECTED) {
    connection.dead = true;
    return;
  }
  transport_read(connection);
  transport_dispatch(state, connection);
  transport_flush(connection, write_budget);
  if (connection.outbox.empty() && connection.close_after_write) {
    connection.dead = true;
  }
}

} // namespace

HttpTransport &http_transport_instance() {
  static HttpTransport transport;
  return transport;
}

bool http_transport_is_listening() {
  return transport_state().server.is_valid();
}

bool HttpTransport::listen(uint16_t port, const std::string &host) {
  if (!transport_host_is_loopback(host)) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::Transport,
        "HTTP transport listen rejected: '" + host +
            "' is not a loopback address");
    return false;
  }
  TransportState &state = transport_state();
  if (state.server.is_valid()) {
    state.server->stop();
    state.server.unref();
  }
  state.connections.clear();
  state.bound_port = 0;

  godot::Ref<godot::TCPServer> server;
  server.instantiate();
  const godot::Error error = server->listen(port, godot::String(host.c_str()));
  if (error != godot::OK) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::Transport,
        "HTTP transport listen failed on " + host + ":" + std::to_string(port) +
            " (error " + std::to_string(static_cast<int>(error)) + ")");
    return false;
  }
  state.server = server;
  state.bound_port = static_cast<uint16_t>(server->get_local_port());
  LogSystem::instance().log(LogLevel::Info, LogCategory::Transport,
                            "HTTP transport listening on " + host + ":" +
                                std::to_string(state.bound_port));
  return true;
}

void HttpTransport::stop() {
  TransportState &state = transport_state();
  for (TransportConnection &connection : state.connections) {
    if (connection.peer.is_valid()) {
      connection.peer->disconnect_from_host();
    }
  }
  state.connections.clear();
  if (state.server.is_valid()) {
    state.server->stop();
    state.server.unref();
  }
  state.bound_port = 0;
}

uint16_t HttpTransport::bound_port() const { return transport_state().bound_port; }

void HttpTransport::register_route(const std::string &method,
                                   const std::string &prefix,
                                   HttpRouteHandler handler) {
  transport_state().routes.add(method, prefix, std::move(handler));
}

void HttpTransport::pump() {
  TransportState &state = transport_state();
  if (state.server.is_null()) {
    return;
  }
  const HttpPumpLimits &limits = http_pump_limits();
  size_t accepted = 0;
  while (accepted < limits.max_accepts_per_frame &&
         state.server->is_connection_available()) {
    godot::Ref<godot::StreamPeerTCP> peer = state.server->take_connection();
    if (peer.is_null()) {
      break;
    }
    peer->set_no_delay(true);
    if (state.connections.size() >= limits.max_open_connections) {
      peer->disconnect_from_host();
      continue;
    }
    TransportConnection connection;
    connection.peer = peer;
    state.connections.push_back(std::move(connection));
    ++accepted;
  }
  size_t serviced = 0;
  size_t write_budget = limits.max_write_bytes_per_frame;
  for (size_t index = 0;
       index < state.connections.size() &&
       serviced < limits.max_connections_per_frame;
       ++index) {
    transport_process(state, state.connections[index], write_budget);
    ++serviced;
  }
  state.connections.erase(
      std::remove_if(state.connections.begin(), state.connections.end(),
                     [](const TransportConnection &connection) {
                       return connection.dead;
                     }),
      state.connections.end());
}

} // namespace godot_autopilot
