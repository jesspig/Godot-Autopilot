#ifndef GODOT_AUTOPILOT_ADAPTERS_MCP_SHA256_HPP
#define GODOT_AUTOPILOT_ADAPTERS_MCP_SHA256_HPP

#include <mcp/detail/sha256.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace godot_autopilot {
namespace adapters {

namespace sdk = mcp;

inline std::string Sha256Hex(std::string_view content) {
  static const char digits[] = "0123456789abcdef";
  const auto digest = sdk::detail::Sha256::Hash(content);
  std::string out;
  out.reserve(digest.size() * 2);
  for (uint8_t byte : digest) {
    out.push_back(digits[byte >> 4]);
    out.push_back(digits[byte & 0x0F]);
  }
  return out;
}

} // namespace adapters
} // namespace godot_autopilot

#endif
