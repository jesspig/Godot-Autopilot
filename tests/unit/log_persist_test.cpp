#include "core/log_persist.hpp"
#include "core/sanitize_policy.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <thread>

using godot_autopilot::LogCategory;
using godot_autopilot::LogLevel;
using godot_autopilot::LogPersist;

namespace {

class SanitizePolicyGuard {
public:
  explicit SanitizePolicyGuard(bool enabled)
      : previous_(godot_autopilot::sanitize_policy::enabled()) {
    godot_autopilot::sanitize_policy::set_enabled(enabled);
  }
  ~SanitizePolicyGuard() { godot_autopilot::sanitize_policy::set_enabled(previous_); }
  SanitizePolicyGuard(const SanitizePolicyGuard &) = delete;
  SanitizePolicyGuard &operator=(const SanitizePolicyGuard &) = delete;

private:
  bool previous_;
};

} // namespace

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

TEST(LogPersistTest, StoreTraceImageBuffersBytesFromNonMainThread) {
    SanitizePolicyGuard guard(false);
    LogPersist &persist = LogPersist::instance();
    const std::size_t entries_before = persist.pending_trace_image_count();
    const uint64_t bytes_before = persist.pending_trace_image_bytes();
    const uint64_t dropped_before = persist.dropped_trace_images();
    std::string ref;
    std::thread worker([&ref]() {
        ref = LogPersist::instance().store_trace_image("span-thread", "cap", "aGVsbG8=");
    });
    worker.join();
    EXPECT_EQ(persist.pending_trace_image_count(), entries_before + 1);
    EXPECT_EQ(persist.pending_trace_image_bytes(), bytes_before + 5);
    EXPECT_EQ(persist.dropped_trace_images(), dropped_before);
    EXPECT_EQ(ref.rfind("images/trace-", 0), 0u);
    EXPECT_NE(ref.find("-span-thread-cap.png"), std::string::npos);
}

TEST(LogPersistTest, StoreTraceImageReturnsEmptyWhenDesensitized) {
    SanitizePolicyGuard guard(true);
    EXPECT_EQ(LogPersist::instance().store_trace_image("span-thread", "cap", "aGVsbG8="),
              std::string());
}
