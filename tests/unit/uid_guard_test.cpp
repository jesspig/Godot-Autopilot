#include <gtest/gtest.h>

// resource_ops.cpp defines uid_needs_add() at namespace scope (outside its
// anonymous namespace) so the L1 binary that compiles src/tools/*_ops.cpp can
// link it: the ResourceUID write selection is pinned to table membership, an id
// already in the table must be updated, a new id must be registered.
// The ops header declares only handlers, so the symbol is declared here.
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
