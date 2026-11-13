#include "core/transport/ipc_transport.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <system_error>

#include <mcp/JsonValue.hpp>

#include "core/transport/ipc_framing.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace godot_autopilot {
namespace ipc_transport {

namespace {

#ifdef _WIN32
using native_socket = SOCKET;
using native_socket_len = int;

bool ensure_sockets(std::string &error) {
  static bool started = false;
  if (started) {
    return true;
  }
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    error = "WSAStartup failed";
    return false;
  }
  started = true;
  return true;
}

int last_socket_error() { return WSAGetLastError(); }

bool error_is_would_block(int code) {
  return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS;
}

native_socket to_native(socket_handle handle) {
  return static_cast<native_socket>(handle);
}

void close_socket(socket_handle handle) {
  if (handle != kInvalidSocket) {
    closesocket(to_native(handle));
  }
}

bool set_nonblocking(socket_handle handle) {
  u_long mode = 1;
  return ioctlsocket(to_native(handle), FIONBIO, &mode) == 0;
}
#else
using native_socket = int;
using native_socket_len = socklen_t;

bool ensure_sockets(std::string &) { return true; }

int last_socket_error() { return errno; }

bool error_is_would_block(int code) {
  return code == EAGAIN || code == EWOULDBLOCK || code == EINPROGRESS;
}

native_socket to_native(socket_handle handle) { return handle; }

void close_socket(socket_handle handle) {
  if (handle != kInvalidSocket) {
    ::close(handle);
  }
}

bool set_nonblocking(socket_handle handle) {
  const int flags = fcntl(handle, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(handle, F_SETFL, flags | O_NONBLOCK) >= 0;
}
#endif

bool fill_address(const std::string &socket_path, sockaddr_un &address,
                  std::string &error) {
  std::memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  if (socket_path.empty()) {
    error = "socket path is empty";
    return false;
  }
  if (socket_path.size() >= sizeof(address.sun_path)) {
    error = "socket path too long (" + std::to_string(socket_path.size()) +
            " >= " + std::to_string(sizeof(address.sun_path)) + ")";
    return false;
  }
  std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size());
  address.sun_path[socket_path.size()] = '\0';
  return true;
}

bool fill_tcp_address(const std::string &host, int port, sockaddr_in &address,
                      std::string &error) {
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<std::uint16_t>(port));
  if (host.empty() || host == "127.0.0.1") {
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return true;
  }
  if (host == "::1" || host == "localhost") {
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return true;
  }
  error = "IPC TCP listen only accepts loopback hosts";
  return false;
}

std::string error_frame(const std::string &id_text, int code,
                        const std::string &message) {
  return "{\"jsonrpc\":\"2.0\",\"id\":" + id_text +
         ",\"error\":{\"code\":" + std::to_string(code) +
         ",\"message\":\"" + message + "\"}}";
}

std::string request_id_text(const mcp::JsonValue &doc) {
  const mcp::JsonValue *id = doc.Find("id");
  if (id != nullptr && id->IsInt()) {
    return std::to_string(id->GetInt());
  }
  return "null";
}

} // namespace

IpcTransport::~IpcTransport() { stop(); }

bool IpcTransport::is_listening() const {
  return listen_handle_ != kInvalidSocket;
}

const std::string &IpcTransport::bound_path() const { return bound_path_; }

const std::string &IpcTransport::kind() const { return kind_; }

const std::string &IpcTransport::host() const { return host_; }

int IpcTransport::port() const { return bound_port_; }

bool IpcTransport::acquire_in_flight() {
  std::size_t current = in_flight_.load(std::memory_order_relaxed);
  while (current < kMaxIpcInFlight) {
    if (in_flight_.compare_exchange_weak(current, current + 1,
                                         std::memory_order_acq_rel)) {
      return true;
    }
  }
  return false;
}

void IpcTransport::release_in_flight() {
  in_flight_.fetch_sub(1, std::memory_order_acq_rel);
}

std::size_t IpcTransport::connection_count() const {
  return connections_.size();
}

const IpcConnectionStats &IpcTransport::stats() const { return stats_; }

bool IpcTransport::listen(const std::string &socket_path, std::string &error) {
  return listen_unix(socket_path, std::string(), error);
}

