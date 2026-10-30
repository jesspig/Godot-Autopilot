#include "core/storage_paths.hpp"

#include <string>

#include "core/path_util.hpp"
#include "core/project_store.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kLogsDirName = "logs";
constexpr const char *kTracesDirName = "traces";
constexpr const char *kCapturesDirName = "captures";

std::string normalized_project_path(const std::string &project_path) {
  return trim_path_trailing_separators(project_path);
}

std::string project_sub_dir(const std::string &project_path,
                             const char *name) {
  const std::string dir = StoragePaths::project_dir(project_path);
  if (dir.empty()) {
    return std::string();
  }
  return join_path(dir, name);
}

} // namespace

std::string StoragePaths::global_config_path() {
  return ProjectStore::global_config_path();
}

std::string StoragePaths::project_dir(const std::string &project_path) {
  const std::string normalized = normalized_project_path(project_path);
  if (normalized.empty()) {
    return std::string();
  }
  return ProjectStore::project_dir(std::string(), normalized);
}

std::string StoragePaths::project_config_path(const std::string &project_path) {
  const std::string normalized = normalized_project_path(project_path);
  if (normalized.empty()) {
    return std::string();
  }
  return ProjectStore::project_config_path(normalized);
}

std::string StoragePaths::log_dir(const std::string &project_path) {
  return project_sub_dir(project_path, kLogsDirName);
}

std::string StoragePaths::trace_dir(const std::string &project_path) {
  return project_sub_dir(project_path, kTracesDirName);
}

std::string StoragePaths::captures_dir(const std::string &project_path) {
  return project_sub_dir(project_path, kCapturesDirName);
}

} // namespace godot_autopilot
