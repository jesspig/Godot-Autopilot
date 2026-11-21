#include <gtest/gtest.h>

#include <string>

#include "util/variant_json.hpp"

TEST(DeserializationGuardTest, NodeRefPathRejectsTraversal) {
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe(".."));
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe("../Other"));
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe("Node/../Other"));
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe("Node/.."));
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe("Node/./../Other"));
  EXPECT_FALSE(godot_autopilot::node_ref_path_is_safe(""));
}

TEST(DeserializationGuardTest, NodeRefPathAcceptsPlainPaths) {
  EXPECT_TRUE(godot_autopilot::node_ref_path_is_safe("Player"));
  EXPECT_TRUE(godot_autopilot::node_ref_path_is_safe("Root/Player/Sprite2D"));
  EXPECT_TRUE(godot_autopilot::node_ref_path_is_safe("Node/./Child"));
  EXPECT_TRUE(godot_autopilot::node_ref_path_is_safe("..hidden"));
  EXPECT_TRUE(godot_autopilot::node_ref_path_is_safe("visible..name"));
}
