#include "tools/domains/config/config_ops.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::config_ops::setting_key_is_protected;

TEST(ProtectedKeyTest, LowercaseAutoloadProtected) {
  EXPECT_TRUE(setting_key_is_protected("autoload/Player"));
}

TEST(ProtectedKeyTest, CaseVariantsProtected) {
  EXPECT_TRUE(setting_key_is_protected("Autoload/Player"));
  EXPECT_TRUE(setting_key_is_protected("AUTOLOAD/Player"));
  EXPECT_TRUE(setting_key_is_protected("AuToLoAd/Player"));
}

TEST(ProtectedKeyTest, UnrelatedKeysNotProtected) {
  EXPECT_FALSE(setting_key_is_protected("application/run/main_scene"));
  EXPECT_FALSE(setting_key_is_protected("autoload"));
}
