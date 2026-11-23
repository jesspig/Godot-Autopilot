#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

#include "core/transport/api_token.hpp"

namespace {

using godot_autopilot::api_token::constant_time_equals;
using godot_autopilot::api_token::generate_token;
using godot_autopilot::api_token::kTokenBytes;
using godot_autopilot::api_token::random_bytes_slot;
using godot_autopilot::api_token::register_random_bytes;
using godot_autopilot::api_token::to_hex;

struct RandomGuard {
  godot_autopilot::api_token::RandomBytesFn previous;
  RandomGuard() { previous = random_bytes_slot(); }
  ~RandomGuard() { register_random_bytes(previous); }
};

} // namespace

TEST(ApiTokenTest, GeneratesExpectedLengthHexToken) {
  RandomGuard guard;
  register_random_bytes(godot_autopilot::api_token::RandomBytesFn());
  const std::string token = generate_token();
  EXPECT_EQ(token.size(), kTokenBytes * 2);
  for (const char ch : token) {
    EXPECT_TRUE((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'));
  }
}

TEST(ApiTokenTest, GenerationFailureYieldsEmptyToken) {
  RandomGuard guard;
  register_random_bytes([](uint8_t *, size_t) { return false; });
  EXPECT_TRUE(generate_token().empty());
}

TEST(ApiTokenTest, InjectableRandomSourceProducesDeterministicToken) {
  RandomGuard guard;
  register_random_bytes([](uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) {
      buffer[i] = static_cast<uint8_t>(i);
    }
    return true;
  });
  std::array<uint8_t, kTokenBytes> bytes{};
  for (size_t i = 0; i < kTokenBytes; ++i) {
    bytes[i] = static_cast<uint8_t>(i);
  }
  EXPECT_EQ(generate_token(), to_hex(bytes.data(), bytes.size()));
}

TEST(ApiTokenTest, ConstantTimeEqualsRejectsEmptyAndMismatched) {
  EXPECT_FALSE(constant_time_equals("", ""));
  EXPECT_FALSE(constant_time_equals("abc", ""));
  EXPECT_FALSE(constant_time_equals("", "abc"));
  EXPECT_FALSE(constant_time_equals("abc", "abd"));
  EXPECT_FALSE(constant_time_equals("abc", "abcd"));
}

TEST(ApiTokenTest, ConstantTimeEqualsAcceptsIdentical) {
  EXPECT_TRUE(constant_time_equals("secret-token", "secret-token"));
}
