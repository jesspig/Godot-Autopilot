#include "core/log_persist.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::LogCategory;
using godot_autopilot::LogLevel;
using godot_autopilot::LogPersist;

TEST(LogPersistTest, FormatHumanLineAppendsDetailOnlyWhenRequested) {
    const std::string without_detail =
        LogPersist::format_human_line(1234, "info", "system", "summary", "extra", false);
    EXPECT_EQ(without_detail, "[1234] [info] [system] summary");
    const std::string with_detail =
        LogPersist::format_human_line(1234, "info", "system", "summary", "extra", true);
    EXPECT_EQ(with_detail, "[1234] [info] [system] summary | extra");
    const std::string empty_detail =
        LogPersist::format_human_line(1234, "info", "system", "summary", "", true);
    EXPECT_EQ(empty_detail, "[1234] [info] [system] summary");
}

TEST(LogPersistTest, SessionStampIsUtcFromEpochMilliseconds) {
    EXPECT_EQ(LogPersist::session_stamp_from_ticks(0), "19700101_000000");
    EXPECT_EQ(LogPersist::session_stamp_from_ticks(1000), "19700101_000001");
    EXPECT_EQ(LogPersist::session_stamp_from_ticks(86400000ULL), "19700102_000000");
    EXPECT_EQ(LogPersist::session_stamp_from_ticks(1234567890000ULL), "20090213_233130");
    EXPECT_EQ(LogPersist::session_stamp_from_ticks(1704067200000ULL), "20240101_000000");
}

TEST(LogPersistTest, ShouldPruneBoundaries) {
    const uint64_t max_bytes = LogPersist::kMaxTotalBytes;
    EXPECT_FALSE(LogPersist::should_prune(20, 0));
    EXPECT_FALSE(LogPersist::should_prune(20, max_bytes));
    EXPECT_TRUE(LogPersist::should_prune(21, 1));
    EXPECT_TRUE(LogPersist::should_prune(21, max_bytes));
    EXPECT_TRUE(LogPersist::should_prune(20, max_bytes + 1));
}

TEST(LogPersistTest, LevelNameCoversAllLevels) {
    EXPECT_EQ(LogPersist::level_name(LogLevel::Debug), "debug");
    EXPECT_EQ(LogPersist::level_name(LogLevel::Info), "info");
    EXPECT_EQ(LogPersist::level_name(LogLevel::Warning), "warning");
    EXPECT_EQ(LogPersist::level_name(LogLevel::Error), "error");
    EXPECT_EQ(LogPersist::level_name(static_cast<LogLevel>(42)), "unknown");
}

TEST(LogPersistTest, CategoryNameCoversAllCategories) {
    EXPECT_EQ(LogPersist::category_name(LogCategory::System), "system");
    EXPECT_EQ(LogPersist::category_name(LogCategory::Transport), "transport");
    EXPECT_EQ(LogPersist::category_name(LogCategory::Tools), "tools");
    EXPECT_EQ(LogPersist::category_name(LogCategory::Resources), "resources");
    EXPECT_EQ(LogPersist::category_name(LogCategory::Prompts), "prompts");
    EXPECT_EQ(LogPersist::category_name(static_cast<LogCategory>(42)), "unknown");
}

TEST(LogPersistTest, TraceDirUsesFixedUserPath) {
    EXPECT_EQ(LogPersist::instance().trace_dir(), "user://godot_autopilot/traces");
}

TEST(LogPersistTest, NewAccessorsDefaultToZero) {
    LogPersist& persist = LogPersist::instance();
    EXPECT_EQ(persist.dropped_log_lines(), 0u);
    EXPECT_EQ(persist.dropped_trace_lines(), 0u);
    EXPECT_EQ(persist.bytes_written(), 0u);
    EXPECT_EQ(persist.flush_failures(), 0u);
    EXPECT_EQ(persist.rotations(), 0u);
    EXPECT_EQ(persist.pruned_files(), 0u);
}

TEST(LogPersistTest, HealthSummaryCarriesObservabilityKeys) {
    const std::string summary = LogPersist::instance().health_summary();
    EXPECT_NE(summary.find("dropped_log_lines"), std::string::npos);
    EXPECT_NE(summary.find("flush_failures"), std::string::npos);
    EXPECT_NE(summary.find("bytes_written"), std::string::npos);
}
