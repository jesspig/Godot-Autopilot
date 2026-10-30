#ifndef GODOT_AUTOPILOT_API_TOKEN_HPP
#define GODOT_AUTOPILOT_API_TOKEN_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace godot_autopilot {
namespace api_token {

constexpr size_t kTokenBytes = 32;
constexpr const char *kTokenEnvVar = "GODOT_AUTOPILOT_API_TOKEN";
constexpr const char *kTokenFileName = "api_token";

using RandomBytesFn = std::function<bool(uint8_t *, size_t)>;
using ProjectRootProviderFn = std::function<std::string()>;

RandomBytesFn &random_bytes_slot();
void register_random_bytes(RandomBytesFn provider);
ProjectRootProviderFn &project_root_slot();
void register_project_root_provider(ProjectRootProviderFn provider);

std::string to_hex(const uint8_t *bytes, size_t size);
std::string generate_token();
bool constant_time_equals(const std::string &expected,
                           const std::string &provided);
bool parse_bearer_token(
    const std::unordered_map<std::string, std::string> &headers,
    std::string &out);
std::string resolve_token_file_path();
std::string token_file_path();
bool write_token_file(const std::string &path, const std::string &token);
bool read_token_file(const std::string &path, std::string &out);
void restrict_token_file_permissions(const std::string &path);

} // namespace api_token
} // namespace godot_autopilot

#endif
