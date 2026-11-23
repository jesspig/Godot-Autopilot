#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "core/resource_registry.hpp"
#include "util/input_guards.hpp"

namespace {

using godot_autopilot::resource_registry::is_object_id_key;
using godot_autopilot::resource_registry::oid_key;
using godot_autopilot::util::parse_decimal_int64;
using godot_autopilot::util::parse_signed_decimal_int64;

constexpr int64_t kFailedObjectId = -9223370482294645949LL;

} // namespace

TEST(ObjectIdReferenceTest, NegativeObjectIdStringSurvivesAssignLookupChain) {
  const std::string exact = std::to_string(kFailedObjectId);
  int64_t parsed = 0;
  ASSERT_TRUE(parse_signed_decimal_int64(exact, parsed));
  EXPECT_EQ(parsed, kFailedObjectId);
  EXPECT_TRUE(is_object_id_key(oid_key(parsed)));
  EXPECT_EQ(oid_key(parsed), exact);
}

TEST(ObjectIdReferenceTest, LegacyDecimalParserKeepsRejectingNegativeInput) {
  int64_t parsed = 0;
  EXPECT_FALSE(parse_decimal_int64("-1", parsed));
  EXPECT_FALSE(parse_decimal_int64(std::to_string(kFailedObjectId), parsed));
  EXPECT_TRUE(parse_decimal_int64("123456789", parsed));
  EXPECT_EQ(parsed, 123456789);
}

TEST(ObjectIdReferenceTest, SignedParserStillRejectsMalformedInput) {
  int64_t parsed = 0;
  EXPECT_FALSE(parse_signed_decimal_int64("", parsed));
  EXPECT_FALSE(parse_signed_decimal_int64("-", parsed));
  EXPECT_FALSE(parse_signed_decimal_int64("+1", parsed));
  EXPECT_FALSE(parse_signed_decimal_int64("12a", parsed));
  EXPECT_TRUE(parse_signed_decimal_int64("0", parsed));
  EXPECT_EQ(parsed, 0);
}
