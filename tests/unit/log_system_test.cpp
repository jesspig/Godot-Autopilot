#include "core/log_system.hpp"

#include <gtest/gtest.h>

#include <string>

using godot_autopilot::LogCategory;
using godot_autopilot::LogEntry;
using godot_autopilot::LogLevel;
using godot_autopilot::LogSystem;

TEST(LogSystemTest, InstanceIsSingleton) {
    EXPECT_EQ(&LogSystem::instance(), &LogSystem::instance());
}

TEST(LogSystemTest, QueryFiltersByMinLevel) {
    LogSystem& log = LogSystem::instance();
    log.log(LogLevel::Debug, LogCategory::System, "LEVELMARK_debug_1");
    log.log(LogLevel::Warning, LogCategory::System, "LEVELMARK_warning_1");
    log.log(LogLevel::Error, LogCategory::System, "LEVELMARK_error_1");

    LogSystem::Query q;
    q.filter_text = "LEVELMARK_";
    EXPECT_EQ(log.query(q).size(), 3u);

    q.min_level = LogLevel::Warning;
    auto results = log.query(q);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].level, LogLevel::Warning);
    EXPECT_EQ(results[1].level, LogLevel::Error);
}

TEST(LogSystemTest, QueryFiltersByCategory) {
    LogSystem& log = LogSystem::instance();
    log.log(LogLevel::Info, LogCategory::System, "CATMARK_sys_1");
    log.log(LogLevel::Info, LogCategory::Tools, "CATMARK_tools_1");

    LogSystem::Query q;
    q.filter_text = "CATMARK_";
    q.category = LogCategory::System;
    auto results = log.query(q);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].category, LogCategory::System);
    EXPECT_EQ(results[0].message, "CATMARK_sys_1");
}

TEST(LogSystemTest, QueryFilterTextIsCaseInsensitive) {
    LogSystem& log = LogSystem::instance();
    log.log(LogLevel::Info, LogCategory::System, "CaseMixAbC_1");
    LogSystem::Query q;
    q.filter_text = "cAsEmIx";
    auto results = log.query(q);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].message, "CaseMixAbC_1");
}

TEST(LogSystemTest, RingBufferOverwritesOldestAtMaxEntries) {
    LogSystem& log = LogSystem::instance();
    const int total = LogSystem::MAX_ENTRIES + 50;
    for (int i = 0; i < total; ++i) {
        log.log(LogLevel::Debug, LogCategory::System, "RINGMARK_" + std::to_string(i));
    }
    LogSystem::Query q;
    q.filter_text = "RINGMARK_0";
    EXPECT_TRUE(log.query(q).empty());
    q.filter_text = "RINGMARK_" + std::to_string(total - 1);
    EXPECT_FALSE(log.query(q).empty());
}

TEST(LogSystemTest, QueryReturnsValueCopyNotDanglingPointer) {
    LogSystem& log = LogSystem::instance();
    log.log(LogLevel::Info, LogCategory::System, "SNAPSHOT_test_1");
    auto snap = log.query_recent(1);
    ASSERT_EQ(snap.size(), 1u);
    std::string msg = snap[0].message;
    for (int i = 0; i < LogSystem::MAX_ENTRIES + 10; ++i) {
        log.log(LogLevel::Info, LogCategory::System, "FLOOD_" + std::to_string(i));
    }
    EXPECT_EQ(msg, "SNAPSHOT_test_1");
    EXPECT_EQ(snap[0].message, "SNAPSHOT_test_1");
}

TEST(LogSystemTest, QueryRecentLimitsResults) {
    LogSystem& log = LogSystem::instance();
    for (int i = 0; i < 5; ++i) log.log(LogLevel::Info, LogCategory::System, "RECENT_" + std::to_string(i));
    auto recent = log.query_recent(2);
    EXPECT_LE(recent.size(), 2u);
}

TEST(LogSystemTest, DetailedLogCarriesTraceAndSpanIdsIntoFilter) {
    LogSystem& log = LogSystem::instance();
    const std::string trace_marker = "LOGTRACEUNIQ_5ab1";
    const std::string span_marker = "LOGSPANUNIQ_5ab1";
    log.log_detailed(LogLevel::Info, LogCategory::System, "LOGDETAILMARK_5ab1",
                     "detail body", trace_marker, span_marker);

    LogSystem::Query q;
    q.filter_text = trace_marker;
    auto by_trace = log.query(q);
    ASSERT_FALSE(by_trace.empty());
    bool found = false;
    for (const auto& entry : by_trace) {
        if (entry.trace_id == trace_marker && entry.span_id == span_marker) found = true;
    }
    EXPECT_TRUE(found);

    LogSystem::Query q_span;
    q_span.filter_text = span_marker;
    auto by_span = log.query(q_span);
    ASSERT_FALSE(by_span.empty());
    EXPECT_EQ(by_span[0].span_id, span_marker);
}

TEST(LogSystemTest, LegacyDetailedLogLeavesTraceIdEmpty) {
    LogSystem& log = LogSystem::instance();
    const std::string marker = "LEGACYLOGMARK_6cd2";
    log.log_detailed(LogLevel::Info, LogCategory::System, marker, "legacy detail");
    LogSystem::Query q;
    q.filter_text = marker;
    auto results = log.query(q);
    ASSERT_FALSE(results.empty());
    EXPECT_TRUE(results[0].trace_id.empty());
    EXPECT_TRUE(results[0].span_id.empty());
}
