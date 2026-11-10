#include "core/transport/ipc_endpoint.hpp"

#include "core/storage/project_store.hpp"

namespace godot_autopilot {
namespace ipc_endpoint {

namespace {

std::string join_path(const std::string &dir, const std::string &name) {
  if (dir.empty()) {
    return name;
  }
  const char last = dir.back();
  if (last == '/' || last == '\\') {
    return dir + name;
  }
#ifdef _WIN32
  return dir + "\\" + name;
#else
  return dir + "/" + name;
#endif
}

std::string hash_key(const std::string &project_path) {
  return ProjectStore::project_path_hash(project_path);
}

std::string socket_file_name(const std::string &project_name,
                             const std::string &project_path) {
  const std::string sanitized = ProjectStore::sanitize_project_name(project_name);
  const std::string base = sanitized.empty() ? std::string("project") : sanitized;
  return base + "-" + hash_key(project_path) + ".sock";
}

} // namespace

std::string derive_socket_path(const std::string &project_name,
                               const std::string &project_path,
                               const std::string &run_dir,
                               const std::string &fallback_dir) {
  const std::string candidate =
      join_path(run_dir, socket_file_name(project_name, project_path));
  if (candidate.size() <= kMaxSocketPathChars) {
    return candidate;
  }
  return join_path(fallback_dir, hash_key(project_path) + ".sock");
}

bool should_recreate_leftover(bool path_exists, bool connectable) {
  return path_exists && !connectable;
}

} // namespace ipc_endpoint
} // namespace godot_autopilot