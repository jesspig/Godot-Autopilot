#ifndef GODOT_AUTOPILOT_API_TOKEN_HPP
#define GODOT_AUTOPILOT_API_TOKEN_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace godot_autopilot {
namespace api_token {

constexpr size_t kTokenBytes = 32;

using RandomBytesFn = std::function<bool(uint8_t *, size_t)>;

RandomBytesFn &random_bytes_slot();
void register_random_bytes(RandomBytesFn provider);

std::string to_hex(const uint8_t *bytes, size_t size);
std::string generate_token();
bool constant_time_equals(const std::string &expected,
                          const std::string &provided);

} // namespace api_token
} // namespace godot_autopilot

#endif
