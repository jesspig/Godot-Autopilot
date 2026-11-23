#include "tools/base/call_depth_guard.hpp"

#include <gtest/gtest.h>

using godot_autopilot::meta_detail::call_depth_exceeds;

TEST(CallDepthGuardTest, DepthBelowMaxAllowed) {
  EXPECT_FALSE(call_depth_exceeds(0, 8));
  EXPECT_FALSE(call_depth_exceeds(7, 8));
}

TEST(CallDepthGuardTest, DepthAtOrAboveMaxRejected) {
  EXPECT_TRUE(call_depth_exceeds(8, 8));
  EXPECT_TRUE(call_depth_exceeds(9, 8));
}
