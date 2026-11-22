#include "tools/domains/os/trash_confirm.hpp"

#include <gtest/gtest.h>

using godot_autopilot::os_ops_detail::trash_needs_confirm;

TEST(TrashConfirmTest, TrashRequiresConfirm) {
  EXPECT_TRUE(trash_needs_confirm("trash", false));
  EXPECT_FALSE(trash_needs_confirm("trash", true));
}

TEST(TrashConfirmTest, OtherActionsUnaffected) {
  EXPECT_FALSE(trash_needs_confirm("write", false));
  EXPECT_FALSE(trash_needs_confirm("copy", false));
  EXPECT_FALSE(trash_needs_confirm("ensure", false));
}
