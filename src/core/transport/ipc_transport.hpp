#ifndef GODOT_AUTOPILOT_IPC_TRANSPORT_HPP
#define GODOT_AUTOPILOT_IPC_TRANSPORT_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace godot_autopilot {
namespace ipc_transport {

constexpr std::size_t kMaxIpcConnections = 32;
constexpr std::size_t kMaxIpcAcceptsPerFrame = 8;
constexpr std::size_t kMaxIpcReadPerConnectionPerFrame = 32768;
constexpr std::size_t kMaxIpcConnectionBuffer = 4 * 1024 * 1024;

#ifdef _WIN32
using socket_handle = std::uintptr_t;
constexpr socket_handle kInvalidSocket =
    static_cast<socket_handle>(~static_cast<std::uintptr_t>(0));
#else
using socket_handle = int;
constexpr socket_handle kInvalidSocket = -1;
#endif

struct IpcConnectionStats {
  std::uint64_t accepted{0};
  std::uint64_t closed{0};
  std::uint64_t bytes_read{0};
};

class IpcTransport {
public:
  IpcTransport() = default;
  ~IpcTransport();

  IpcTransport(const IpcTransport &) = delete;
  IpcTransport &operator=(const IpcTransport &) = delete;

  bool listen(const std::string &socket_path, std::string &error);
  void stop();
  bool is_listening() const;
  const std::string &bound_path() const;
  void pump();
  std::size_t connection_count() const;
  const IpcConnectionStats &stats() const;

private:
  struct Connection {
    socket_handle handle{kInvalidSocket};
    std::uint64_t id{0};
    std::string buffer;
  };

  void accept_pending();
  void service_connections();
  void close_connection(Connection &connection);
  void close_all();

  socket_handle listen_handle_{kInvalidSocket};
  std::string bound_path_;
  std::vector<Connection> connections_;
  std::uint64_t next_connection_id_{1};
  IpcConnectionStats stats_;
};

IpcTransport &ipc_transport_instance();

} // namespace ipc_transport
} // namespace godot_autopilot

#endif