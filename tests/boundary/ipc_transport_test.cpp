#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "core/transport/ipc_transport.hpp"

namespace fs = std::filesystem;

namespace {

fs::path short_socket(const char *name) {
  const fs::path path = fs::current_path() / name;
  std::error_code ec;
  fs::remove(path, ec);
  return path;
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