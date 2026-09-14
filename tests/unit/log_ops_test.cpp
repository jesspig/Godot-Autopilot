#include "tools/log_ops.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

std::vector<std::string> sample_lines() {
  return {"INFO: boot complete", "WARN: disk almost full",
          "INFO: asset loaded", "error: low level failure",
          "INFO: disk check passed"};
}

} // namespace

TEST(LogOpsFilterTest, FilterEmptyReturnsAll) {
  const std::vector<std::string> lines = sample_lines();
  const std::vector<std::string> result =
      godot_autopilot::log_ops::filter_log_lines(lines, "", 2);
  EXPECT_EQ(result, lines);
}

TEST(LogOpsFilterTest, FilterMatchesSubstring) {
  const std::vector<std::string> lines = sample_lines();
  const std::vector<std::string> upper =
      godot_autopilot::log_ops::filter_log_lines(lines, "INFO", 0);
  ASSERT_EQ(upper.size(), 3u);
  EXPECT_EQ(upper[0], "INFO: boot complete");
  EXPECT_EQ(upper[1], "INFO: asset loaded");
  EXPECT_EQ(upper[2], "INFO: disk check passed");

  const std::vector<std::string> lower =
      godot_autopilot::log_ops::filter_log_lines(lines, "info", 0);
  EXPECT_TRUE(lower.empty());

  const std::vector<std::string> single =
      godot_autopilot::log_ops::filter_log_lines(lines, "error", 0);
  ASSERT_EQ(single.size(), 1u);
  EXPECT_EQ(single[0], "error: low level failure");
}

TEST(LogOpsFilterTest, FilterKeepsLastLimit) {
  const std::vector<std::string> lines = {"INFO one", "INFO two", "INFO three",
                                          "WARN unrelated"};
  const std::vector<std::string> result =
      godot_autopilot::log_ops::filter_log_lines(lines, "INFO", 2);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0], "INFO two");
  EXPECT_EQ(result[1], "INFO three");
}

TEST(LogOpsFilterTest, FilterLimitZeroOrNegativeReturnsAllMatches) {
  const std::vector<std::string> lines = {"INFO one", "WARN unrelated",
                                          "INFO two", "INFO three"};
  const std::vector<std::string> zero =
      godot_autopilot::log_ops::filter_log_lines(lines, "INFO", 0);
  ASSERT_EQ(zero.size(), 3u);
  const std::vector<std::string> negative =
      godot_autopilot::log_ops::filter_log_lines(lines, "INFO", -1);
  EXPECT_EQ(negative, zero);
}
