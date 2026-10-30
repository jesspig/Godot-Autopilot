#include "core/api_token.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#include "core/http_parse.hpp"
#include "core/log_system.hpp"
#include "core/path_util.hpp"
#include "core/project_store.hpp"

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace godot_autopilot {
namespace api_token {

namespace {

std::mutex &path_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::string &cached_token_path() {
  static std::string path;
  return path;
}

std::string compute_token_file_path(const std::string &project_root) {
  if (project_root.empty()) {
    return std::string();
  }
  const std::string dir =
      ProjectStore::project_dir(std::string(), project_root);
  if (dir.empty()) {
    return std::string();
  }
  return join_path(dir, kTokenFileName);
}

bool default_random_bytes(uint8_t *buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return false;
  }
  try {
    std::random_device device;
    for (size_t index = 0; index < size; ++index) {
      buffer[index] = static_cast<uint8_t>(device() & 0xFFu);
    }
  } catch (const std::exception &) {
    return false;
  } catch (...) {
    return false;
  }
  return true;
}

} // namespace

RandomBytesFn &random_bytes_slot() {
  static RandomBytesFn provider;
  return provider;
}

void register_random_bytes(RandomBytesFn provider) {
  random_bytes_slot() = std::move(provider);
}

ProjectRootProviderFn &project_root_slot() {
  static ProjectRootProviderFn provider;
  return provider;
}

void register_project_root_provider(ProjectRootProviderFn provider) {
  project_root_slot() = std::move(provider);
}

std::string to_hex(const uint8_t *bytes, size_t size) {
  static const char kDigits[] = "0123456789abcdef";
  std::string out;
  if (bytes == nullptr) {
    return out;
  }
  out.reserve(size * 2);
  for (size_t index = 0; index < size; ++index) {
    out.push_back(kDigits[(bytes[index] >> 4) & 0x0Fu]);
    out.push_back(kDigits[bytes[index] & 0x0Fu]);
  }
  return out;
}

std::string generate_token() {
  uint8_t buffer[kTokenBytes];
  RandomBytesFn &provider = random_bytes_slot();
  const bool filled = provider ? provider(buffer, kTokenBytes)
                               : default_random_bytes(buffer, kTokenBytes);
  if (!filled) {
    return std::string();
  }
  return to_hex(buffer, kTokenBytes);
}

bool constant_time_equals(const std::string &expected,
                           const std::string &provided) {
  if (expected.empty() || provided.empty()) {
    return false;
  }
  if (expected.size() != provided.size()) {
    return false;
  }
  unsigned char difference = 0;
  for (size_t index = 0; index < expected.size(); ++index) {
    difference = static_cast<unsigned char>(
        difference |
        (static_cast<unsigned char>(expected[index]) ^
         static_cast<unsigned char>(provided[index])));
  }
  return difference == 0;
}

bool parse_bearer_token(
    const std::unordered_map<std::string, std::string> &headers,
    std::string &out) {
  out.clear();
  const std::string *value = http_parse::find_header(headers, "authorization");
  if (value == nullptr) {
    return false;
  }
  const std::string trimmed = http_parse::trim(*value);
  const size_t space = trimmed.find(' ');
  if (space == std::string::npos) {
    return false;
  }
  if (http_parse::to_lower(trimmed.substr(0, space)) != "bearer") {
    return false;
  }
  const std::string token = http_parse::trim(trimmed.substr(space + 1));
  if (token.empty()) {
    return false;
  }
  out = token;
  return true;
}

std::string resolve_token_file_path() {
  ProjectRootProviderFn &provider = project_root_slot();
  const std::string root = provider ? provider() : std::string();
  const std::string path = compute_token_file_path(root);
  std::lock_guard<std::mutex> lock(path_mutex());
  cached_token_path() = path;
  return path;
}

std::string token_file_path() {
  std::lock_guard<std::mutex> lock(path_mutex());
  return cached_token_path();
}

bool write_token_file(const std::string &path, const std::string &token) {
  if (path.empty() || token.empty()) {
    return false;
  }
  const std::filesystem::path target = to_fs_path(path);
  const std::filesystem::path parent = target.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      LogSystem::instance().log(
          LogLevel::Error, LogCategory::System,
          "the API token directory creation failed: " + path);
      return false;
    }
  }
  std::ofstream out(target, std::ios::binary | std::ios::trunc);
  if (!out) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "the API token file write failed: " + path);
    return false;
  }
  out << token;
  out.close();
  if (!out) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "the API token file write failed: " + path);
    return false;
  }
  restrict_token_file_permissions(path);
  return true;
}

bool read_token_file(const std::string &path, std::string &out) {
  out.clear();
  if (path.empty()) {
    return false;
  }
  std::ifstream in(to_fs_path(path), std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  out = http_parse::trim(buffer.str());
  return !out.empty();
}

void restrict_token_file_permissions(const std::string &path) {
  if (path.empty()) {
    return;
  }
#ifndef _WIN32
  if (chmod(to_fs_path(path).c_str(), S_IRUSR | S_IWUSR) != 0) {
    LogSystem::instance().log(
        LogLevel::Warning, LogCategory::System,
        "the API token file permission hardening failed: " + path);
  }
#else
  LogSystem::instance().log(
      LogLevel::Warning, LogCategory::System,
      "the platform has no POSIX permission bits; the API token file "
      "inherits the GDA home directory ACL: " +
          path);
#endif
}

} // namespace api_token
} // namespace godot_autopilot
