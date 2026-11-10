#include "core/transport/ipc_transport.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <afunix.h>
#else
#include <fcntl.h>
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

} // namespace

IpcTransport::~IpcTransport() { stop(); }

bool IpcTransport::is_listening() const {
  return listen_handle_ != kInvalidSocket;
}

const std::string &IpcTransport::bound_path() const { return bound_path_; }

std::size_t IpcTransport::connection_count() const {
  return connections_.size();
}

const IpcConnectionStats &IpcTransport::stats() const { return stats_; }

bool IpcTransport::listen(const std::string &socket_path, std::string &error) {
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
  return true;
}

void IpcTransport::stop() {
  close_all();
  if (listen_handle_ != kInvalidSocket) {
    close_socket(listen_handle_);
    listen_handle_ = kInvalidSocket;
  }
  bound_path_.clear();
}

void IpcTransport::pump() {
  if (!is_listening()) {
    return;
  }
  accept_pending();
  service_connections();
}

void IpcTransport::accept_pending() {
  std::size_t accepted = 0;
  while (accepted < kMaxIpcAcceptsPerFrame &&
         connections_.size() < kMaxIpcConnections) {
    sockaddr_un peer;
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
    connections_.push_back(std::move(connection));
    stats_.accepted += 1;
    ++accepted;
  }
}

void IpcTransport::service_connections() {
  for (Connection &connection : connections_) {
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