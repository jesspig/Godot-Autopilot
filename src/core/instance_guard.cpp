#include "core/instance_guard.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/instance_registry.hpp"
#include "core/path_util.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace godot_autopilot {

namespace {

constexpr const char *kMultiInstanceEnvName = "GODOT_AUTOPILOT_ALLOW_MULTI_INSTANCE";

enum class PortProbe { kFree, kOccupied, kUnknown };

#ifdef _WIN32
using SocketHandle = SOCKET;

constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
constexpr int kAddressInUseError = WSAEADDRINUSE;

void close_socket(SocketHandle handle) { closesocket(handle); }

int last_socket_error() { return WSAGetLastError(); }

bool start_socket_library() {
  WSADATA data;
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
}

void stop_socket_library() { WSACleanup(); }
#else
using SocketHandle = int;

constexpr SocketHandle kInvalidSocket = -1;
constexpr int kAddressInUseError = EADDRINUSE;

void close_socket(SocketHandle handle) { ::close(handle); }

int last_socket_error() { return errno; }

bool start_socket_library() { return true; }

void stop_socket_library() {}
#endif

bool parse_env_flag(const char *raw) {
  if (raw == nullptr) {
    return false;
  }
  const std::string value(raw);
  if (value.empty()) {
    return false;
  }
  if (value == "0" || value == "false" || value == "off" || value == "no") {
    return false;
  }
  return true;
}

std::string normalize_project_path(const std::string &path) {
  return trim_path_trailing_separators(path);
}

#ifdef _WIN32
bool equals_ignore_case(const std::string &left, const std::string &right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t index = 0; index < left.size(); ++index) {
    if (std::tolower(static_cast<unsigned char>(left[index])) !=
        std::tolower(static_cast<unsigned char>(right[index]))) {
      return false;
    }
  }
  return true;
}
#endif

bool host_is_ipv6_loopback(const std::string &host) {
  return host == "::1" || host == "[::1]";
}

PortProbe probe_bind_v4(int port) {
  const SocketHandle handle = socket(AF_INET, SOCK_STREAM, 0);
  if (handle == kInvalidSocket) {
    return PortProbe::kUnknown;
  }
  sockaddr_in address;
  std::memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<uint16_t>(port));
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const int result =
      bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address));
  const int error = result == 0 ? 0 : last_socket_error();
  close_socket(handle);
  if (result == 0) {
    return PortProbe::kFree;
  }
  if (error == kAddressInUseError) {
    return PortProbe::kOccupied;
  }
  return PortProbe::kUnknown;
}

PortProbe probe_bind_v6(int port) {
  const SocketHandle handle = socket(AF_INET6, SOCK_STREAM, 0);
  if (handle == kInvalidSocket) {
    return PortProbe::kUnknown;
  }
  sockaddr_in6 address;
  std::memset(&address, 0, sizeof(address));
  address.sin6_family = AF_INET6;
  address.sin6_port = htons(static_cast<uint16_t>(port));
  address.sin6_addr = in6addr_loopback;
  const int result =
      bind(handle, reinterpret_cast<sockaddr *>(&address), sizeof(address));
  const int error = result == 0 ? 0 : last_socket_error();
  close_socket(handle);
  if (result == 0) {
    return PortProbe::kFree;
  }
  if (error == kAddressInUseError) {
    return PortProbe::kOccupied;
  }
  return PortProbe::kUnknown;
}

} // namespace

bool process_is_alive(int pid) {
  if (pid <= 0) {
    return false;
  }
#ifdef _WIN32
  HANDLE handle =
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                  static_cast<DWORD>(pid));
  if (handle == nullptr) {
    return GetLastError() == ERROR_ACCESS_DENIED;
  }
  DWORD exit_code = 0;
  const bool queried = GetExitCodeProcess(handle, &exit_code) != FALSE;
  CloseHandle(handle);
  if (!queried) {
    return false;
  }
  return exit_code == STILL_ACTIVE;
#else
  if (kill(static_cast<pid_t>(pid), 0) == 0) {
    return true;
  }
  return errno == EPERM;
#endif
}

bool port_is_in_use(int port, const std::string &host) {
  if (port <= 0 || port > 65535) {
    return false;
  }
  if (!start_socket_library()) {
    return false;
  }
  const PortProbe probe = host_is_ipv6_loopback(host)
                              ? probe_bind_v6(port)
                              : probe_bind_v4(port);
  stop_socket_library();
  return probe == PortProbe::kOccupied;
}

bool InstanceGuard::multi_instance_allowed() {
  return parse_env_flag(std::getenv(kMultiInstanceEnvName));
}

std::string InstanceGuard::multi_instance_env_name() {
  return std::string(kMultiInstanceEnvName);
}

bool InstanceGuard::same_project(const std::string &left,
                                 const std::string &right) {
  if (left.empty() || right.empty()) {
    return false;
  }
  const std::string normalized_left = normalize_project_path(left);
  const std::string normalized_right = normalize_project_path(right);
  if (normalized_left.empty() || normalized_right.empty()) {
    return false;
  }
#ifdef _WIN32
  return equals_ignore_case(normalized_left, normalized_right);
#else
  return normalized_left == normalized_right;
#endif
}

InstanceConflict
InstanceGuard::evaluate(const std::vector<InstanceCandidate> &candidates,
                         const std::string &current_project_path) {
  InstanceConflict result;
  for (const InstanceCandidate &candidate : candidates) {
    if (!same_project(candidate.project_path, current_project_path)) {
      continue;
    }
    if (!candidate.process_alive) {
      continue;
    }
    if (!candidate.port_in_use) {
      continue;
    }
    result.conflict = true;
    result.candidate = candidate;
    return result;
  }
  return result;
}

InstanceConflict InstanceGuard::detect(const std::string &current_project_path,
                                       int current_pid) {
  std::vector<InstanceCandidate> candidates;
  for (const InstanceRecord &record : InstanceRegistry::list_all()) {
    if (current_pid > 0 && record.pid == current_pid) {
      continue;
    }
    if (!same_project(record.project_path, current_project_path)) {
      continue;
    }
    InstanceCandidate candidate;
    candidate.instance_id = record.instance_id;
    candidate.project_path = record.project_path;
    candidate.pid = record.pid;
    candidate.port = record.port;
    candidate.host = record.endpoints.mcp.host;
    candidate.process_alive = process_is_alive(record.pid);
    candidate.port_in_use =
        candidate.process_alive &&
        port_is_in_use(record.port, candidate.host);
    candidates.push_back(candidate);
  }
  return evaluate(candidates, current_project_path);
}

std::string InstanceGuard::conflict_message(const InstanceConflict &conflict) {
  return "another Godot Autopilot instance is already running for this project on port " +
         std::to_string(conflict.candidate.port) + " (pid " +
         std::to_string(conflict.candidate.pid) +
         "); a second instance on the same project is refused because Godot "
         "loads the plugin through a shared temporary copy and the editor can "
         "die without any message; close the other editor for this project, or "
         "set " +
         multi_instance_env_name() +
         "=1 to start it anyway on another port "
         "(GODOT_AUTOPILOT_PORT=<port>); the editor may still crash while the "
         "other instance is running";
}

} // namespace godot_autopilot
