#include <gtest/gtest.h>

#include <cstdint>

#include <godot_cpp/classes/editor_undo_redo_manager.hpp>

#include "tools/base/code_exec_ops.hpp"

namespace {

using godot_autopilot::code_exec_ops::batch_resolve_history_id;
using godot_autopilot::code_exec_ops::batch_rollback_report;

constexpr int32_t kGlobalHistory =
    static_cast<int32_t>(godot::EditorUndoRedoManager::GLOBAL_HISTORY);
constexpr int32_t kInvalidHistory =
    static_cast<int32_t>(godot::EditorUndoRedoManager::INVALID_HISTORY);

} // namespace

TEST(BatchRollbackTest, UsesObjectHistoryProvidedByTheEditedScene) {
  EXPECT_EQ(batch_resolve_history_id(3, true), 3);
  EXPECT_EQ(batch_resolve_history_id(kInvalidHistory, true), kGlobalHistory);
  EXPECT_EQ(batch_resolve_history_id(3, false), kGlobalHistory);
}

TEST(BatchRollbackTest, FirstUndoableStepIsFlaggedWhenHistoryVersionDoesNotGrow) {
  const auto report = batch_rollback_report(0, 1, 0);
  EXPECT_EQ(report.expected_actions, 1);
  EXPECT_TRUE(report.partial);
}

TEST(BatchRollbackTest, CompleteRollbackWhenSnapshotHistoryMatchesPropertyWrites) {
  const auto report = batch_rollback_report(1, 1, 1);
  EXPECT_EQ(report.expected_actions, 1);
  EXPECT_FALSE(report.partial);
}

TEST(BatchRollbackTest, NoRollbackExpectedWhenNoUndoableStepSucceeded) {
  const auto report = batch_rollback_report(0, 0, 0);
  EXPECT_EQ(report.expected_actions, 0);
  EXPECT_FALSE(report.partial);
}
