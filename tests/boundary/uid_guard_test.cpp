#include <gtest/gtest.h>

namespace godot_autopilot {
namespace resource_ops {
bool uid_needs_add(bool already_registered);
} // namespace resource_ops
} // namespace godot_autopilot

TEST(UidGuardTest, RegistersIdThatIsNotInTheTable) {
  EXPECT_TRUE(godot_autopilot::resource_ops::uid_needs_add(false));
}

TEST(UidGuardTest, UpdatesIdThatIsAlreadyInTheTable) {
  EXPECT_FALSE(godot_autopilot::resource_ops::uid_needs_add(true));
}
