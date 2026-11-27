#include "core/export_guard.hpp"
#include "tools/base/dispatch.hpp"

#include <gtest/gtest.h>

namespace {

TEST(ExportStateGuardTest, BlocksOnlyWhileExporting) {
  godot_autopilot::ExportState state;
  EXPECT_FALSE(godot_autopilot::ExportGuard::is_exporting());
  {
    godot_autopilot::ExportState::Scope scope(state);
    EXPECT_TRUE(state.exporting());
    EXPECT_TRUE(godot_autopilot::ExportGuard::is_exporting());
  }
  EXPECT_FALSE(state.exporting());
  EXPECT_FALSE(godot_autopilot::ExportGuard::is_exporting());
}

TEST(ExportStateGuardTest, BlockedResultIsStableAndMachineReadable) {
  const auto result = godot_autopilot::dispatch::export_blocked_json_result();
  const auto *error = result.Find("error");
  ASSERT_NE(error, nullptr);
  ASSERT_TRUE(error->IsString());
  EXPECT_EQ(error->GetString(),
            godot_autopilot::dispatch::export_blocked_error_text());
  const auto *code = result.Find("code");
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(code->GetString(), "export_in_progress");
  const auto *retryable = result.Find("retryable");
  ASSERT_NE(retryable, nullptr);
  EXPECT_TRUE(retryable->GetBool());
}

} // namespace
