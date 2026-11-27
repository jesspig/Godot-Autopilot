#ifndef GODOT_AUTOPILOT_SHA256_HPP
#define GODOT_AUTOPILOT_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace godot_autopilot {
namespace util {

namespace sha256_detail {

inline uint32_t rotr(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32 - bits));
}


inline std::array<uint8_t, 32> hash(const std::string_view &input) {
  static constexpr std::array<uint32_t, 64> kConstants = {
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
      0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
      0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
      0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
      0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
      0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
      0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
      0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
      0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
      0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
  uint32_t h0 = 0x6a09e667u;
  uint32_t h1 = 0xbb67ae85u;
  uint32_t h2 = 0x3c6ef372u;
  uint32_t h3 = 0xa54ff53au;
  uint32_t h4 = 0x510e527fu;
  uint32_t h5 = 0x9b05688cu;
  uint32_t h6 = 0x1f83d9abu;
  uint32_t h7 = 0x5be0cd19u;
  const std::size_t padded = ((input.size() + 9 + 63) / 64) * 64;
  for (std::size_t offset = 0; offset < padded; offset += 64) {
    std::array<uint32_t, 64> words{};
    for (std::size_t i = 0; i < 16; ++i) {
      uint32_t word = 0;
      for (std::size_t byte = 0; byte < 4; ++byte) {
        const std::size_t index = offset + i * 4 + byte;
        const uint8_t value = index < input.size()
                                  ? static_cast<uint8_t>(input[index])
                                  : (index == input.size() ? 0x80 : 0);
        word = (word << 8) | value;
      }
      words[i] = word;
    }
    if (offset + 64 == padded) {
      const uint64_t bits = static_cast<uint64_t>(input.size()) * 8;
      words[14] = static_cast<uint32_t>(bits >> 32);
      words[15] = static_cast<uint32_t>(bits);
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const uint32_t s0 = rotr(words[i - 15], 7) ^ rotr(words[i - 15], 18) ^
                          (words[i - 15] >> 3);
      const uint32_t s1 = rotr(words[i - 2], 17) ^ rotr(words[i - 2], 19) ^
                          (words[i - 2] >> 10);
      words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;
    uint32_t e = h4;
    uint32_t f = h5;
    uint32_t g = h6;
    uint32_t h = h7;
    for (std::size_t i = 0; i < 64; ++i) {
      const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      const uint32_t choose = (e & f) ^ (~e & g);
      const uint32_t temp1 = h + s1 + choose + kConstants[i] + words[i];
      const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const uint32_t temp2 = s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temp1;
      d = c;
      c = b;
      b = a;
      a = temp1 + temp2;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
    h5 += f;
    h6 += g;
    h7 += h;
  }
  std::array<uint8_t, 32> digest{};
  const std::array<uint32_t, 8> state = {h0, h1, h2, h3, h4, h5, h6, h7};
  for (std::size_t i = 0; i < 8; ++i) {
    digest[i * 4] = static_cast<uint8_t>(state[i] >> 24);
    digest[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
    digest[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
    digest[i * 4 + 3] = static_cast<uint8_t>(state[i]);
  }
  return digest;
}

} // namespace sha256_detail

inline std::string sha256_hex(std::string_view content) {
  static const char digits[] = "0123456789abcdef";
  const auto digest = sha256_detail::hash(content);
  std::string out;
  out.reserve(digest.size() * 2);
  for (uint8_t byte : digest) {
    out.push_back(digits[byte >> 4]);
    out.push_back(digits[byte & 0x0F]);
  }
  return out;
}

} // namespace util
} // namespace godot_autopilot

#endif
