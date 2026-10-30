#include "core/api_auth.hpp"

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "core/api_token.hpp"
#include "core/log_system.hpp"

namespace godot_autopilot {

std::string ApiAuth::load_token() const {
  const char *env = std::getenv(api_token::kTokenEnvVar);
  if (env != nullptr && *env != '\0') {
    return std::string(env);
  }
  const std::string path = api_token::token_file_path();
  if (path.empty()) {
    return std::string();
  }
  std::string token;
  if (!api_token::read_token_file(path, token)) {
    return std::string();
  }
  return token;
}

std::string ApiAuth::issue_token() const {
  const char *env = std::getenv(api_token::kTokenEnvVar);
  if (env != nullptr && *env != '\0') {
    return std::string(env);
  }
  const std::string token = api_token::generate_token();
  if (token.empty()) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "the API token generation failed");
    return std::string();
  }
  const std::string path = api_token::resolve_token_file_path();
  if (path.empty()) {
    LogSystem::instance().log(
        LogLevel::Error, LogCategory::System,
        "the API token file path is unresolved; the API endpoints stay "
        "unauthenticated");
    return std::string();
  }
  if (!api_token::write_token_file(path, token)) {
    LogSystem::instance().log(LogLevel::Error, LogCategory::System,
                              "the API token file write failed");
    return std::string();
  }
  return token;
}

bool ApiAuth::verify(
    const std::unordered_map<std::string, std::string> &headers) const {
  std::string provided;
  if (!api_token::parse_bearer_token(headers, provided)) {
    return false;
  }
  const std::string expected = load_token();
  if (expected.empty()) {
    return false;
  }
  return api_token::constant_time_equals(expected, provided);
}

} // namespace godot_autopilot
