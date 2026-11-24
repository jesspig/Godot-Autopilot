#include "util/persistence_integrity.hpp"

#include <gtest/gtest.h>

using godot_autopilot::persistence::is_false_confirmation;
using godot_autopilot::persistence::persistence_state;
using godot_autopilot::persistence::PersistenceState;

TEST(PersistenceIntegrityTest, MemoryOnlyIsAFalseConfirmation) {
  const auto applied = persistence_state(true, true);
  EXPECT_EQ(applied, PersistenceState::kPersisted);
  EXPECT_FALSE(is_false_confirmation(applied));

  const auto memory_only = persistence_state(true, false);
  EXPECT_EQ(memory_only, PersistenceState::kMemoryOnly);
  EXPECT_TRUE(is_false_confirmation(memory_only));

  const auto not_applied = persistence_state(false, true);
  EXPECT_EQ(not_applied, PersistenceState::kNotApplied);
  EXPECT_FALSE(is_false_confirmation(not_applied));
}
