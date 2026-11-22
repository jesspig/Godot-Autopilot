#include "tools/domains/property/property_rollback.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using godot_autopilot::property_ops_detail::partition_rollback;

TEST(PropertyRollbackTest, ConfirmedItemsGoToRolledBack) {
  std::vector<std::string> names = {"a", "b", "c"};
  std::vector<bool> restored = {true, true, true};
  auto outcome = partition_rollback(names, restored);
  EXPECT_EQ(outcome.rolled_back, names);
  EXPECT_TRUE(outcome.unrestored.empty());
}

TEST(PropertyRollbackTest, UnconfirmedItemsAreSeparated) {
  std::vector<std::string> names = {"a", "b", "c"};
  std::vector<bool> restored = {true, false, true};
  auto outcome = partition_rollback(names, restored);
  ASSERT_EQ(outcome.rolled_back.size(), 2u);
  EXPECT_EQ(outcome.rolled_back[0], "a");
  EXPECT_EQ(outcome.rolled_back[1], "c");
  ASSERT_EQ(outcome.unrestored.size(), 1u);
  EXPECT_EQ(outcome.unrestored[0], "b");
}

TEST(PropertyRollbackTest, RestoredShorterThanNamesIsTreatedAsUnrestored) {
  std::vector<std::string> names = {"a", "b"};
  std::vector<bool> restored = {true};
  auto outcome = partition_rollback(names, restored);
  EXPECT_EQ(outcome.rolled_back.size(), 1u);
  EXPECT_EQ(outcome.unrestored.size(), 1u);
}
