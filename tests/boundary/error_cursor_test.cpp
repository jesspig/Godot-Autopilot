#include "runtime/error_cursor.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace {

using godot_autopilot::runtime::error_cursor::error_seq_after;
using godot_autopilot::runtime::error_cursor::next_error_seq;

} // namespace

TEST(ErrorCursorTest, ErrorSeqAfterIsExclusive) {
  EXPECT_TRUE(error_seq_after(1, 0));
  EXPECT_TRUE(error_seq_after(2, 1));
  EXPECT_FALSE(error_seq_after(0, 0));
  EXPECT_FALSE(error_seq_after(5, 5));
  EXPECT_FALSE(error_seq_after(4, 5));
}

TEST(ErrorCursorTest, ErrorSeqAfterHandlesZeroAndNegativeBounds) {
  EXPECT_TRUE(error_seq_after(0, -1));
  EXPECT_FALSE(error_seq_after(-1, -1));
  EXPECT_FALSE(error_seq_after(-2, -1));
  EXPECT_TRUE(error_seq_after(-1, -2));
}

TEST(ErrorCursorTest, NextErrorSeqPrefersReturnedMaximum) {
  EXPECT_EQ(next_error_seq(7, 3), 7);
  EXPECT_EQ(next_error_seq(3, 7), 3);
  EXPECT_EQ(next_error_seq(5, 5), 5);
}

TEST(ErrorCursorTest, NextErrorSeqFallsBackToCurrentWhenNothingReturned) {
  EXPECT_EQ(next_error_seq(0, 9), 9);
  EXPECT_EQ(next_error_seq(0, 0), 0);
  EXPECT_EQ(next_error_seq(-1, 5), 5);
}

TEST(ErrorCursorTest, NextErrorSeqKeepsInt64Boundaries) {
  constexpr int64_t kMax = std::numeric_limits<int64_t>::max();
  EXPECT_EQ(next_error_seq(kMax, 0), kMax);
  EXPECT_EQ(next_error_seq(0, kMax), kMax);
}
