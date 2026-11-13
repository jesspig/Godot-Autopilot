#ifndef GODOT_AUTOPILOT_IPC_TRANSPORT_HPP
#define GODOT_AUTOPILOT_IPC_TRANSPORT_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace godot_autopilot {
namespace ipc_transport {

constexpr std::size_t kMaxIpcConnections = 8;
constexpr std::size_t kMaxIpcInFlight = 16;
constexpr std::size_t kMaxIpcAcceptsPerFrame = 8;
constexpr std::size_t kMaxIpcReadPerConnectionPerFrame = 32768;
constexpr std::size_t kMaxIpcConnectionBuffer = 4 * 1024 * 1024;
constexpr std::chrono::minutes kIpcIdleTimeout{10};

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
  bool listen_unix(const std::string &socket_path, const std::string &token,
                   std::string &error);
  bool listen_tcp(const std::string &host, int requested_port,
                  const std::string &token, std::string &error);
  void stop();
  bool is_listening() const;
  const std::string &bound_path() const;
  const std::string &kind() const;
  const std::string &host() const;
  int port() const;
  void pump();
  std::size_t connection_count() const;
  const IpcConnectionStats &stats() const;
  bool acquire_in_flight();
  void release_in_flight();

  using LineHandler = std::function<void(std::uint64_t connection_id,
                                         const std::string &line)>;
  void set_line_handler(LineHandler handler);
  void queue_line(std::uint64_t connection_id, std::string text);

private:
  struct Connection {
    socket_handle handle{kInvalidSocket};
    std::uint64_t id{0};
    std::string buffer;
    std::string write_buffer;
    bool authenticated{false};
    bool close_after_flush{false};
    std::chrono::steady_clock::time_point last_activity{};
  };

  void accept_pending();
  void service_connections();
  void drain_outbox();
  void flush_writes();
  void close_connection(Connection &connection);
  void close_all();
  bool authenticate(std::uint64_t connection_id, const std::string &frame);

  socket_handle listen_handle_{kInvalidSocket};
  std::string bound_path_;
  std::string kind_;
  std::string host_;
  int bound_port_{0};
  std::string token_;
  std::vector<Connection> connections_;
  std::uint64_t next_connection_id_{1};
  IpcConnectionStats stats_;
  LineHandler line_handler_;
  std::mutex outbox_mutex_;
  std::vector<std::pair<std::uint64_t, std::string>> outbox_;
  std::atomic<std::size_t> in_flight_{0};
};

IpcTransport &ipc_transport_instance();

} // namespace ipc_transport
} // namespace godot_autopilot

#endif