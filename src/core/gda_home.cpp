#include "core/gda_home.hpp"

#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <utility>
#include <vector>

#include "core/log_system.hpp"
#include "core/path_util.hpp"

namespace godot_autopilot {

namespace {

constexpr const char *kHomeOverrideEnv = "GODOT_AUTOPILOT_HOME";
constexpr const char *kHomeEnv = "HOME";
constexpr const char *kUserProfileEnv = "USERPROFILE";
constexpr const char *kAppDataEnv = "APPDATA";
constexpr const char *kXdgDataHomeEnv = "XDG_DATA_HOME";

std::string read_home_env(const char *name) {
  const char *value = std::getenv(name);
  if (value == nullptr) {
    return std::string();
  }
  return std::string(value);
}

std::string home_default_data_dir(const std::string &env_home,
                                   const std::string &env_appdata) {
#ifdef _WIN32
  return env_appdata;
#else
  const std::string xdg = read_home_env(kXdgDataHomeEnv);
  if (!xdg.empty()) {
    return xdg;
  }
  if (env_home.empty()) {
    return std::string();
  }
  return join_path(env_home, ".local/share");
#endif
}

} // namespace

HomeResolver::DataDirProvider &HomeResolver::data_dir_provider() {
  static DataDirProvider provider;
  return provider;
}

void HomeResolver::set_data_dir_provider(DataDirProvider provider) {
  data_dir_provider() = std::move(provider);
}

HomeResolver::HostPlatform HomeResolver::current_platform() {
#ifdef _WIN32
  return HostPlatform::kWindows;
#else
  return HostPlatform::kPosix;
#endif
}

bool HomeResolver::validate_shape(const std::string &path) {
  if (path.empty()) {
    return false;
  }
  if (is_filesystem_root(path)) {
    return false;
  }
  return true;
}

bool HomeResolver::validate_exists(const std::string &path) {
  if (!validate_shape(path)) {
    return false;
  }
  std::error_code ec;
  const bool exists =
      std::filesystem::is_directory(to_fs_path(path), ec);
  return exists && !ec;
}

bool HomeResolver::validate(const std::string &path) {
  return validate_shape(path) && validate_exists(path);
}

std::string HomeResolver::resolve(const std::string &env_home,
                                  const std::string &env_userprofile,
                                  const std::string &env_appdata,
                                  const std::string &data_dir) {
  return resolve(env_home, env_userprofile, env_appdata, data_dir,
                 current_platform(),
                 DirectoryProbe(&HomeResolver::validate_exists));
}

std::string HomeResolver::resolve(const std::string &env_home,
                                  const std::string &env_userprofile,
                                  const std::string &env_appdata,
                                  const std::string &data_dir,
                                  HostPlatform platform,
                                  const DirectoryProbe &probe) {
  std::vector<std::string> candidates;
  if (!env_home.empty()) {
    candidates.push_back(env_home);
  }
  if (platform == HostPlatform::kWindows && !env_userprofile.empty()) {
    candidates.push_back(env_userprofile);
  }
  if (platform == HostPlatform::kWindows && !env_appdata.empty()) {
    candidates.push_back(parent_dir(env_appdata, 2));
  }
  if (platform == HostPlatform::kPosix && !data_dir.empty()) {
    candidates.push_back(parent_dir(data_dir, 2));
  }
  for (const std::string &candidate : candidates) {
    if (!validate_shape(candidate)) {
      continue;
    }
    if (probe && !probe(candidate)) {
      continue;
    }
    return candidate;
  }
  return std::string();
}

std::string HomeResolver::gda_home() {
  const std::string override_home = read_home_env(kHomeOverrideEnv);
  if (!override_home.empty()) {
    if (!validate_shape(override_home)) {
      LogSystem::instance().log(
          LogLevel::Warning, LogCategory::System,
          "GDA home override is not a usable directory path: " +
              override_home);
    }
    return override_home;
  }

  const std::string env_home = read_home_env(kHomeEnv);
  const std::string env_userprofile = read_home_env(kUserProfileEnv);
  const std::string env_appdata = read_home_env(kAppDataEnv);
  DataDirProvider &provider = data_dir_provider();
  const std::string data_dir =
      provider ? provider()
               : home_default_data_dir(env_home, env_appdata);

  const std::string resolved =
      resolve(env_home, env_userprofile, env_appdata, data_dir);
  if (resolved.empty()) {
    LogSystem::instance().log(LogLevel::Warning, LogCategory::System,
                               "Unable to resolve the GDA home directory");
  }
  return resolved;
}

} // namespace godot_autopilot
