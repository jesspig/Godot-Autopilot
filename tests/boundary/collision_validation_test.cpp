#include "tools/domains/tileset/collision_validation.hpp"

#include <gtest/gtest.h>

using godot_autopilot::tileset_detail::physics_layer_valid;

TEST(CollisionValidationTest, ValidLayerAccepted) {
  EXPECT_TRUE(physics_layer_valid(0, 2));
  EXPECT_TRUE(physics_layer_valid(1, 2));
}

TEST(CollisionValidationTest, OutOfRangeRejected) {
  EXPECT_FALSE(physics_layer_valid(2, 2));
  EXPECT_FALSE(physics_layer_valid(-1, 2));
  EXPECT_FALSE(physics_layer_valid(0, 0));
}
