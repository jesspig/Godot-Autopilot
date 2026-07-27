#include <gtest/gtest.h>

#include "core/log_system.hpp"

#include <string>
#include <thread>
#include <vector>

namespace gsd = godot_self_driving;

TEST(LogSystemTest, EmptyQuery) {
    gsd::LogSystem sys;
    auto result = sys.query({});
    EXPECT_TRUE(result.empty());
}

TEST(LogSystemTest, BasicLogAndQuery) {
    gsd::LogSystem sys;
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::System, "hello world");

    auto result = sys.query({});
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(gsd::LogLevel::Info, result[0]->level);
    EXPECT_EQ(gsd::LogCategory::System, result[0]->category);
    EXPECT_EQ("hello world", result[0]->message);
    EXPECT_NE(std::chrono::system_clock::time_point{}, result[0]->timestamp);
}

TEST(LogSystemTest, LevelFilter) {
    gsd::LogSystem sys;
    sys.log(gsd::LogLevel::Debug, gsd::LogCategory::System, "debug msg");
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::System, "info msg");
    sys.log(gsd::LogLevel::Warning, gsd::LogCategory::System, "warning msg");
    sys.log(gsd::LogLevel::Error, gsd::LogCategory::System, "error msg");

    {
        gsd::LogSystem::Query q;
        q.min_level = gsd::LogLevel::Warning;
        auto result = sys.query(q);
        ASSERT_EQ(2, result.size());
        EXPECT_EQ(gsd::LogLevel::Warning, result[0]->level);
        EXPECT_EQ(gsd::LogLevel::Error, result[1]->level);
    }
    {
        gsd::LogSystem::Query q;
        q.min_level = gsd::LogLevel::Info;
        auto result = sys.query(q);
        ASSERT_EQ(3, result.size());
    }
}

TEST(LogSystemTest, TextFilter) {
    gsd::LogSystem sys;
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::System, "this is a test message");
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::System, "another log entry");

    {
        gsd::LogSystem::Query q;
        q.filter_text = "test";
        auto result = sys.query(q);
        ASSERT_EQ(1, result.size());
        EXPECT_EQ("this is a test message", result[0]->message);
    }
    {
        gsd::LogSystem::Query q;
        q.filter_text = "TEST";
        auto result = sys.query(q);
        ASSERT_EQ(1, result.size());
    }
}

TEST(LogSystemTest, CategoryFilter) {
    gsd::LogSystem sys;
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::System, "sys msg");
    sys.log(gsd::LogLevel::Info, gsd::LogCategory::Transport, "transport msg");

    gsd::LogSystem::Query q;
    q.category = gsd::LogCategory::System;
    auto result = sys.query(q);
    ASSERT_EQ(1, result.size());
    EXPECT_EQ(gsd::LogCategory::System, result[0]->category);
}

TEST(LogSystemTest, RingBufferLimit) {
    gsd::LogSystem sys;
    for (int i = 0; i < gsd::LogSystem::MAX_ENTRIES + 5; ++i) {
        sys.log(gsd::LogLevel::Debug, gsd::LogCategory::System, std::to_string(i));
    }
    auto result = sys.query({});
    ASSERT_EQ(gsd::LogSystem::MAX_ENTRIES, result.size());
    EXPECT_EQ("5", result[0]->message);
}

TEST(LogSystemTest, CallbackNotification) {
    gsd::LogSystem sys;
    gsd::LogLevel captured_level = gsd::LogLevel::Debug;
    gsd::LogCategory captured_category = gsd::LogCategory::System;
    std::string captured_message;
    bool called = false;
    sys.set_on_new_entry([&](const gsd::LogEntry& e) {
        captured_level = e.level;
        captured_category = e.category;
        captured_message = e.message;
        called = true;
    });

    sys.log(gsd::LogLevel::Error, gsd::LogCategory::Tools, "callback msg");
    EXPECT_TRUE(called);
    EXPECT_EQ(gsd::LogLevel::Error, captured_level);
    EXPECT_EQ(gsd::LogCategory::Tools, captured_category);
    EXPECT_EQ("callback msg", captured_message);
}

TEST(LogSystemTest, ThreadSafety) {
    gsd::LogSystem sys;
    constexpr int THREADS = 4;
    constexpr int LOGS_PER_THREAD = 500;
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; ++t) {
        threads.emplace_back([&sys, t] {
            for (int i = 0; i < LOGS_PER_THREAD; ++i) {
                sys.log(gsd::LogLevel::Info, gsd::LogCategory::System,
                        "thread" + std::to_string(t) + "_" + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    auto result = sys.query({});
    EXPECT_EQ(THREADS * LOGS_PER_THREAD, result.size());
}
