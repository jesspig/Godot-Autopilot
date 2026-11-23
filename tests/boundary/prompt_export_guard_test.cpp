#include "prompts/prompt_export_guard.hpp"

#include <gtest/gtest.h>

using godot_autopilot::prompts_detail::should_block_prompt;

TEST(PromptExportGuardTest, BlocksOnlyWhileExporting) {
  EXPECT_TRUE(should_block_prompt(true));
  EXPECT_FALSE(should_block_prompt(false));
}
