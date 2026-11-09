#include "core/instance_guard.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

using godot_autopilot::InstanceCandidate;
using godot_autopilot::InstanceConflict;
using godot_autopilot::InstanceGuard;

namespace {

void set_guard_env(const char *name, const char *value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

void unset_guard_env(const char *name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

int current_process_id() {
#ifdef _WIN32
  return static_cast<int>(_getpid());
#else
  return static_cast<int>(getpid());
#endif
}

InstanceCandidate make_candidate(const std::string &project_path, int pid,
                                  int port, bool process_alive,
                                  bool port_in_use) {  InstanceCandidate candidate;
  candidate.instance_id = "gda-test-instance";
  candidate.project_path = project_path;
  candidate.pid = pid;
  candidate.port = port;
  candidate.host = "127.0.0.1";
  candidate.process_alive = process_alive;
  candidate.port_in_use = port_in_use;
  return candidate;
}

class PortHolder {
public:
  PortHolder() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
    handle_ = socket(AF_INET, SOCK_STREAM, 0);
#else
    handle_ = socket(AF_INET, SOCK_STREAM, 0);
#endif
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = 0;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (handle_ == kInvalid) {
      return;
    }
    if (bind(handle_, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) != 0) {
      return;
    }
    listen(handle_, 1);
    sockaddr_in bound;
    std::memset(&bound, 0, sizeof(bound));
    socklen_t length = sizeof(bound);
    if (getsockname(handle_, reinterpret_cast<sockaddr *>(&bound), &length) !=
        0) {
      return;
    }
    port_ = ntohs(bound.sin_port);
  }

  ~PortHolder() {
    if (handle_ != kInvalid) {
#ifdef _WIN32
      closesocket(handle_);
      WSACleanup();
#else
      ::close(handle_);
#endif
    }
  }

  int port() const { return port_; }

private:
#ifdef _WIN32
  static constexpr SOCKET kInvalid = INVALID_SOCKET;
  SOCKET handle_{INVALID_SOCKET};
#else
  static constexpr int kInvalid = -1;
  int handle_{-1};
#endif
  int port_{0};
};

} // namespace

TEST(InstanceGuardTest, SameProjectWithListeningPortIsAConflict) {
  const std::vector<InstanceCandidate> candidates = {
      make_candidate("C:/projects/Demo", 4242, 9527, true, true)};
  const InstanceConflict conflict =
      InstanceGuard::evaluate(candidates, "C:/projects/Demo");
  EXPECT_TRUE(conflict.conflict);
  EXPECT_EQ(conflict.candidate.pid, 4242);
  EXPECT_EQ(conflict.candidate.port, 9527);
}

TEST(InstanceGuardTest, SameProjectWithReleasedPortIsNotAConflict) {
  const std::vector<InstanceCandidate> candidates = {
      make_candidate("C:/projects/Demo", 4242, 9527, true, false)};
  const InstanceConflict conflict =
      InstanceGuard::evaluate(candidates, "C:/projects/Demo");
  EXPECT_FALSE(conflict.conflict);
}

TEST(InstanceGuardTest, OtherProjectIsNotAConflict) {
  const std::vector<InstanceCandidate> candidates = {
      make_candidate("C:/projects/Other", 4242, 9527, true, true)};
  const InstanceConflict conflict =
      InstanceGuard::evaluate(candidates, "C:/projects/Demo");
  EXPECT_FALSE(conflict.conflict);
}

TEST(InstanceGuardTest, DeadProcessIsNotAConflict) {
  const std::vector<InstanceCandidate> candidates = {
      make_candidate("C:/projects/Demo", 4242, 9527, false, true)};
  const InstanceConflict conflict =
      InstanceGuard::evaluate(candidates, "C:/projects/Demo");
  EXPECT_FALSE(conflict.conflict);
}

TEST(InstanceGuardTest, EmptyCandidateListIsNotAConflict) {
  const InstanceConflict conflict =
      InstanceGuard::evaluate({}, "C:/projects/Demo");
  EXPECT_FALSE(conflict.conflict);
}

TEST(InstanceGuardTest, ConflictIsReportedForTheFirstLiveCandidate) {
  const std::vector<InstanceCandidate> candidates = {
      make_candidate("C:/projects/Other", 1111, 9528, true, true),
      make_candidate("C:/projects/Demo", 2222, 9529, true, false),
      make_candidate("C:/projects/Demo", 3333, 9530, true, true),
      make_candidate("C:/projects/Demo", 4444, 9531, true, true)};
  const InstanceConflict conflict =
      InstanceGuard::evaluate(candidates, "C:/projects/Demo");
  EXPECT_TRUE(conflict.conflict);
  EXPECT_EQ(conflict.candidate.pid, 3333);
  EXPECT_EQ(conflict.candidate.port, 9530);
}

TEST(InstanceGuardTest, SameProjectIgnoresSeparatorAndTrailingSlash) {
  EXPECT_TRUE(
      InstanceGuard::same_project("C:/projects/Demo", "C:\\projects\\Demo\\"));
  EXPECT_TRUE(InstanceGuard::same_project("C:/projects/Demo", "C:/projects/Demo/"));
  EXPECT_FALSE(InstanceGuard::same_project("C:/projects/Demo", "C:/projects/Demo2"));
  EXPECT_FALSE(InstanceGuard::same_project("", "C:/projects/Demo"));
}

TEST(InstanceGuardTest, SameProjectIgnoresCaseOnWindowsOnly) {
#ifdef _WIN32
  EXPECT_TRUE(InstanceGuard::same_project("C:/projects/Demo", "c:/projects/demo"));
#else
  EXPECT_FALSE(
      InstanceGuard::same_project("C:/projects/Demo", "c:/projects/demo"));
#endif
}

TEST(InstanceGuardTest, MultiInstanceIsRefusedByDefault) {
  const std::string env_name = InstanceGuard::multi_instance_env_name();
  unset_guard_env(env_name.c_str());
  EXPECT_FALSE(InstanceGuard::multi_instance_allowed());
}

TEST(InstanceGuardTest, MultiInstanceExemptionIsOptIn) {
  const std::string env_name = InstanceGuard::multi_instance_env_name();
  set_guard_env(env_name.c_str(), "1");
  EXPECT_TRUE(InstanceGuard::multi_instance_allowed());
  set_guard_env(env_name.c_str(), "0");
  EXPECT_FALSE(InstanceGuard::multi_instance_allowed());
  set_guard_env(env_name.c_str(), "false");
  EXPECT_FALSE(InstanceGuard::multi_instance_allowed());
  set_guard_env(env_name.c_str(), "off");
  EXPECT_FALSE(InstanceGuard::multi_instance_allowed());
  unset_guard_env(env_name.c_str());
  EXPECT_FALSE(InstanceGuard::multi_instance_allowed());
}

TEST(InstanceGuardTest, ConflictMessageNamesThePortAndTheExemption) {
  const InstanceConflict conflict{true,
                                   make_candidate("C:/projects/Demo", 4242, 9527,
                                                  true, true)};
  const std::string message = InstanceGuard::conflict_message(conflict);
  EXPECT_NE(message.find("9527"), std::string::npos);
  EXPECT_NE(message.find("4242"), std::string::npos);
  EXPECT_NE(message.find(InstanceGuard::multi_instance_env_name()),
            std::string::npos);
  EXPECT_EQ(message.find("api_token"), std::string::npos);
}

TEST(InstanceGuardTest, ProcessLivenessFollowsTheCurrentProcess) {
  EXPECT_TRUE(godot_autopilot::process_is_alive(current_process_id()));
  EXPECT_FALSE(godot_autopilot::process_is_alive(0));
  EXPECT_FALSE(godot_autopilot::process_is_alive(-1));
}

TEST(InstanceGuardTest, PortProbeDetectsAnOccupiedPort) {
  PortHolder holder;
  ASSERT_GT(holder.port(), 0);
  EXPECT_TRUE(godot_autopilot::port_is_in_use(holder.port(), "127.0.0.1"));
}

TEST(InstanceGuardTest, PortProbeRejectsInvalidPorts) {
  EXPECT_FALSE(godot_autopilot::port_is_in_use(0, "127.0.0.1"));
  EXPECT_FALSE(godot_autopilot::port_is_in_use(-1, "127.0.0.1"));
  EXPECT_FALSE(godot_autopilot::port_is_in_use(70000, "127.0.0.1"));
}
