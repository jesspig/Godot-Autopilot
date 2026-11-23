#ifndef GODOT_AUTOPILOT_HASH_UTIL_HPP
#define GODOT_AUTOPILOT_HASH_UTIL_HPP

#include <cstdint>
#include <cstdio>
#include <string>

namespace godot_autopilot {
namespace util {

inline constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
inline constexpr uint64_t kFnvPrime = 1099511628211ULL;

inline uint64_t fnv1a64(const std::string &bytes) {
  uint64_t hash = kFnvOffsetBasis;
  for (unsigned char c : bytes) {
    hash ^= static_cast<uint64_t>(c);
    hash *= kFnvPrime;
  }
  return hash;
}

inline uint64_t fnv1a64_chars(const char *data, size_t size) {
  uint64_t hash = kFnvOffsetBasis;
  for (size_t i = 0; i < size; ++i) {
    hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
    hash *= kFnvPrime;
  }
  return hash;
}

inline std::string fnv1a64_hex(const std::string &bytes) {
  static const char digits[] = "0123456789abcdef";
  uint64_t hash = fnv1a64(bytes);
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<size_t>(i)] = digits[hash & 0xFULL];
    hash >>= 4;
  }
  return out;
}

inline std::string fnv1a64_hex_printf(const std::string &bytes) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx",
                static_cast<unsigned long long>(fnv1a64(bytes)));
  return std::string(buf);
}

} // namespace util
} // namespace godot_autopilot

#endif
