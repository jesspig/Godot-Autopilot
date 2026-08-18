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
    EXPECT_EQ(results[0]->level, LogLevel::Warning);
    EXPECT_EQ(results[1]->level, LogLevel::Error);
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
    EXPECT_EQ(results[0]->category, LogCategory::System);
    EXPECT_EQ(results[0]->message, "CATMARK_sys_1");
}

TEST(LogSystemTest, QueryFilterTextIsCaseInsensitive) {
    LogSystem& log = LogSystem::instance();
    log.log(LogLevel::Info, LogCategory::System, "CaseMixAbC_1");
    LogSystem::Query q;
    q.filter_text = "cAsEmIx";
    auto results = log.query(q);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0]->message, "CaseMixAbC_1");
}

TEST(LogSystemTest, OnNewEntryCallbackReceivesEntry) {
    LogSystem& log = LogSystem::instance();
    bool called = false;
    std::string received;
    LogCategory received_category = LogCategory::System;
    log.set_on_new_entry([&](const LogEntry& e) {
        called = true;
        received = e.message;
        received_category = e.category;
    });
    log.log(LogLevel::Info, LogCategory::Prompts, "CBMARK_hello");
    EXPECT_TRUE(called);
    EXPECT_EQ(received, "CBMARK_hello");
    EXPECT_EQ(received_category, LogCategory::Prompts);
    log.set_on_new_entry({});
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
