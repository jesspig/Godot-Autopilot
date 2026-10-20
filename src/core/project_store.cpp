#include "core/project_store.hpp"

#include <cstddef>
#include <string>

#include "core/config.hpp"
#include "core/gda_home.hpp"
#include "core/path_util.hpp"
#include "core/trace_recorder.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kStoreProjectsDirName = "projects";
constexpr const char *kStoreInstancesDirName = "instances";
constexpr const char *kStoreGlobalConfigFileName = "config.json";
constexpr const char *kStoreInstancesIndexFileName = "instances.json";
constexpr std::size_t kStoreProjectNameMaxLength = 40;

} // namespace

std::string ProjectStore::sanitize_project_name(const std::string &project_name) {
  std::string sanitized = sanitize_path_component(project_name);
  if (sanitized.size() > kStoreProjectNameMaxLength) {
    sanitized.resize(kStoreProjectNameMaxLength);
  }
  return sanitized;
}

std::string ProjectStore::project_path_hash(const std::string &project_path) {
  const std::string normalized = normalize_path_separators(project_path);
  const std::string digest = TraceRecorder::fnv1a_hex(normalized);
  if (digest.size() <= static_cast<std::size_t>(GDA_PROJECT_HASH_LENGTH)) {
    return digest;
  }
  return digest.substr(0, static_cast<std::size_t>(GDA_PROJECT_HASH_LENGTH));
}

std::string ProjectStore::project_dir_name(const std::string &project_name,
                                           const std::string &project_path) {
  std::string prefix_source = path_basename(project_path);
  if (prefix_source.empty()) {
    prefix_source = project_name;
  }
  return sanitize_project_name(prefix_source) + "-" +
         project_path_hash(project_path);
}

std::string ProjectStore::root_dir() {
  const std::string home = HomeResolver::gda_home();
  if (home.empty()) {
    return std::string();
  }
  return join_path(home, GDA_HOME_DIR_NAME);
}

std::string ProjectStore::project_dir_for_path(const std::string &project_path) {
  const std::string root = root_dir();
  if (root.empty()) {
    return std::string();
  }
  return join_path(join_path(root, kStoreProjectsDirName),
                   project_dir_name(std::string(), project_path));
}

std::string ProjectStore::project_dir(const std::string &project_name,
                                      const std::string &project_path) {
  const std::string root = root_dir();
  if (root.empty()) {
    return std::string();
  }
  return join_path(join_path(root, kStoreProjectsDirName),
                   project_dir_name(project_name, project_path));
}

std::string ProjectStore::global_config_path() {
  const std::string root = root_dir();
  if (root.empty()) {
    return std::string();
  }
  return join_path(root, kStoreGlobalConfigFileName);
}

std::string ProjectStore::project_config_path(const std::string &project_path) {
  const std::string project = project_dir_for_path(project_path);
  if (project.empty()) {
    return std::string();
  }
  return join_path(project, kStoreGlobalConfigFileName);
}

std::string ProjectStore::instances_index_path() {
  const std::string root = root_dir();
  if (root.empty()) {
    return std::string();
  }
  return join_path(root, kStoreInstancesIndexFileName);
}

std::string ProjectStore::instance_dir(const std::string &project_path) {
  const std::string project = project_dir_for_path(project_path);
  if (project.empty()) {
    return std::string();
  }
  return join_path(project, kStoreInstancesDirName);
}

} // namespace godot_autopilot
