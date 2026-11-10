#ifndef GODOT_AUTOPILOT_IPC_ENDPOINT_HPP
#define GODOT_AUTOPILOT_IPC_ENDPOINT_HPP

#include <cstddef>
#include <string>

namespace godot_autopilot {
namespace ipc_endpoint {

constexpr std::size_t kMaxSocketPathChars = 99;

std::string derive_socket_path(const std::string &project_name,
                               const std::string &project_path,
                               const std::string &run_dir,
                               const std::string &fallback_dir);

bool should_recreate_leftover(bool path_exists, bool connectable);

} // namespace ipc_endpoint
} // namespace godot_autopilot

#endif