bool IpcTransport::listen_unix(const std::string &socket_path,
                               const std::string &token, std::string &error) {
  if (is_listening()) {
    error = "ipc transport is already listening";
    return false;
  }
  if (!ensure_sockets(error)) {
    return false;
  }
  sockaddr_un address;
  if (!fill_address(socket_path, address, error)) {
    return false;
  }
  const socket_handle handle =
      static_cast<socket_handle>(::socket(AF_UNIX, SOCK_STREAM, 0));
  if (handle == kInvalidSocket) {
    error = "socket() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  if (!set_nonblocking(handle)) {
    close_socket(handle);
    error = "failed to switch the socket to non-blocking mode";
    return false;
  }
  if (::bind(to_native(handle), reinterpret_cast<sockaddr *>(&address),
             static_cast<native_socket_len>(sizeof(address))) != 0) {
    close_socket(handle);
    error = "bind() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  if (::listen(to_native(handle), SOMAXCONN) != 0) {
    close_socket(handle);
    error = "listen() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  listen_handle_ = handle;
  bound_path_ = socket_path;
  kind_ = "unix";
  host_.clear();
  bound_port_ = 0;
  token_ = token;
  return true;
}

bool IpcTransport::listen_tcp(const std::string &host, int requested_port,
                              const std::string &token, std::string &error) {
  if (is_listening()) {
    error = "ipc transport is already listening";
    return false;
  }
  if (!ensure_sockets(error)) {
    return false;
  }
  sockaddr_in address;
  if (!fill_tcp_address(host, requested_port, address, error)) {
    return false;
  }
  const socket_handle handle =
      static_cast<socket_handle>(::socket(AF_INET, SOCK_STREAM, 0));
  if (handle == kInvalidSocket) {
    error = "socket() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  int reuse = 1;
  ::setsockopt(to_native(handle), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char *>(&reuse), sizeof(reuse));
  if (!set_nonblocking(handle)) {
    close_socket(handle);
    error = "failed to switch the socket to non-blocking mode";
    return false;
  }
  if (::bind(to_native(handle), reinterpret_cast<sockaddr *>(&address),
             static_cast<native_socket_len>(sizeof(address))) != 0) {
    close_socket(handle);
    error = "bind() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  if (::listen(to_native(handle), SOMAXCONN) != 0) {
    close_socket(handle);
    error = "listen() failed (" + std::to_string(last_socket_error()) + ")";
    return false;
  }
  sockaddr_in bound{};
  native_socket_len bound_len = static_cast<native_socket_len>(sizeof(bound));
  if (getsockname(to_native(handle), reinterpret_cast<sockaddr *>(&bound),
                  &bound_len) != 0) {
    close_socket(handle);
    error = "getsockname() failed (" + std::to_string(last_socket_error()) +
            ")";
    return false;
  }
  listen_handle_ = handle;
  bound_path_.clear();
  kind_ = "tcp";
  host_ = "127.0.0.1";
  bound_port_ = ntohs(bound.sin_port);
  token_ = token;
  return true;
}

void IpcTransport::stop() {
  close_all();
  if (listen_handle_ != kInvalidSocket) {
    close_socket(listen_handle_);
    listen_handle_ = kInvalidSocket;
  }
  if (!bound_path_.empty()) {
    std::error_code ec;
    std::filesystem::remove(bound_path_, ec);
  }
  bound_path_.clear();
  kind_.clear();
  host_.clear();
  bound_port_ = 0;
  token_.clear();
}

void IpcTransport::pump() {
  if (!is_listening()) {
    return;
  }
  accept_pending();
  drain_outbox();
  service_connections();
  drain_outbox();
  flush_writes();
}

void IpcTransport::set_line_handler(LineHandler handler) {
  line_handler_ = std::move(handler);
}

void IpcTransport::queue_line(std::uint64_t connection_id, std::string text) {
  if (text.empty()) {
    return;
  }
  std::string frame = ipc_framing::encode_length_prefixed(text);
  std::lock_guard<std::mutex> lock(outbox_mutex_);
  outbox_.emplace_back(connection_id, std::move(frame));
}

bool IpcTransport::authenticate(std::uint64_t connection_id,
                                const std::string &frame) {
  if (token_.empty()) {
    return true;
  }
  mcp::JsonValue doc;
  try {
    doc = mcp::JsonValue::Parse(frame);
  } catch (...) {
    return false;
  }
  if (!doc.IsObject()) {
    return false;
  }
  const std::string id_text = request_id_text(doc);
  const mcp::JsonValue *method = doc.Find("method");
  if (method == nullptr || !method->IsString() ||
      method->GetString() != "initialize") {
    queue_line(connection_id,
               error_frame(id_text, -32002, "IPC initialize required"));
    return false;
  }
  const mcp::JsonValue *params = doc.Find("params");
  std::string presented;
  if (params != nullptr && params->IsObject()) {
    const mcp::JsonValue *meta = params->Find("_meta");
    if (meta != nullptr && meta->IsObject()) {
      const mcp::JsonValue *token = meta->Find("gda_token");
      if (token != nullptr && token->IsString()) {
        presented = token->GetString();
      }
    }
    if (presented.empty()) {
      const mcp::JsonValue *token = params->Find("gda_token");
      if (token != nullptr && token->IsString()) {
        presented = token->GetString();
      }
    }
  }
  if (presented != token_) {
    queue_line(connection_id,
               error_frame(id_text, -32001, "IPC token rejected"));
    return false;
  }
  return true;
}

void IpcTransport::drain_outbox() {
  std::vector<std::pair<std::uint64_t, std::string>> pending;
  {
    std::lock_guard<std::mutex> lock(outbox_mutex_);
    pending.swap(outbox_);
  }
  for (auto &entry : pending) {
    for (Connection &connection : connections_) {
      if (connection.id == entry.first) {
        connection.write_buffer += entry.second;
        break;
      }
    }
  }
}

void IpcTransport::accept_pending() {
  std::size_t accepted = 0;
  while (accepted < kMaxIpcAcceptsPerFrame &&
         connections_.size() < kMaxIpcConnections) {
    sockaddr_storage peer;
    native_socket_len peer_len = static_cast<native_socket_len>(sizeof(peer));
    const socket_handle handle = static_cast<socket_handle>(::accept(
        to_native(listen_handle_), reinterpret_cast<sockaddr *>(&peer),
        &peer_len));
    if (handle == kInvalidSocket) {
      return;
    }
    if (!set_nonblocking(handle)) {
      close_socket(handle);
      ++accepted;
      continue;
    }
    Connection connection;
    connection.handle = handle;
    connection.id = next_connection_id_++;
    connection.last_activity = std::chrono::steady_clock::now();
    connection.authenticated = token_.empty();
    connections_.push_back(std::move(connection));
    stats_.accepted += 1;
    ++accepted;
  }
}

void IpcTransport::service_connections() {
  const auto now = std::chrono::steady_clock::now();
  for (Connection &connection : connections_) {
    if (connection.handle == kInvalidSocket) {
      continue;
    }
    if (now - connection.last_activity > kIpcIdleTimeout) {
      close_connection(connection);
      continue;
    }
    char buffer[4096];
    std::size_t read_budget = kMaxIpcReadPerConnectionPerFrame;
    bool closed = false;
    while (read_budget > 0 && !closed) {
      const std::size_t want =
          std::min<std::size_t>(sizeof(buffer), read_budget);
      const int got = ::recv(to_native(connection.handle), buffer,
                             static_cast<int>(want), 0);
      if (got > 0) {
        const std::size_t room = kMaxIpcConnectionBuffer > connection.buffer.size()
                                     ? kMaxIpcConnectionBuffer - connection.buffer.size()
                                     : 0;
        connection.buffer.append(
            buffer, std::min<std::size_t>(static_cast<std::size_t>(got), room));
        connection.last_activity = now;
        stats_.bytes_read += static_cast<std::uint64_t>(got);
        read_budget -= static_cast<std::size_t>(got);
        continue;
      }
      if (got == 0) {
        closed = true;
        break;
      }
      const int code = last_socket_error();
      if (!error_is_would_block(code)) {
        closed = true;
      }
      break;
    }
    if (closed) {
      close_connection(connection);
      continue;
    }
    const ipc_framing::ExtractedFrames frames =
        ipc_framing::extract_length_prefixed(connection.buffer,
                                             kMaxIpcConnectionBuffer);
    connection.buffer = frames.remainder;
    if (frames.overflow) {
      close_connection(connection);
      continue;
    }
    for (const std::string &frame : frames.frames) {
      if (!connection.authenticated) {
        if (!authenticate(connection.id, frame)) {
          connection.close_after_flush = true;
          break;
        }
        connection.authenticated = true;
      }
      if (line_handler_) {
        line_handler_(connection.id, frame);
      }
    }
  }
  const std::size_t before = connections_.size();
  connections_.erase(
      std::remove_if(connections_.begin(), connections_.end(),
                     [](const Connection &connection) {
                       return connection.handle == kInvalidSocket;
                     }),
      connections_.end());
  if (before > connections_.size()) {
    stats_.closed += static_cast<std::uint64_t>(before - connections_.size());
  }
}

void IpcTransport::flush_writes() {
  for (Connection &connection : connections_) {
    if (connection.handle == kInvalidSocket) {
      continue;
    }
    std::size_t budget = 65536;
    while (!connection.write_buffer.empty() && budget > 0) {
      const std::size_t want =
          std::min<std::size_t>(connection.write_buffer.size(), budget);
      const int sent = ::send(to_native(connection.handle),
                              connection.write_buffer.data(),
                              static_cast<int>(want), 0);
      if (sent <= 0) {
        const int code = last_socket_error();
        if (!error_is_would_block(code)) {
          close_connection(connection);
        }
        break;
      }
      connection.write_buffer.erase(0, static_cast<std::size_t>(sent));
      budget -= static_cast<std::size_t>(sent);
    }
    if (connection.close_after_flush && connection.write_buffer.empty()) {
      close_connection(connection);
    }
  }
}

void IpcTransport::close_connection(Connection &connection) {
  close_socket(connection.handle);
  connection.handle = kInvalidSocket;
}

void IpcTransport::close_all() {
  for (Connection &connection : connections_) {
    close_connection(connection);
  }
  connections_.clear();
}

IpcTransport &ipc_transport_instance() {
  static IpcTransport transport;
  return transport;
}

} // namespace ipc_transport
} // namespace godot_autopilot