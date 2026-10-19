#include "core/export_guard.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using godot_autopilot::ExportState;

TEST(ExportStateTest, BeginAndEndToggleExportingFlag) {
  ExportState state;
  EXPECT_FALSE(state.exporting());
  state.begin();
  EXPECT_TRUE(state.exporting());
  state.end();
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, ScopeResetsFlagWhenLeavingAbnormally) {
  ExportState state;
  try {
    ExportState::Scope scope(state);
    EXPECT_TRUE(state.exporting());
    throw std::runtime_error("export begin failed");
  } catch (const std::exception &) {
  }
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, ReleasedScopeKeepsFlagUntilEnd) {
  ExportState state;
  {
    ExportState::Scope scope(state);
    scope.release();
  }
  EXPECT_TRUE(state.exporting());
  state.end();
  EXPECT_FALSE(state.exporting());
}

TEST(ExportStateTest, InstancesAreIsolated) {
  ExportState first;
  ExportState second;
  first.begin();
  EXPECT_TRUE(first.exporting());
  EXPECT_FALSE(second.exporting());
  first.end();
  EXPECT_FALSE(first.exporting());
  EXPECT_FALSE(second.exporting());
}
