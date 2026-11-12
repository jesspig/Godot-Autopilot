#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "core/transport/ipc_framing.hpp"
#include "core/transport/ipc_transport.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
using test_socket = SOCKET;
constexpr test_socket kInvalidTestSocket = INVALID_SOCKET;
void close_test_socket(test_socket socket) { closesocket(socket); }
#else
using test_socket = int;
constexpr test_socket kInvalidTestSocket = -1;
void close_test_socket(test_socket socket) { ::close(socket); }
#endif

fs::path short_socket(const char *name) {
#ifdef _WIN32
  fs::path base = fs::current_path();
  if (base.filename() == "tests") {
    base = base.parent_path().parent_path().parent_path();
  }
#else
  const fs::path base = fs::temp_directory_path();
#endif
  const fs::path path = base / name;
  std::error_code ec;
  fs::remove(path, ec);
  return path;
}

test_socket connect_tcp(int port) {
#ifdef _WIN32
  WSADATA data;
  WSAStartup(MAKEWORD(2, 2), &data);
#endif
  const test_socket socket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (socket == kInvalidTestSocket) {
    return kInvalidTestSocket;
  }
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<unsigned short>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(socket, reinterpret_cast<sockaddr *>(&address),
                sizeof(address)) != 0) {
    close_test_socket(socket);
    return kInvalidTestSocket;
  }
#ifdef _WIN32
  DWORD timeout = 3000;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,
             reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
  timeval timeout{};
  timeout.tv_sec = 3;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
  return socket;
}

bool send_all(test_socket socket, const std::string &data) {
  std::size_t offset = 0;
  while (offset < data.size()) {
    const int sent = ::send(socket, data.data() + offset,
                            static_cast<int>(data.size() - offset), 0);
    if (sent <= 0) {
      return false;
    }
    offset += static_cast<std::size_t>(sent);
  }
  return true;
}

std::string recv_frame(test_socket socket) {
  unsigned char header[4];
  std::size_t offset = 0;
  while (offset < 4) {
    const int got = ::recv(socket, reinterpret_cast<char *>(header) + offset,
                           static_cast<int>(4 - offset), 0);
    if (got <= 0) {
      return std::string();
    }
    offset += static_cast<std::size_t>(got);
  }
  const std::uint32_t size = (static_cast<std::uint32_t>(header[0]) << 24) |
                             (static_cast<std::uint32_t>(header[1]) << 16) |
                             (static_cast<std::uint32_t>(header[2]) << 8) |
                             static_cast<std::uint32_t>(header[3]);
  std::string payload(size, '\0');
  offset = 0;
  while (offset < size) {
    const int got = ::recv(socket, payload.data() + offset,
                           static_cast<int>(size - offset), 0);
    if (got <= 0) {
      return std::string();
    }
    offset += static_cast<std::size_t>(got);
  }
  return payload;
}

} // namespace

TEST(IpcTransportTest, RejectsEmptyPath) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  EXPECT_FALSE(transport.listen("", error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(transport.is_listening());
}

TEST(IpcTransportTest, RejectsOverlongPath) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  const std::string overlong = std::string(200, 'x');
  EXPECT_FALSE(transport.listen(overlong, error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(transport.is_listening());
}

TEST(IpcTransportTest, ListenPumpStopLifecycle) {
  const fs::path path = short_socket("i1.sock");
  std::error_code ec;
  fs::remove(path, ec);
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  ASSERT_TRUE(transport.listen(path.string(), error)) << error;
  EXPECT_TRUE(transport.is_listening());
  EXPECT_EQ(transport.bound_path(), path.string());
  transport.pump();
  EXPECT_EQ(transport.connection_count(), 0u);
  transport.stop();
  EXPECT_FALSE(transport.is_listening());
  EXPECT_TRUE(transport.bound_path().empty());
  fs::remove(path, ec);
}

TEST(IpcTransportTest, SecondListenIsRejected) {
  const fs::path path = short_socket("i2.sock");
  std::error_code ec;
  fs::remove(path, ec);
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  ASSERT_TRUE(transport.listen(path.string(), error)) << error;
  std::string second_error;
  EXPECT_FALSE(transport.listen(path.string(), second_error));
  EXPECT_NE(second_error.find("already listening"), std::string::npos);
  transport.stop();
  fs::remove(path, ec);
}

TEST(IpcTransportTest, StopWithoutListenIsSafe) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  transport.stop();
  transport.pump();
  EXPECT_FALSE(transport.is_listening());
  EXPECT_EQ(transport.connection_count(), 0u);
}

TEST(IpcTransportTest, TcpListenAssignsRandomPort) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  ASSERT_TRUE(transport.listen_tcp("127.0.0.1", 0, "test-token", error))
      << error;
  EXPECT_TRUE(transport.is_listening());
  EXPECT_EQ(transport.kind(), "tcp");
  EXPECT_EQ(transport.host(), "127.0.0.1");
  EXPECT_GT(transport.port(), 0);
  EXPECT_TRUE(transport.bound_path().empty());
  transport.pump();
  transport.stop();
  EXPECT_FALSE(transport.is_listening());
  EXPECT_EQ(transport.port(), 0);
}

TEST(IpcTransportTest, TcpListenRejectsNonLoopbackHost) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  EXPECT_FALSE(transport.listen_tcp("0.0.0.0", 0, "test-token", error));
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(transport.is_listening());
}

TEST(IpcTransportTest, RejectsWrongTokenAndAcceptsCorrectToken) {
  godot_autopilot::ipc_transport::IpcTransport transport;
  std::string error;
  ASSERT_TRUE(transport.listen_tcp("127.0.0.1", 0, "secret-token", error))
      << error;
  std::atomic<bool> running{true};
  std::atomic<int> handled{0};
  transport.set_line_handler([&](std::uint64_t connection_id,
                                 const std::string &) {
    handled.fetch_add(1);
    transport.queue_line(connection_id,
                         "{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{}}");
  });
  std::thread pump([&]() {
    while (running.load()) {
      transport.pump();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  const test_socket rejected = connect_tcp(transport.port());
  ASSERT_NE(rejected, kInvalidTestSocket);
  const std::string bad_request =
      godot_autopilot::ipc_framing::encode_length_prefixed(
          "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
          "\"params\":{\"_meta\":{\"gda_token\":\"wrong\"}}}");
  ASSERT_TRUE(send_all(rejected, bad_request));
  const std::string bad_response = recv_frame(rejected);
  close_test_socket(rejected);
  EXPECT_NE(bad_response.find("-32001"), std::string::npos) << bad_response;

  const test_socket accepted = connect_tcp(transport.port());
  ASSERT_NE(accepted, kInvalidTestSocket);
  const std::string good_request =
      godot_autopilot::ipc_framing::encode_length_prefixed(
          "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"initialize\","
          "\"params\":{\"_meta\":{\"gda_token\":\"secret-token\"}}}");
  ASSERT_TRUE(send_all(accepted, good_request));
  const std::string good_response = recv_frame(accepted);
  close_test_socket(accepted);
  EXPECT_NE(good_response.find("\"id\":7"), std::string::npos)
      << good_response;

  running.store(false);
  pump.join();
  transport.stop();
  EXPECT_EQ(handled.load(), 1);
}