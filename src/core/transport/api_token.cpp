#include "core/transport/api_token.hpp"

#include <cstdint>
#include <random>
#include <string>
#include <utility>

namespace godot_autopilot {
namespace api_token {

namespace {

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

} // namespace api_token
} // namespace godot_autopilot
