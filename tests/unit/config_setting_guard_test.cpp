#include <gtest/gtest.h>

#include <string>

#include "tools/domains/config/config_ops.hpp"

TEST(ConfigSettingGuardTest, ProtectedPrefixesRejected) {
  EXPECT_TRUE(
      godot_autopilot::config_ops::setting_key_is_protected("autoload/Foo"));
  EXPECT_TRUE(godot_autopilot::config_ops::setting_key_is_protected(
      "autoload/Singleton"));
  EXPECT_TRUE(godot_autopilot::config_ops::setting_key_is_protected("autoload/"));
}

TEST(ConfigSettingGuardTest, OrdinaryKeysAllowed) {
  EXPECT_FALSE(godot_autopilot::config_ops::setting_key_is_protected(
      "application/config/name"));
  EXPECT_FALSE(godot_autopilot::config_ops::setting_key_is_protected(
      "rendering/quality/foo"));
  EXPECT_FALSE(godot_autopilot::config_ops::setting_key_is_protected(""));
  EXPECT_FALSE(
      godot_autopilot::config_ops::setting_key_is_protected("my_autoload/X"));
  EXPECT_FALSE(godot_autopilot::config_ops::setting_key_is_protected("autoload"));
}
