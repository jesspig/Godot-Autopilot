#include "core/command_queue.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>

using godot_autopilot::CommandQueue;

TEST(CommandQueueTest, SubmitDrainReturnsValuesInOrder) {
    CommandQueue q;
    auto f1 = q.submit([] { return 1; });
    auto f2 = q.submit([] { return 2; });
    auto f3 = q.submit([] { return 3; });
    q.drain();
    EXPECT_EQ(f1.get(), 1);
    EXPECT_EQ(f2.get(), 2);
    EXPECT_EQ(f3.get(), 3);
}

TEST(CommandQueueTest, VoidTaskRunsAndCompletes) {
    CommandQueue q;
    bool ran = false;
    auto f = q.submit([&ran] { ran = true; });
    q.drain();
    f.get();
    EXPECT_TRUE(ran);
}

TEST(CommandQueueTest, ExceptionPropagatesThroughFuture) {
    CommandQueue q;
    auto f = q.submit([]() -> int { throw std::runtime_error("boom"); });
    q.drain();
    EXPECT_THROW(f.get(), std::runtime_error);
}

TEST(CommandQueueTest, CrossThreadSubmitDrainedOnMainThread) {
    CommandQueue q;
    std::future<int> f1;
    std::future<int> f2;
    std::thread worker([&] {
        f1 = q.submit([] { return 10; });
        f2 = q.submit([] { return 20; });
    });
    worker.join();
    q.drain();
    EXPECT_EQ(f1.get(), 10);
    EXPECT_EQ(f2.get(), 20);
}

TEST(CommandQueueTest, TasksExecuteOnDrainingThread) {
    CommandQueue q;
    std::thread::id executed;
    auto f = q.submit([&executed] { executed = std::this_thread::get_id(); });
    std::thread drainer([&] { q.drain(); });
    const auto drainer_id = drainer.get_id();
    drainer.join();
    f.get();
    EXPECT_EQ(executed, drainer_id);
    EXPECT_NE(executed, std::this_thread::get_id());
}

TEST(CommandQueueTest, IsMainThreadFlipsAfterDrain) {
    CommandQueue q;
    EXPECT_FALSE(q.is_main_thread());
    q.drain();
    EXPECT_TRUE(q.is_main_thread());
    std::thread other([&] { EXPECT_FALSE(q.is_main_thread()); });
    other.join();
}

TEST(CommandQueueTest, EmptyDrainIsHarmless) {
    CommandQueue q;
    q.drain();
    q.drain();
    EXPECT_TRUE(q.is_main_thread());
}

TEST(CommandQueueTest, CancelRemovesQueuedTaskAndRejectsFuture) {
    CommandQueue q;
    bool ran = false;
    auto tracked = q.submit_tracked([&ran] { ran = true; });
    EXPECT_TRUE(q.cancel(tracked.id));
    q.drain();
    EXPECT_FALSE(ran);
    EXPECT_THROW(tracked.future.get(), std::runtime_error);
    EXPECT_EQ(q.stats().cancelled, 1u);
}

TEST(CommandQueueTest, CancelExecutedTaskReturnsFalse) {
    CommandQueue q;
    bool ran = false;
    auto tracked = q.submit_tracked([&ran] { ran = true; });
    q.drain();
    EXPECT_TRUE(ran);
    EXPECT_FALSE(q.cancel(tracked.id));
    tracked.future.get();
}

TEST(CommandQueueTest, CancelUnknownIdReturnsFalse) {
    CommandQueue q;
    EXPECT_FALSE(q.cancel(12345));
    EXPECT_FALSE(q.cancel(0));
}

TEST(CommandQueueTest, LastDrainAgeAdvancesAfterDrain) {
    CommandQueue q;
    EXPECT_EQ(q.last_drain_age_ms(), -1);
    q.drain();
    EXPECT_GE(q.last_drain_age_ms(), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    EXPECT_GE(q.last_drain_age_ms(), 1);
}
