#include "util/api_host_context.hpp"

#include <gtest/gtest.h>

using godot_autopilot::util::api_host_process_alive;
using godot_autopilot::util::api_host_process_id;
using godot_autopilot::util::api_host_project_root;

TEST(ApiHostContextTest, HeadlessFallbacksWhenGodotApiIsUnavailable) {
  if (godot_autopilot::util::godot_api_available()) {
    GTEST_SKIP() << "Godot API is initialized in this test process";
  }
  EXPECT_EQ(api_host_process_id(), 0);
  EXPECT_TRUE(api_host_project_root().empty());
  EXPECT_FALSE(api_host_process_alive(1));
}
